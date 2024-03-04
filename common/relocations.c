#include "heap.h"
#include "memory.h"
#include "stddef.h"
#include <relocations.h>

static light_relocation_t**
__find_relocation(light_relocation_t** start, uintptr_t addr, size_t size)
{
  light_relocation_t** i;

  for (i = start; *i; i = &(*i)->next) {
    light_relocation_t* _i = *i;

    if (_i->target == addr && _i->size == size)
      break;
  }

  return i;
}

light_relocation_t*
create_relocation(light_relocation_t** link_start, uintptr_t current, uintptr_t target, size_t size)
{
  light_relocation_t** slot;
  light_relocation_t* reloc = heap_allocate(sizeof(light_relocation_t));

  reloc->current = current;
  reloc->target = target;
  reloc->size = size;

  slot = __find_relocation(link_start, target, size);

  if (*slot) {
    heap_free(reloc);
    return nullptr;
  } else {
    *slot = reloc;
    reloc->next = nullptr;
  }

  return reloc;
}

bool 
relocation_does_overlap(light_relocation_t** start, uintptr_t addr, size_t size)
{
  for (light_relocation_t** i = start; *i; i = &(*i)->next) {
    /* Def not */
    if (addr > ((*i)->target + (*i)->size))
      continue;

    if (addr <= (*i)->target && addr + size >= (*i)->target)
      return true;

    if (addr > (*i)->target && addr <= ((*i)->target + (*i)->size))
      return true;
  }

  return false;
}

/*
void 
remove_relocation(light_relocation_t** link_start, light_relocation_t* relocation)
{

}
*/

uintptr_t 
highest_relocation_addr(light_relocation_t* link_start, uintptr_t previous_addr)
{
  uintptr_t ret = previous_addr;

  for (light_relocation_t* i = link_start; i; i = i->next) {
    if ((i->target + i->size) > ret)
      ret = i->target + i->size;
  }

  ret = memory_get_closest_usable_addr(ret);

  if (ret == previous_addr)
    return ret;

  return highest_relocation_addr(link_start, ret);
}

