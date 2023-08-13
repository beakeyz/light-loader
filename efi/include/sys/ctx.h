#ifndef __LIGHTLOADER_EFI_CTX__
#define __LIGHTLOADER_EFI_CTX__

#include "efiprot.h"

typedef struct efi_ctx {

  EFI_LOADED_IMAGE* lightloader_image;

  /*
   * These protocols are directly pulled from the lightloader image. That means that they are 
   * confined to the partition that the image was put in. Trying to get the GPT header at the 
   * absolute start of the physical disk will take a different I/O protocol
   */
  EFI_DISK_IO_PROTOCOL* bootdisk_io;
  EFI_BLOCK_IO_PROTOCOL* bootdisk_block_io;
  EFI_FILE_PROTOCOL* bootdisk_file;

} efi_ctx_t;

efi_ctx_t* get_efi_context();

#endif // !__LIGHTLOADER_EFI_CTX__
