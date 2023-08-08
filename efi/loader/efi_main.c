#include "efidef.h"
#include "efidevp.h"
#include "heap.h"
#include "stddef.h"
#include <efi.h>
#include <efierr.h>
#include <efilib.h>

#define INITIAL_HEAPSIZE 64 * Mib 

void
efi_exit(EFI_STATUS exit)
{
  BS->Exit(IH, exit, 0, 0);
}

/*
 * Welcome to the new and imporved bootloader for the lighthouse opperating system!
 *
 * In this loader we aim to achieve a few things that we failed to do in the previous itteration:
 *  - Speed: we where slow ;-;
 *  - Ease of use: we where hard
 *  - Customizability: just non-existent
 *  - Beauty: also non-existent...
 *
 * What are the steps from here?
 * 1) First: initialize a system heap
 * 2) Initialize a framebuffer so we can have pixels
 * 3) Initialize the frontend so we can have pretty pixels
 * 4) Check for a keyboard and mouse
 * 5) Check and cache any system objects like ACPI pointers or SMBIOS stuff
 * 6) Initialize file access n stuff
 * 7) Let the user do stuff
 * 8) Boot the kernel =D
 */
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

  if (stat != EFI_SUCCESS) {
		ST->ConOut->OutputString(ST->ConOut, (CHAR16 *)L"Failed to allocate memory for heap.\r\n");
		BS->Exit(IH, stat, 0, NULL);
	}

  /* We'll initialize 64 Mib of heap =D */
  init_heap(heap_addr, INITIAL_HEAPSIZE);

  ST->ConOut->EnableCursor(ST->ConOut, false);
  ST->ConOut->ClearScreen(ST->ConOut);

  ST->ConOut->OutputString(ST->ConOut, (CHAR16*)L"Allocating 8 bytes\n\r");

  void* alloc = heap_allocate(8);
  void* lucy2 = heap_allocate(8);
  void* lucky = heap_allocate(8);
  heap_allocate(8);
  heap_allocate(8);

  ST->ConOut->OutputString(ST->ConOut, (CHAR16*)L"Correct?\n\r");

  debug_heap();

  heap_free(alloc);

  ST->ConOut->OutputString(ST->ConOut, (CHAR16*)L"Correct?\n\r");

  debug_heap();

  heap_free(lucky);

  ST->ConOut->OutputString(ST->ConOut, (CHAR16*)L"Correct?\n\r");

  debug_heap();

  heap_free(lucy2);

  ST->ConOut->OutputString(ST->ConOut, (CHAR16*)L"Correct?\n\r");

  debug_heap();

  for (;;) {}
  
  return 0;
}
