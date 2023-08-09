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
memcpy(uint8_t* dst, const uint8_t* src, size_t size)
{
  uint8_t* _dst = (uint8_t*)dst;
  uint8_t* _src = (uint8_t*)src;

  for (uint64_t i = 0; i < size; i++) {
    _dst[i] = _src[i];
  }

  return _dst;
}
