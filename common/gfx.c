#include "ctx.h"
#include "efilib.h"
#include "font.h"
#include "key.h"
#include "logo.h"
#include "mouse.h"
#include "ui/component.h"
#include <gfx.h>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>

static light_gfx_t light_gfx = { 0 };

static light_component_t* root_component;

light_color_t WHITE = CLR(0xFFFFFFFF);
light_color_t BLACK = CLR(0x00000000);
light_color_t GRAY = CLR(0x1e1e1eFF);

void gfx_load_font(struct light_font* font)
{
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

  if (!gfx || !gfx->current_font)
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
gfx_draw_rect(light_gfx_t* gfx, uint32_t x, uint32_t y, uint32_t width, uint32_t height, light_color_t clr)
{
  for (uint32_t i = 0; i < height; i++) {
    for (uint32_t j = 0; j < width; j++) {
      gfx_draw_pixel(gfx, x + j, y + i, clr);
    }
  }
}

void 
gfx_draw_rect_outline(light_gfx_t* gfx, uint32_t x, uint32_t y, uint32_t width, uint32_t height, light_color_t clr)
{
  for (uint32_t i = 0; i < width; i++) {
    gfx_draw_pixel(gfx, x + i, y, clr);
    gfx_draw_pixel(gfx, x + i, y + height - 1, clr);
  }

  for (uint32_t i = 0; i < height; i++) {
    gfx_draw_pixel(gfx, x, y + i, clr);
    gfx_draw_pixel(gfx, x + width - 1, y + i, clr);
  }
}

void
gfx_draw_circle(light_gfx_t* gfx, uint32_t x, uint32_t y, uint32_t radius, light_color_t clr)
{
  for (uint32_t i = 0; i <= radius; i++) {
    for (uint32_t j = 0; j <= radius; j++) {
      if (i * i + j * j > radius * radius)
        continue;

      gfx_draw_pixel(gfx, x + j, y + i, clr);
    }
  }
}

/*
 * NOTE: temporary
 */
static uint32_t printf_x = 4;
static uint32_t printf_y = 4;

int
gfx_putchar(char c)
{
  gfx_draw_char(&light_gfx, c, printf_x, printf_y, WHITE);

  printf_x += light_gfx.current_font->width;
  return 0;
}

int
gfx_printf(char* str, ...)
{
  /*
   * TODO: get a bootloader tty context 
   * from this we get:
   *  - the current starting x and y coords where we can print the next string of info
   *  - the forground color and background color
   */
  gfx_draw_str(&light_gfx, str, printf_x, printf_y, WHITE);

  printf_y += light_gfx.current_font->height;
  printf_x = 4;
  return 0;
}

void
gfx_display_logo(light_gfx_t* gfx, uint32_t x, uint32_t y, gfx_logo_pos_t pos)
{
  switch (pos) {
    case LOGO_POS_CENTER:
      {
        x = (gfx->width - 1) >> 1;
        y = (gfx->height - 1) >> 1;

        x -= (default_logo.width - 1) >> 1;
        y -= (default_logo.height - 1) >> 1;
      }
      break;
    case LOGO_POS_TOP_BAR_RIGHT:
      {
        x = (gfx->width - 1) - default_logo.width - LOGO_SIDE_PADDING;
        y = LOGO_SIDE_PADDING;
      }
      break;
    case LOGO_POS_BOTTOM_BAR_RIGHT:
      {
        x = (gfx->width - 1) - default_logo.width - LOGO_SIDE_PADDING;
        y = (gfx->height - 1) - default_logo.height - LOGO_SIDE_PADDING;
      }
      break;
    case LOGO_POS_NONE:
    default:
      break;
  }

  for (uint32_t i = 0; i < default_logo.height; i++) {
    for (uint32_t j = 0; j < default_logo.width; j++) {

      uint8_t r = default_logo.pixel_data[i * default_logo.bytes_per_pixel * 128 + j * default_logo.bytes_per_pixel + 0];
      uint8_t g = default_logo.pixel_data[i * default_logo.bytes_per_pixel * 128 + j * default_logo.bytes_per_pixel + 1];
      uint8_t b = default_logo.pixel_data[i * default_logo.bytes_per_pixel * 128+ j * default_logo.bytes_per_pixel + 2];

      gfx_draw_pixel(gfx, x + j, y + i, (light_color_t) {
        .alpha = 0xFF,
        .red = r,
        .green = g,
        .blue = b,
      });
    }
  }
}

gfx_frontend_result_t gfx_enter_frontend()
{
  light_ctx_t* ctx = get_light_ctx();
  light_key_t key_buffer = { 0 };
  light_mousepos_t mouse_buffer = { 0 };
  gfx_frontend_result_t result;

  if (!ctx->f_has_keyboard()) {
    printf("Could not locate a keyboard!");
    return REBOOT;
  }

  /* Display the logo */
  gfx_display_logo(&light_gfx, 0, 0, LOGO_POS_TOP_BAR_RIGHT);

  /* Build the UI stack */
  create_component(&root_component, COMPONENT_TYPE_LABEL, "Light Loader", 4, 4, 130, 24, NULL);

  while (true) {
    FOREACH_UI_COMPONENT(i, root_component) {
      draw_component(i, key_buffer, mouse_buffer);
    }
  }
}

void 
get_light_gfx(light_gfx_t** gfx)
{
  if (!gfx)
    return;

  *gfx = &light_gfx;
}

void 
init_light_gfx()
{
  /* Make sure the structure is clean */
  memset(&light_gfx, 0, sizeof(light_gfx));

  light_gfx.current_font = &default_light_font;
  root_component = nullptr;
}
