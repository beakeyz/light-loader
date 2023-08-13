#include "fat32.h"
#include "fs.h"
#include <heap.h>
#include <disk.h>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>

typedef struct fat_private {
  fat_bpb_t bpb;
  uint32_t root_directory_sectors;
  uint32_t total_reserved_sectors;
  uint32_t total_usable_sectors;
  uint32_t cluster_count;
  fat_dir_entry_t root_entry;
} fat_private_t;

int 
fat32_probe(light_fs_t* fs, disk_dev_t* device)
{
  int error;
  fat_private_t* private;
  fat_bpb_t bpb = { 0 };

  error = device->f_bread(device, &bpb, 1, device->first_sector);

  if (error)
    return error;

  // address for the identifier on fat32
  const char* fat32_ident = (((void*)&bpb) + 0x52);

  // address for the identifier on fat32 but fancy
  const char* fat32_ident_fancy = (((void*)&bpb) + 0x03);

  if (strncmp(fat32_ident, "FAT", 3) != 0 && strncmp(fat32_ident_fancy, "FAT32", 5) != 0)
    return -1;

  if (!bpb.bytes_per_sector)
    return -1;

  private = heap_allocate(sizeof(fat_private_t));

  private->root_directory_sectors = (bpb.root_entries_count * 32 + (bpb.bytes_per_sector - 1)) / bpb.bytes_per_sector;
  private->total_reserved_sectors = bpb.reserved_sector_count + (bpb.fat_num * bpb.sectors_num_per_fat) + private->root_directory_sectors;
  private->total_usable_sectors = bpb.sector_num_fat32 - private->total_reserved_sectors;
  private->cluster_count = private->total_usable_sectors / bpb.sectors_per_cluster;

  /* Not FAT32 */
  if (private->cluster_count < 65525) {
    heap_free(private);
    return -1;
  }

  memcpy(&private->bpb, &bpb, sizeof(fat_bpb_t));

  fs->private = private;

  return 0;
}

light_fs_t fat32_fs = 
{
  .fs_type = FS_TYPE_FAT32,
  .f_probe = fat32_probe,
};

void 
init_fat_fs()
{
  register_fs(&fat32_fs);
}
