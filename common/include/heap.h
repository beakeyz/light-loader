#ifndef __LIGHT_HEAP__
#define __LIGHT_HEAP__

void init_heap(unsigned long long heap_addr, unsigned long long heap_size);
void* sbrk(unsigned long long size);

void* heap_allocate(unsigned long long size);
void heap_free(void* addr);

void debug_heap();

#endif // !__LIGHT_HEAP__
