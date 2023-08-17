
#include "efiapi.h"
#include "eficon.h"
#include "efidef.h"
#include "efidevp.h"
#include "efilib.h"
#include "heap.h"
#include "key.h"
#include "mouse.h"
#include "stddef.h"
#include "stdint.h"

EFI_GUID dev_path_guid = DEVICE_PATH_PROTOCOL;
EFI_GUID input_guid = SIMPLE_TEXT_INPUT_PROTOCOL;

void
efi_init_mouse()
{

}

void
efi_init_keyboard()
{

}

bool
efi_has_mouse()
{
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
  EFI_HANDLE* handles;
  EFI_DEVICE_PATH* path;

  ACPI_HID_DEVICE_PATH* acpi_devpath;
  USB_CLASS_DEVICE_PATH* usb_devpath;

  /* Find all the SIMPLE_TEXT_INPUT_PROTOCOL handles */
  handle_count = locate_handle_with_buffer(ByProtocol, input_guid, &size, &handles);

  if (!handle_count)
    return false;

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

out:
  heap_free(handles);
  return (error == 0);
}

int
efi_get_keypress(light_key_t* key)
{
  return 0;
}

int
efi_get_mousepos(light_mousepos_t* pos)
{
  return 0;
}
