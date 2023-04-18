#ifndef __LIGHTLOADER_PMM__
#define __LIGHTLOADER_PMM__
#include "lib/multiboot.h"
#include <lib/light_mainlib.h>

typedef struct {
  uint64_t base;
  uint64_t limit;
  uint32_t type;
  uint32_t zero;
} mmap_descriptor_t;

// TODO: look into having just one mmap from UEFI (mainly, how to recover after the fact?)
typedef struct mmap_struct {
  mmap_descriptor_t *original_mmap; // used for mallocation
  mmap_descriptor_t *lightloader_mmap; // used for restoring UEFI memory and recovery later 

  size_t original_mmap_entries; 
  size_t lightloader_mmap_entries;

  size_t max_mmap_entries;
} mmap_struct_t;


typedef struct {
  size_t up;
  size_t lo;
} meminfo_t;

#define MEMMAP_USABLE 1
#define MEMMAP_RESERVED 2
#define MEMMAP_ACPI_RECLAIMABLE 3
#define MEMMAP_ACPI_NVS 4
#define MEMMAP_BAD_MEMORY 5
#define MEMMAP_BOOTLOADER_RECLAIMABLE 0x1000
#define MEMMAP_EFI_RECLAIMABLE 0x2000
#define MEMMAP_KRNL 0x1001
#define MEMMAP_FRAMEBUFFER 0x1002

#define ONE_MB_MARK 0x100000
#define FOUR_GB_MARK 0x100000000

#define SMALL_PAGE_SIZE 0x1000u

// defines for alignment
#define ALIGN_UP(addr, size)                                                   \
  ((addr % size == 0) ? (addr) : (addr) + size - ((addr) % size))

#define ALIGN_DOWN(addr, size) ((addr) - ((addr) % size))

meminfo_t mmap_get_info(size_t mmap_count, mmap_descriptor_t *mmap);

LIGHT_STATUS init_memmap(void);

// TODO: make all bootloader allocations go through the memmap, so when it
// eventually
//       gets passed to the kernel, it knows where things are to a certain
//       extend

LIGHT_STATUS pmm_mark_in_mmap(mmap_descriptor_t* map, size_t* mmap_size, uint64_t addr, size_t len, uint32_t mem_desc_type, uint32_t old_desc_type);
LIGHT_STATUS pmm_clean_entries_mmap(mmap_descriptor_t* map, size_t* entries); 

void *pmm_malloc(size_t len, uint32_t type);
void *pmm_free(void *ptr, size_t len);

multiboot_memory_map_t* get_raw_multiboot_memmap(size_t* count);

void pmm_debug_mmap();

extern mmap_struct_t g_mmap_struct;
#endif // !__LIGHTLOADER_PMM__
