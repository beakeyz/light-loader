#include "multiboot2_boot.h"
#include "drivers/disk/fs/filesystem.h"
#include "drivers/disk/volume.h"
#include "frontend/loading_screen.h"
#include "lib/liblelf.h"
#include "lib/light_mainlib.h"
#include "mem/pmm.h"
#include "mem/ranges.h"

typedef void (*MULTIBOOT_BOOTSTUB) (
  uint32_t entry,
  uint32_t ranges,
  uint32_t ranges_count
);

extern void multiboot2_boot_entry(uint32_t entry, uint32_t ranges, uint32_t ranges_count, uint32_t new_stub_location, uint32_t arg);
extern char multiboot2_bootstub[];
extern char multiboot2_bootstub_end[];

static void secure_bootstub();

void secure_bootstub() {
  // TODO
}

__attribute__((noreturn)) void multiboot2_boot (const char* path, light_framebuffer_t* fb) {

  // open file

  handle_t* kernel_handle = open_file(g_volume_store.store[1], path);

  if (kernel_handle == NULL) {
    light_log(L"Failed to get handle!\n\r");
    hang();
  }

  loading_screen_set_status_and_update("Opend ELF file", fb);

  size_t kernel_size = kernel_handle->file_size;
  loaded_handle_t* kernel = f_readall(kernel_handle);

  loading_screen_set_status_and_update("Read ELF file", fb);

  kernel_handle->fCleanHandle(kernel_handle);

  loading_screen_set_status_and_update("Cleaned ELF file", fb);

  mb2_header_t* mb_header = NULL;

  for (size_t i = 0; i < 0x4000; i+= 8) {
    mb_header = (void*)(kernel->m_buffer + i);

    if (mb_header->m_magic == MB2_HEADER_MAGIC) {
      break;
    }
  }

  // validate header
  uint32_t zero_checksum = mb_header->m_magic + mb_header->m_header_size + mb_header->m_arch + mb_header->m_checksum;
  
  if (zero_checksum != 0 || mb_header->m_magic != MB2_HEADER_MAGIC) {
    // TODO: panic
    g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"MultibootHeader checksum is invalid!\n\r");
    for (;;) {}
  }

  loading_screen_set_status_and_update("Validated headers", fb);

  mem_range_t* ranges;
  size_t range_count;
  uintptr_t entry_buffer;
  uint32_t elf_type = get_elf_type(kernel->m_buffer);

  switch (elf_type) {
    case BITS64:
      break;
    case BITS32:

      if (buffer_load_elf32(kernel->m_buffer, &entry_buffer, &ranges, &range_count) == LIGHT_FAIL) {
        light_log(L"Could not load elf32 to buffer!\n\r");
        for (;;) {}
      }

      break;
    case 0:
      light_log(L"invalid elf header");
      break;
    default:

      // invalid elf_type
      break;
  }

  loading_screen_set_status_and_update("buffered elf", fb);

  //mem_range_t* inclusive_ranges = pmm_malloc(sizeof(mem_range_t) * (range_count + 1), MEMMAP_BOOTLOADER_RECLAIMABLE);

  //memcpy(inclusive_ranges, ranges, sizeof(mem_range_t) * range_count);
  // pmm_free(ranges, sizeof(mem_range_t) * range_count);
  // ranges = inclusive_ranges;

  size_t bootstub_size = (size_t)multiboot2_bootstub_end - (size_t)&multiboot2_bootstub;
  void* new_bootstub_location = pmm_malloc(bootstub_size, MEMMAP_BOOTLOADER_RECLAIMABLE);
  memcpy(new_bootstub_location, multiboot2_bootstub, bootstub_size);

  exit_boot_services();


  multiboot2_boot_entry((uint32_t)entry_buffer, (uint32_t)((uintptr_t)ranges), (uint32_t)range_count, (uint32_t)((uintptr_t)new_bootstub_location), 0);
  loading_screen_set_status_and_update("Trying to relocate...", fb);

  for (;;) {}

}
