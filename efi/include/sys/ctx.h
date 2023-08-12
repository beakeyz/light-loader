#ifndef __LIGHTLOADER_EFI_CTX__
#define __LIGHTLOADER_EFI_CTX__

#include "efiprot.h"

typedef struct efi_ctx {

  EFI_LOADED_IMAGE* lightloader_image;

  EFI_DISK_IO_PROTOCOL* bootdisk_io;
  EFI_BLOCK_IO_PROTOCOL* bootdisk_block_io;
  EFI_FILE_PROTOCOL* bootdisk_file;

} efi_ctx_t;

efi_ctx_t* get_efi_context();

#endif // !__LIGHTLOADER_EFI_CTX__
