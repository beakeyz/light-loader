#ifndef __LIGHTLOADER_GUID__
#define __LIGHTLOADER_GUID__

#include <stdint.h>

typedef struct guid {
  uint32_t a;
  uint16_t b;
  uint16_t c;
  uint8_t d[8];
} guid_t;

#endif // !__LIGHTLOADER_GUID__
