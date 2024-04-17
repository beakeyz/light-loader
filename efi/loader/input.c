
#include "efiapi.h"
#include "eficon.h"
#include "efidef.h"
#include "efidevp.h"
#include "efierr.h"
#include "efilib.h"
#include "efipoint.h"
#include "gfx.h"
#include "heap.h"
#include "key.h"
#include "mouse.h"
#include "stddef.h"
#include "stdint.h"
#include <memory.h>

EFI_GUID dev_path_guid = DEVICE_PATH_PROTOCOL;
EFI_GUID input_guid = SIMPLE_TEXT_INPUT_PROTOCOL;
EFI_GUID pointer_guid = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
EFI_GUID abs_pointer_guid = EFI_ABSOLUTE_POINTER_PROTOCOL_GUID;
EFI_SIMPLE_POINTER_PROTOCOL* pointer_protocol;
EFI_ABSOLUTE_POINTER_PROTOCOL* abs_pointer_protocol;
EFI_SIMPLE_TEXT_IN_PROTOCOL* text_in_protocol;

void
efi_init_mouse()
{
  light_gfx_t* gfx;
  get_light_gfx(&gfx);

  /* Make sure the limits are set */
  reset_mousepos(gfx->width >> 1, gfx->height >> 1, gfx->width - 1, gfx->height - 1);
}

void
efi_init_keyboard()
{
  int error;
  size_t handle_count;
  size_t size;
  EFI_HANDLE* handles = NULL;

  handle_count = locate_handle_with_buffer(ByProtocol, input_guid, &size, &handles);

  if (!handle_count)
    return;

  text_in_protocol = ST->ConIn;

  efi_deallocate(handles, size);
  return;

  // FIXME: this fucks us on real hw?
  /*
  for (uintptr_t i = 0; i < handle_count; i++) {
    EFI_HANDLE handle = handles[i];

    // Try to open the protocol on the handle
    error = open_protocol(handle, &input_guid, (void**)&text_in_protocol);

    if (error || !text_in_protocol)
      continue;

    // If we can reset the device, we are golden
    error = text_in_protocol->Reset(text_in_protocol, false);

    if (error == EFI_SUCCESS)
      break;

    text_in_protocol = NULL;
  }

  heap_free(handles);

  if (!text_in_protocol)
    text_in_protocol = ST->ConIn;
  */
}

bool
efi_has_mouse()
{
  int error;
  size_t handle_count;
  size_t size;
  EFI_HANDLE* handles = NULL;

  pointer_protocol = NULL;
  abs_pointer_protocol = NULL;

  handle_count = locate_handle_with_buffer(ByProtocol, pointer_guid, &size, &handles);

  if (!handle_count)
    return false;

  for (uintptr_t i = 0; i < handle_count; i++) {
    EFI_HANDLE handle = handles[i];

    /* Try to open the protocol on the handle */
    error = open_protocol(handle, &pointer_guid, (void**)&pointer_protocol);

    if (error || !pointer_protocol)
      continue;

    /* If we can reset the device, we are golden */
    error = pointer_protocol->Reset(pointer_protocol, true);

    if (error == EFI_SUCCESS) {
      return true;
    }
  }

  efi_deallocate(handles, size);
  handles = NULL;

  handle_count = locate_handle_with_buffer(ByProtocol, abs_pointer_guid, &size, &handles);

  if (!handle_count)
    return false;

  for (uintptr_t i = 0; i < handle_count; i++) {
    EFI_HANDLE handle = handles[i];

    /* Try to open the protocol on the handle */
    error = open_protocol(handle, &abs_pointer_guid, (void**)&abs_pointer_protocol);

    if (error || !abs_pointer_protocol)
      continue;

    /* If we can reset the device, we are golden */
    error = abs_pointer_protocol->Reset(abs_pointer_protocol, true);

    if (error == EFI_SUCCESS) {
      efi_deallocate(handles, size);
      return true;
    }
  }
  
  efi_deallocate(handles, size);
  return false;
}

static inline bool
is_acpi_keyboard(EFI_DEVICE_PATH* path)
{
  return (path->Type == ACPI_DEVICE_PATH && (path->SubType == ACPI_DP || path->SubType == EXPANDED_ACPI_DP));
}

static inline bool
is_usb_keyboard(EFI_DEVICE_PATH* path)
{
  return (path->Type == MESSAGING_DEVICE_PATH && path->SubType == MSG_USB_CLASS_DP);
}

bool
efi_has_keyboard()
{
  int error;
  size_t handle_count;
  size_t size;
  EFI_STATUS status;
  EFI_HANDLE* handles = NULL;
  EFI_DEVICE_PATH* path;

  ACPI_HID_DEVICE_PATH* acpi_devpath;
  USB_CLASS_DEVICE_PATH* usb_devpath;

  handle_count = locate_handle_with_buffer(ByProtocol, input_guid, &size, &handles);

  /* 
   * Loop over them to see if they are valid keyboards 
   * This kind of check is inspired by FreeBSD
   * credit to: https://github.com/freebsd/freebsd-src/blob/main/stand/efi/loader/main.c
   */
  for (uintptr_t i = 0; i < handle_count; i++) {
    EFI_HANDLE handle = handles[i];

    error = open_protocol(handle, &dev_path_guid, (void**)&path);

    if (error)
      continue;

    /* Make sure error is set */
    error = 1;

    /* Walk the device path */
    while (!IsDevicePathEnd(path)) {

      if (is_acpi_keyboard(path)) {
        acpi_devpath = (ACPI_HID_DEVICE_PATH*)path;

        if ((EISA_ID_TO_NUM(acpi_devpath->HID) & 0xff00) == 0x300
            && (acpi_devpath->HID & PNP_EISA_ID_MASK) == PNP_EISA_ID_CONST) {

          error = 0;
          goto out;
        }
      } else if (is_usb_keyboard(path)) {
        usb_devpath = (USB_CLASS_DEVICE_PATH*)path;

        if (usb_devpath->DeviceClass == 3 && usb_devpath->DeviceSubclass == 1 && usb_devpath->DeviceProtocol == 1) {

          error = 0;
          goto out;
        }
      }

      path = NextDevicePathNode(path);
    }
  }

check_conin:
  {
    SIMPLE_INPUT_INTERFACE* conin = ST->ConIn;

    error = 1;
    status = conin->Reset(conin, true);

    if (EFI_ERROR(status))
      goto out;

    error = 0;
  }

out:
  if (handles)
    heap_free(handles);
  return (error == 0);
}

int
efi_get_keypress(light_key_t* key)
{
  EFI_STATUS status;
  EFI_INPUT_KEY key_output;

  if (!key || !text_in_protocol)
    return -1;

  memset(key, 0, sizeof(light_key_t));

  status = text_in_protocol->ReadKeyStroke(text_in_protocol, &key_output);

  switch (status) {
    case EFI_NOT_READY:
      {
        return 0;
      }
    case EFI_SUCCESS:
      {
        key->typed_char = key_output.UnicodeChar;
        key->scancode = key_output.ScanCode;
        return 0;
      }
    case EFI_DEVICE_ERROR:
      return -1;
    default:
      break;
  }

  return -2;
}

int
efi_get_mousepos(light_mousepos_t* pos)
{
  EFI_STATUS status;
  EFI_SIMPLE_POINTER_STATE state;
  EFI_ABSOLUTE_POINTER_STATE abs_state;
  light_mousepos_t previous;

  if (!pos || (!pointer_protocol && !abs_pointer_protocol))
    return -1;

  memset(pos, 0, sizeof(*pos));

  get_previous_mousepos(&previous);

  if (!pointer_protocol) {

    if (abs_pointer_protocol) {

      status = abs_pointer_protocol->GetState(abs_pointer_protocol, &abs_state);

      switch (status) {
        case EFI_NOT_READY:
          {
            *pos = previous;
            return 0;
          }
        case EFI_SUCCESS:
          {

            /*
             * The absolute pointer protocol returns the absolute position of the pointer device (probably a trackpad on a laptop or sm)
             * and we want to transform this to the curret position of the cursor on the screen. We do this by setting the new position 
             * to the previous position and the new position of the trackpad together
             */

            /* Yikes, idk man */
            pos->x = abs_state.CurrentX;
            pos->y = abs_state.CurrentY;

            if (abs_state.ActiveButtons & 1) pos->btn_flags |= MOUSE_LEFTBTN;
            else pos->btn_flags &= ~MOUSE_LEFTBTN;

            limit_mousepos(pos);

            /* Mouse moved, make sure to reset the gfx button selection */
            gfx_reset_btn_select();

            set_previous_mousepos(*pos);
            break;
          }
        case EFI_DEVICE_ERROR:
          {
            abs_pointer_protocol->Reset(abs_pointer_protocol, false);
          }
      }
      return 0;
    }

    return -1;
  }

  status = pointer_protocol->GetState(pointer_protocol, &state);

  switch (status) {
    case EFI_NOT_READY:
      {
        *pos = previous;
        return 0;
      }
    case EFI_SUCCESS:
      {
        pos->x = previous.x + state.RelativeMovementX;
        pos->y = previous.y + state.RelativeMovementY;

        pos->btn_flags = NULL;

        if (state.LeftButton) pos->btn_flags |= MOUSE_LEFTBTN;
        else pos->btn_flags &= ~MOUSE_LEFTBTN;

        if (state.RightButton) pos->btn_flags |= MOUSE_RIGHTBTN;
        else pos->btn_flags &= ~MOUSE_RIGHTBTN;

        /* Make sure we are not going over the set edge */
        limit_mousepos(pos);

        /* Mouse moved, make sure to reset the gfx button selection */
        gfx_reset_btn_select();

        /* Set this position as the previous for next reads */
        set_previous_mousepos(*pos);

        return 0;
      }
    default:
      pointer_protocol->Reset(pointer_protocol, false);
      break;
  }

  return -1;
}
