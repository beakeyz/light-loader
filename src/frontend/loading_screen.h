#ifndef __LIGHT_LOADING_SCREEN__
#define __LIGHT_LOADING_SCREEN__

#include "drivers/display/framebuffer.h"

void init_loading_screen(light_framebuffer_t* buffer);

void draw_loading_screen();

void loading_screen_set_status(const char* status_msg);

void loading_screen_set_status_and_update(const char* status_msg);

void clear_loading_screen();


#endif // !__LIGHT_LOADING_SCREEN__
