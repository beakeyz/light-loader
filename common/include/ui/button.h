#ifndef __LIGHTLOADER_BUTTON__
#define __LIGHTLOADER_BUTTON__

#include "ui/component.h"
typedef struct button_component {
  light_component_t* parent;
  void* internal;

  int (*f_click_func) (struct button_component* this);
} button_component_t;

button_component_t* create_button(light_component_t** link, uint32_t x, uint32_t y, uint32_t width, uint32_t height, int (*onclick)(button_component_t* this), int (*ondraw)(light_component_t* this));

#endif // !__LIGHTLOADER_BUTTON__
