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
  light_image_t* full_image;
  light_image_t* scaled_image;
  light_component_t* parent;
  image_component_t* image_component;

  scaled_image = full_image = image;

  if (image_type == IMAGE_TYPE_BMP) {
    char* path = (char*)image;

    full_image = load_bmp_image(path);

    if (!full_image)
      return nullptr;

    scaled_image = scale_image(parent->gfx, full_image, width, height);

    if (!scaled_image)
      goto dealloc_and_error;

    heap_free(full_image);
    full_image = nullptr;
  }

  parent = create_component(link, COMPONENT_TYPE_IMAGE, label, x, y, width, height, image_draw_func);

  if (!parent)
    goto dealloc_and_error;

  image_component = heap_allocate(sizeof(image_component_t));

  if (!image_component)
    goto dealloc_and_error;

  memset(image_component, 0, sizeof(image_component_t));

  image_component->parent = parent;
  image_component->image_type = image_type;
  image_component->inline_img_data = scaled_image;

  parent->private = image_component;

  return image_component;

dealloc_and_error:
  if (scaled_image)
    heap_free(scaled_image);

  if (full_image)
    heap_free(full_image);

  /* FIXME: dealloc and unlink */
  if (parent)
    parent->should_update = false;

  if (image_component)
    heap_free(image_component);

  return nullptr;
}

