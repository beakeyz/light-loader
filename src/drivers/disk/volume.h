#ifndef __LIGHTLOADER_VOLUME__
#define __LIGHTLOADER_VOLUME__

#include "lib/liblguid.h"
#include <lib/light_mainlib.h>

struct _FatManager;

#define VOLUME_FSTYPE_UNKNOWN       (0)
#define VOLUME_FSTYPE_FAT32         (1)

typedef struct light_volume {
  struct light_volume* parent_dev; // Can be null

  EFI_HANDLE volume_handle;
  EFI_HANDLE partition_handle;
  EFI_BLOCK_IO* block_io;
  EFI_DISK_IO* disk_io;

  uint32_t idx;

  bool is_opt;

  int partition;
  int max_partition;

  size_t sector_amount;
  size_t sector_size;
  uintptr_t first_sector;

  char* fslable;
  guid_t volume_guid;
  guid_t partition_guid;

  uint32_t optimal_transfer_length_granularity;

  uint32_t fs_type;

  union {
    struct _FatManager* fat_manager;
  };

} __attribute__((packed)) light_volume_t;

typedef struct {
  light_volume_t* volume;

  void* cache_ptr; // buffer
  uintptr_t cached_block; // block number
  size_t cache_blk_count; // amount of blocks cached
  size_t cache_size; // cache size in bytes
} __attribute__((packed)) cache_t;

cache_t* create_cache_for_volume(light_volume_t* volume);
cache_t* init_cache_for_volume(light_volume_t* volume, void*);
LIGHT_STATUS clean_cache(cache_t* cache);

typedef struct volume_store {
  size_t store_size;
  light_volume_t** store;
} volume_store_t;

typedef struct {
  light_volume_t* boot_volume;
} public_volumes_t;

extern public_volumes_t g_public_volumes;
extern volume_store_t g_volume_store;

public_volumes_t get_public_volumes(); // TODO

void all_volumes(); // TODO

light_volume_t* get_volume_through_guid(guid_t*); // TODO
light_volume_t* get_volume_through_efi_handle(EFI_HANDLE); // TODO
light_volume_t* get_volume(int drive_num, int partnum, bool is_opt); // TODO

LIGHT_STATUS read_volume(light_volume_t* part, void* buffer, uintptr_t location, size_t amount); // TODO

LIGHT_STATUS volume_read_blocks(light_volume_t* volume, void* buffer, uintptr_t block, size_t count);

LIGHT_STATUS create_volume_store_from_disk();

LIGHT_STATUS find_boot_volume(light_volume_t* ret_volume); // TODO

#endif // !__LIGHTLOADER_VOLUME__
