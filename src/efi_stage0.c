#include "boot/multiboot2_boot.h"
#include "drivers/disk/fs/fat32/fat32_fs.h"
#include "drivers/disk/fs/filesystem.h"
#include "drivers/disk/volume.h"
#include "drivers/display/framebuffer.h"
#include <frontend/screen.h>
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "frontend/loading_screen.h"
#include "lib/linkedlist.h"
#include "mem/pmm.h"
#include "system/acpi.h"
#include "system/interupts/x86_apic.h"
#include "system/interupts/x86_idt.h"
#include <efi.h>
#include <stddef.h>
#include <system/x86_gdt.h>
#include <lib/light_mainlib.h>
#include <drivers/timer/timer.h>

EFI_STATUS efi_main(EFI_HANDLE img_handle, EFI_SYSTEM_TABLE *syst) {
  // welcome =D, set some efi shit
  syst->ConOut->EnableCursor(syst->ConOut, false);
  syst->ConOut->ClearScreen(syst->ConOut);
  syst->ConOut->OutputString(syst->ConOut, (CHAR16*)L"[LIGHTLOADER_EFI] Welcome to the Lighthouse OS EFI bootloader!\n\r");

  EFI_STATUS wd_stat = syst->BootServices->SetWatchdogTimer(0, 0x10000, 0, NULL);
  if (wd_stat != EFI_SUCCESS) {
    syst->ConOut->OutputString(syst->ConOut, (CHAR16*)L"[WARNING] Unable to disable the WatchdogTimer!\n\r");
  }

  init_light_lib(img_handle, syst);

  //if (init_periodic_timer() != LIGHT_SUCCESS) {
  //  light_log(L"[FATAL] Failed to initialize timer!\n\r");
  //  hang();
  //}

  init_kb_driver();

  g_light_info.has_mouse = (init_mouse() == LIGHT_SUCCESS);

  if (g_light_info.has_mouse){
    light_log(L"Mouse");
  } else {
    light_log(L"no Mouse");
  }

  // TODO: unified halt, hang, panic
  if (init_memmap() != LIGHT_SUCCESS) {
    syst->ConOut->OutputString(syst->ConOut, (CHAR16*)L"[FATAL] failed to dump system memorymap!\n\r");
    hang();
  }

  light_framebuffer_t* framebuffer = init_optimal_framebuffer();

  /* TODO: announce failure */
  if (!framebuffer)
    hang();

  init_loading_screen(framebuffer);

  loading_screen_set_status_and_update("Setting up...");

  // this is prob so useless lmao
  install_gdt();

  loading_screen_set_status_and_update("Loading apics...");
    
  init_apics();

  loading_screen_set_status_and_update("Disk...");

  // FIXME: (is this useless because we use uefi BootServices for 'interrupts'?)

  if (create_volume_store_from_disk() != LIGHT_SUCCESS) {
    syst->ConOut->OutputString(syst->ConOut, (CHAR16*)L"[WARNING] Failed to create volume store!\n\r");
  }

  // TODO: login / main menu

  //for (;;) {
    /* Do menu rendering stuff */

    /* Handle keyevents and mouseevents */

    /* Do menu reactive stuff */

    /* Check if things r finished */

    /* Repeat */
  //}

  multiboot2_boot("kernel.elf", framebuffer);

  for (;;) {}
  return EFI_SUCCESS;
}
