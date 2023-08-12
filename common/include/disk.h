#ifndef __LIGHTLOADER_DISK__
#define __LIGHTLOADER_DISK__

#include "efidef.h"
#include "guid.h"
#include <stdint.h>

struct light_fs;
struct gpt_header;

typedef struct disk_dev {

  uintptr_t start_offset;
  size_t total_size;
  size_t sector_size;
  size_t total_sectors;
  size_t optimal_transfer_factor;
  
  /* Platform specific stuff */
  void* private;

  /*
   * We have 8 caches to accomedate 8 blocks of data on disk
   * when accessing a cache, we increase the usage count of that
   * cache by one. When we run out of caches, we clear the least 
   * used cache. If there is no one cache that is used the least, 
   * we clear the oldest cache
   */
  struct {
    void* cache_ptr[8];
    uint32_t cache_block[8];
    uint8_t cache_dirty_flags;
    uint8_t cache_oldest;
    uint32_t cache_size;
    uint8_t cache_usage_count[8];
  } cache;

  struct light_fs* filesystem;
  struct gpt_header* partition_header;
  struct disk_dev* next_partition;

  /* I/O opperations for every disk */
  int (*f_read)(struct disk_dev* dev, void* buffer, size_t size, uintptr_t offset);
  int (*f_write)(struct disk_dev* dev, void* buffer, size_t size, uintptr_t offset);
  int (*f_bread)(struct disk_dev* dev, void* buffer, size_t count, uintptr_t lba);
  int (*f_bwrite)(struct disk_dev* dev, void* buffer, size_t count, uintptr_t lba);
} disk_dev_t;

void register_bootdevice(disk_dev_t* device);
void register_partition(disk_dev_t* device);
void cache_gpt_header(disk_dev_t* device);

int disk_init_cache(disk_dev_t* device);

uint8_t disk_select_cache(disk_dev_t* device, uint64_t block);
int disk_clear_cache(disk_dev_t* device, uint64_t block);

#define GPT_SIGNATURE 0x5452415020494645ULL

typedef struct gpt_header {
  uint8_t signature[8];
  uint32_t revision;
  uint32_t header_size;
  uint32_t header_crc32;

  uint32_t reserved;

  uint64_t my_lba;
  uint64_t alt_lba;
  uint64_t first_usable_lba;
  uint64_t last_usable_lba;

  guid_t disk_guid;

  uint64_t partition_entry_lba;
  uint32_t partition_entry_num;
  uint32_t partition_entry_size;
  uint32_t partition_entry_array_crc32;
  // the rest of the block is reserved by UEFI and must be zero
}__attribute__((packed)) gpt_header_t;

typedef struct gpt_entry {

  guid_t partition_type_guid;
  guid_t unique_partition_guid;

  // range that the partition spans on disk, in blocks
  uint64_t starting_lba;
  uint64_t ending_lba;

  uint64_t attributes;

  // (UEFI specification, page 125)
  // Null-terminated string containing a
  // human-readable name of the partition.
  uint16_t partition_name[36];
  
}__attribute__((packed)) gpt_entry_t;

#endif // !__LIGHTLOADER_DISK__
