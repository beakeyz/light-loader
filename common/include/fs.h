#ifndef __LIGHTLOADER_FS__
#define __LIGHTLOADER_FS__

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

typedef struct light_fs {

  struct disk_dev* device;

  /* Private part of the filesystem */
  void* private;

  uint8_t fs_type;
  
  struct light_file* (*f_open)(struct light_fs* fs, char* path);
  int (*f_close)(struct light_file*);
} light_fs_t;

#endif // !__LIGHTLOADER_FS__
