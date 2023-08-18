
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
  return -1;
}

light_file_t* fopen(char* path)
{
  disk_dev_t* rootdevice = get_bootdevice();

  if (!rootdevice || !rootdevice->filesystem)
    return nullptr;

  return rootdevice->filesystem->f_open(rootdevice->filesystem, path);
}
