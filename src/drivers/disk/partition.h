#ifndef __LIGHTLOADER_PARTITION__
#define __LIGHTLOADER_PARTITION__
#include "drivers/disk/volume.h"
#include "lib/liblguid.h"
#include <lib/light_mainlib.h>

typedef enum {
  NO_PART = 0,
  END,
  INVALID,
  VALID
} PART_SCAN_STATUS;

typedef struct {
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

typedef struct {

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

LIGHT_STATUS get_gpt_guid(light_volume_t* volume, guid_t* guid_ret); // TODO
PART_SCAN_STATUS check_header(light_volume_t* volume, gpt_header_t* header, gpt_entry_t* entry, uint32_t lb_size, uint32_t idx);
PART_SCAN_STATUS get_gpt_partition(light_volume_t* volume_ret, light_volume_t* volume, uint32_t partition_num);


#endif // !__LIGHTLOADER_PARTITION__
