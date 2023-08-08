#include "memory.h"

void
memset(void* start, int val, size_t size)
{
  uint8_t* itt = (uint8_t*)start;
  for (uint64_t i = 0; i < size; i++) {
    itt[i] = val;
  }
}
