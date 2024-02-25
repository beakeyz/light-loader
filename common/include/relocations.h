#ifndef __LIGHTLOADER_RELOCATIONS__
#define __LIGHTLOADER_RELOCATIONS__

#include <stdint.h>

typedef struct light_relocation {
  uintptr_t current;
  uintptr_t target;
  size_t size;
  struct light_relocation* next;
} light_relocation_t;

light_relocation_t* create_relocation(light_relocation_t** link_start, uintptr_t current, uintptr_t target, size_t size);
//void remove_relocation(light_relocation_t** link_start, light_relocation_t* relocation);

bool relocation_does_overlap(light_relocation_t** start, uintptr_t addr, size_t size);
uintptr_t highest_relocation_addr(light_relocation_t* link_start, uintptr_t previous_size);

#endif // !__LIGHTLOADER_RELOCATIONS__
