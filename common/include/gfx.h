#ifndef __LIGHTLOADER_GFX__
#define __LIGHTLOADER_GFX__

/*
 * The lightloader GFX context is to generefy the way that graphics attributes are
 * communicated
 */

#include <stdint.h>

struct light_font;
struct light_ctx;
struct light_image;
struct light_component;

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
extern light_color_t LIGHT_GRAY;
extern light_color_t DARK_GRAY;

extern light_color_t PINK;
extern light_color_t DARK_PINK;

extern light_color_t PURPLE;
extern light_color_t DARK_PURPLE;

extern light_color_t BLUE;
extern light_color_t DARK_BLUE;

extern light_color_t CYAN;
extern light_color_t DARK_CYAN; /* Yay, I love dark light blue =))))))) */

extern light_color_t GREEN;
extern light_color_t DARK_GREEN;

extern light_color_t RED;
extern light_color_t DARK_RED;


/* TODO: implement color blending opperations to make use of the alpha chanel */
int lclr_blend(light_color_t fg, light_color_t bg, light_color_t* out);

#define GFX_TYPE_NONE (0x0000)
#define GFX_TYPE_GOP (0x0001)
#define GFX_TYPE_UGA (0x0002) /* TODO: should we support UGA? */
#define GFX_TYPE_VEGA (0x0004) /* Should never be seen in these lands */

#define GFX_FLAG_DRAWING_CURSOR (0x0001)
#define GFX_FLAG_SHOULD_DRAW_CURSOR (0x0002)
#define GFX_FLAG_SHOULD_CHANGE_SCREEN (0x0004)
#define GFX_FLAG_SHOULD_EXIT_FRONTEND (0x0008)

typedef struct light_gfx {
  uintptr_t phys_addr;
  uintptr_t size;
  uint32_t width, height, stride;
  uint32_t red_mask, green_mask, blue_mask, alpha_mask;
  uint8_t bpp;
  uint8_t type;
  uint16_t flags;

  void* priv;

  struct light_font* current_font;
  struct light_ctx* ctx;

  /* Yay, doublebuffering 0.0 */
  uintptr_t back_fb;
  size_t back_fb_pages;

  struct light_image* _check_image;
  struct light_image* _checkbox_image;

} light_gfx_t;

void gfx_load_font(struct light_font* font);

uint32_t gfx_get_pixel(light_gfx_t* gfx, uint32_t x, uint32_t y);

light_color_t gfx_transform_pixel(light_gfx_t* gfx, uint32_t clr);

void gfx_clear_screen(light_gfx_t* gfx);

void gfx_draw_char(light_gfx_t* gfx, char c, uint32_t x, uint32_t y, light_color_t clr);
void gfx_draw_str(light_gfx_t* gfx, char* str, uint32_t x, uint32_t y, light_color_t clr);

void gfx_draw_rect_raw(light_gfx_t* gfx, uint32_t _x, uint32_t _y, uint32_t width, uint32_t height, uint32_t clr);
void gfx_draw_rect_img(light_gfx_t* gfx, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t* clrs);
void gfx_draw_rect(light_gfx_t* gfx, uint32_t x, uint32_t y, uint32_t width, uint32_t height, light_color_t clr);
void gfx_draw_rect_outline(light_gfx_t* gfx, uint32_t x, uint32_t y, uint32_t width, uint32_t height, light_color_t clr);
void gfx_draw_circle(light_gfx_t* gfx, uint32_t x, uint32_t y, uint32_t radius, light_color_t clr);

int gfx_printf(char* str, ...);
int gfx_putchar(char c);

int gfx_switch_buffers(light_gfx_t* gfx);

void get_light_gfx(light_gfx_t** gfx);

int gfx_select_inputbox(light_gfx_t* gfx, struct light_component* component);

void gfx_reset_btn_select();

static inline bool gfx_is_drawing_cursor(light_gfx_t* gfx)
{
  return ((gfx->flags & GFX_FLAG_DRAWING_CURSOR) == GFX_FLAG_DRAWING_CURSOR);
}

static inline void gfx_set_drawing_cursor(light_gfx_t* gfx)
{
  gfx->flags |= GFX_FLAG_DRAWING_CURSOR;
}

static inline void gfx_clear_drawing_cursor(light_gfx_t* gfx)
{
  gfx->flags &= ~GFX_FLAG_DRAWING_CURSOR;
}

typedef enum gfx_logo_pos {
  LOGO_POS_NONE = 0,
  LOGO_POS_CENTER = 1,
  LOGO_POS_TOP_BAR_RIGHT,
  LOGO_POS_BOTTOM_BAR_RIGHT,
} gfx_logo_pos_t;

void gfx_display_logo(light_gfx_t* gfx, uint32_t x, uint32_t y, gfx_logo_pos_t pos);

/*
 * When we exit the frontend, these are the things we can do
 */
typedef enum gfx_frontend_result {
  /* We are probably in a good install. Just boot from this disk */
  BOOT_MULTIBOOT = 0,
  /* Just clean and reboot (probably an error) */
  REBOOT,
  /* We are not sure about the install, verify files */
  VERIFY_INSTALL,
  /* We want to install on a different disk */
  LAUNCH_INSTALLER,
  /* Launch the autoupdater to check for updates */
  LAUNCH_AUTOUPDATES,
} gfx_frontend_result_t;

gfx_frontend_result_t gfx_enter_frontend();

void init_light_gfx();

#endif // !__LIGHTLOADER_GFX__
