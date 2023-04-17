#ifndef __LIGHTLOADER_LIBLGUID__
#define __LIGHTLOADER_LIBLGUID__
#include <lib/light_mainlib.h>

typedef struct guid {
  uint32_t a;
  uint16_t b;
  uint16_t c;
  uint8_t d[8];
} guid_t;

bool is_guid_valid(char*);

#endif // !__LIGHTLOADER_LIBLGUID__
