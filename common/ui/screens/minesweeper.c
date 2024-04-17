#include "ctx.h"
#include "gfx.h"
#include "image.h"
#include "ui/button.h"
#include "ui/component.h"
#include <stdint.h>
#include <stdio.h>
#include <ui/screens/minesweeper.h>
#include <heap.h>
#include <font.h>
#include <memory.h>

struct tile {
  bool has_mine;
  bool is_uncovered;
  bool is_flagged;
  /* There are a maximum of 8 mines around a particular tile, which means 
     we only need four bits to encode that */
  uint8_t mines_around;
  uint32_t index;
  button_component_t* btn;
}; 

#define TILE_WIDTH 36
#define TILE_HEIGHT 36

/*
 * This array is a one-to-one mapping of the amount
 * of mines that surround a particular tile and which sprite
 * needs to be used
 */
char* minecount_sprites[9] = {
  "res/sweeper/zero.bmp",
  "res/sweeper/one.bmp",
  "res/sweeper/two.bmp",
  "res/sweeper/three.bmp",
  "res/sweeper/four.bmp",
  "res/sweeper/five.bmp",
  "res/sweeper/six.bmp",
  "res/sweeper/seven.bmp",
  "res/sweeper/eight.bmp",
};

light_image_t* minecount_images[9] = { NULL, };

static char* mine_sprite = "res/sweeper/mine.bmp";
static char* flag_sprite = "res/sweeper/flag.bmp";
static char* reg_tile_sprite = "res/sweeper/tile.bmp";

static light_image_t* mine_image;
static light_image_t* flag_image;
static light_image_t* reg_tile_image;

/* Array of the tiles */
static light_ctx_t* ctx;
static struct tile* tiles;
static uint32_t tilecount;
static uint32_t minecount;
static uint32_t field_xres;
static uint32_t field_yres;
static uint32_t field_width;
static uint32_t field_height;

/* Local player data */
static bool is_alive;
static uint32_t levels_cleared;
static uint32_t tiles_cleared;
static uint32_t actual_mines_left;
static uint32_t mines_left;
static uint32_t explode_count;

static inline void
_reset_tiles_state()
{
  for (uint32_t i = 0; i < tilecount; i++) {
    /* Everything +4 bytes is persistant data */
    memset(&tiles[i], 0, 4);
  }
}

static inline void
_init_mine_tile(struct tile* start, uint8_t hstep, uint8_t vstep)
{
  uint32_t x_offset = 0;
  uint32_t y_offset = 0;

  for (uint32_t v = 0; v < vstep; v++) {
    for (uint32_t h = 0; h < hstep; h++) {
      start[x_offset + y_offset].mines_around++;

      x_offset++;
    }
    x_offset = 0;
    y_offset += field_xres;
  }
}

static void
_is_tile_at_edge(struct tile* t, bool* left, bool* right, bool* top, bool* bottom)
{
  *left = *right = *top = *bottom =  false;

  if (t->index % field_xres == 0)
    *left = true;
  else if (t->index % (field_xres-1) == 0)
    *right = true;

  if (t->index < field_xres)
    *top = true;
  else if (t->index > (tilecount - field_xres))
    *bottom = true;
}

/*!
 * @brief: Loops over the neighboring tiles to update the ->mines_around field
 */
static void
init_mine_tile(struct tile* t)
{
  struct tile* walker;
  uint8_t hstep, vstep;
  bool left, right, top, bottom;
  uint32_t i = t->index;

  walker = t;
  hstep = vstep = 3;

  _is_tile_at_edge(t, &left, &right, &top, &bottom);

  /* Not at the left edge, we can start from our left */
  if (!left)
    walker--;

  /* Not at the top, we can start from the topleft */
  if (!top)
    walker -= field_xres;

  /* Do one less step */
  if (right || left)
    hstep--;

  /* Do one lest vertical step */
  if (top || bottom)
    vstep--;

  _init_mine_tile(walker, hstep, vstep);
}

static void
init_minefield()
{
  struct tile* c_tile;
  uint64_t rng_val;

  /* Be cool */
  _reset_tiles_state();

  tiles_cleared = 0;
  /* TODO: Make this user-defined */
  minecount = 4;

  is_alive = true;
  mines_left = actual_mines_left = minecount;

  for (uint32_t i = 0; i < minecount; i++) {

    /* Find a non-mine tile */
    do {
      rng_val = NULL;

      ctx->f_get_random_num((uint8_t*)&rng_val, sizeof(rng_val));
      
      c_tile = &tiles[rng_val % tilecount];
    } while (c_tile->has_mine);

    c_tile->has_mine = true;
  }

  for (uint32_t i = 0; i < tilecount; i++) {
    c_tile = &tiles[i];

    c_tile->index = i;

    if (c_tile->has_mine)
      init_mine_tile(c_tile);

    /* Update */
    if (c_tile->btn && (c_tile->is_uncovered || c_tile->is_flagged))
      c_tile->btn->parent->should_update = true;
  }
}

static int
reset_onclick(button_component_t* btn)
{
  is_alive = true;

  init_minefield();
  return 0;
}

static int
tile_onclick(button_component_t* btn)
{
  struct tile* tile = (struct tile*)btn->private;

  if (!is_alive)
    return 0;

  /* Right mousebutton = flag */
  if (btn->parent->mouse_buffer && is_rmb_clicked(*btn->parent->mouse_buffer) && !tile->is_uncovered) {
    tile->is_flagged = !tile->is_flagged;

    if (tile->is_flagged && mines_left) {
      mines_left--;

      /* Adding a flag to a tile with a mine */
      if (tile->has_mine)
        actual_mines_left--;

    } else if (!tile->is_flagged) {
      mines_left++;

      /* Removing a flag from a tile with a mine */
      if (tile->has_mine)
        actual_mines_left++;
    }
  } else if (!tile->has_mine && !tile->is_flagged) {
    tile->is_uncovered = true;

    tiles_cleared++;

    /* Won! */
    if (tiles_cleared >= (tilecount - minecount)) {
      levels_cleared++;
      is_alive = false;
    }
  } else if (tile->has_mine) {
    is_alive = false;
    explode_count++;

    for (uint32_t i = 0; i < tilecount; i++) {
      if (!tiles[i].has_mine)
        continue;

      tiles[i].is_uncovered = true;
      tiles[i].btn->parent->should_update = true;
    }
  }

  btn->parent->should_update = true;
  return 0;
}

static int
tile_ondraw(light_component_t* c)
{
  button_component_t* btn = c->private;
  struct tile* tile = (struct tile*)btn->private;

  if (tile->is_uncovered && tile->has_mine)
    draw_image(c->gfx, c->x, c->y, mine_image);
  else if (tile->is_uncovered)
    draw_image(c->gfx, c->x, c->y, minecount_images[tile->mines_around]);
  else if (tile->is_flagged)
    draw_image(c->gfx, c->x, c->y, flag_image);
  else
    draw_image(c->gfx, c->x, c->y, reg_tile_image);

  return 0;
}

static int
value_track_ondraw(light_component_t* c)
{
  char* value;
  uint32_t strwidth;
  uint32_t center_y;
  button_component_t* btn = c->private;

  if (!btn->private)
    return -1;

  center_y = c->y + (c->height >> 1) - (c->font->height >> 1);
  strwidth = lf_get_str_width(c->font, c->label);
  value = to_string(*(uint32_t*)btn->private);

  gfx_draw_rect(c->gfx, c->x, c->y, c->width, c->height, LIGHT_GRAY);

  gfx_draw_str(c->gfx, c->label, c->x, center_y, BLACK); 
  gfx_draw_str(c->gfx, c->label, c->x + 1, center_y, WHITE); 

  gfx_draw_str(c->gfx, value, c->x + strwidth, center_y, BLACK);
  gfx_draw_str(c->gfx, value, c->x + strwidth + 1, center_y, WHITE);
  return 0;
}

static bool
value_track_update(light_component_t* c)
{
  return true;
}

static void
unload_sprites()
{
  if (mine_image)
    heap_free(mine_image);
  if (flag_image)
    heap_free(flag_image);
  if (reg_tile_image)
    heap_free(reg_tile_image);

  for (uint32_t i = 0; i < (sizeof minecount_sprites / sizeof minecount_sprites[0]); i++) {
    if (minecount_images[i])
      heap_free(minecount_images[i]);
  }
}

static int
try_load_sprites()
{
  mine_image = nullptr;
  flag_image = nullptr;
  reg_tile_image = nullptr;

  memset(minecount_images, 0, sizeof minecount_images);

  mine_image = load_bmp_image(mine_sprite);

  if (!mine_image)
    goto clean_and_exit;

  flag_image = load_bmp_image(flag_sprite);

  if (!flag_image)
    goto clean_and_exit;

  reg_tile_image = load_bmp_image(reg_tile_sprite);

  if (!reg_tile_image)
    goto clean_and_exit;

  for (uint32_t i = 0; i < (sizeof minecount_sprites / sizeof minecount_sprites[0]); i++) {
    minecount_images[i] = load_bmp_image(minecount_sprites[i]);

    if (!minecount_images[i])
      goto clean_and_exit;
  }

  return 0;
clean_and_exit:
  unload_sprites();
  return -1;
}

int 
construct_minesweeper_screen(light_component_t** root, light_gfx_t* gfx)
{
  struct tile* c_tile;
  button_component_t* c_btn;

  /* Cache the conext */
  ctx = gfx->ctx;

  field_xres = 8;
  field_yres = 8;
  field_width = field_xres * TILE_WIDTH;
  field_height = field_yres * TILE_HEIGHT;
  tilecount = field_xres * field_yres;
  tiles = ctx->f_allocate(tilecount * sizeof(struct tile));

  memset(tiles, 0, tilecount * sizeof(struct tile));

  /* Yikes */
  if (try_load_sprites() != 0)
    return 0;

  init_minefield();

  levels_cleared = 0;
  explode_count = 0;

  /* Use a button for this */
  c_btn = create_button(root,
      "Levels cleared: ", 8, 52, 256, 32, nullptr, value_track_ondraw);
  c_btn->private = (uintptr_t)&levels_cleared;
  c_btn->parent->f_should_update = value_track_update;

  c_btn = create_button(root,
      "Mines left: ", 8, 52 + 32 + 4, 256, 32, nullptr, value_track_ondraw);
  c_btn->private = (uintptr_t)&mines_left;
  c_btn->parent->f_should_update = value_track_update;
  
  c_btn = create_button(root,
      "Times exploded: ", 8, 52 + 32 + 32 + 8, 256, 32, nullptr, value_track_ondraw);
  c_btn->private = (uintptr_t)&explode_count;
  c_btn->parent->f_should_update = value_track_update;

  create_button(root, "Reset", 
      (gfx->width >> 1) - 128,
      (gfx->height >> 1) + (field_height >> 1) + 8,
      256, 42, reset_onclick, nullptr);

  uint32_t idx = 0;

  for (uint32_t i = 0; i < field_yres; i++) {
    for (uint32_t j = 0; j < field_xres; j++) {
      c_tile = &tiles[idx++];

      c_btn = create_button(root, nullptr,
          (gfx->width >> 1) - (field_width >> 1) + j * TILE_WIDTH,
          (gfx->height >> 1) - (field_height >> 1) + i * TILE_HEIGHT,
          TILE_WIDTH,
          TILE_HEIGHT,
          tile_onclick,
          tile_ondraw);

      c_btn->private = (uintptr_t)c_tile;
      c_tile->btn = c_btn;
    }
  }

  return 0;
}
