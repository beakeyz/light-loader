#ifndef __LIGHTLOADER_UI_IMAGE__
#define __LIGHTLOADER_UI_IMAGE__

#include "ui/component.h"
#include <stdint.h>
#include <file.h>

struct light_image;

#define IMAGE_TYPE_BMP      (0)
#define IMAGE_TYPE_PNG      (1)
#define IMAGE_TYPE_INLINE   (2)

typedef struct image_component {
  light_component_t* parent;
  uint8_t image_type;

  union {
    struct light_image* inline_img_data;
    const char* image_path;
  };
} image_component_t;

image_component_t* create_image(light_component_t** link, char* label, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t image_type, void* image);

#endif // !__LIGHTLOADER_UI_IMAGE__
