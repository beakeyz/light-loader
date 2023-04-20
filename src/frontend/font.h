#ifndef __LIGHTLOADER_FONT__
#define __LIGHTLOADER_FONT__
#include <lib/libldef.h>

// Credit goes to https://github.com/apple/darwin-xnu/blob/main/osfmk/console/iso_font.c

extern uint8_t light_font[256 * 16];

#define LIGHT_FONT_START        0x00
#define LIGHT_FONT_END          0xFF
#define LIGHT_FONT_CHAR_WIDTH   8
#define LIGHT_FONT_CHAR_HEIGHT  16

#endif // !__LIGHTLOADER_FONT__

