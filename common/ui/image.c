#include "image.h"
#include "ui/component.h"
#include <stdio.h>
#include <ui/image.h>
#include <heap.h>
#include <memory.h>

int 
image_draw_func(light_component_t* c)
{
  image_component_t* image = c->private;

  switch (image->image_type) {
    case IMAGE_TYPE_BMP:
    case IMAGE_TYPE_INLINE:
      {
        light_image_t* l_image = image->inline_img_data;

        draw_image(c->gfx, c->x, c->y, l_image);
      }
      break;
    case IMAGE_TYPE_PNG:
    default:
      return -1;
  }

  return 0;
}

image_component_t* 
create_image(light_component_t** link, char* label, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t image_type, void* image)
{
  light_component_t* parent = create_component(link, COMPONENT_TYPE_IMAGE, label, x, y, width, height, image_draw_func);
  image_component_t* image_component = heap_allocate(sizeof(image_component_t));

  memset(image_component, 0, sizeof(image_component_t));

  image_component->parent = parent;
  image_component->image_type = image_type;
  image_component->inline_img_data = image;

  parent->private = image_component;

  if (image_type == IMAGE_TYPE_BMP) {
    char* path = (char*)image;

    light_image_t* full_image = load_bmp_image(path);

    image_component->inline_img_data = scale_image(parent->gfx, full_image, width, height);

    heap_free(full_image);
  }

  return image_component;
}

