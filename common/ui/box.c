#include "gfx.h"
#include "heap.h"
#include "ui/component.h"
#include <ui/box.h>
#include <memory.h>

static int
box_ondraw(struct light_component* component)
{
  if (component->type != COMPONENT_TYPE_BOX)
    return -1;
  
  box_component_t* box = component->private;

  if (box->filled)
    gfx_draw_rect(component->gfx, component->x, component->y, component->width, component->height, box->clr);
  else 
    gfx_draw_rect_outline(component->gfx, component->x, component->y, component->width, component->height, box->clr);

  return 0;
}

box_component_t*
create_box(light_component_t** link, char* label, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t radius, bool filled, light_color_t clr)
{
  light_component_t* parent = create_component(link, COMPONENT_TYPE_BOX, label, x, y, width, height, box_ondraw);
  box_component_t* box = heap_allocate(sizeof(box_component_t));

  memset(box, 0, sizeof(box_component_t));

  box->filled = filled;
  box->radius = radius;
  box->clr = clr;
  box->parent = parent;

  parent->private = box;

  return box;
}
