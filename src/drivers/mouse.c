
#include "mouse.h"
#include "efipoint.h"
#include "lib/light_mainlib.h"

static EFI_GUID mouse_guid = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
static EFI_SIMPLE_POINTER_PROTOCOL* mouse_proto;

static uintptr_t __current_x;
static uintptr_t __current_y;

static uintptr_t __max_x;
static uintptr_t __max_y;

LIGHT_STATUS init_mouse() {

  EFI_STATUS status;
  uintptr_t handle_size;

  mouse_proto = NULL;
  __current_y = 0;
  __current_x = 0;
  __max_x = 0;
  __max_y = 0;

  status = g_light_info.boot_services->HandleProtocol(g_light_info.sys_table->ConsoleInHandle, &mouse_guid, (void**)&mouse_proto);

  if (status != EFI_SUCCESS) {
    return LIGHT_FAIL;
  }

  /* Reset the device */
  status = mouse_proto->Reset(mouse_proto, true);

  if (status != EFI_SUCCESS) {
    return LIGHT_FAIL;
  }

  return LIGHT_SUCCESS;
}

LIGHT_STATUS set_mouse_limits(uintptr_t max_x, uintptr_t max_y) {

  /* Plz no */
  if (!max_x || !max_y) {
    return LIGHT_FAIL;
  }

  __max_x = max_x;
  __max_y = max_y;

  return LIGHT_SUCCESS;
}

LIGHT_STATUS reset_mouse_pos()
{
  __current_x = __max_x / 2;
  __current_y = __max_y / 2;
  return LIGHT_SUCCESS;
}

mouse_packet_t get_mouse_state() {

  EFI_STATUS status;
  mouse_packet_t ret;
  EFI_SIMPLE_POINTER_STATE state; 

  status = mouse_proto->GetState(mouse_proto, &state);

  int32_t delta_x = state.RelativeMovementX;
  int32_t delta_y = state.RelativeMovementY;

  uint32_t abs_x = (delta_x < 0) ? (0x7FFFFFFF & (delta_x >> 1)) : delta_x;
  uint32_t abs_y = (delta_y < 0) ? (0x7FFFFFFF & (delta_y >> 1)) : delta_y;

  /*
  if (delta_x + __current_x < 0)
    __current_x = 0;
  else if (delta_y + __current_y < 0)
    __current_y = 0;
  else if (delta_x + __current_x > __max_x)
    __current_x = __max_x;
  else if (delta_y + __current_y > __max_y)
    __current_y = __max_y;
  else {
  */
  __current_x += state.RelativeMovementX;
  __current_y += state.RelativeMovementY;
  //}

  if (state.LeftButton) {
    light_log(L"Hi");
  }

  ret.m_x = __current_x;
  ret.m_y = __current_y;
  ret.m_left = state.LeftButton;
  ret.m_right = state.RightButton;

  return ret;
}
