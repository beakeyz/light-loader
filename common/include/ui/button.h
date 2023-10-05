#ifndef __LIGHTLOADER_BUTTON__
#define __LIGHTLOADER_BUTTON__

#include "ui/component.h"

struct light_image;

typedef struct button_component {
  light_component_t* parent;
  bool is_clicked;

  uintptr_t private;

  int (*f_click_func) (struct button_component* this);
} button_component_t;

/*
 * Private structure of a tab button
 */
typedef struct tab_btn_private {
  int index;
  struct light_image* icon_image;
} tab_btn_private_t;

button_component_t* create_tab_button(light_component_t** link, char* label, uint32_t x, uint32_t y, uint32_t width, uint32_t height, int (*onclick)(button_component_t* this), uint32_t target_index);
button_component_t* create_button(light_component_t** link, char* label, uint32_t x, uint32_t y, uint32_t width, uint32_t height, int (*onclick)(button_component_t* this), int (*ondraw)(light_component_t* this));

#endif // !__LIGHTLOADER_BUTTON__
