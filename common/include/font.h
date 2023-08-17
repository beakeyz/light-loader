#ifndef __LIGHTLOADER_FONT__
#define __LIGHTLOADER_FONT__

#include <stdint.h>
#include <memory.h>

typedef struct light_font {
  /* Width and Height, counted in pixels */
  uint8_t width, height;
  uint16_t bytes_per_glyph;
  uint32_t glyph_count;
  uint8_t data[];
} light_font_t;

light_font_t* create_light_font(uint8_t* font_data, uint8_t width, uint8_t height, uint32_t glyph_count, uint16_t bytes_per_glyph);
void destroy_light_font(light_font_t* font);

typedef struct light_glyph {
  uint8_t* data;
  light_font_t* parent_font;
} light_glyph_t;

int lf_get_glyph_at(light_font_t* font, uint32_t index, light_glyph_t* out);
int lf_get_glyph_for_char(light_font_t* font, char c, light_glyph_t* out);

static inline size_t
lf_get_str_width(light_font_t* font, char* string)
{
  return strlen(string) * font->width;
}

extern light_font_t default_light_font;

#endif // !__LIGHTLOADER_FONT__
