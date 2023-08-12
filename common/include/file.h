#ifndef __LIGHTLOADER_FILE__
#define __LIGHTLOADER_FILE__

#include <stddef.h>
#include <stdint.h>

struct light_fs;

typedef struct light_file {

  size_t filesize;

  struct light_fs* parent_fs;
  
  int (*f_read)(struct light_file* file, void* buffer, size_t size, uintptr_t offset);
  int (*f_write)(struct light_file* file, void* buffer, size_t size, uintptr_t offset);
  int (*f_readall)(struct light_file* file, void* buffer);

  int (*f_close)(struct light_file* file);
} light_file_t;

int fread(light_file_t* file, void* buffer, size_t size, uintptr_t offset);
int freadall(light_file_t* file, void* buffer);
int fwrite(light_file_t* file, void* buffer, size_t size, uintptr_t offset);

#endif // !__LIGHTLOADER_FILE__
