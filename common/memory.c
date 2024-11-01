#include "memory.h"
#include "ctx.h"
#include "heap.h"
#include <stddef.h>

uintptr_t
memory_get_closest_usable_addr(uintptr_t addr)
{
    uintptr_t best_addr;
    uintptr_t c_delta;
    light_mmap_entry_t* mmap;

    for (uintptr_t i = 0; i < g_light_ctx.mmap_entries; i++) {
        mmap = &g_light_ctx.mmap[i];

        if (mmap->type != MEMMAP_USABLE)
            continue;

        /* Great! This address is just usable */
        if (addr >= mmap->paddr && addr < mmap->paddr + mmap->size)
            return addr;

        if (addr > mmap->paddr + mmap->size)
            continue;

        if ((mmap->paddr - addr) < (best_addr - addr))
            best_addr = mmap->paddr;
    }

    return best_addr;
}

void memset(void* start, int val, size_t size)
{
    uint8_t* itt = (uint8_t*)start;
    for (uint64_t i = 0; i < size; i++) {
        itt[i] = val;
    }
}

void* memcpy(void* dst, const void* src, size_t size)
{
    uint8_t* _dst = (uint8_t*)dst;
    uint8_t* _src = (uint8_t*)src;

    for (uint64_t i = 0; i < size; i++) {
        _dst[i] = _src[i];
    }

    return _dst;
}

int strncmp(const char* s1, const char* s2, size_t n)
{
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

const char* strdup_ex(const char* s, size_t len)
{
    char* ret;

    /* Allocate */
    ret = heap_allocate(len + 1);

    /* Copy the string */
    memcpy(ret, s, len);
    /* Terminate the string */
    ret[len] = '\0';

    return ret;
}

static char to_str_buff[128 * 2];

char* to_string(uint64_t val)
{
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
