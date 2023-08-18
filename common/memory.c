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

bool memcmp(const void* a, const void* b, size_t size)
{
  uint8_t* _a = (uint8_t*)a;
  uint8_t* _b = (uint8_t*)b;
  for (uintptr_t i = 0; i < size; i++) {
    if (_a[i] != _b[i])
      return false;
  }

  return true;
}

size_t strlen(const char* s)
{
  size_t ret = 0;
  while (s[ret])
    ret++;
  return ret;
}

static char to_str_buff[128 * 2];

char* to_string(uint64_t val) {
    memset(to_str_buff, 0, sizeof(to_str_buff));
    uint8_t size = 0;
    uint64_t size_test = val;
    while (size_test / 10 > 0) {
        size_test /= 10;
        size++;
    }

    uint8_t index = 0;
    
    while (val / 10 > 0) {
        uint8_t remain = val % 10;
        val /= 10;
        to_str_buff[size - index] = remain + '0';
        index++;
    }
    uint8_t remain = val % 10;
    to_str_buff[size - index] = remain + '0';
    to_str_buff[size + 1] = 0;
    return to_str_buff;
}
