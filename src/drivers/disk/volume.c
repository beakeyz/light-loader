#include "volume.h"
#include "drivers/disk/partition.h"
#include "efierr.h"
#include "lib/liblmath.h"
#include "lib/light_mainlib.h"
#include "mem/pmm.h"

public_volumes_t g_public_volumes;
volume_store_t g_volume_store;

public_volumes_t get_public_volumes() {
  return g_public_volumes;
}

light_volume_t* get_volume_through_efi_handle(EFI_HANDLE handle) {

  return NULL;
}

// TODO: bring back caches
LIGHT_STATUS read_volume(light_volume_t* part, void* buffer, uintptr_t location, size_t amount) {
  
  if (part == NULL) {
    g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[INFO] oof!\n\r");
    return LIGHT_FAIL;
  }

  size_t block_size = part->optimal_transfer_length_granularity * part->sector_size;
  size_t sector_size_funnie = (part->sector_size / 512);

  if (part->first_sector % sector_size_funnie) {
    g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[INFO] oof! nr. 2\n\r");
    return LIGHT_FAIL;
  }

  cache_t* cache = create_cache_for_volume(part);
  cache->cached_block = location;

  void* pre_buffer = pmm_malloc(block_size, MEMMAP_BOOTLOADER_RECLAIMABLE);

  // loop over every block and cache it
  for (uint64_t i = 0; i < amount; ) {
    uintptr_t block_addr = location + i;
    size_t block_num     = block_addr / block_size;
    size_t block_offset  = block_addr % block_size;
    size_t block_chunk   = min(amount - i, block_size - block_offset);

    uint32_t optimal_tlg = part->optimal_transfer_length_granularity;
    while (true) {

      // try to read the block
      if (volume_read_blocks(part, pre_buffer, (part->first_sector / sector_size_funnie) + block_num * part->optimal_transfer_length_granularity, optimal_tlg) == LIGHT_SUCCESS) {
        // TODO: more stuff
        cache->cache_blk_count++;
        break;
      }

      // hihi tow fixmes :clown:
      // FIXME: fix this crap
      optimal_tlg--;
      if (optimal_tlg == 0) {
        // yikes
        // FIXME: I know this mf does funnies, idk how to fix it yet =/
        // NOTE: it doesn't seem to have great impact on functionality so yk fuck it
        return LIGHT_FAIL;
      }
    }
    
    // we have our block in the prebuffer now

    //memcpy(cache->cache_ptr + i, &pre_buffer[block_offset], block_chunk);
    memcpy(buffer + i, &pre_buffer[block_offset], block_chunk);
    //g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[INFO] volume block read!\n\r");

    cache->cache_size += block_chunk;
    i += block_chunk;
    // TODO: cache
  }

  //memcpy(buffer, cache, sizeof(cache_t));
  pmm_free(pre_buffer, block_size);
  return LIGHT_SUCCESS;
}

LIGHT_STATUS volume_read_blocks(light_volume_t* volume, void* buffer, uintptr_t block, size_t count) {
  if (!volume || !count || !volume->block_io){
    return LIGHT_FAIL;
  }

  EFI_STATUS read_status = volume->block_io->ReadBlocks(volume->block_io, volume->block_io->Media->MediaId, block, count * volume->sector_size, buffer);

  if (read_status != EFI_SUCCESS) {
    return LIGHT_FAIL;
  }

  return LIGHT_SUCCESS;
}

LIGHT_STATUS create_volume_store_from_disk() {

  EFI_HANDLE tmp[1];
  EFI_HANDLE* handles_ptr = tmp;
  
  EFI_GUID bloc_io_guid = BLOCK_IO_PROTOCOL;
  UINTN handles_size = 0;

  EFI_STATUS s = g_light_info.boot_services->LocateHandle(ByProtocol, &bloc_io_guid, NULL, &handles_size, handles_ptr);
  if (s != EFI_SUCCESS && s != EFI_BUFFER_TOO_SMALL) {
    return LIGHT_FAIL_WITH_WARNING;
  }

  if (handles_size == 0) {
    return LIGHT_FAIL;
  }

  handles_ptr = pmm_malloc(handles_size, MEMMAP_BOOTLOADER_RECLAIMABLE);

  if (g_light_info.boot_services->LocateHandle(ByProtocol, &bloc_io_guid, NULL, &handles_size, handles_ptr) != EFI_SUCCESS) {
    return LIGHT_FAIL;
  }

  const uint8_t MAX_VOLUME_STORE_SIZE = 64;
  g_volume_store.store = pmm_malloc(sizeof(light_volume_t*) * MAX_VOLUME_STORE_SIZE, MEMMAP_BOOTLOADER_RECLAIMABLE);

  struct {
    uint32_t optical_count;
    uint32_t hdd_count;
  } volume_type_store;

  size_t handle_count = handles_size / sizeof(EFI_HANDLE);

  for (size_t i = 0; i < handle_count; i++) {

    if (g_volume_store.store_size == MAX_VOLUME_STORE_SIZE) {
      //g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[INFO] Broke out (store limit reached)\n\r");
      break;
    }

    EFI_GUID disk_io_guid = DISK_IO_PROTOCOL;
    EFI_DISK_IO* disk_io = NULL;
    EFI_BLOCK_IO* block_io = NULL;

    EFI_STATUS a = g_light_info.boot_services->HandleProtocol(handles_ptr[i], &disk_io_guid, (void**)&disk_io);

    if (a) {
      disk_io = NULL;
      //g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[INFO] Continue (disk_io == NULL)\n\r");
    }

    EFI_STATUS b = g_light_info.boot_services->HandleProtocol(handles_ptr[i], &bloc_io_guid, (void**)&block_io);

    if (b != 0 || block_io == NULL || !block_io->Media->LastBlock) {
      //g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[INFO] Continue (sum stuff == NULL)\n\r");
      continue;
    }

    if (block_io->Media->LogicalPartition) {
      //g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[INFO] Continue (LogicalPartition)\n\r");
      continue;
    }

    block_io->Media->WriteCaching = false;

    uint64_t funnie;
    EFI_STATUS read_stat;
    EFI_STATUS write_stat;
    if (disk_io != NULL){
      read_stat = disk_io->ReadDisk(disk_io, block_io->Media->MediaId, 0, sizeof(uint64_t), &funnie);
    } else { continue; }

    if (read_stat) {
      continue;
    }

    if (disk_io != NULL) {
      write_stat = disk_io->WriteDisk(disk_io, block_io->Media->MediaId, 0, sizeof(uint64_t), &funnie);
    } else { continue; }

    light_volume_t* block = pmm_malloc(sizeof(light_volume_t), MEMMAP_BOOTLOADER_RECLAIMABLE);

    if (write_stat || block_io->Media->ReadOnly) {
      block->is_opt = true;
      block->idx = volume_type_store.optical_count++;
    } else {
      block->idx = volume_type_store.hdd_count++;
    }

    block->block_io = block_io;
    block->disk_io = disk_io;

    block->volume_handle = handles_ptr[i];

    block->partition = 0;
    block->max_partition = -1;

    block->parent_dev = NULL;

    block->sector_size = block_io->Media->BlockSize;
    block->first_sector = 0;
    block->sector_amount = block_io->Media->LastBlock + 1;

    if (block_io->Revision >= EFI_BLOCK_IO_INTERFACE_REVISION3) {
      block->optimal_transfer_length_granularity = block_io->Media->OptimalTransferLengthGranularity;
    }

    // TODO: check that this does not fuck with the value
    block->optimal_transfer_length_granularity = (uint32_t)min((uint32_t)max(block->optimal_transfer_length_granularity, 0), 512);

    if (block->optimal_transfer_length_granularity == 0) {
      block->optimal_transfer_length_granularity = 64;
    }

    g_volume_store.store[g_volume_store.store_size++] = block;

    // TODO: function?
    for (size_t i = 0; ; i++) {
      light_volume_t _part = {0};
      PART_SCAN_STATUS p_stat = get_gpt_partition(&_part, block, i);
      if (p_stat == NO_PART){
        continue;
      }
      if (p_stat == INVALID || p_stat == END) {
        break;
      }

      light_volume_t* part = pmm_malloc(sizeof(light_volume_t), MEMMAP_BOOTLOADER_RECLAIMABLE);
      memcpy(part, &_part, sizeof(light_volume_t));

      g_volume_store.store[g_volume_store.store_size++] = part;

      block->max_partition++;
    }
  }
  // last entry is null
  g_volume_store.store[g_volume_store.store_size++] = NULL;
  
  pmm_free(handles_ptr, handles_size);

  return LIGHT_SUCCESS;
}

cache_t* create_cache_for_volume(light_volume_t* volume) {
  if (!volume) {
    return NULL;
  }
  
  cache_t* ptr = NULL;

  ptr->volume = volume;
  ptr->cache_ptr = NULL;
  ptr->cache_blk_count = 0;
  ptr->cache_size = 0;
  ptr->cached_block = 0;
  return ptr;
}

cache_t* init_cache_for_volume(light_volume_t* volume, void* cache_ptr) {
  if (!volume) {
    return NULL;
  }
  
  cache_t* ptr = NULL;

  ptr->volume = volume;
  ptr->cache_ptr = cache_ptr;
  ptr->cache_blk_count = 0;
  ptr->cache_size = 0;
  ptr->cached_block = 0;
  return ptr;
}


LIGHT_STATUS clean_cache(cache_t* cache) {

  if (!cache->cache_ptr) {
    return LIGHT_FAIL;
  }

  pmm_free(cache->cache_ptr, cache->cache_size);
  return LIGHT_SUCCESS;
}
