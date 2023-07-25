#include "main.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "efierr.h"
#include "frontend/components/button.h"
#include "lib/light_mainlib.h"
#include <frontend/screen.h>

light_screen_t* main_menu;
light_screen_t* current_subscreen;

static int side_btn_draw(btn_t* btn, light_screen_t* screen)
{
  if (btn_is_hovered(btn)) {
    screen->fDrawString(screen, btn->label, btn->x + 2, btn->y + 2, 0xFFFFFFFF);
  }

  screen->fDrawBox(screen, btn->x, btn->y + btn->height, btn->width, 1, 0xFFFFFFFF);

  return 0;
}

static btn_t __side_buttons[] = {
  { "Boot", 0, 0, 16, 16, side_btn_draw, },
  { "Settings", 0, 0, 16, 16, side_btn_draw, },
  { "Cmd", 0, 0, 16, 16, side_btn_draw, },
  { "Utils", 0, 0, 16, 16, side_btn_draw, },
};

static const size_t __button_count = sizeof(__side_buttons) / sizeof(*__side_buttons);
static size_t __button_end_y = 0;

void init_main_menu(light_framebuffer_t* buffer, struct main_context* context)
{
  main_menu = init_light_screen(buffer);

  /* FIXME: create a solid panic for the bootloader */
  if (!main_menu)
    hang();

  /* We use subscreens to display things like settings and crap */
  current_subscreen = NULL;

  reset_mouse_pos();

  /* Setup the button skeleton */
  for (uintptr_t i = 0; i < __button_count; i++) {
    __side_buttons[i].y = __button_end_y;
    __button_end_y += __side_buttons[i].height;
  }
}

void render_main_menu(struct main_context ctx)
{
  if (!main_menu)
    return;

  /* TODO: */

  memset(main_menu->m_double_buffer, 0, main_menu->m_buffer_size);

  main_menu->fDrawBox(main_menu, ctx.mouse_x, ctx.mouse_y, 2, 2, 0xFFFFFFFF);

  for (uintptr_t i = 0; i < __button_count; i++) {
    if (__side_buttons[i].on_draw)
      __side_buttons[i].on_draw(&__side_buttons[i], main_menu);
  }

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

  if (stat == LIGHT_SUCCESS) {

    __side_buttons[context->window_idx].flags &= ~BTN_IS_HOVERED;

    /* Check for keyevents and update */
    /* TODO: check if this is universal */
    switch (context->kb_packet.keyData.Key.ScanCode) {
      case 1:
        context->selector_idx++;
        break;
      case 2:
        context->selector_idx--;
        break;
      case 3:
        if (context->window_idx < (__button_count-1))
          context->window_idx++;
        break;
      case 4:
        if (context->window_idx > 0)
          context->window_idx--;
        break;
      default:
        break;
    }
  }

  __side_buttons[context->window_idx].flags |= BTN_IS_HOVERED;

  CHAR16 thing[] = { context->kb_packet.keyData.Key.UnicodeChar, 0 };
  light_log(thing);

  //main_menu->fDrawString(main_menu, to_string(context->kb_packet.keyData.Key.ScanCode), 0, 0, 0xFFFFFFFF);
}

void main_menu_set_subscreen(light_screen_t* screen)
{
  /* TODO: if the screen we replace is not one we can go back to, 
   we should be able to kill it */
  current_subscreen = screen;
}
