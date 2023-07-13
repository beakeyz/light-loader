#ifndef __LIGHTLOADER_FILESYSTEM__
#define __LIGHTLOADER_FILESYSTEM__

#include "drivers/disk/volume.h"
#include "efiprot.h"
#include "lib/liblguid.h"
#include <lib/light_mainlib.h>

struct file_handle;
struct loaded_file_handle;

typedef void (*F_NONGLOBALREAD) (
  struct file_handle* file_handle,
  void* buffer,
  uintptr_t location,
  size_t size
);

typedef void (*F_NONGLOBALCLOSE) (
  struct file_handle* file_handle
);

typedef LIGHT_STATUS (*F_CLEAN_FILE_HANDLE) (
  struct file_handle* file_handle
);

// TODO: add data describing the filesystem this file is in
typedef struct file_handle {
  light_volume_t* volume;

  uint8_t* path;
  void* file_descriptor; // internal file handle
  size_t file_descriptor_size;

  size_t file_size;

  F_NONGLOBALREAD fRead;
  F_NONGLOBALCLOSE fClose;

  // check for the file descriptor and if it needs cleaning
  F_CLEAN_FILE_HANDLE fCleanHandle; 

  EFI_HANDLE partition_handle;
} __attribute__((packed)) handle_t;

typedef LIGHT_STATUS (*F_CLEAN_LOADED_FILE_HANDLE) (
  struct loaded_file_handle* loaded_fh 
);

typedef struct loaded_file_handle {
  handle_t* m_handle;

  // TODO: (work out how to design this crap)
  // be able to clean up used memory from the load
  // be able to easily find loaded file in memory
  uint8_t* m_buffer;

  F_CLEAN_LOADED_FILE_HANDLE fCleanHandle;
  // also take m_handle
  F_CLEAN_LOADED_FILE_HANDLE fCleanHandleFull;

} __attribute__((packed)) loaded_handle_t;

handle_t* open_file_ex(light_volume_t* volume, const char* path);
handle_t* open_file(const char* path);

void f_close(handle_t* handle); // TODO
void f_read(handle_t* handle, void* buffer, uintptr_t addr, size_t len); // TODO

loaded_handle_t* f_readall(handle_t* handle);

uint8_t* f_readall_raw(handle_t* handle);

handle_t* open_file_uri(char* uri); // TODO

// Identify the filesystem of a partition by utilising its guid
LIGHT_STATUS get_filesystem_guid(guid_t* guid_ret, light_volume_t* partition); // TODO

#endif // !__LIGHTLOADER_FILESYSTEM__
