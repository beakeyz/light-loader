#ifndef __LIGHTLOADER_FILE__
#define __LIGHTLOADER_FILE__

#include <stdint.h>

struct light_fs;

typedef struct light_file {

    size_t filesize;

    struct light_fs* parent_fs;

    void* private;

    int (*f_write)(struct light_file* file, void* buffer, size_t size, uintptr_t offset);
    int (*f_read)(struct light_file* file, void* buffer, size_t size, uintptr_t offset);
    int (*f_readall)(struct light_file* file, void* buffer);

    int (*f_close)(struct light_file* file);
} light_file_t;

int fread(light_file_t* file, void* buffer, size_t size, uintptr_t offset);
int freadall(light_file_t* file, void* buffer);
int fwrite(light_file_t* file, void* buffer, size_t size, uintptr_t offset);

struct light_file* fopen(char* path);
struct light_file* fopen_idx(char* path, uintptr_t idx);

int fcreate(const char* path);

#endif // !__LIGHTLOADER_FILE__
