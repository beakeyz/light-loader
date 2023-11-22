#ifndef __LIGHTLOADER_EFIDISK__
#define __LIGHTLOADER_EFIDISK__

#include "efiprot.h"
typedef struct efi_disk_stuff {
  EFI_BLOCK_IO_PROTOCOL* blockio;
  EFI_DISK_IO_PROTOCOL* diskio;
  EFI_BLOCK_IO_MEDIA* media;
} efi_disk_stuff_t;

void init_efi_bootdisk();
void efi_discover_present_volumes();

#endif // !__LIGHTLOADER_EFIDISK__
