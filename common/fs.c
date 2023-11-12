#include "fs.h"
#include <stddef.h>
#include "fs/fat/fat32.h"
#include "heap.h"
#include <memory.h>

/*
 * These are placeholder objects that we use as a reference for when we mount them and copy
 * them over into an entry of @mounted_filesystems
 */
static light_fs_t* filesystems;
/* These are the filesystems that are currently active on their respective disks */
static light_fs_t* mounted_filesystems;

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

/*!
 * @brief: This installs and probes a filesystem of type @type on @device
 *
 * After the ->f_install call succeeds, we will call ->f_probe to make sure
 * the filesystem we created is valid and useable for us
 */
int 
disk_install_fs(disk_dev_t* device, uint8_t type)
{
  light_fs_t* fs;

  fs = get_fs(type);

  if (!fs || !fs->f_install)
    return -1;

  /* Try to install */
  if (fs->f_install(fs, device))
    return -2;

  /* Make sure the installed filesystem is valid */
  return disk_probe_fs(device);
}

/*!
 * @brief: Try to see if a filesystem fits on a disk device @device
 *
 * When we find that the filesystem fits, we copy it over to the @mounted_filesystems
 * list
 */
int
disk_probe_fs(disk_dev_t* device)
{
  int error = -1;
  light_fs_t* fs;
  light_fs_t* copy_fs;

  for (fs = filesystems; fs; fs = fs->next) {
    /* Every filesystem is responsible for cleanup if this function fails */
    error = fs->f_probe(fs, device);

    if (!error)
      break;
  }

  /* Make sure these are set up correctly */
  if (fs) {
    copy_fs = heap_allocate(sizeof(*fs));

    /* Copy over the filesystem into its permanent memory location */
    memcpy(copy_fs, fs, sizeof(*fs));

    /* Link it into the mounted filesystems */
    copy_fs->next = mounted_filesystems;
    mounted_filesystems = copy_fs;

    /* Arm the device for filesystem opperations */
    copy_fs->device = device;
    device->filesystem = copy_fs;

    /* Make sure the old filesystem has a cleared private field (this is now owned by copy_fs) */
    fs->private = nullptr;
    fs->device = nullptr;
  }

  return error;
}

void
init_fs()
{
  filesystems = nullptr;
  mounted_filesystems = nullptr;

  init_fat_fs();
}
