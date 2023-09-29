#ifndef __LIGHLOADER_LOGO__
#define __LIGHLOADER_LOGO__

#include "gfx.h"
#include <stdint.h>

#define LOGO_SIDE_PADDING 4

typedef struct light_image {
  uint_t  	 width;
  uint_t  	 height;
  uint_t  	 bytes_per_pixel;
  uint8_t 	 pixel_data[];
} light_image_t;

struct bmp_header {
  uint8_t magic[2];
  uint32_t size;
  uint32_t res;
  uint32_t image_start;
  uint32_t header_size;
  int32_t width;
  int32_t height;
  uint16_t color_planes;
  uint16_t bpp;
  uint8_t pixels[];
} __attribute__((packed));

light_image_t* load_bmp_image(char* path);

void draw_image(light_gfx_t* gfx, uint32_t x, uint32_t y, light_image_t* l_image);
light_image_t* scale_image(light_gfx_t* gfx, light_image_t* image, uint32_t new_width, uint32_t new_height);

extern light_image_t default_logo;

#endif // !__LIGHLOADER_LOGO__
