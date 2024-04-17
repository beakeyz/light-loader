#include "ctx.h"
#include "disk.h"
#include "efiapi.h"
#include "efidef.h"
#include "efierr.h"
#include "efilib.h"
#include "efiprot.h"
#include "file.h"
#include "fs.h"
#include "guid.h"
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
  uint8_t cache_idx = disk_select_cache(dev, block);

  if (!dev->cache.cache_ptr[cache_idx])
    goto out;

  /* Block already in our cache */
  if ((dev->cache.cache_dirty_flags & (1 << cache_idx)) && dev->cache.cache_block[cache_idx] == block)
    goto success;

  /* Make sure there are no other cache entries with this block */
  disk_clear_cache(dev, block);

  /* Read one block */
  error = dev->f_bread(dev, dev->cache.cache_ptr[cache_idx], 1, block);

  if (error)
    return 0;

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

  /* Add stuff */
  offset += (dev->first_sector * dev->effective_sector_size);

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
  int error;
  uint64_t current_block, current_offset, current_delta;
  uint64_t lba_size, read_size;
  uint8_t current_cache_idx;

  current_offset = 0;
  lba_size = dev->sector_size;

  /* Add stuff */
  offset += (dev->first_sector * dev->effective_sector_size);

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

    /* Write to disk Yay */
    error = dev->f_bwrite(dev, (dev->cache.cache_ptr[current_cache_idx]), 1, current_block);

    /* Try to write this fucker to disk lol */
    if (error)
      return -2;

    current_offset += read_size;
  }

  return 0;
}

/*!
 * @brief: Special Routine to clear a region of disk
 */
static int
__write_zero(struct disk_dev* dev, size_t size, uintptr_t offset)
{
  int error;
  uint64_t current_block, current_offset, current_delta;
  uint64_t lba_size, read_size;
  uint8_t current_cache_idx;

  current_offset = 0;
  lba_size = dev->sector_size;

  /* Add stuff */
  offset += (dev->first_sector * dev->effective_sector_size);

  while (current_offset < size) {
    current_block = (offset + current_offset) / lba_size;
    current_delta = (offset + current_offset) % lba_size;

    /* Simply select a cache for this block */
    current_cache_idx = disk_select_cache(dev, current_block);

    /* Fuck */
    if (current_cache_idx == 0xFF)
      return -1;

    /* Make sure we have no lingering caches left of this block */
    disk_clear_cache(dev, current_block);

    read_size = size - current_offset;

    /* Constantly shave off lba_size */
    if (read_size > lba_size - current_delta)
      read_size = lba_size - current_delta;

    /* Clear part of the buffer */
    memset(&(dev->cache.cache_ptr[current_cache_idx])[current_delta], NULL, read_size);

    /* Write a part of the buffer to disk */
    error = dev->f_bwrite(dev, (dev->cache.cache_ptr[current_cache_idx]), 1, current_block);

    /* Try to write this fucker to disk lol */
    if (error)
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
    return status;

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
    return status;
  
  return 0;
}

/*!
 * @brief: Just to be sure lol
 *
 */
static int
__flush(struct disk_dev* dev)
{
  efi_disk_stuff_t* stuff;

  if (!dev || !dev->private)
    return -1;

  stuff = dev->private;

  return stuff->blockio->FlushBlocks(stuff->blockio);
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

  ret->private = efi_private;

  ret->total_size = media->LastBlock * media->BlockSize;
  ret->total_sectors = media->LastBlock + 1;
  ret->first_sector = 0;

  ret->optimal_transfer_factor = media->OptimalTransferLengthGranularity;

  /* Clamp the transfer size */
  if (ret->optimal_transfer_factor == 0)         ret->optimal_transfer_factor = 64;
  else if (ret->optimal_transfer_factor > 512)   ret->optimal_transfer_factor = 512;

  ret->effective_sector_size = media->BlockSize;
  ret->sector_size = media->BlockSize * ret->optimal_transfer_factor;
  
  if (media->RemovableMedia)
    ret->flags |= DISK_FLAG_REMOVABLE;

  /*
   * Cache writes by default to increase speed 
   * This means that writes only go directly to disk once we 
   * swap blocks inside a cache entry, or when we manually flush
   * this disk (with calling disk_flush(...))
   *
   * TODO: Fix
   */
  disk_disable_cache_writes(ret);

  /* Make sure we aren't caching writes on EFI side lmao */
  media->WriteCaching = false;

  ret->f_read = __read;
  ret->f_write = __write;
  ret->f_write_zero = __write_zero;
  ret->f_bread = __bread;
  ret->f_bwrite = __bwrite;
  ret->f_flush = __flush;

  disk_init_cache(ret);

  return ret;
}

/*!
 * @brief: Check if a device contains a filesystem we know and files we expect for the bootloader
 *
 * TODO: make these paths centralised (For if we want to support multiple configurations)
 */
static inline int
check_lightloader_files(disk_dev_t* device)
{
  light_file_t* current_file;

  if (!device || !device->filesystem || !device->filesystem->f_open)
    return -1;

  /* First, check if the device has an X86_64 generic application (Our bootloader perchance) */
  current_file = device->filesystem->f_open(device->filesystem, "EFI/BOOT/BOOTX64.EFI");

  if (!current_file)
    return -2;

  current_file->f_close(current_file);

  /* Check for the kernel */
  current_file = device->filesystem->f_open(device->filesystem, "aniva.elf");

  if (!current_file)
    return -3;

  current_file->f_close(current_file);

  /* Check for a resources directory and a background image */
  current_file = device->filesystem->f_open(device->filesystem, "res/bckgrnd.bmp");

  if (!current_file)
    return -4;

  current_file->f_close(current_file);
  return 0;
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

  disk_dev_t* current_disk_dev;
  efi_disk_stuff_t* efi_disk_stuff;

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

  error = disk_probe_fs(bootdevice);

  /* If this fails, we should try a discovery and do a system-wide scan for a partition with the right stuff */
  if (!error) {
    register_bootdevice(bootdevice);
    return;
  }

  /* Go over every partition */
  for (uint32_t i = 0; i < ctx->present_volume_count; i++) {
    current_disk_dev = ctx->present_volume_list[i];
    efi_disk_stuff = current_disk_dev->private;

    /* Skip physical disks */
    if (!efi_disk_stuff)
      continue;

    current_disk_dev = current_disk_dev->next_partition;

    while (current_disk_dev) {
      error = disk_probe_fs(current_disk_dev);

      printf("Probing for filesystem...");

      if (error)
        goto cycle;

      printf("Checking for files...");
      error = check_lightloader_files(current_disk_dev);

      /*
       * FIXME: since we don't expect to find a shitload of FAT filesystems on a single 
       * it really does not matter right now that we don't do any cleanup here, but we 
       * really should...
       */
      if (error)
        goto cycle;

      printf("Registering!");
      /* If we've reached this point, we can simply register this device and dip */
      register_bootdevice(current_disk_dev);
      return;
cycle:
      current_disk_dev = current_disk_dev->next_partition;
    }
  }

  printf("Could not probe for filesystem on the bootdisk");
  for (;;) {}
}

/*!
 * @brief: Grabs the used partitions from a disk device with GPT
 */
static void
grab_disk_partitions(disk_dev_t* device)
{
  gpt_header_t hdr;
  gpt_entry_t entry;
  guid_t unused_guid;
  efi_disk_stuff_t* p_disk;

  if (!device || (device->flags & DISK_FLAG_PARTITION) == DISK_FLAG_PARTITION)
    return;

  p_disk = device->private;

  if (device->f_read(device, &hdr, sizeof(hdr), device->effective_sector_size))
    return;

  if (strncmp((const char*)hdr.signature, "EFI PART", 8))
    return;

  memset(&unused_guid, 0, sizeof(unused_guid));

  for (uint32_t i = 0; i < hdr.partition_entry_num; i++) {
    /* Try to read this partition entry */
    if (device->f_read(device, &entry, sizeof(entry), (hdr.partition_entry_lba * device->effective_sector_size) + (i * sizeof(entry))))
      continue;

    /* Check if the entry is used */
    if (memcmp(&entry.partition_type_guid, &unused_guid, sizeof(guid_t)))
      continue;

    create_and_link_partition_dev(device, entry.starting_lba, entry.ending_lba);
  }
}

/*!
 * @brief: Tries to find all the present volumes on the system through EFI
 *
 * This allocates an array of disk_dev_t pointers and puts those in the global light_ctx structure
 * To find the devices, we'll loop through all the handles we find that support the BLOCK_IO protocol
 * and add them if they turn out to be valid
 *
 * This gets called right before we enter the GFX frontend
 */
void 
efi_discover_present_volumes()
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

  ctx->present_volume_list = nullptr;
  ctx->present_volume_count = 0;

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

    /* Skip partitions from UEFI */
    if (!current_protocol->Media->MediaPresent || current_protocol->Media->LogicalPartition)
      continue;

    physical_count++;
  }

  /* Allocate the array */
  ctx->present_volume_count = physical_count;
  ctx->present_volume_list = heap_allocate(sizeof(disk_dev_t*) * physical_count);

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
    ctx->present_volume_list[physical_count] = create_efi_disk(current_protocol, nullptr);

    /* Try to cache the partitions on this disk */
    grab_disk_partitions(ctx->present_volume_list[physical_count]);

    physical_count++;
  }

  heap_free(handles);
}
