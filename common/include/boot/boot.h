#ifndef __LIGHTLOADER_BOOT_CORE__
#define __LIGHTLOADER_BOOT_CORE__

#include "gfx.h"
#include <stdint.h>

struct light_ctx;
struct disk_dev;

/* We get one page of kernel configuration */
#define LIGHT_BOOT_KERNEL_OPTS_LEN 4096

typedef enum LIGHT_BOOT_METHOD {
  LBM_MULTIBOOT2 = 0,   /* Load a kernel with the multiboot2 protocol */
  LBM_RAW,              /* Load a kernel without any protocol, only raw execute (TODO: implement?) */
  LBM_REBOOT,           /* No further booting, simply reset and let EFI reboot */
} LIGHT_BOOT_METHOD_t;

#define LBOOT_FLAG_NOFB (0x00000001)

typedef struct light_boot_config {
  char* kernel_file; /* Path to the kernel ELF that is situated in the bootdevice, together with the bootloader */
  char* ramdisk_file; /* Path to the kernels ramdisk, also in the bootdevice. Can be NULL */
  char *kernel_opts;
  uint32_t flags;
  LIGHT_BOOT_METHOD_t method;
} light_boot_config_t;

void  __attribute__((noreturn)) boot_context_configuration(struct light_ctx* ctx);

void boot_add_kernel_opt(struct light_ctx* ctx, const char* key, const char* value);

extern const char* default_kernel_path;
extern const char* default_ramdisk_path;
#endif // !__LIGHTLOADER_BOOT_CORE__
