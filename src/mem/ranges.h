#ifndef __LIGHTLOADER_RANGES__ 
#define __LIGHTLOADER_RANGES__
#include "lib/light_mainlib.h"
#include <lib/libldef.h>

typedef struct {
  uintptr_t m_current_location;
  uintptr_t m_target;
  size_t m_range_size;
} mem_range_t;

bool does_range_overlap(mem_range_t* one, mem_range_t* two);
mem_range_t load_range(uintptr_t current, uintptr_t target, size_t size);
LIGHT_STATUS load_range_into_chain(mem_range_t** chain, size_t* chain_length, mem_range_t* range);

uintptr_t chain_find_highest_addr(mem_range_t** chain, size_t chain_length);

#endif
