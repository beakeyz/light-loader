#include "font.h"
#include "gfx.h"
#include "mouse.h"
#include "ui/component.h"
#include <stdio.h>
#include <ui/button.h>
#include <heap.h>
#include <memory.h>

static bool 
btn_should_update(light_component_t* component)
{
  button_component_t* btn = component->private;

  /* Make sure this button is clickable lmao */
  if (!btn->f_click_func)
    return true;

  if (component_is_hovered(component) && is_lmb_clicked(*component->mouse_buffer) && !btn->is_clicked) {
    btn->is_clicked = true;
    btn->f_click_func(btn);
  } else if (!is_lmb_clicked(*component->mouse_buffer)){
    btn->is_clicked = false;
  }

  return true;
}

button_component_t* 
create_button(light_component_t** link, char* label, uint32_t x, uint32_t y, uint32_t width, uint32_t height, int (*onclick)(button_component_t* this), int (*ondraw)(light_component_t* this))
{
  light_component_t* parent = create_component(link, COMPONENT_TYPE_BUTTON, label, x, y, width, height, ondraw);
  button_component_t* btn = heap_allocate(sizeof(button_component_t));

  memset(btn, 0, sizeof(button_component_t));

  btn->parent = parent;
  btn->f_click_func = onclick;

  parent->f_should_update = btn_should_update;
  parent->private = btn;

  return btn;
}

static int
draw_tab_btn(light_component_t* c)
{
  gfx_draw_rect(c->gfx, c->x, c->y, c->width, c->height, LIGHT_GRAY);

  if (component_is_hovered(c))
    gfx_draw_rect_outline(c->gfx, c->x, c->y, c->width, c->height, BLACK);

  if (c->label) {
    uint32_t label_x = (c->width >> 1) - (lf_get_str_width(c->gfx->current_font, c->label) >> 1);
    uint32_t label_y = (c->height >> 1) - (c->gfx->current_font->height >> 1);
    component_draw_string_at(c, c->label, label_x, label_y, WHITE);
  }
  return 0;
}

button_component_t* 
create_tab_button(light_component_t** link, char* label, uint32_t x, uint32_t y, uint32_t width, uint32_t height, int (*onclick)(button_component_t* this), uint32_t target_index)
{
  button_component_t* ret = create_button(link, label, x, y, width, height, onclick, draw_tab_btn);

  /* For tabs, we can simply store the target index directly in ->private */
  ret->private = (uint64_t)target_index;

  return ret;
}
