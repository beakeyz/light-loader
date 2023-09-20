#ifndef __LIGHTLOADER_INPUT_BOX__
#define __LIGHTLOADER_INPUT_BOX__

#include <ui/component.h>

#define INPUTBOX_BUFFERSIZE (256)

typedef struct inputbox_component {
  light_component_t* parent;

  bool focussed;
  bool is_clicked;
  uint16_t current_input_size;
  uint16_t current_max_ip_size;

  char input_buffer[INPUTBOX_BUFFERSIZE];
} inputbox_component_t;

inputbox_component_t* create_inputbox(light_component_t** link, char* label, char* default_input, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

#endif // !__LIGHTLOADER_INPUT_BOX__
