#ifndef __LIGHTLOADER_MAINMENU__
#define __LIGHTLOADER_MAINMENU__

#include "drivers/display/framebuffer.h"
#include "frontend/screen.h"

void init_main_menu(light_framebuffer_t* buffer);

void render_main_menu();

void main_menu_set_subscreen(light_screen_t* screen);

#endif // !__LIGHTLOADER_MAINMENU__
