#include "ui/component.h"
#include "font.h"
#include "gfx.h"
#include "heap.h"
#include <memory.h>
#include <stdio.h>

int default_label_draw(light_component_t* comp);
int default_btn_draw(light_component_t* comp);
int default_inputbox_draw(light_component_t* comp);
int default_switch_draw(light_component_t* comp);

static int (*default_draw_funcs[])(light_component_t*) = {
  [COMPONENT_TYPE_BUTTON] = default_btn_draw,
  [COMPONENT_TYPE_LABEL] = default_label_draw,
  [COMPONENT_TYPE_SWITCH] = default_switch_draw,
  [COMPONENT_TYPE_INPUTBOX] = default_inputbox_draw,
};

light_component_t*
create_component(light_component_t** link, uint8_t type, char* label, uint32_t x, uint32_t y, uint32_t width, uint32_t height, int (*ondraw)(struct light_component*))
{
  light_component_t* component = heap_allocate(sizeof(light_component_t));

  memset(component, 0, sizeof(light_component_t));

  component->x = x;
  component->y = y;
  component->width = width;
  component->height = height;
  component->should_update = true;

  component->type = type;
  component->label = label;

  get_light_gfx(&component->gfx);
  component->font = component->gfx->current_font;

  if (ondraw)
    component->f_ondraw = ondraw;
  else 
    component->f_ondraw = default_draw_funcs[type];

  /* Add to the link */

  if (link) {
    component->next = *link;
    *link = component;
  }

  return component;
}

/*!
 * @brief Draw a string inside a component
 *
 * component: the component to draw inside
 * str: the string to draw
 * x: relative x from the components x
 * y: relative x from the components y
 */
int
component_draw_string_at(light_component_t* component, char* str, uint32_t x, uint32_t y, light_color_t clr)
{
  uint32_t i = 0;
  uint32_t c_x = component->x + x;
  uint32_t c_y = component->y + y;

  while (str[i]) {
    gfx_draw_char(component->gfx, str[i], c_x, c_y, clr);

    c_x += component->font->width;

    if (c_x >= component->x + component->width) {
      c_x = component->x + x;
      c_y += component->font->height;
    }

    /* Just quit when we reach the absolute end of the component */
    if (c_y >= component->y + component->height) {
      break;
    }

    i++;
  }

  return 0;
}

void
draw_component(light_component_t* component, light_key_t key, light_mousepos_t mouse)
{
  if (!component->should_update)
    return;

  /* We only redraw the component if it has changed its internal state */
  component->f_ondraw(component);
  component->should_update = false;
}

int
default_label_draw(light_component_t* comp)
{
  gfx_draw_rect(comp->gfx, comp->x, comp->y, comp->width, comp->height, GRAY);

  uint32_t label_draw_x = (comp->width >> 1) - (lf_get_str_width(comp->gfx->current_font, comp->label) >> 1);
  uint32_t label_draw_y = (comp->height >> 1) - (comp->gfx->current_font->height >> 1);

  return component_draw_string_at(comp, comp->label, label_draw_x, label_draw_y, WHITE);
}

int
default_btn_draw(light_component_t* comp)
{
  return 0;
}

int 
default_inputbox_draw(light_component_t* comp)
{
  return 0;
}

int
default_switch_draw(light_component_t* comp)
{
  return 0;
}
