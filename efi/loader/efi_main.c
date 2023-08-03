#include "efidef.h"
#include <efi.h>
#include <efierr.h>
#include <efilib.h>

#define INITIAL_HEAPSIZE 64 * 1024 * 1024

void
efi_exit(EFI_STATUS exit)
{
  BS->Exit(IH, exit, 0, 0);
}

EFI_STATUS
efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE* system_table)
{
  EFI_STATUS stat;
  uint64_t heap_addr;
  IH = image_handle;
  ST = system_table;
  BS = ST->BootServices;
  RT = ST->RuntimeServices;

  /* We're up, lets allocate a heap to begin with */
  stat = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, EFI_SIZE_TO_PAGES(INITIAL_HEAPSIZE), &heap_addr);


  
  return 0;
}
