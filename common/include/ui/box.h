#ifndef __LIGHTLOADER_UI_BOX__
#define __LIGHTLOADER_UI_BOX__

#include "gfx.h"
#include "ui/component.h"

typedef struct box_component {
  light_component_t* parent;
  light_color_t clr;
  uint32_t radius;
  bool filled;
} box_component_t;

box_component_t* create_box(light_component_t** link, char* label, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t radius, bool filled, light_color_t clr);

#endif // !__LIGHTLOADER_UI_BOX__
