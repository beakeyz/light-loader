#include "keyboard.h"
#include "mem/pmm.h"

EFI_GUID text_prot_ex_guid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;
EFI_GUID sproto_guid = EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID;
EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL* text_prot_ex = NULL;
EFI_SIMPLE_TEXT_IN_PROTOCOL *text_in_proto = NULL;

static bool is_buffer_dirty;

void init_kb_driver() {
  if (g_light_info.boot_services->HandleProtocol(g_light_info.sys_table->ConsoleInHandle, &text_prot_ex_guid, (void**)&text_prot_ex) != EFI_SUCCESS) {
    if (g_light_info.boot_services->HandleProtocol(g_light_info.sys_table->ConsoleInHandle, &text_prot_ex_guid, (void**)&text_prot_ex) != EFI_SUCCESS) {
      if (g_light_info.sys_table->ConIn != NULL) {
        text_in_proto = g_light_info.sys_table->ConIn;
      } else {
        light_log(L"Fuckman dicka ss");
      }
    }
    // no text protocol available
    g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"No text input prot ex!\n\r");
    return;
  }
}

void flush_kb_buffer() {

}

LIGHT_STATUS read_kb_press(kb_press_packet_t* pck)
{
  EFI_KEY_DATA data;
  EFI_STATUS status;

  if (!pck)
    return LIGHT_FAIL;

  memset(pck, 0, sizeof(kb_press_packet_t));

  if (text_prot_ex != NULL) {
    status = text_prot_ex->ReadKeyStrokeEx(text_prot_ex, &data);
  } else {
    status = text_in_proto->ReadKeyStroke(text_in_proto, &data.Key);
  }

  switch (status) {
    case EFI_SUCCESS:
      pck->keyData = data;
      pck->status = status;
      return LIGHT_SUCCESS;
    case EFI_NOT_READY:
    case EFI_DEVICE_ERROR:
    case EFI_UNSUPPORTED:
      return LIGHT_FAIL;
  }

  return LIGHT_FAIL;
}

