#include "gfx.h"
#include "heap.h"
#include "image.h"
#include <memory.h>
#include <stdio.h>
#include <ui/cursor.h>

#define DEFAULT_CURSOR_WIDTH 16
#define DEFAULT_CURSOR_HEIGHT 16
#define CURSOR_PATH "res/lcrsor.bmp"

light_image_t* cursor;
bool has_cache;
size_t backbuffer_size;
uint32_t* cursor_backbuffer = nullptr;
uint32_t old_x, old_y;

light_image_t*
load_cursor(char* path)
{
    return load_bmp_image(path);
}

void init_cursor(light_gfx_t* gfx)
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

    gfx->flags |= GFX_FLAG_SHOULD_DRAW_CURSOR;
}

inline void
update_cursor_pixel(light_gfx_t* gfx, uint32_t x, uint32_t y, uint32_t clr)
{
    if (!cursor_backbuffer)
        return;

    /* If this pixel is contained in the buffer */
    if (x >= old_x && x < old_x + DEFAULT_CURSOR_WIDTH && y >= old_y && y < old_y + DEFAULT_CURSOR_HEIGHT)
        cursor_backbuffer[(y - old_y) * DEFAULT_CURSOR_WIDTH + (x - old_x)] = clr;
}

void draw_cursor(light_gfx_t* gfx, uint32_t x, uint32_t y)
{
    /* We're drawing the cursor */
    gfx_set_drawing_cursor(gfx);

    uint64_t cache_idx = 0;

    if (has_cache) {
        gfx_draw_rect_img(gfx, old_x, old_y, DEFAULT_CURSOR_WIDTH, DEFAULT_CURSOR_HEIGHT, cursor_backbuffer);
        // gfx_get_pixels(gfx, cursor_backbuffer, DEFAULT_CURSOR_WIDTH, DEFAULT_CURSOR_HEIGHT);
    }

    /* Cache the pixels on this location */
    for (uint32_t i = 0; i < DEFAULT_CURSOR_HEIGHT; i++)
        for (uint32_t j = 0; j < DEFAULT_CURSOR_WIDTH; j++)
            cursor_backbuffer[cache_idx++] = gfx_get_pixel(gfx, x + j, y + i);

    has_cache = true;

    /* Draw the cursor */
    draw_image(gfx, x, y, cursor);

    old_x = x;
    old_y = y;

    /* We're finished drawing the cursor */
    gfx_clear_drawing_cursor(gfx);
}
