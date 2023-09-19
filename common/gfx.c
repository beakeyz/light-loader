#include "ctx.h"
#include "efilib.h"
#include "efiprot.h"
#include "font.h"
#include "key.h"
#include "image.h"
#include "mouse.h"
#include "stddef.h"
#include "ui/box.h"
#include "ui/button.h"
#include "ui/component.h"
#include "ui/cursor.h"
#include "ui/image.h"
#include "ui/input_box.h"
#include "ui/screens/home.h"
#include "ui/screens/options.h"
#include <gfx.h>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>

static light_gfx_t light_gfx = { 0 };

/* Immutable root */
static light_component_t* root_component;

/* Mark the current component that is mounted as the root of the screen */
static light_component_t* current_screen_root;

static light_component_t* current_selected_btn;
static light_component_t* current_selected_inputbox;

#define SCREEN_HOME_IDX 0
#define SCREEN_OPTIONS_IDX 1

/* The different components that can be mounted as screenroot */
static light_component_t* screens[] = {
  [SCREEN_HOME_IDX] = nullptr,
  [SCREEN_OPTIONS_IDX] = nullptr,
};

static char* screen_labels[] = {
  [SCREEN_HOME_IDX] = "Home",
  [SCREEN_OPTIONS_IDX] = "Options",
};

static const size_t screens_count = sizeof screens / sizeof screens[0];

light_color_t WHITE = CLR(0xFFFFFFFF);
light_color_t BLACK = CLR(0x000000FF);
light_color_t GRAY = CLR(0x1e1e1eFF);
light_color_t LIGHT_GRAY = CLR(0x303030FF);

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

light_color_t 
gfx_transform_pixel(light_gfx_t* gfx, uint32_t clr)
{
  light_color_t ret = { 0 };

  ret.red = clr & gfx->red_mask;
  ret.green = clr & gfx->green_mask;
  ret.blue = clr & gfx->blue_mask;
  ret.alpha = clr & gfx->alpha_mask;

  return ret;
}

uint32_t
gfx_get_pixel(light_gfx_t* gfx, uint32_t x, uint32_t y)
{
  if (x >= gfx->width || y >= gfx->height || !gfx->back_fb)
    return NULL;

  return *(uint32_t volatile*)(gfx->back_fb + x * gfx->bpp / 8 + y * gfx->stride * sizeof(uint32_t));
}

void
gfx_draw_pixel_raw(light_gfx_t* gfx, uint32_t x, uint32_t y, uint32_t clr)
{
  if (x >= gfx->width || y >= gfx->height)
    return;

  if (!gfx->back_fb || !gfx->ctx->has_fw) {
    /* Direct access */
    *(uint32_t volatile*)(gfx->phys_addr + x * gfx->bpp / 8 + y * gfx->stride * sizeof(uint32_t)) = clr;
    return;
  }

  *(uint32_t volatile*)(gfx->back_fb + x * gfx->bpp / 8 + y * gfx->stride * sizeof(uint32_t)) = clr;

  /* When we are not drawing the cursor, we should look for any pixel updates */
  if (!gfx_is_drawing_cursor(gfx))
    update_cursor_pixel(gfx, x, y);
}

int 
lclr_blend(light_color_t fg, light_color_t bg, light_color_t* out)
{
  /* TODO: actual blending */

  if (!fg.alpha) {
    *out = bg;
    return 0;
  }

  *out = fg;
  return 0;
}

void
gfx_draw_pixel(light_gfx_t* gfx, uint32_t x, uint32_t y, light_color_t clr)
{
  if (!clr.alpha)
    return;
  
  if (x >= gfx->width || y >= gfx->height)
    return;

  /*
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL pixel = { 
    .Red = clr.red,
    .Green = clr.green, 
    .Blue = clr.blue,
    .Reserved = clr.alpha
  };
  */

  gfx_draw_pixel_raw(gfx, x, y, transform_light_clr(gfx, clr));
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
gfx_clear_screen(light_gfx_t* gfx)
{
  for (uint32_t i = 0; i < gfx->height; i++) {
    for (uint32_t j = 0; j < gfx->width; j++) {
      gfx_draw_pixel_raw(gfx, j, i, 0x00);
    }
  }

  /* Just in case */
  gfx_switch_buffers(gfx);
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

int 
gfx_switch_buffers(light_gfx_t* gfx)
{
  EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = gfx->priv;

  if (!gfx->back_fb || !gfx->ctx->has_fw)
    return -1;

  gop->Blt(gop, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)gfx->back_fb, EfiBltBufferToVideo, NULL, NULL, NULL, NULL, gfx->width, gfx->height, NULL);
  /*
  for (uint32_t i = 0; i < gfx->height; i++) {
    for (uint32_t j = 0; j < gfx->width; j++) {
      *(uint32_t volatile*)(gfx->phys_addr + j * gfx->bpp / 8 + i * gfx->stride * sizeof(uint32_t)) = gfx_get_pixel(gfx, j, i);
    }
  }
  */
  return 0;
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

/*
 * GFX screen management
 */

static light_component_t*
gfx_get_screen(uint32_t idx) 
{
  if (idx > screens_count)
    return nullptr;

  return screens[idx];
}

static light_component_t*
gfx_get_current_screen()
{
  if (!current_screen_root)
    return nullptr;

  return current_screen_root->next;
}

static uint32_t 
gfx_get_current_screen_type()
{
  light_component_t* current_screen = gfx_get_current_screen();

  if (!current_screen)
    return -1;

  for (uint32_t i = 0; i < screens_count; i++) {
    if (screens[i] == current_screen)
      return i;
  }

  return -1;
}

static int
gfx_do_screenswitch(light_gfx_t* gfx, light_component_t* new_screen)
{
  current_screen_root->next = new_screen;

  current_selected_btn = nullptr;
  gfx_select_inputbox(gfx, nullptr);

  gfx->flags |= GFX_FLAG_SHOULD_CHANGE_SCREEN;

  return 0;
}

static int
gfx_do_screen_update()
{
  light_component_t* root = root_component;

  /* Make sure every component gets rendered on the screenswitch */
  for (; root; root = root->next) {
    root->should_update = true;
  }

  return 0;
}

static int 
tab_btn_onclick(button_component_t* c)
{
  light_component_t* target;
  uint32_t target_index = c->private;

  /* Grab the target */
  target = gfx_get_screen(target_index);

  /* NOTE: we don't care if target is null, since that will be okay */
  gfx_do_screenswitch(c->parent->gfx, target);
  return 0;
}

static void
get_next_btn(light_component_t** out)
{
  light_component_t* itt;

  if (*out)
    itt = (*out)->next;
  else
    itt = root_component;

  while (itt) {

    if (itt->type == COMPONENT_TYPE_BUTTON)
      break;
    
    itt = itt->next;
  }

  *out = itt;
}

/*!
 * @brief Set the component @component as the current selected inputbox
 *
 * Nothing to add here...
 */
int 
gfx_select_inputbox(light_gfx_t* gfx, struct light_component* component)
{
  inputbox_component_t* inputbox;

  /* If the current inputbox is set, it is ensured to be of the type inputbox =) */
  if (current_selected_inputbox && current_selected_inputbox != component) {
    inputbox = current_selected_inputbox->private;

    inputbox->focussed = false;
    inputbox->current_input_size = 0;

    memset(inputbox->input_buffer, 0, INPUTBOX_BUFFERSIZE);
  }

  if (!component) {
    current_selected_inputbox = nullptr;
    return 0;
  }

  if (component->type != COMPONENT_TYPE_INPUTBOX)
    return -1;

  inputbox = component->private;

  if (!inputbox)
    return -2;

  inputbox->focussed = true;
  current_selected_inputbox = component; 

  return 0;
}

/*!
 * @brief Main bootloader frontend loop
 *
 * Nothing to add here...
 */
gfx_frontend_result_t gfx_enter_frontend()
{
  light_ctx_t* ctx = light_gfx.ctx;
  light_key_t key_buffer = { 0 };
  light_mousepos_t mouse_buffer = { 0 };
  gfx_frontend_result_t result;
  uint32_t prev_mousepx;

  current_selected_btn = nullptr;
  current_selected_inputbox = nullptr;

  gfx_clear_screen(&light_gfx);

  if (!ctx->f_has_keyboard()) {
    printf("Could not locate a keyboard! Plug in a keyboard and restart the PC.");
    return REBOOT;
  }

  /*
   * FIXME: the loader should still be able to function, even without a mouse
   */
  if (!ctx->f_has_mouse()) {
    printf("Could not locate a mouse!");
    return REBOOT;
  }

  init_keyboard();

  init_mouse();

  /* Initialize the cursor */
  init_cursor(&light_gfx);

  /* 
   * Build the UI stack 
   * We first build how the root layout is going to look. Think about the 
   * navigation bar, navigation buttons, any headers or footers, ect.
   *
   * after that we construct our screens, seperate from the root and then we
   * connect the screen up to the bottom of the root link if we want to show that
   * screen
   *
   * TODO: widgets -> interacable islands that are again their seperate things. we 
   * can use these for popups and things, but these need to be initialized seperately
   * aswell. Widgets are always relative to a certain component, and they are linked to
   * the ->active_widget field in the component and from that they each link through the
   * ->next field that every component has
   */

  /* Main background color */
  create_box(&root_component, nullptr, 0, 0, light_gfx.width, light_gfx.height, 0, true, GRAY);

  /* Navigation bar */
  create_box(&root_component, nullptr, 0, 0, light_gfx.width, 24, 0, false, LIGHT_GRAY);

  /* Navigation buttons */
  for (uint32_t i = 0; i < screens_count; i++) {
    const uint32_t btn_width = 80;
    const uint32_t initial_offset = 4;

    /* Add btn_width for every screen AND add the initial offset times the amount of buttons before us */
    uint32_t x_offset = i * btn_width + initial_offset * (i + 1);

    create_tab_button(&root_component, screen_labels[i], x_offset, 4, btn_width, 16, tab_btn_onclick, i);
  }

  /* Create a box for the home screen */
  construct_homescreen(&screens[SCREEN_HOME_IDX], &light_gfx);

  /* Create a box for the options screen */
  construct_optionsscreen(&screens[SCREEN_OPTIONS_IDX], &light_gfx);

  /*
   * Create the screen link, which never gets rendered, but simply acts as a connector for the screens to 
   * plug into if they want to get rendered
   * NOTE: this should be the last component to be directly linked into the actual root 
   */
  current_screen_root = create_component(&root_component, COMPONENT_TYPE_LINK, nullptr, NULL, NULL, NULL, NULL, NULL);

  /* Make sure we start at the homescreen */
  gfx_do_screenswitch(&light_gfx, screens[SCREEN_HOME_IDX]);

  /*
   * Loop until something wants us to exit
   */
  while ((light_gfx.flags & GFX_FLAG_SHOULD_EXIT_FRONTEND) != GFX_FLAG_SHOULD_EXIT_FRONTEND) {

    ctx->f_get_keypress(&key_buffer);
    ctx->f_get_mousepos(&mouse_buffer);

    /* Tab (temp) */
    if (key_buffer.typed_char == '\t' && !current_selected_inputbox) {
      get_next_btn(&current_selected_btn);

      if (current_selected_btn) {
        set_previous_mousepos((light_mousepos_t) {
          .x = current_selected_btn->x + 1,
          .y = current_selected_btn->y + 1,
          .btn_flags = 0
        });
      }
    } 

    FOREACH_UI_COMPONENT(i, root_component) {

      /* Skip lmao */
      if (!i->f_should_update || !i->f_ondraw)
        continue;

      i->key_buffer = &key_buffer;
      i->mouse_buffer = &mouse_buffer;

      update_component(i, key_buffer, mouse_buffer);

      /* Update prompted a screen chance. Do it */
      if ((light_gfx.flags & GFX_FLAG_SHOULD_CHANGE_SCREEN) == GFX_FLAG_SHOULD_CHANGE_SCREEN) {
        gfx_do_screen_update();
        break;
      }

      draw_component(i, key_buffer, mouse_buffer);
    }
    
    /* keep this clear after a draw pass */
    light_gfx.flags &= ~GFX_FLAG_SHOULD_CHANGE_SCREEN;

    draw_cursor(&light_gfx, mouse_buffer.x, mouse_buffer.y);

    gfx_switch_buffers(&light_gfx);
  }

  /* Make sure to not update the cursor stuff when we are not in the frontend */
  light_gfx.flags &= ~GFX_FLAG_SHOULD_DRAW_CURSOR;

  /* Default option, just boot our kernel */
  return BOOT_MULTIBOOT;
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
  light_gfx.ctx = get_light_ctx();
  root_component = nullptr;
}
