#ifndef __LIGHTLOADER_UI_OPTIONS_SCREEN__
#define __LIGHTLOADER_UI_OPTIONS_SCREEN__

#include "ui/button.h"
#include "ui/component.h"
#include "ui/input_box.h"

#define LOPTION_TYPE_BOOL   0
#define LOPTION_TYPE_NUMBER 1
#define LOPTION_TYPE_STRING 2

struct light_option {
  char* name;
  char* opt;
  uint8_t type;

  uintptr_t value;

  union {
    button_component_t* btn;
    inputbox_component_t* input_box;
  };
};

int construct_optionsscreen(light_component_t** root, light_gfx_t* gfx);

struct light_option* get_light_option(const char* name);

extern struct light_option l_options[];
extern uint32_t l_options_len;
#endif // !__LIGHTLOADER_UI_OPTIONS_SCREEN__
