#ifndef __LIGHTLOADER_VMM__
#define __LIGHTLOADER_VMM__

#include "lib/light_mainlib.h"

LIGHT_STATUS vmm_map_page(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags);

#endif // __LIGHTLOADER_VMM__

