#include "ctx.h"
#include "disk.h"
#include "efiapi.h"
#include "efidef.h"
#include "efierr.h"
#include "efilib.h"
#include "efiprot.h"
#include "elf64.h"
#include "file.h"
#include "fs.h"
#include "gfx.h"
#include "heap.h"
#include "stddef.h"
#include "sys/ctx.h"
#include <memory.h>
#include <stdio.h>
#include <sys/efidisk.h>

/*!
 * @brief Gets the cache where we put the block, or puts the block in a new cache
 *
 * Nothing to add here...
 */
static uint8_t 
__cached_read(struct disk_dev* dev, uintptr_t block)
{
  int error;
  uintptr_t start_block;
  uint8_t cache_idx = disk_select_cache(dev, block);

  if (!dev->cache.cache_ptr[cache_idx])
    goto out;

  /* Block already in our cache */
  if ((dev->cache.cache_dirty_flags & (1 << cache_idx)) && dev->cache.cache_block[cache_idx] == block)
    goto success;

  start_block = dev->first_sector / (dev->sector_size / 512);

  disk_clear_cache(dev, block);

  /* Read one block */
  error = dev->f_bread(dev, dev->cache.cache_ptr[cache_idx], 1, start_block + block);

  if (error)
    goto out;

success:
  dev->cache.cache_dirty_flags |= (1 << cache_idx);
  dev->cache.cache_block[cache_idx] = block;
  dev->cache.cache_usage_count[cache_idx]++;

  return cache_idx;
out:
  return 0xFF;
}

static int 
__read(struct disk_dev* dev, void* buffer, size_t size, uintptr_t offset)
{
  uint64_t current_block, current_offset, current_delta;
  uint64_t lba_size, read_size;
  uint8_t current_cache_idx;

  current_offset = 0;
  lba_size = dev->sector_size;

  while (current_offset < size) {
    current_block = (offset + current_offset) / lba_size;
    current_delta = (offset + current_offset) % lba_size;

    current_cache_idx = __cached_read(dev, current_block);

    if (current_cache_idx == 0xFF)
      return -1;

    read_size = size - current_offset;

    /* Constantly shave off lba_size */
    if (read_size > lba_size - current_delta)
      read_size = lba_size - current_delta;

    memcpy(buffer + current_offset, &(dev->cache.cache_ptr[current_cache_idx])[current_delta], read_size);

    current_offset += read_size;
  }

  return 0;
}

static int
__write(struct disk_dev* dev, void* buffer, size_t size, uintptr_t offset)
{
  uint64_t current_block, current_offset, current_delta;
  uint64_t lba_size, read_size;
  uint8_t current_cache_idx;

  current_offset = 0;
  lba_size = dev->sector_size;

  while (current_offset < size) {
    current_block = (offset + current_offset) / lba_size;
    current_delta = (offset + current_offset) % lba_size;

    /* Grab the block either from cache or from disk */
    current_cache_idx = __cached_read(dev, current_block);

    /* Fuck */
    if (current_cache_idx == 0xFF)
      return -1;

    read_size = size - current_offset;

    /* Constantly shave off lba_size */
    if (read_size > lba_size - current_delta)
      read_size = lba_size - current_delta;

    /* Copy from our buffer into the cache */
    memcpy(&(dev->cache.cache_ptr[current_cache_idx])[current_delta], buffer + current_offset, read_size);

    /* Try to write this fucker to disk lol */
    if (dev->f_bwrite(dev, (dev->cache.cache_ptr[current_cache_idx]), 1, current_block))
      return -2;

    current_offset += read_size;
  }

  return 0;
}

static int
__bread(struct disk_dev* dev, void* buffer, size_t count, uintptr_t lba)
{
  efi_disk_stuff_t* stuff;
  EFI_STATUS status;

  stuff = dev->private;

  status = stuff->blockio->ReadBlocks(stuff->blockio, stuff->media->MediaId, lba * dev->optimal_transfer_factor, count * dev->sector_size, buffer);

  if (EFI_ERROR(status))
    return -1;

  return 0;
}

static int
__bwrite(struct disk_dev* dev, void* buffer, size_t count, uintptr_t lba)
{
  EFI_STATUS status;
  efi_disk_stuff_t* stuff;

  stuff = dev->private;

  status = stuff->blockio->WriteBlocks(stuff->blockio, stuff->media->MediaId, lba * dev->optimal_transfer_factor, count * dev->sector_size, buffer);

  if (EFI_ERROR(status))
    return -1;
  
  return 0;
}

static disk_dev_t*
create_efi_disk(EFI_BLOCK_IO_PROTOCOL* blockio, EFI_DISK_IO_PROTOCOL* diskio) 
{
  disk_dev_t* ret;
  efi_disk_stuff_t* efi_private;
  EFI_BLOCK_IO_MEDIA* media;

  /* Fucky ducky, we can't live without blockio ;-; */
  if (!blockio)
    return nullptr;

  efi_private = heap_allocate(sizeof(efi_disk_stuff_t));
  ret = heap_allocate(sizeof(disk_dev_t));

  memset(ret, 0, sizeof(disk_dev_t));
  memset(efi_private, 0, sizeof(efi_disk_stuff_t));

  media = blockio->Media;

  /* NOTE: Diskio is mostly unused right now, and disks can be created without diskio */
  efi_private->diskio = diskio;
  efi_private->blockio = blockio;
  efi_private->media = media;

  /* Make sure we aren't caching writes lmao */
  media->WriteCaching = false;

  ret->private = efi_private;

  ret->total_size = media->LastBlock * media->BlockSize;
  ret->first_sector = 0;

  ret->optimal_transfer_factor = media->OptimalTransferLengthGranularity;

  /* Clamp the transfer size */
  if (ret->optimal_transfer_factor == 0)    ret->optimal_transfer_factor = 32;
  if (ret->optimal_transfer_factor > 512)   ret->optimal_transfer_factor = 512;

  ret->sector_size = media->BlockSize * ret->optimal_transfer_factor;

  ret->f_read = __read;
  ret->f_write = __write;
  ret->f_bread = __bread;
  ret->f_bwrite = __bwrite;

  disk_init_cache(ret);

  return ret;
}

#define MAX_PARTITION_COUNT 64

/*!
 * @brief: Initialize the filesystem and stuff on the 'disk device' (really just the partition) we booted off
 *
 * This creates a disk device and tries to mount a filesystem
 */
void
init_efi_bootdisk()
{
  int error;
  light_ctx_t* ctx;
  efi_ctx_t* efi_ctx;

  ctx = get_light_ctx();
  efi_ctx = get_efi_context();

  if (!efi_ctx->bootdisk_block_io || !efi_ctx->bootdisk_io)
    return;

  /*
   * NOTE: The 'bootdevice' is actually the bootpartition, so don't go trying to look
   * for a partitioning sceme on this 'disk' LMAO
   */
  disk_dev_t* bootdevice = create_efi_disk(efi_ctx->bootdisk_block_io, efi_ctx->bootdisk_io);

  if (!bootdevice)
    return;

  /* UEFI Gives us a handle to our partition, not the entire disk lol */
  bootdevice->flags |= DISK_FLAG_PARTITION;

  register_bootdevice(bootdevice);

  error = disk_probe_fs(bootdevice);
}

/*!
 * @brief: Tries to find all the present physical disk drives on the system through EFI
 *
 * This allocates an array of disk_dev_t pointers and puts those in the global light_ctx structure
 * To find the devices, we'll loop through all the handles we find that support the BLOCK_IO protocol
 * and add them if they turn out to be valid
 *
 * This gets called right before we enter the GFX frontend
 */
void 
efi_discover_present_diskdrives()
{
  light_ctx_t* ctx;
  uint32_t physical_count;
  size_t handle_count;
  size_t buffer_size;
  EFI_STATUS status;
  EFI_HANDLE* handles;
  EFI_GUID block_io_guid = EFI_BLOCK_IO_PROTOCOL_GUID;
  EFI_BLOCK_IO_PROTOCOL* current_protocol;

  ctx = get_light_ctx();

  ctx->present_disk_list = nullptr;
  ctx->present_disk_count = 0;

  /* Grab the handles that support BLOCK_IO */
  handle_count = locate_handle_with_buffer(ByProtocol, block_io_guid, &buffer_size, &handles);

  /* Fuck */
  if (!handle_count)
    return;

  physical_count = 0;

  /* Loop over them to count the physical devices */
  for (uint32_t i = 0; i < handle_count; i++) {
    EFI_HANDLE current_handle = handles[i];

    status = open_protocol(current_handle, &block_io_guid, (void**)&current_protocol);

    if (EFI_ERROR(status))
      continue;

    if (!current_protocol->Media->MediaPresent || current_protocol->Media->LogicalPartition)
      continue;

    physical_count++;
  }

  /* Allocate the array */
  ctx->present_disk_count = physical_count;
  ctx->present_disk_list = heap_allocate(sizeof(disk_dev_t*) * physical_count);

  /* Reuse this variable as an index */
  physical_count = 0;

  /* Loop again to create our disk devices */
  for (uint32_t i = 0; i < handle_count; i++) {
    EFI_HANDLE current_handle = handles[i];

    status = open_protocol(current_handle, &block_io_guid, (void**)&current_protocol);

    if (EFI_ERROR(status))
      continue;

    if (!current_protocol->Media->MediaPresent || current_protocol->Media->LogicalPartition)
      continue;

    /* Create disks without a diskio protocol */
    ctx->present_disk_list[physical_count++] = create_efi_disk(current_protocol, nullptr);

    /* TODO: register partitions */
  }
}
