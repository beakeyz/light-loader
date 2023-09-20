#include "font.h"
#include "gfx.h"
#include "heap.h"
#include "stddef.h"
#include <memory.h>
#include "ui/component.h"
#include <ui/input_box.h>

#define INPUTBOX_TEXT_OFFSET 4

static bool 
inputbox_is_empty(inputbox_component_t* box)
{
  return (box->input_buffer[0] == NULL);
}

static bool 
inputbox_has_reached_current_max(inputbox_component_t* box)
{
  return (box->current_input_size >= box->current_max_ip_size);
}

static int 
draw_default_inputbox (light_component_t* c)
{
  inputbox_component_t* inputbox;
  uint32_t label_y = 0;
  uint32_t label_x = 0;

  inputbox = c->private;
  label_y = (c->height >> 1) - (c->gfx->current_font->height >> 1);

  gfx_draw_rect(c->gfx, c->x, c->y, c->width, c->height, GRAY);

  if (inputbox->focussed)
    gfx_draw_rect_outline(c->gfx, c->x, c->y, c->width, c->height, DARK_GRAY);

  if (inputbox_is_empty(inputbox) && !inputbox->focussed) {
    label_x = (c->width >> 1) - (lf_get_str_width(c->gfx->current_font, c->label) >> 1);

    component_draw_string_at(c, c->label, label_x, label_y, WHITE);
  } else {
    component_draw_string_at(c, inputbox->input_buffer, INPUTBOX_TEXT_OFFSET, label_y, WHITE);
  }

  if (!inputbox_has_reached_current_max(inputbox) && inputbox->focussed)
    gfx_draw_rect(c->gfx,
        c->x + INPUTBOX_TEXT_OFFSET + (inputbox->current_input_size * c->font->width),
        c->y + c->height - 6,
        c->font->width - 2, 
        2,
        WHITE);

  return 0;
}

static bool 
inputbox_should_update(light_component_t* c)
{
  inputbox_component_t* inputbox = c->private;
  light_key_t* key = c->key_buffer;

  /* How many chars can fit inside this box? (FIXME: should we recompute this every update?) */
  inputbox->current_max_ip_size = (ALIGN_DOWN(c->width, c->font->width) / c->font->width) - 1;

  if (inputbox->focussed && key->typed_char) {
    switch (key->typed_char) {
      /* Return should deselect our inputbox */
      case '\r':
        gfx_select_inputbox(c->gfx, nullptr);
        break;
      /* Implement backspacing */
      case '\b':
        if (inputbox->current_input_size) {
          inputbox->current_input_size--;
          inputbox->input_buffer[inputbox->current_input_size] = NULL;
        }
        break;
      /* Regular character print */
      default:
        /* Prevent overflow of the box */
        if (!inputbox_has_reached_current_max(inputbox))
          inputbox->input_buffer[inputbox->current_input_size++] = key->typed_char;
    }
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
create_inputbox(light_component_t** link, char* label, char* default_input, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
  inputbox_component_t* inputbox;
  light_component_t* parent;
  uint32_t default_len;

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

  if (default_input) {
    default_len = strlen(default_input);

    if (default_len > INPUTBOX_BUFFERSIZE)
      default_len = INPUTBOX_BUFFERSIZE;

    memcpy(inputbox->input_buffer, default_input, default_len);
    inputbox->input_buffer[INPUTBOX_BUFFERSIZE-1] = NULL;

    inputbox->current_input_size = default_len;

  }

  return inputbox;
}
