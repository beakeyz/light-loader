#ifndef __LIGHTLOADER_TARFS__
#define __LIGHTLOADER_TARFS__

#include "file.h"
#include "fs.h"
#include <stddef.h>

typedef struct tar_file {
  uint8_t fn[100];
  uint8_t mode[8];
  uint8_t owner_id[8];
  uint8_t gid[8];

  uint8_t size[12];
  uint8_t m_time[12];

  uint8_t checksum[8];

  uint8_t type;
  uint8_t lnk[100];

  uint8_t ustar[6];
  uint8_t version[2];

  uint8_t owner[32];
  uint8_t group[32];

  uint8_t d_maj[8];
  uint8_t d_min[8];

  uint8_t prefix[155];
} __attribute__((packed)) tar_file_t;

light_fs_t* create_tarfs(light_file_t* file);

#endif // !__LIGHTLOADER_TARFS__
