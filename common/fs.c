#include "fs.h"
#include <stddef.h>
#include "fs/fat/fat32.h"

static light_fs_t* filesystems;

static light_fs_t**
__find_fs(light_fs_t* fs) 
{
  light_fs_t** itt;

  itt = &filesystems;

  for (; *itt; itt = &(*itt)->next) {
    if (*itt == fs)
      return itt;
  }

  return itt;
}

static light_fs_t**
__find_fs_type(uint8_t fs_type) 
{
  light_fs_t** itt;

  itt = &filesystems;

  for (; *itt; itt = &(*itt)->next) {
    if ((*itt)->fs_type == fs_type)
      return itt;
  }

  return itt;
}

int
register_fs(light_fs_t* fs)
{
  light_fs_t** itt = __find_fs(fs);

  if (*itt)
    return -1;

  *itt = fs;
  fs->next = nullptr;

  return 0;
}

int
unregister_fs(light_fs_t* fs)
{
  light_fs_t** itt = &filesystems;

  while (*itt) {

    if (*itt == fs) {
      *itt = fs->next;
      fs->next = nullptr;

      return 0;
    }
    
    itt = &(*itt)->next;
  }

  return -1;
}

light_fs_t*
get_fs(uint8_t type)
{
  light_fs_t** fs = __find_fs_type(type);

  if (!fs)
    return nullptr;

  return *fs;
}

int
disk_probe_fs(disk_dev_t* device)
{
  int error = -1;
  light_fs_t* fs;

  for (fs = filesystems; fs; fs = fs->next) {
    error = fs->f_probe(fs, device);

    if (!error)
      break;
  }

  /* Make sure these are set up correctly */
  fs->device = device;
  device->filesystem = fs;

  return error;
}

void
init_fs()
{
  filesystems = nullptr;
  init_fat_fs();
}
