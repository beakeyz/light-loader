#include "font.h"
#include "gfx.h"
#include "heap.h"
#include "stddef.h"
#include <memory.h>
#include "ui/component.h"
#include <ui/input_box.h>

static bool 
inputbox_is_empty(inputbox_component_t* box)
{
  return (box->input_buffer[0] == NULL);
}

static int 
draw_default_inputbox (light_component_t* c)
{
  inputbox_component_t* inputbox;
  uint32_t label_y;

  inputbox = c->private;
  label_y = (c->height >> 1) - (c->gfx->current_font->height >> 1);

  gfx_draw_rect(c->gfx, c->x, c->y, c->width, c->height, GRAY);

  if (inputbox_is_empty(inputbox) && !inputbox->focussed) {
    component_draw_string_at(c, c->label, 4, label_y, WHITE);
  } else {
    component_draw_string_at(c, inputbox->input_buffer, 4, label_y, WHITE);
  }

  return 0;
}

static bool 
inputbox_should_update(light_component_t* c)
{
  inputbox_component_t* inputbox = c->private;
  light_key_t* key = c->key_buffer;

  if (inputbox->focussed && key->typed_char) {
    inputbox->input_buffer[inputbox->current_input_size++] = key->typed_char;
  }

  if (component_is_hovered(c) && is_lmb_clicked(*c->mouse_buffer) && !inputbox->is_clicked) {
    inputbox->is_clicked = true;

    gfx_select_inputbox(c->gfx, c);
  } else if (!is_lmb_clicked(*c->mouse_buffer)){
    inputbox->is_clicked = false;
  }

  return true;
}


inputbox_component_t* 
create_inputbox(light_component_t** link, char* label, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
  inputbox_component_t* inputbox;
  light_component_t* parent;

  parent = create_component(link, COMPONENT_TYPE_INPUTBOX, label, x, y, width, height, draw_default_inputbox);

  if (!parent)
    return nullptr;

  inputbox = heap_allocate(sizeof(*inputbox));

  /* Yk fuck the parent component lol */
  if (!inputbox) 
    return nullptr;

  memset(inputbox, 0, sizeof(*inputbox));

  parent->f_should_update = inputbox_should_update;
  parent->private = inputbox;

  inputbox->parent = parent;

  return inputbox;
}
