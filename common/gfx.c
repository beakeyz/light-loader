#include "font.h"
#include <gfx.h>
#include <memory.h>
#include <stdint.h>

static uint32_t gfx_refcount;
static light_gfx_t light_gfx = { 0 };

light_color_t WHITE = CLR(0xFFFFFFFF);
light_color_t BLACK = CLR(0x00000000);
light_color_t GRAY = CLR(0x1e1e1eFF);

void gfx_load_font(struct light_font* font)
{
  if (gfx_refcount)
    return;

  light_gfx.current_font = font;
}

static uint32_t
expand_byte_to_dword(uint8_t byte)
{
  uint32_t ret = 0;

  for (uint8_t i = 0; i < sizeof(uint32_t); i++) {
    ret |= (((uint32_t)(byte)) << (i * 8));
  }
  return ret;
}

static uint32_t
transform_light_clr(light_gfx_t* gfx, light_color_t clr)
{
  uint32_t fb_color = 0;

  fb_color |= expand_byte_to_dword(clr.red) & gfx->red_mask;
  fb_color |= expand_byte_to_dword(clr.green) & gfx->green_mask;
  fb_color |= expand_byte_to_dword(clr.blue) & gfx->blue_mask;
  fb_color |= expand_byte_to_dword(clr.alpha) & gfx->alpha_mask;

  return fb_color;
}

void
gfx_draw_pixel(light_gfx_t* gfx, uint32_t x, uint32_t y, light_color_t clr)
{
  if (x >= gfx->width || y >= gfx->height)
    return;

  /* TODO; */
  if (gfx->back_fb) {
    // return;
  }

  *(uint32_t volatile*)(gfx->phys_addr + x * gfx->bpp / 8 + y * gfx->stride * sizeof(uint32_t)) = transform_light_clr(gfx, clr);
}

void
gfx_draw_char(light_gfx_t* gfx, char c, uint32_t x, uint32_t y, light_color_t clr)
{
  int error;
  light_glyph_t glyph;

  if (!gfx->current_font)
    return;

  error = lf_get_glyph_for_char(gfx->current_font, c, &glyph);

  if (error)
    return;

  for (uint8_t _y = 0; _y < gfx->current_font->height; _y++) {
    char glyph_part = glyph.data[_y];
    for (uint8_t _x = 0; _x < gfx->current_font->width; _x++) {
      if (glyph_part & (1 << _x)) {
        gfx_draw_pixel(gfx, x + _x, y + _y, clr);
      }
    }
  }
}

void
gfx_draw_str(light_gfx_t* gfx, char* str, uint32_t x, uint32_t y, light_color_t clr)
{
  uint32_t x_idx = 0;
  char* c = str;

  /*
   * Loop over every char, draw them after one another over the x-axis, no looping 
   * when the screen-edge is hit
   */
  while (*c) {
    gfx_draw_char(gfx, *c, x + x_idx, y, clr);
    x_idx += gfx->current_font->width;
    c++;
  }
}

void 
get_light_gfx(light_gfx_t** gfx)
{
  if (!gfx)
    return;

  /* Only allow one gfx reference at once */
  if (gfx_refcount) {
    *gfx = nullptr;
    return;
  }

  *gfx = &light_gfx;

  gfx_refcount++;
}

void 
put_light_gfx()
{
  if (!gfx_refcount)
    return;

  gfx_refcount--;
}

void 
init_light_gfx()
{
  /* Make sure the structure is clean */
  memset(&light_gfx, 0, sizeof(light_gfx));

  gfx_refcount = 0;
  light_gfx.current_font = &default_light_font;
}
