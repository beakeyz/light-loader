#ifndef __LIGHLOADER_MEMORY__
#define __LIGHLOADER_MEMORY__

#include <stddef.h>

void memset(void*, int, size_t);
void* memcpy(uint8_t* dst, const uint8_t* src, size_t size);

#endif // !__LIGHLOADER_MEMORY__
