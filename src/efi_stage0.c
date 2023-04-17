#include "boot/multiboot2_boot.h"
#include "drivers/disk/fs/fat32/fat32_fs.h"
#include "drivers/disk/fs/filesystem.h"
#include "drivers/disk/volume.h"
#include "drivers/display/framebuffer.h"
#include <frontend/screen.h>
#include "drivers/keyboard.h"
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

  if (init_periodic_timer() != LIGHT_SUCCESS) {
    light_log(L"[FATAL] Failed to initialize timer!\n\r");
    hang();
  }

  init_kb_driver();

  // TODO: unified halt, hang, panic
  if (init_memmap() != LIGHT_SUCCESS) {
    syst->ConOut->OutputString(syst->ConOut, (CHAR16*)L"[FATAL] failed to dump system memorymap!\n\r");
    hang();
  }

  light_framebuffer_t* framebuffer = init_framebuffer(1024, 768, 32);
  draw_loading_screen(framebuffer);
  loading_screen_set_status_and_update("Setting up...", framebuffer);

  // this is prob so useless lmao
  install_gdt();

  loading_screen_set_status_and_update("Loaded internals", framebuffer);
    
  init_apics();
  
  loading_screen_set_status_and_update("Loaded apics", framebuffer);

  // FIXME: (is this useless because we use uefi BootServices for 'interrupts'?)

  if (create_volume_store_from_disk() != LIGHT_SUCCESS) {
    syst->ConOut->OutputString(syst->ConOut, (CHAR16*)L"[WARNING] Failed to create volume store!\n\r");
  }

  //hang();

  //loading_screen_set_status_and_update("Loaded disk io", framebuffer);

  // init the drivers
  // init_keyboard();   (keyboard, duh)
  // init_diskdev();    (load info about the disk + prepare the utilities for interfacing. This )
  // init_displaydev(); (load the displaydriver (gop))

  // hihi touch our own thing =)

  // What's next?
  // - [X] interrupts (apic, acpi (for wich we will need some cpu functions (cpuid, out8, in8, ect.)))
  // - [X] some disk funnies (find out what the fuck is going on)
  // - [X] gop (graphics driver, fonts)
  // - login / main menu
  // - an actual way to boot an elf file xD

  multiboot2_boot("kernel.elf", framebuffer);

  for (;;) {}
  return EFI_SUCCESS;
}