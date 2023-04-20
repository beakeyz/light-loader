#include "screen.h"
#include "lib/liblmath.h"
#include "lib/light_mainlib.h"
#include "mem/pmm.h"
#include "font.h"

LIGHT_STATUS _init_doublebuffer (
  light_screen_t*
);

LIGHT_STATUS _delete_doublebuffer (
  light_screen_t*
);

LIGHT_STATUS _swap_buffers (
  light_screen_t*
);

LIGHT_STATUS _draw_pixel (
  light_screen_t*,
  size_t,
  size_t,
  uint32_t
);

uint32_t _get_pixel_color (
  light_screen_t*,
  size_t,
  size_t
);

LIGHT_STATUS _draw_box(
  struct light_screen* screen,
  size_t x,
  size_t y,
  size_t width,
  size_t height,
  uint32_t clr  
);

LIGHT_STATUS _draw_horizontal_gradient_box(
  struct light_screen* screen,
  size_t x,
  size_t y,
  size_t width,
  size_t height,
  uint32_t clr1,
  uint32_t clr2
);


LIGHT_STATUS _draw_circle (
  struct light_screen* screen,
  size_t x,
  size_t y,
  size_t r,
  uint32_t clr
);

LIGHT_STATUS _draw_char (
  struct light_screen* screen,
  char c,
  size_t x,
  size_t y,
  uint32_t clr
);

LIGHT_STATUS _draw_string(
  struct light_screen* screen,
  const char* str,
  size_t x,
  size_t y,
  uint32_t clr
);

light_screen_t* init_light_screen (light_framebuffer_t* fb) {

  light_screen_t* ret = pmm_malloc(sizeof(light_screen_t), MEMMAP_BOOTLOADER_RECLAIMABLE);

  ret->m_fb = fb;

  ret->fDeleteDoubleBuffer = _delete_doublebuffer;
  ret->fInitDoubleBuffer = _init_doublebuffer;
  ret->fSwapBuffers = _swap_buffers;

  ret->fDrawBox = _draw_box;
  ret->fDrawBoxGradientHorizontal = _draw_horizontal_gradient_box;

  ret->fDrawPixel = _draw_pixel;
  ret->fGetPixelColor = _get_pixel_color;
  ret->fDrawCircle = _draw_circle;
  ret->fDrawChar = _draw_char;
  ret->fDrawString = _draw_string;

  ret->fInitDoubleBuffer(ret);

  return ret;
}

uint32_t blend(uint32_t foreground, uint32_t background) {
  uint8_t alpha_one = 255 - TO_ALPHA(foreground);
  uint8_t alpha_two = TO_ALPHA(foreground) + 1;

  uint8_t transform_r = (uint8_t) ((alpha_one * TO_RED(foreground) + alpha_two * TO_RED(background)) / 256);
  uint8_t transform_g = (uint8_t) ((alpha_one * TO_GREEN(foreground) + alpha_two * TO_GREEN(background)) / 256);
  uint8_t transform_b = (uint8_t) ((alpha_one * TO_BLUE(foreground) + alpha_two * TO_BLUE(background)) / 256);

  return ARGB(0, transform_r, transform_g, transform_b);
}

LIGHT_STATUS _draw_pixel (light_screen_t* screen, size_t x, size_t y, uint32_t clr) {

  if (x >= 0 && y >= 0 && x < screen->m_fb->m_fb_width && y < screen->m_fb->m_fb_height) {
    *((uint32_t*)(screen->m_double_buffer + 4 * screen->m_fb->m_gop->Mode->Info->PixelsPerScanLine * y + 4 * x)) = clr;
    return LIGHT_SUCCESS;
  }

  return LIGHT_FAIL;
}

uint32_t _get_pixel_color(light_screen_t* screen, size_t x, size_t y) {

  if (x >= 0 && y >= 0 && x < screen->m_fb->m_fb_width && y < screen->m_fb->m_fb_height) {
    return *(uint32_t*)(screen->m_double_buffer + 4 * screen->m_fb->m_gop->Mode->Info->PixelsPerScanLine * y + 4 * x);
  }
  return (uint32_t)0x000000;
}

LIGHT_STATUS _draw_box(light_screen_t* screen, size_t x, size_t y, size_t width, size_t height, uint32_t clr) {

  for (size_t i = y; i < y + height; i++) {
    for (size_t j = x; j < x + width; j++) {
      screen->fDrawPixel(screen, j, i, clr);
    }
  }
  return LIGHT_SUCCESS;
}

// inefficient lmao
LIGHT_STATUS _draw_horizontal_gradient_box (light_screen_t* screen, size_t x, size_t y, size_t width, size_t height, uint32_t clr1, uint32_t clr2) {

  // only available on sse systems
  //if (!g_light_info.has_sse) {
  //  return LIGHT_FAIL;
  //}

  uint8_t r1 = TO_RED(clr1);
  uint8_t g1 = TO_GREEN(clr1);
  uint8_t b1 = TO_BLUE(clr1);

  uint8_t r2 = TO_RED(clr2);
  uint8_t g2 = TO_GREEN(clr2);
  uint8_t b2 = TO_BLUE(clr2);

  for (size_t i = y; i < y + height; i++) {
    for (size_t j = x; j < x + width; j++) {

      size_t x_perc = j*1024 / (x + width);

      uint8_t interp_r = (uint8_t)int_interpolate(r1, r2, x_perc);
      uint8_t interp_g = (uint8_t)int_interpolate(g1, g2, x_perc);
      uint8_t interp_b = (uint8_t)int_interpolate(b1, b2, x_perc);

      screen->fDrawPixel(screen, j, i, ARGB(0, interp_r, interp_g, interp_b));
    }
  }
  return LIGHT_SUCCESS;
}

LIGHT_STATUS _draw_circle (struct light_screen* screen, size_t x, size_t y, size_t r, uint32_t clr) {

  int64_t starting_point_x = ((int64_t)(x - r) < 0) ? 0 : (x - r);
  int64_t starting_point_y = ((int64_t)(y - r) < 0) ? 0 : (y - r);

  for (int64_t i = starting_point_y; i < y + r; i++) {
    for (int64_t j = starting_point_x; j < x + r; j++) {
      uint64_t drawX = j;
      uint64_t drawY = i;
      if (drawX >= 0 && drawY >= 0) {
        int64_t deltaY = y - drawY;
        int64_t deltaX = x - drawX;
        int64_t distance_sq = deltaX*deltaX + deltaY*deltaY;
        if (distance_sq < r*r) {
          screen->fDrawPixel(screen, drawX, drawY, clr);
        }
      }
    }
  }

  return LIGHT_SUCCESS;
}

LIGHT_STATUS _draw_char (struct light_screen* screen, char c, size_t x, size_t y, uint32_t clr) {

  for (uint8_t _y = 0; _y < LIGHT_FONT_CHAR_HEIGHT; _y++) {
    for (uint8_t _x = 0; _x < LIGHT_FONT_CHAR_WIDTH; _x++) {

      uint8_t glyph_part = light_font[c*16 + _y];

      screen->fDrawPixel(screen, x + _x, y + _y, 0x00000000);

      if (glyph_part & (1 << _x)) {
        screen->fDrawPixel(screen, x + _x, y + _y, clr);
      }
    }
  }

  return LIGHT_SUCCESS;
}

LIGHT_STATUS _draw_string (struct light_screen* screen, const char* c, size_t x, size_t y, uint32_t clr) {

  uint64_t index = 0;

  while (c[index]) {
    screen->fDrawChar(screen, c[index], x + index * LIGHT_FONT_CHAR_WIDTH, y, clr);
    index++;
  }

  return LIGHT_FAIL;
}

LIGHT_STATUS _init_doublebuffer (light_screen_t* screen) {

  screen->m_buffer_size =  4 * screen->m_fb->m_gop->Mode->Info->PixelsPerScanLine * (screen->m_fb->m_fb_height - 1) + 4 * (screen->m_fb->m_fb_width - 1);
  screen->m_double_buffer = pmm_malloc(screen->m_buffer_size, MEMMAP_BOOTLOADER_RECLAIMABLE);

  return LIGHT_FAIL;
}

LIGHT_STATUS _swap_buffers (light_screen_t* screen) {

  memcpy((uint32_t*)screen->m_fb->m_fb_addr, screen->m_double_buffer, screen->m_buffer_size);

  return LIGHT_SUCCESS;
}

LIGHT_STATUS _delete_doublebuffer (light_screen_t* screen) {

  // NOTE: make sure buffer_size does not change
  pmm_free(screen->m_double_buffer, screen->m_buffer_size);
  screen->m_buffer_size = 0;

  return LIGHT_FAIL;
}

