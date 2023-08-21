#ifndef __LIGHTLOADER_ELF__
#define __LIGHTLOADER_ELF__

#include "relocations.h"
#include <stddef.h>

int load_relocatable_elf(void* buffer, size_t size, uintptr_t* entry_ptr, size_t* relocation_count, light_relocation_t** relocations);

#endif // !__LIGHTLOADER_ELF__
