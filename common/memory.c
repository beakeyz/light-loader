#include "memory.h"

void
memset(void* start, int val, size_t size)
{
  uint8_t* itt = (uint8_t*)start;
  for (uint64_t i = 0; i < size; i++) {
    itt[i] = val;
  }
}

void*
memcpy(void* dst, const void* src, size_t size)
{
  uint8_t* _dst = (uint8_t*)dst;
  uint8_t* _src = (uint8_t*)src;

  for (uint64_t i = 0; i < size; i++) {
    _dst[i] = _src[i];
  }

  return _dst;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c1 = s1[i], c2 = s2[i];
        if (c1 != c2)
            return c1 < c2 ? -1 : 1;
        if (!c1)
            return 0;
    }

    return 0;
}
