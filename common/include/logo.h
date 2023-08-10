#ifndef __LIGHLOADER_LOGO__
#define __LIGHLOADER_LOGO__

#include <stdint.h>

typedef struct light_logo {
  uint_t  	 width;
  uint_t  	 height;
  uint_t  	 bytes_per_pixel;
  uint8_t 	 pixel_data[128 * 128 * 3 + 1];
} light_logo_t;

extern light_logo_t default_logo;

#endif // !__LIGHLOADER_LOGO__
