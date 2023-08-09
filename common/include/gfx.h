#ifndef __LIGHTLOADER_GFX__
#define __LIGHTLOADER_GFX__

/*
 * The lightloader GFX context is to generefy the way that graphics attributes are
 * communicated
 */

#include <stdint.h>

struct light_font;

/*
 * Simple union to generefy the way to represent colors in a framebuffer
 */
typedef union light_color {
  struct {
    uint8_t alpha;
    uint8_t blue;
    uint8_t green;
    uint8_t red;
  };
  uint32_t clr;
} light_color_t;

#define CLR(rgba) (light_color_t) { .clr = (uint32_t)(rgba) }

/* Default color variables */
extern light_color_t WHITE;
extern light_color_t BLACK;
extern light_color_t GRAY;

/* TODO: implement color blending opperations to make use of the alpha chanel */
int lclr_blend(light_color_t fg, light_color_t bg, light_color_t* out);

#define GFX_TYPE_NONE (0x0000)
#define GFX_TYPE_GOP (0x0001)
#define GFX_TYPE_UGA (0x0002) /* TODO: should we support UGA? */
#define GFX_TYPE_VEGA (0x0004) /* Should never be seen in these lands */

typedef struct light_gfx {
  uintptr_t phys_addr;
  uintptr_t size;
  uint32_t width, height, stride;
  uint32_t red_mask, green_mask, blue_mask, alpha_mask;
  uint8_t bpp;
  uint8_t type;

  void* priv;

  struct light_font* current_font;

  /* Yay, doublebuffering 0.0 */
  uint32_t* back_fb;
  size_t back_fb_pages;

} light_gfx_t;

void gfx_load_font(struct light_font* font);

void gfx_draw_pixel(light_gfx_t* gfx, uint32_t x, uint32_t y, light_color_t clr);
void gfx_draw_char(light_gfx_t* gfx, char c, uint32_t x, light_color_t y);

void get_light_gfx(light_gfx_t** gfx);
void put_light_gfx();

void init_light_gfx();

#endif // !__LIGHTLOADER_GFX__
