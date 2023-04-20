#ifndef __LIGHTLOADER_SCREEN__ 
#define __LIGHTLOADER_SCREEN__
#include "drivers/display/framebuffer.h"
#include "lib/light_mainlib.h"

#define TO_ALPHA(rgb) ((uint8_t)(rgb >> 24))
#define TO_RED(rgb)   ((uint8_t)(rgb >> 16))
#define TO_GREEN(rgb) ((uint8_t)(rgb >> 8))
#define TO_BLUE(rgb)  ((uint8_t)(rgb))

#define FROM_ALPHA(a) ((a << 24))
#define FROM_RED(r)   (((r & 0xFF) << 16))
#define FROM_GREEN(g) (((g & 0xFF) << 8))
#define FROM_BLUE(b)  (((b & 0xFF)))

#define ARGB(a,r,g,b) (FROM_ALPHA(a) | FROM_RED(r) | FROM_GREEN(g) | FROM_BLUE(b))

struct light_screen;

typedef LIGHT_STATUS (*INIT_DOUBLE_BUFFER) (
  struct light_screen* screen
);

typedef LIGHT_STATUS (*DELETE_DOUBLE_BUFFER) (
  struct light_screen* screen
);

// drawing functions

typedef LIGHT_STATUS (*DRAW_PIXEL) (
  struct light_screen* screen,
  size_t x,
  size_t y,
  uint32_t clr
);

typedef uint32_t (*GET_PIXEL_COLOR) (
  struct light_screen* screen,
  size_t x,
  size_t y
);

typedef LIGHT_STATUS (*DRAW_BOX) (
  struct light_screen* screen,
  size_t x,
  size_t y,
  size_t width,
  size_t height,
  uint32_t clr  
);

typedef LIGHT_STATUS (*DRAW_HORIZONTAL_GRADIENT_BOX) (
  struct light_screen* screen,
  size_t x,
  size_t y,
  size_t width,
  size_t height,
  uint32_t clr1,
  uint32_t clr2
);

typedef LIGHT_STATUS (*DRAW_VERTICAL_GRADIENT_BOX) (
  struct light_screen* screen,
  size_t x,
  size_t y,
  size_t width,
  size_t height,
  uint32_t clr1,
  uint32_t clr2
);


typedef LIGHT_STATUS (*DRAW_ROUNDED_RECT) (
  struct light_screen* screen,
  size_t x,
  size_t y,
  size_t width,
  size_t height,
  size_t border_radius,
  uint32_t clr  
);

typedef LIGHT_STATUS (*DRAW_CIRCLE) (
  struct light_screen* screen,
  size_t x,
  size_t y,
  size_t r,
  uint32_t clr
);

typedef LIGHT_STATUS (*DRAW_LINE) (
  struct light_screen* screen,
  size_t x1,
  size_t y1,
  size_t x2,
  size_t y2,
  size_t thickness,
  uint32_t clr
);

// TODO: load font
typedef LIGHT_STATUS (*DRAW_CHAR) (
  struct light_screen* screen,
  char c,
  size_t x,
  size_t y,
  uint32_t clr
);

typedef LIGHT_STATUS (*DRAW_STRING) (
  struct light_screen* screen,
  const char* str,
  size_t x,
  size_t y,
  uint32_t clr
);

typedef LIGHT_STATUS (*SWAP_BUFFERS) (
  struct light_screen* screen
);

typedef struct light_screen {
  light_framebuffer_t* m_fb;

  void* m_double_buffer;
  size_t m_buffer_size;

  INIT_DOUBLE_BUFFER fInitDoubleBuffer;
  DELETE_DOUBLE_BUFFER fDeleteDoubleBuffer;
  SWAP_BUFFERS fSwapBuffers;

  DRAW_PIXEL fDrawPixel;
  GET_PIXEL_COLOR fGetPixelColor;
  DRAW_BOX fDrawBox;
  DRAW_HORIZONTAL_GRADIENT_BOX fDrawBoxGradientHorizontal;
  DRAW_CIRCLE fDrawCircle;
  DRAW_CHAR fDrawChar;
  DRAW_STRING fDrawString;

} __attribute__((packed)) light_screen_t;

light_screen_t* init_light_screen (light_framebuffer_t* fb);
uint32_t blend (uint32_t foreground, uint32_t background);

#endif // !
