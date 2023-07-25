#ifndef __LIGHT_LOADER_BUTTON__
#define __LIGHT_LOADER_BUTTON__

#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "frontend/screen.h"
#include <lib/light_mainlib.h>

#define BTN_IS_HOVERED  (0x00000001)

typedef struct btn {
  char* label;
  uint32_t x, y;
  uint32_t width, height;
  /* For now we trust that buttons stay within their boundries, but we really shouldnt */
  int (*on_draw)(struct btn*, light_screen_t* screen);
  int (*on_click)(struct btn*, mouse_packet_t* packet);
  int (*on_key)(struct btn*, kb_press_packet_t* packet);

  uint32_t flags;
  /*
  struct {
    bool is_hovered:1;
  } flags;
  */
} btn_t, button_t;

static inline bool btn_is_hovered(btn_t* btn)
{
  return ((btn->flags & BTN_IS_HOVERED) == BTN_IS_HOVERED);
}

#endif // !__LIGHT_LOADER_BUTTON__
