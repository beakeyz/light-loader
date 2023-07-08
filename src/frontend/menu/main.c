#include "main.h"
#include "lib/light_mainlib.h"
#include <frontend/screen.h>

light_screen_t* main_menu;

light_screen_t* current_subscreen;

void init_main_menu(light_framebuffer_t* buffer)
{
  main_menu = init_light_screen(buffer);

  /* FIXME: create a solid panic for the bootloader */
  if (!main_menu)
    hang();

  /* We use subscreens to display things like settings and crap */
  current_subscreen = NULL;
}

void render_main_menu()
{
  if (!main_menu)
    return;

  /* TODO: */
}

void main_menu_set_subscreen(light_screen_t* screen)
{
  /* TODO: if the screen we replace is not one we can go back to, 
   we should be able to kill it */
  current_subscreen = screen;
}
