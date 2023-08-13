#include "ctx.h"
#include "disk.h"
#include "efidef.h"
#include "efierr.h"
#include "efiprot.h"
#include "heap.h"
#include "stddef.h"
#include "sys/ctx.h"
#include <memory.h>
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
  uintptr_t current_transfer_factor;
  uint8_t cache_idx = disk_select_cache(dev, block);

  if (!dev->cache.cache_ptr[cache_idx])
    goto out;

  /* Block already in our cache */
  if ((dev->cache.cache_dirty_flags & (1 << cache_idx)) && dev->cache.cache_block[cache_idx] == block)
    goto success;

  start_block = dev->first_sector / (dev->sector_size / 512);
  current_transfer_factor = dev->optimal_transfer_factor;

  disk_clear_cache(dev, block);

  while (1) {
    error = dev->f_bread(dev, dev->cache.cache_ptr[cache_idx], current_transfer_factor, start_block + block * dev->optimal_transfer_factor);

    if (!error)
      break;

    if (current_transfer_factor)
      current_transfer_factor--;
    else
      goto out;
  }

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
  lba_size = dev->optimal_transfer_factor * dev->sector_size;

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

    memcpy(buffer + current_offset, &dev->cache.cache_ptr[current_cache_idx][current_delta], read_size);

    current_offset += read_size;
  }

  return 0;
}

static int
__write(struct disk_dev* dev, void* buffer, size_t size, uintptr_t offset)
{
  return 0;
}

static int
__bread(struct disk_dev* dev, void* buffer, size_t count, uintptr_t lba)
{
  efi_disk_stuff_t* stuff;
  EFI_STATUS status;

  stuff = dev->private;

  status = stuff->blockio->ReadBlocks(stuff->blockio, stuff->media->MediaId, lba, count * dev->sector_size, buffer);

  if (EFI_ERROR(status))
    return -1;

  return 0;
}

static int
__bwrite(struct disk_dev* dev, void* buffer, size_t count, uintptr_t lba)
{
  return 0;
}

static disk_dev_t*
create_efi_disk(EFI_BLOCK_IO_PROTOCOL* blockio, EFI_DISK_IO_PROTOCOL* diskio) 
{
  disk_dev_t* ret;
  efi_disk_stuff_t* efi_private;
  EFI_BLOCK_IO_MEDIA* media;

  /* Fucky ducky */
  if (!blockio || !diskio)
    return nullptr;

  efi_private = heap_allocate(sizeof(efi_disk_stuff_t));
  ret = heap_allocate(sizeof(disk_dev_t));

  media = blockio->Media;

  efi_private->diskio = diskio;
  efi_private->blockio = blockio;
  efi_private->media = media;

  ret->private = efi_private;

  ret->total_size = media->LastBlock * media->BlockSize;
  ret->sector_size = media->BlockSize;
  ret->first_sector = 0;

  ret->optimal_transfer_factor = media->OptimalTransferLengthGranularity;

  /* Clamp the transfer size */
  if (ret->optimal_transfer_factor == 0)    ret->optimal_transfer_factor = 64;
  if (ret->optimal_transfer_factor > 512)   ret->optimal_transfer_factor = 512;

  ret->f_read = __read;
  ret->f_write = NULL;
  ret->f_bread = __bread;
  ret->f_bwrite = NULL;

  disk_init_cache(ret);

  return ret;
}

#define MAX_PARTITION_COUNT 64

void
init_efi_bootdisk()
{
  light_ctx_t* ctx;
  efi_ctx_t* efi_ctx;

  ctx = get_light_ctx();
  efi_ctx = get_efi_context();

  if (!efi_ctx->bootdisk_block_io || !efi_ctx->bootdisk_io)
    return;

  /*
   * NOTE: The 'bootdevice' is actually the bootpartition
   */
  disk_dev_t* bootdevice = create_efi_disk(efi_ctx->bootdisk_block_io, efi_ctx->bootdisk_io);

  if (!bootdevice)
    return;

  register_bootdevice(bootdevice);

  if (bootdevice->partition_header) {
    printf("Found partition header: ");
    char d[9];
    memcpy((uint8_t*)d, bootdevice->partition_header->signature, 8);
    d[8] = 0;
    printf(d);
  } else {
    printf("Could not find partition_header");
  }

  /* Go through each partition */
  for (uint8_t i = 0; i < MAX_PARTITION_COUNT; i++) {
    
  }

}
