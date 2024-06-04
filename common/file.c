
#include "disk.h"
#include <file.h>
#include <fs.h>


int 
fread(light_file_t* file, void* buffer, size_t size, uintptr_t offset)
{
  return file->f_read(file, buffer, size, offset);
}

int 
freadall(light_file_t* file, void* buffer)
{
  return file->f_readall(file, buffer);
}

int 
fwrite(light_file_t* file, void* buffer, size_t size, uintptr_t offset)
{
  if (!file->f_write)
    return -1;

  return file->f_write(file, buffer, size, offset);
}

/*!
 * @brief: This opens a file on the boot device
 */
light_file_t* fopen(char* path)
{
  disk_dev_t* rootdevice = get_bootdevice();

  if (!rootdevice || !rootdevice->filesystem)
    return nullptr;

  return rootdevice->filesystem->f_open(rootdevice->filesystem, path);
}

light_file_t* fopen_idx(char* path, uint64_t idx)
{
  disk_dev_t* rootdevice = get_bootdevice();

  if (!rootdevice || !rootdevice->filesystem || !rootdevice->filesystem->f_open_idx)
    return nullptr;

  return rootdevice->filesystem->f_open_idx(rootdevice->filesystem, path, idx);
}

int fcreate(const char* path)
{
  disk_dev_t* rootdevice = get_bootdevice();

  if (!rootdevice || !rootdevice->filesystem || !rootdevice->filesystem)
    return -1;

  return rootdevice->filesystem->f_create_path(rootdevice->filesystem, path);
}
