#include "disk.h"
#include "efilib.h"
#include "gfx.h"
#include "heap.h"
#include "stddef.h"
#include <memory.h>
#include <ctx.h>
#include <stdio.h>

disk_dev_t* bootdevice = nullptr;

void
register_bootdevice(disk_dev_t* device)
{
  bootdevice = device;
  device->next_partition = nullptr;
}

void
register_partition(disk_dev_t* device)
{
  disk_dev_t** part;

  part = &bootdevice;

  while (*part) {
    part = &(*part)->next_partition;
  }

  *part = device;
  device->next_partition = nullptr;
}

disk_dev_t* 
get_bootdevice()
{
  return bootdevice;
}

int 
disk_init_cache(disk_dev_t* device)
{
  device->cache.cache_oldest = 0;
  device->cache.cache_dirty_flags = NULL;

  device->cache.cache_size = device->sector_size;

  for (uint8_t i = 0; i < 8; i++) {
    device->cache.cache_ptr[i] = heap_allocate(device->cache.cache_size);
  }
  return 0;
}

int 
disk_clear_cache(disk_dev_t* device, uint64_t block)
{
  for (uint8_t i = 0; i < 8; i++) {
    if (device->cache.cache_block[i] != block)
      continue;

    /* Clear the dirty flag */
    device->cache.cache_dirty_flags &= ~(1 << i);
    device->cache.cache_usage_count[i] = NULL;
    device->cache.cache_block[i] = NULL;
    return 0;
  }

  return -1;
}

uint8_t
disk_select_cache(disk_dev_t* device, uint64_t block)
{
  uint8_t lower_count = 0;
  uint8_t preferred_cache_idx = (uint8_t)-1;
  uint8_t lowest_usecount = 0xFF;

  /*
   * Loop one: look for unused blocks, or blocks that are still in our
   * cache that we haven't written to yet (and flushed to disk so disk is
   * out of sync with the cache)
   */
  for (uint8_t i = 0; i < 8; i++) {

    /* Free cache? Mark it and go next */
    if ((device->cache.cache_dirty_flags & (1 << i)) == 0) {
      preferred_cache_idx = i;
      continue;
    }

    /* If there already is a cache with this block, get that one */
    if (device->cache.cache_block[i] == block)
      return i;
  }

  /* If there was a cache with a lowest count, use that */
  if (preferred_cache_idx != (uint8_t)-1)
    return preferred_cache_idx;

  /*
   * Loop two: find the cache with the lowest usecount so we can use that
   * boi to store our shit
   */
  for (uint8_t i = 0; i < 8; i++) {
    if (device->cache.cache_usage_count[i] < lowest_usecount) {
      lowest_usecount = device->cache.cache_usage_count[i];
      lower_count++;
      preferred_cache_idx = i;
    }
  }

  /* NOTE: Increased lower count implies that preferred_cache_idx got set to a valid index */
  if (lower_count)
    return preferred_cache_idx;

  /* Fuck: last option, use the in-house cycle to cycle to the next 'oldest' cache =/ */
  preferred_cache_idx = device->cache.cache_oldest;

  device->cache.cache_oldest = (device->cache.cache_oldest + 1) % 7;

  return preferred_cache_idx;
}

/*!
 * NOTE: ->f_bread is permitted here, since we allocate a buffer of sector_size on the stack
 * BUT: this buffer might exeed stack size so FIXME
 */
void
cache_gpt_header(disk_dev_t* device)
{
  int error;

  /* LMAO */
  if (device->flags & DISK_FLAG_PARTITION)
    return;

  uint32_t lb_size = 0;
  uint32_t lb_guesses[] = {
    512,
    4096,
  };

  uint8_t buffer[device->sector_size];
  gpt_header_t* header = (gpt_header_t*)buffer;

  for (size_t i = 0; i < (sizeof(lb_guesses) / sizeof(uint32_t)); i++) {

    /* Read one block */
    error = device->f_read(device, buffer, lb_guesses[i], sizeof(gpt_header_t));

    if (error)
      return;

    /* Check if the signature matches the one we need */
    if (strncmp((void*)header->signature, "EFI PART", 8))
      continue;

    lb_size = lb_guesses[i];

    break;
  }

  if (!lb_size)
    return;

  if (!device->partition_header)
    device->partition_header = heap_allocate(sizeof(gpt_header_t));

  memcpy((void*)device->partition_header, (void*)header, sizeof(*header));
}

void
cache_gpt_entry(disk_dev_t* device)
{
  int error;
  uint8_t buffer[device->sector_size];
  gpt_entry_t* entry;

  error = device->f_bread(device, buffer, 1, 0);

  if (error)
    return;

  entry = (gpt_entry_t*)buffer;

  for (uint64_t i = 0; i < 36*8; i++) {
    gfx_putchar(*((char*)entry->partition_name+i));
    if (i % 24 == 0)
      printf(" | ");
  }
}

void
disk_install_partitions(disk_dev_t* device)
{
  light_ctx_t* ctx;
  gpt_header_t header_template;
  gpt_entry_t current_entry;
  uint32_t partition_count;
  uint32_t total_required_size;

  /* Can't install to a partition lmao */
  if ((device->flags & DISK_FLAG_PARTITION) == DISK_FLAG_PARTITION)
    return;

  ctx = get_light_ctx();
  partition_count = 4;
  total_required_size = ALIGN_UP(sizeof(header_template), device->effective_sector_size) + ALIGN_UP(partition_count * sizeof(gpt_entry_t), device->effective_sector_size);;

  /* Prevent accidental installations */
  if (!ctx->install_confirmed)
    return;

  /*
   * TODO: CRCs????
   */

  /* Let's check if this disk already has a GPT header set up */
  cache_gpt_header(device);

  /* Great, we only need to write a little bit of stuff! */
  if (device->partition_header && memcmp(device->partition_header->signature, "EFI PART", 8)) {

    for (uint32_t i = 0; i < device->partition_header->partition_entry_num; i++) {
      // TODO:
    }

    return;
  }

  /* Bruh, we need to setup an entire GPT header + entries ourselves -_- */
  memset(&header_template, 0, sizeof(header_template));

  memcpy(&header_template.signature, "EFI PART", 8);

  /* Protective MBR should be created at lba 0 */
  header_template.my_lba = 1;
  header_template.alt_lba = -1;

  printf("Fuck, no GPT to yoink on that disk =/");
  for (;;) {}

}
