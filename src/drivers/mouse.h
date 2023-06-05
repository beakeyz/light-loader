#ifndef __LIGHT_LOADER_MOUSE_DRV__
#define __LIGHT_LOADER_MOUSE_DRV__

#include "lib/light_mainlib.h"

typedef struct {
  uintptr_t m_x;
  uintptr_t m_y;
  bool m_left;
  bool m_right;
} mouse_packet_t;

LIGHT_STATUS init_mouse();

LIGHT_STATUS set_mouse_limits(uintptr_t max_x, uintptr_t max_y);

mouse_packet_t get_mouse_state();

#endif // !__LIGHT_LOADER_MOUSE_DRV__
