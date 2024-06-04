#ifndef __LIGHTLOADER_FS__
#define __LIGHTLOADER_FS__

#include "stddef.h"
#include <stdint.h>

struct disk_dev;
struct light_file;

#define FS_TYPE_NONE    0xFF
#define FS_TYPE_FAT12   0x00
#define FS_TYPE_FAT16   0x01
#define FS_TYPE_FAT32   0x02
#define FS_TYPE_ISO     0x03
#define FS_TYPE_EXT2    0x04
#define FS_TYPE_EXT3    0x05
#define FS_TYPE_EXT4    0x06
#define FS_TYPE_NTFS    0x07
#define FS_TYPE_TARFS   0x08

/*
 * The Lightloader Filesystem model
 *
 * TODO: we gotta change this model a bit if we want to support bootloader-based installing of the system
 * onto physical disk drives, since both the boot device and the install target may use the same filesyste 
 * and under this model we would not be able to copy files between the two
 *
 * Filesystems under the lightloader function a little different than in normal for any OS / Bootloader.
 * We provide a bunch of filesystem types which are only able to be 'mounted' once. Filesystems are
 * supposed to provide a structured I/O platform for lts devices, but for a bootloader, there
 * really should only be once filesystem/disk/partition that we are interested in: the bootpartition.
 * This partition SHOULD hold any configuration, the bootloader binary itself, ramdisks and most 
 * important: the kernel. This is why, when probing for the presence of a filesystem on a particular
 * disk device, we halt on a probe success, since we have found the correct filesystem type for 
 * a given bootpartition. After that, any structures or caches that are prepared for the usage of 
 * this filesystem are stored on the filesystem struct itself. No fucking around with vnodes, inodes
 * or vobjects. One filesystem that acts as a root for the bootloader
 */
typedef struct light_fs {

  struct disk_dev* device;

  /* Private part of the filesystem */
  void* private;

  uint8_t fs_type;
  
  /* Open an existing filesystem object */
  struct light_file* (*f_open)(struct light_fs* fs, char* path);
  struct light_file* (*f_open_idx)(struct light_fs* fs, char* path, uintptr_t idx);
  /* Create the path specified inside the filesystem */
  int (*f_create_path)(struct light_fs* fs, const char* path);
  /* Remove a path inside the filesystem */
  int (*f_remove_path)(struct light_fs* fs, const char* path);
  /* Close an opend filesytem object */
  int (*f_close)(struct light_fs* fs, struct light_file*);
  /* Probe the disk for this filesystem */
  int (*f_probe)(struct light_fs* fs, struct disk_dev* device);
  int (*f_install)(struct light_fs* fs, struct disk_dev* device);
  struct light_fs* next;
} light_fs_t;

void init_fs();

int register_fs(light_fs_t* fs);
int unregister_fs(light_fs_t* fs);

static inline bool
is_fs_used(light_fs_t* fs)
{
  return (fs->device != nullptr);
}

light_fs_t* get_fs(uint8_t type);

int disk_install_fs(struct disk_dev* device, uint8_t type);
int disk_probe_fs(struct disk_dev* device);

#endif // !__LIGHTLOADER_FS__
