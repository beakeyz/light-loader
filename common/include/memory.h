#ifndef __LIGHLOADER_MEMORY__
#define __LIGHLOADER_MEMORY__

#include <stdint.h>

// defines for alignment
#define ALIGN_UP(addr, size) \
    (((addr) % (size) == 0) ? (addr) : (addr) + (size) - ((addr) % (size)))

#define ALIGN_DOWN(addr, size) ((addr) - ((addr) % (size)))

#define PAGE_SHIFT 12
#define PAGE_SIZE (1 << PAGE_SHIFT)

#define MEMMAP_USABLE 1
#define MEMMAP_RESERVED 2
#define MEMMAP_ACPI_RECLAIMABLE 3
#define MEMMAP_ACPI_NVS 4
#define MEMMAP_BAD_MEMORY 5
#define MEMMAP_BOOTLOADER_RECLAIMABLE 0x1000
#define MEMMAP_EFI_RECLAIMABLE 0x2000
#define MEMMAP_KRNL 0x1001
#define MEMMAP_FRAMEBUFFER 0x1002

typedef struct light_mmap_entry {
  uint32_t type;
  uint32_t pad;
  uintptr_t paddr;
  size_t size;
} light_mmap_entry_t;

typedef struct {
  size_t up;
  size_t lo;
} meminfo_t;

uintptr_t memory_get_closest_usable_addr(uintptr_t addr);

void memset(void*, int, size_t);
void* memcpy(void* dst, const void* src, size_t size);

bool memcmp(const void* a, const void* b, size_t size);
int strncmp(const char *s1, const char *s2, size_t n);

size_t strlen(const char* s);

char* to_string(uint64_t val);

#endif // !__LIGHLOADER_MEMORY__
