#ifndef __LIGHTLOADER_BOOT_CORE__
#define __LIGHTLOADER_BOOT_CORE__

#include "boot/cldr.h"
#include "gfx.h"
#include <stdint.h>

struct light_ctx;
struct disk_dev;

/* We get one page of kernel configuration */
#define LIGHT_BOOT_KERNEL_OPTS_LEN 4096

typedef enum LIGHT_BOOT_METHOD {
    LBM_MULTIBOOT2, /* Load a kernel with the multiboot2 protocol */
    LBM_MULTIBOOT1, /* Load a kernel with the multiboot1 protocol */
    LBM_CHAINLOAD, /* Load a different EFI image/bootloader stage */
    LBM_LINUX, /* Load a kernel with the linux protocol */
    LBM_RAW, /* Load a kernel without any protocol, only raw execute (TODO: implement?) */
} LIGHT_BOOT_METHOD_t;

#define LBOOT_FLAG_NOFB (0x00000001)
#define LBOOT_FLAG_INVAL (0x00000002)
#define LBOOT_FLAG_HAS_FILE (0x00000004)

typedef struct light_boot_config {
    char* kernel_image; /* Path to the kernel ELF that is situated in the bootdevice, together with the bootloader */
    char* ramdisk_image; /* Path to the kernels ramdisk, also in the bootdevice. Can be NULL */
    char* kernel_opts;
    /* File used to build this config */
    config_file_t* cfg_file;
    uint32_t flags;
    LIGHT_BOOT_METHOD_t method;
} light_boot_config_t;

void __attribute__((noreturn)) boot_context_configuration(struct light_ctx* ctx);

void boot_add_kernel_opt(struct light_ctx* ctx, const char* key, const char* value);
void boot_config_from_file(light_boot_config_t* config, config_file_t* file);

extern const char* default_kernel_path;
extern const char* default_ramdisk_path;
#endif // !__LIGHTLOADER_BOOT_CORE__
