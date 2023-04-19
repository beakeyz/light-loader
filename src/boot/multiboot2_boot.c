#include "lib/multiboot.h"
#include "multiboot2_boot.h"
#include "drivers/disk/fs/filesystem.h"
#include "drivers/disk/volume.h"
#include "frontend/loading_screen.h"
#include "lib/liblelf.h"
#include "lib/light_mainlib.h"
#include "mem/pmm.h"
#include "mem/ranges.h"
#include "system/acpi.h"

typedef void (*MULTIBOOT_BOOTSTUB) (
  uint32_t entry,
  uint32_t ranges,
  uint32_t ranges_count
);

extern __attribute__((noreturn)) void multiboot2_boot_entry(uint32_t entry, uint32_t ranges, uint32_t ranges_count, uint32_t new_stub_location, uint32_t multiboot_addr, uint32_t multiboot_magic);
extern char multiboot2_bootstub[];
extern char multiboot2_bootstub_end[];

uint8_t* multiboot_info;
uintptr_t multiboot_current_index;

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

  struct multiboot_header* mb_header = NULL;

  for (size_t i = 0; i < MULTIBOOT_SEARCH; i += MULTIBOOT_HEADER_ALIGN) {
    mb_header = (void*)(kernel->m_buffer + i);

    if (mb_header->magic == MULTIBOOT2_HEADER_MAGIC) {
      break;
    }
  }

  // validate header
  uint32_t zero_checksum = mb_header->magic + mb_header->header_length + mb_header->architecture + mb_header->checksum;
  
  if (zero_checksum != 0 || mb_header->magic != MULTIBOOT2_HEADER_MAGIC) {
    // TODO: panic
    g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"MultibootHeader checksum is invalid!\n\r");
    for (;;) {}
  }

  loading_screen_set_status_and_update("Validated headers", fb);

  mem_range_t* ranges;
  size_t range_count = 1;
  uintptr_t entry_buffer;
  uint32_t elf_type = get_elf_type(kernel->m_buffer);

  switch (elf_type) {
    case BITS64:

      if (buffer_load_elf64(kernel->m_buffer, &entry_buffer, &ranges, &range_count) == LIGHT_FAIL) {
        light_log(L"Could not load elf64 to buffer!\n\r");
        for (;;) {}
      }

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

  /* Relocate bootstub */
  size_t bootstub_size = (size_t)multiboot2_bootstub_end - (size_t)&multiboot2_bootstub;
  void* new_bootstub_location = pmm_malloc(bootstub_size, MEMMAP_BOOTLOADER_RECLAIMABLE);
  memcpy(new_bootstub_location, multiboot2_bootstub, bootstub_size);

  /* Prepare multiboot */

  /* one page will prob be enough for everything right? */
  const size_t prealloced_mb_size = 0x1000;
  const uintptr_t multiboot_new_loc = chain_find_highest_addr(&ranges, range_count);

  multiboot_info = pmm_malloc(prealloced_mb_size, MEMMAP_BOOTLOADER_RECLAIMABLE);
  multiboot_current_index = 0;

  mem_range_t multiboot_range = load_range((uintptr_t)multiboot_info, multiboot_new_loc, prealloced_mb_size);

  if (load_range_into_chain(&ranges, &range_count, &multiboot_range) == LIGHT_FAIL) {
    loading_screen_set_status_and_update("Failed to load multiboot range", fb);
    for (;;) {}
  }

  memset(multiboot_info, 0x00, prealloced_mb_size);

  /* Start tag */
  multiboot_current_index += 8;

  /* Name tag */

  /* TODO: move global bootloader data (i.e. name, version, ect) somewhere nice */
  const char* bootloader_name = "__LightLoader__";
  struct multiboot_tag_string* bootloader_name_tag = (struct multiboot_tag_string*)(multiboot_info + multiboot_current_index);

  bootloader_name_tag->type = MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME;
  bootloader_name_tag->size = sizeof(struct multiboot_tag_string) + strlen(bootloader_name);

  memset(bootloader_name_tag->strn, 0, strlen(bootloader_name) + 1);
  strcpy(bootloader_name_tag->strn, bootloader_name);

  multiboot_current_index += ALIGN_UP(bootloader_name_tag->size, MULTIBOOT_TAG_ALIGN);

/* Multiboot stuff */
  for (struct multiboot_header_tag *tag = (struct multiboot_header_tag*)(mb_header + 1); // header + 1 to skip the header struct.
    tag < (struct multiboot_header_tag *)((uintptr_t)mb_header + mb_header->header_length) && tag->type != MULTIBOOT_HEADER_TAG_END;
    tag = (struct multiboot_header_tag *)((uintptr_t)tag + ALIGN_UP(tag->size, MULTIBOOT_TAG_ALIGN))) {

    switch (tag->type) {
      case MULTIBOOT_HEADER_TAG_FRAMEBUFFER: {

        struct multiboot_tag_framebuffer* tag = (struct multiboot_tag_framebuffer*)(multiboot_info + multiboot_current_index);

        tag->common.size = sizeof(struct multiboot_tag_framebuffer);
        tag->common.type = MULTIBOOT_TAG_TYPE_FRAMEBUFFER;
        tag->common.reserved = 0;

        /* Should we use the framebuffer that we get passed, or should we delete that one and create a new one? */
        tag->common.framebuffer_addr = fb->m_fb_addr;
        tag->common.framebuffer_bpp = fb->m_fb_bpp;
        tag->common.framebuffer_pitch = fb->m_fb_pitch;
        tag->common.framebuffer_width = fb->m_fb_width;
        tag->common.framebuffer_height = fb->m_fb_height;

        /* TODO: validate this field */
        tag->common.framebuffer_type = 0;

        multiboot_current_index += ALIGN_UP(tag->common.size, MULTIBOOT_TAG_ALIGN);
      }
      break;
    }
  }

  sdt_ptrs_t ptrs = get_sdtp();
  
  if (ptrs.xsdp) {
    /* Make xsdp tag */
    struct multiboot_tag_new_acpi* xsdp_tag = (struct multiboot_tag_new_acpi*)(multiboot_info + multiboot_current_index);
    xsdp_tag->size = sizeof(struct multiboot_tag_new_acpi) + sizeof(rsdp_t);
    xsdp_tag->type = MULTIBOOT_TAG_TYPE_ACPI_NEW;
    // Copy the entire structure into this tag
    memcpy(xsdp_tag->rsdp, ptrs.xsdp, sizeof(rsdp_t));

    multiboot_current_index += ALIGN_UP(xsdp_tag->size, MULTIBOOT_TAG_ALIGN);
  } else if (ptrs.rsdp) {
    /* Make rsdp tag */
    struct multiboot_tag_new_acpi* rsdp_tag = (struct multiboot_tag_new_acpi*)(multiboot_info + multiboot_current_index);
    rsdp_tag->size = sizeof(struct multiboot_tag_new_acpi) + sizeof(rsdp_t);
    rsdp_tag->type = MULTIBOOT_TAG_TYPE_ACPI_NEW;
    // Copy the entire structure into this tag
    memcpy(rsdp_tag->rsdp, ptrs.rsdp, sizeof(rsdp_t));

    multiboot_current_index += ALIGN_UP(rsdp_tag->size, MULTIBOOT_TAG_ALIGN);
  } else {
    /* Die */
  }

  exit_boot_services();

  /* Mem map */
  struct multiboot_tag_mmap* mmap_tag = (struct multiboot_tag_mmap*)(multiboot_info + multiboot_current_index);

  size_t mmap_entries = 0x1000;
  multiboot_memory_map_t* multiboot_map = get_raw_multiboot_memmap(&mmap_entries);

  mmap_tag->type = MULTIBOOT_TAG_TYPE_MMAP;
  mmap_tag->entry_size = sizeof(multiboot_memory_map_t);
  mmap_tag->size = sizeof(struct multiboot_tag_mmap) + (sizeof(multiboot_memory_map_t) * mmap_entries);
  mmap_tag->entry_version = 0;

  for (uintptr_t i = 0; i < mmap_entries; i++) {
    multiboot_memory_map_t map = multiboot_map[i];

    mmap_tag->entries[i] = map;
  } 

  multiboot_current_index += ALIGN_UP(mmap_tag->size, MULTIBOOT_TAG_ALIGN);

  /* End */

  struct multiboot_tag* end_tag = (struct multiboot_tag*)(multiboot_info + multiboot_current_index);

  end_tag->type = 0;
  end_tag->size = sizeof(struct multiboot_tag);

  multiboot_current_index += ALIGN_UP(end_tag->size, MULTIBOOT_TAG_ALIGN);

  multiboot2_boot_entry((uint32_t)entry_buffer, (uint32_t)((uintptr_t)ranges), (uint32_t)range_count, (uint32_t)((uintptr_t)new_bootstub_location), (uint32_t)((uintptr_t)multiboot_new_loc), MULTIBOOT2_BOOTLOADER_MAGIC);
}
