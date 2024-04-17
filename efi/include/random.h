#ifndef __LIGHTLOADER_EFI_RANDOM__
#define __LIGHTLOADER_EFI_RANDOM__

#include "efidef.h"

int init_efi_random();

EFI_STATUS get_efi_random(uint32_t len, uint8_t* buf);

#endif // !__LIGHTLOADER_EFI_RANDOM__
