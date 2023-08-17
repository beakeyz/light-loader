#include "ctx.h"
#include "disk.h"
#include "efidef.h"
#include "efidevp.h"
#include "efiprot.h"
#include "framebuffer.h"
#include "fs.h"
#include "gfx.h"
#include "heap.h"
#include "stddef.h"
#include "sys/ctx.h"
#include "sys/efidisk.h"
#include <efi.h>
#include <efierr.h>
#include <efilib.h>
#include <memory.h>
#include <stdio.h>

#define INITIAL_HEAPSIZE 64 * Mib 

static efi_ctx_t __efi_ctx = { 0 };
efi_ctx_t* efi_ctx;

extern void efi_init_keyboard();
extern void efi_init_mouse();
extern bool efi_has_keyboard();
extern bool efi_has_mouse();
extern int efi_get_mousepos(light_mousepos_t* pos);
extern int efi_get_keypress(light_key_t* key);

void
efi_exit(EFI_STATUS exit)
{
  BS->Exit(IH, exit, 0, 0);
}

static int
efi_exit_bs()
{
  /* TODO */
  /* Create and cache final memmap */
  /* Exit BootServices */
  return 0;
}

static int
efi_gather_sysinfo()
{
  light_ctx_t* ctx = get_light_ctx();

  gather_system_pointers(&ctx->sys_ptrs);

  return gather_memmap(ctx);
}

static void
efi_setup_ctx(light_ctx_t* ctx)
{
  static EFI_GUID img_guid = LOADED_IMAGE_PROTOCOL;
  static EFI_GUID io_guid = DISK_IO_PROTOCOL;
  static EFI_GUID block_guid = BLOCK_IO_PROTOCOL;
  //static EFI_GUID file_guid = ;
  EFI_STATUS status;

  memset(ctx, 0, sizeof(*ctx));

  ctx->f_exit = efi_exit_bs;
  ctx->f_allocate = efi_allocate;
  ctx->f_deallcoate = efi_deallocate;

  /* For any EFI loader, use the generic gfx printf routine, to get formated pixels on the screen */
  ctx->f_printf = gfx_printf;
  ctx->f_gather_sys_info = efi_gather_sysinfo;

  ctx->f_init_mouse = efi_init_mouse;
  ctx->f_has_mouse = efi_has_mouse;

  ctx->f_init_keyboard = efi_init_keyboard;
  ctx->f_has_keyboard = efi_has_keyboard;

  ctx->f_get_keypress = efi_get_keypress;
  ctx->f_get_mousepos = efi_get_mousepos;

  efi_ctx = &__efi_ctx;

  /*
   * TODO: good halting mechanism
   */

  status = open_protocol(IH, &img_guid, (void**)&efi_ctx->lightloader_image);

  status = BS->HandleProtocol(efi_ctx->lightloader_image->DeviceHandle, &io_guid, (void**)&efi_ctx->bootdisk_io);
  status = BS->HandleProtocol(efi_ctx->lightloader_image->DeviceHandle, &block_guid, (void**)&efi_ctx->bootdisk_block_io);

  ctx->private = efi_ctx;
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
 * 1) [X] First: initialize a system heap
 * 2) [X] Initialize a framebuffer so we can have pixels
 * 3) [ ] Initialize the frontend so we can have pretty pixels
 * 4) [ ] Check for a keyboard and mouse
 * 5) [ ] Check and cache any system objects like ACPI pointers or SMBIOS stuff
 * 6) [ ] Initialize file access n stuff
 * 7) [ ] Let the user do stuff
 * 8) [ ] Boot the kernel =D
 */
EFI_STATUS
efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE* system_table)
{
  gfx_frontend_result_t result;
  EFI_STATUS stat;
  uint64_t heap_addr;

  /* Initialize global UEFI variables */
  IH = image_handle;
  ST = system_table;
  BS = ST->BootServices;
  RT = ST->RuntimeServices;

  /* Quickly setup the main terminal for clean output */
  ST->ConOut->EnableCursor(ST->ConOut, false);
  ST->ConOut->ClearScreen(ST->ConOut);

  /* Disable the watchdog timer. Ignore any errors */
  BS->SetWatchdogTimer(0, 0, 0, NULL);

  /* We're up, lets allocate a heap to begin with */
  stat = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, EFI_SIZE_TO_PAGES(INITIAL_HEAPSIZE), &heap_addr);

  if (stat != EFI_SUCCESS) {
    ST->ConOut->OutputString(ST->ConOut, (CHAR16 *)L"Failed to allocate memory for heap.\r\n");
    efi_exit(stat);
  }

  /* We'll initialize 64 Mib of heap =D */
  init_heap(heap_addr, INITIAL_HEAPSIZE);

  /*
   * Setup the simple gfx framework for generic rendering
   */
  init_light_gfx();

  /*
   * Initialize the generic bootloader context, so the 'common' part of the bootloader can
   * have a little more power
   */
  init_light_ctx(efi_setup_ctx);

  /* Initialize the framebuffer for quick debug capabilities */
  init_framebuffer();

  /* Initialize the filesystems */
  init_fs();

  /* Create the efi bootdisk, aka the 'partition' that we where booted from and where the kernel files should be aswell */
  init_efi_bootdisk();

  /* Grab some system information */
  get_light_ctx()->f_gather_sys_info();

  /* Enter the frontend for user interaction */
  result = gfx_enter_frontend();

  /* TODO */
  switch (result) {
    case BOOT_MULTIBOOT:
      break;
    default:
      break;
  }

  for (;;) {}
  
  return 0;
}
