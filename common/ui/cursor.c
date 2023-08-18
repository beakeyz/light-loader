#include "file.h"
#include "gfx.h"
#include "heap.h"
#include "image.h"
#include "memory.h"
#include <stdio.h>
#include <ui/cursor.h>

#define DEFAULT_CURSOR_WIDTH 16
#define DEFAULT_CURSOR_HEIGHT 16
#define CURSOR_PATH "res/lcrsor.bmp"

light_image_t* cursor;
bool has_cache;
size_t backbuffer_size;
uint32_t* cursor_backbuffer;
uint32_t old_x, old_y;

light_image_t*
load_cursor(char* path)
{
  return load_bmp_image(path);
}

void 
init_cursor()
{
  cursor = load_cursor(CURSOR_PATH);

  if (!cursor)
    return;

  backbuffer_size = sizeof(light_color_t) * cursor->width * cursor->height;
  cursor_backbuffer = heap_allocate(backbuffer_size);

  if (cursor_backbuffer)
    return;

  memset(cursor_backbuffer, 0, backbuffer_size);

  has_cache = false;
  old_x = old_y = NULL;
}

void 
draw_cursor(light_gfx_t* gfx, uint32_t x, uint32_t y)
{
  uint64_t cache_idx = 0;

  if (!has_cache)
    goto do_redraw;

  /* Draw back pixels if we have them */
  for (uint32_t i = 0; i < DEFAULT_CURSOR_HEIGHT; i++) {
    for (uint32_t j = 0; j < DEFAULT_CURSOR_WIDTH; j++) {
      gfx_draw_pixel_raw(gfx, old_x + j, old_y + i, cursor_backbuffer[cache_idx++]);
    }
  }

do_redraw:
  cache_idx = 0;

  /* Cache the pixels on this location */
  for (uint32_t i = 0; i < DEFAULT_CURSOR_HEIGHT; i++) {
    for (uint32_t j = 0; j < DEFAULT_CURSOR_WIDTH; j++) {
      cursor_backbuffer[cache_idx++] = gfx_get_pixel(gfx, x + j, y + i);
    }
  }

  has_cache = true;

  /* Draw the cursor */
  draw_image(gfx, x, y, cursor);

  old_x = x;
  old_y = y;
}