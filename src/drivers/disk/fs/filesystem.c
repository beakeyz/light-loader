#include "filesystem.h"
#include <drivers/disk/fs/fat32/fat32_fs.h>
#include <mem/pmm.h>

LIGHT_STATUS _clean_file_handle (handle_t* handle);
LIGHT_STATUS _clean_loaded_file_handle (loaded_handle_t* lhandle);
LIGHT_STATUS _clean_loaded_file_handle_full (loaded_handle_t* lhandle);


handle_t* open_file(light_volume_t* volume, const char* path) { 

  if (path[0] == '/') {
    memset((void*)&path[0], 0, sizeof(char));
    path++;
  }

  handle_t* ret = NULL;
  if ((ret = open_fat32(volume, path)) != NULL) {
    return ret;
  }

  return ret;
}

uint8_t* f_readall_raw(handle_t* handle) {

  void* ret = pmm_malloc(handle->file_size, MEMMAP_BOOTLOADER_RECLAIMABLE);

  handle->fRead(handle, ret, 0, handle->file_size);
  handle->fClose(handle);

  handle->file_descriptor = ret;
  return ret;
}

loaded_handle_t* f_readall(handle_t* handle) {

  loaded_handle_t* loaded_handle = pmm_malloc(sizeof(loaded_handle_t), MEMMAP_BOOTLOADER_RECLAIMABLE);

  loaded_handle->m_handle = handle;
  loaded_handle->fCleanHandle = _clean_loaded_file_handle;
  loaded_handle->fCleanHandleFull = _clean_loaded_file_handle_full;

  uint8_t* ret = pmm_malloc(handle->file_size, MEMMAP_BOOTLOADER_RECLAIMABLE);

  handle->fRead(handle, ret, 0, handle->file_size);
  handle->fClose(handle);

  loaded_handle->m_buffer = ret;

  return loaded_handle;
}


LIGHT_STATUS _clean_file_handle (handle_t* handle) {

  LIGHT_STATUS ret;

  if (handle->file_descriptor != NULL) {
    fat_file_handle_t* descriptor = handle->file_descriptor;

    if (descriptor->CleanFatHandle != NULL) {
      ret = descriptor->CleanFatHandle(descriptor);
    }
  }

  pmm_free(handle, sizeof(handle_t));

  // if we fail to clean our file descriptor, we just kinda die =)
  return ret;
}

LIGHT_STATUS _clean_loaded_file_handle (loaded_handle_t* lhandle) {
  pmm_free(lhandle->m_buffer, lhandle->m_handle->file_size);
  pmm_free(lhandle, sizeof(loaded_handle_t));
  // pretty much can't fail =)
  return LIGHT_SUCCESS;
}

LIGHT_STATUS _clean_loaded_file_handle_full (loaded_handle_t* lhandle) {
  pmm_free(lhandle->m_buffer, lhandle->m_handle->file_size);

  lhandle->m_handle->fCleanHandle(lhandle->m_handle);

  pmm_free(lhandle, sizeof(loaded_handle_t));
  // pretty much can't fail =)
  return LIGHT_SUCCESS;
}

