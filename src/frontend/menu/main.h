#ifndef __LIGHTLOADER_MAINMENU__
#define __LIGHTLOADER_MAINMENU__

#include "drivers/display/framebuffer.h"
#include "drivers/keyboard.h"
#include "frontend/screen.h"

struct main_context {
  bool should_load;
  bool mouse_down;
  uint32_t mouse_x;
  uint32_t mouse_y;
  kb_press_packet_t kb_packet;
};

void init_main_menu(light_framebuffer_t* buffer, struct main_context* ctx);

void render_main_menu(struct main_context ctx);
void update_main_menu(struct main_context* ctx);

void main_menu_set_subscreen(light_screen_t* screen);

#endif // !__LIGHTLOADER_MAINMENU__
