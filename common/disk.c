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

  device->cache.cache_size = device->optimal_transfer_factor * device->sector_size;

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
  uint8_t preferred_cache_idx = 0;
  uint8_t lowest_usecount = 0xFF;
  for (uint8_t i = 0; i < 8; i++) {

    /* Free cache? Yoink */
    if ((device->cache.cache_dirty_flags & (1 << i)) == 0)
      return i;

    /* If there already is a cache with this block, get that one */
    if (device->cache.cache_block[i] == block) {
      return i;
    }

    if (device->cache.cache_usage_count[i] < lowest_usecount) {
      lowest_usecount = device->cache.cache_usage_count[i];
      lower_count++;
      preferred_cache_idx = i;
    }
  }

  /* If there was a cache with a lowest count, use that */
  if (lower_count)
    return preferred_cache_idx;

  preferred_cache_idx = device->cache.cache_oldest;

  device->cache.cache_oldest = (device->cache.cache_oldest + 1) % 7;

  return preferred_cache_idx;
}

void
cache_gpt_header(disk_dev_t* device)
{
  int error;

  uint32_t lb_size = 0;
  uint32_t lb_guesses[] = {
    512,
    4096,
  };

  uint8_t buffer[device->sector_size];
  gpt_header_t* header = (gpt_header_t*)buffer;

  for (size_t i = 0; i < (sizeof(lb_guesses) / sizeof(uint32_t)); i++) {

    /* Read one block */
    error = device->f_bread(device, buffer, 1, lb_guesses[i] / device->sector_size);

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
