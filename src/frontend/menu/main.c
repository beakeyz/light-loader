#include "main.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "efierr.h"
#include "lib/light_mainlib.h"
#include <frontend/screen.h>

light_screen_t* main_menu;

light_screen_t* current_subscreen;

void init_main_menu(light_framebuffer_t* buffer, struct main_context* context)
{
  main_menu = init_light_screen(buffer);

  /* FIXME: create a solid panic for the bootloader */
  if (!main_menu)
    hang();

  /* We use subscreens to display things like settings and crap */
  current_subscreen = NULL;

  reset_mouse_pos();
}

void render_main_menu(struct main_context ctx)
{
  if (!main_menu)
    return;

  /* TODO: */

  main_menu->fDrawBox(main_menu, ctx.mouse_x, ctx.mouse_y, 2, 2, 0xFFFFFFFF);


  main_menu->fSwapBuffers(main_menu);
}

void update_main_menu(struct main_context* context)
{
  LIGHT_STATUS stat;
  mouse_packet_t mouse_packet;
  
  if (!context)
    return;

  mouse_packet = get_mouse_state();

  context->mouse_x = mouse_packet.m_x;
  context->mouse_y = mouse_packet.m_y;

  /* TODO: config main mouse btn */
  context->mouse_down = mouse_packet.m_left;

  stat = read_kb_press(&context->kb_packet);

  if (stat == LIGHT_FAIL)
    return;

  /* Check for keyevents and update */
  CHAR16 thing[] = { context->kb_packet.keyData.Key.UnicodeChar, 0 };
  light_log(thing);
}

void main_menu_set_subscreen(light_screen_t* screen)
{
  /* TODO: if the screen we replace is not one we can go back to, 
   we should be able to kill it */
  current_subscreen = screen;
}
