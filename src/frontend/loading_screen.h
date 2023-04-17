#ifndef __LIGHT_LOADING_SCREEN__
#define __LIGHT_LOADING_SCREEN__

#include "drivers/display/framebuffer.h"

void draw_loading_screen(light_framebuffer_t* buffer);

void loading_screen_set_status(const char* status_msg);

void loading_screen_set_status_and_update(const char* status_msg, light_framebuffer_t* fb);

#endif // !__LIGHT_LOADING_SCREEN__
