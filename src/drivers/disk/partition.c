#include "partition.h"
#include "drivers/disk/volume.h"
#include "lib/liblguid.h"

LIGHT_STATUS get_gpt_guid(light_volume_t* volume, guid_t* guid_ret) {
  // TODO
  return LIGHT_FAIL;
}

PART_SCAN_STATUS check_header(light_volume_t* volume, gpt_header_t* header, gpt_entry_t* entry, uint32_t lb_size, uint32_t idx) {

  if (!lb_size || header->revision != 0x00010000) {
    return INVALID;
  }

  if (header->partition_entry_num  <= idx) {
    return END;
  }

  uint32_t lb_offset = header->partition_entry_lba * lb_size;
  uint32_t part_offset = idx * sizeof(gpt_entry_t);
  
  if (read_volume(volume, entry, lb_offset + part_offset, sizeof(gpt_entry_t)) != LIGHT_SUCCESS) {
    return INVALID;
  }

  guid_t nill = {0};
  if (!memcmp(&entry->unique_partition_guid, &nill, sizeof(guid_t))) {
    return NO_PART;
  }

  return VALID;
}

PART_SCAN_STATUS get_gpt_partition(light_volume_t* volume_ret, light_volume_t* volume, uint32_t partition_num) {

  gpt_header_t header = {0};
  gpt_entry_t entry = {0};

  uint32_t lb_guesses[] = {
    512,
    4096
  };

  uint32_t lb_size = 0;
  for (size_t i = 0; i < (sizeof(lb_guesses) / sizeof(uint32_t)); i++) {

    read_volume(volume, &header, lb_guesses[i] * 1, sizeof(header));

    if (strncmp((const char*)header.signature, "EFI PART", 8)) {
      continue;
    }

    lb_size = lb_guesses[i];

    break;
  }

  PART_SCAN_STATUS scan_status = check_header(volume, &header, &entry, lb_size, partition_num); 
  if (scan_status != VALID) {
    return scan_status;
  }
  

  volume_ret->partition_guid = entry.unique_partition_guid;
  volume_ret->volume_handle = volume->volume_handle;
  volume_ret->block_io = volume->block_io;
  volume_ret->disk_io = volume->disk_io;
  volume_ret->parent_dev = volume;

  volume_ret->optimal_transfer_length_granularity = volume->optimal_transfer_length_granularity;

  volume_ret->idx = volume->idx;
  volume_ret->partition = partition_num + 1;
  volume_ret->sector_size = volume->sector_size;
  volume_ret->first_sector = entry.starting_lba * (lb_size / 512);
  volume_ret->sector_amount = ((entry.ending_lba - entry.starting_lba) + 1) * (lb_size / 512);

  volume_ret->is_opt = volume->is_opt;

  //g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"HI GPT PART =D\n\r");

  return scan_status;
}
