
#include "heap.h"
#include "efidef.h"

void init_heap(unsigned long long heap_addr, unsigned long long heap_size)
{
  (void)heap_addr;
  (void)heap_size;
}

void* sbrk(unsigned long long size)
{
  (void)size;
  return NULL;
}


void* heap_allocate(unsigned long long size)
{
  (void)size;
  return NULL;
}

void heap_free(void* addr, unsigned long long size)
{
  (void)addr;
  (void)size;
}
