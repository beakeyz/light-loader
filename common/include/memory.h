#ifndef __LIGHLOADER_MEMORY__
#define __LIGHLOADER_MEMORY__

#include <stddef.h>

// defines for alignment
#define ALIGN_UP(addr, size) \
    (((addr) % (size) == 0) ? (addr) : (addr) + (size) - ((addr) % (size)))

#define ALIGN_DOWN(addr, size) ((addr) - ((addr) % (size)))

void memset(void*, int, size_t);
void* memcpy(uint8_t* dst, const uint8_t* src, size_t size);
int strncmp(const char *s1, const char *s2, size_t n);

#endif // !__LIGHLOADER_MEMORY__
