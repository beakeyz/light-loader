#ifndef __LIGHTLOADER_MULTIBOOT2_BOOT__ 
#define __LIGHTLOADER_MULTIBOOT2_BOOT__
#include "drivers/display/framebuffer.h"
#include <lib/libldef.h>

// defines (TODO: hehe)
#define MB2_HEADER_MAGIC 0xe85250d6
#define MB2_BL_MAGIC 0x36d76289

// structs (TODO: hihi)
typedef struct {

  uint32_t m_magic;

  uint32_t m_arch;

  uint32_t m_header_size;

  uint32_t m_checksum;

} __attribute__((packed)) mb2_header_t;

typedef struct {

  uint16_t m_type;

  uint16_t m_flags;

  uint32_t m_size;

} __attribute__((packed)) mb2_header_tag_t;

typedef struct {

} __attribute__((packed)) mb2_header_tag_info_request_t;

typedef struct {

} __attribute__((packed)) mb2_header_tag_address_t;

typedef struct {

} __attribute__((packed)) mb2_header_tag_entry_address_t;

typedef struct {

} __attribute__((packed)) mb2_header_tag_console_flags_t;

typedef struct {

} __attribute__((packed)) mb2_header_tag_framebuffer_t;

typedef struct {

} __attribute__((packed)) mb2_header_tag_module_align_t;

typedef struct {

} __attribute__((packed)) mb2_header_tag_relocate_t;

typedef struct {

} __attribute__((packed)) mb2_tag_start_t;

typedef struct {

} __attribute__((packed)) mb2_color; 

typedef struct mb2_mmap_entry {

} __attribute__((packed)) mb2_mmap_entry_t;

typedef struct {

} __attribute__((packed)) mb2_tag_t;

typedef struct {

} __attribute__((packed)) mb2_tag_string_t;

typedef struct {

} __attribute__((packed)) mb2_tag_module_t;

typedef struct {

} __attribute__((packed)) mb2_tag_basic_meminfo_t;

typedef struct {

} __attribute__((packed)) mb2_tag_boot_dev_t;

typedef struct {

} __attribute__((packed)) mb2_tag_mmap_t;

typedef struct {
  uint8_t ext_spec[512];  
} __attribute__((packed)) mb2_vbe_info_blk_t;

typedef struct {
  uint8_t ext_spec[256];  
} __attribute__((packed)) mb2_vbe_mode_info_blk_t;

typedef struct {

} __attribute__((packed)) mb2_tag_vbe_t;

// the framebuffer tag in mb2 basically consists of two structures squished into one
typedef struct {

} __attribute__((packed)) mb2_tag_fb_common_t;

typedef struct {

} __attribute__((packed)) mb2_tag_fb_t;

typedef struct {

} __attribute__((packed)) mb2_tag_elf_sections_t;

typedef struct {

} __attribute__((packed)) mb2_tag_apm_t;

typedef struct {

} __attribute__((packed)) mb2_tag_efi32_t;

typedef struct {

} __attribute__((packed)) mb2_tag_efi64_t;
  
typedef struct {

} __attribute__((packed)) mb2_tag_smbios_t;

typedef struct {

} __attribute__((packed)) mb2_tag_old_acpi_t;

typedef struct {

} __attribute__((packed)) mb2_tag_new_acpi_t;

typedef struct {

} __attribute__((packed)) mb2_tag_network_t;

typedef struct {

} __attribute__((packed)) mb2_tag_efi_mmap_t;

typedef struct {

} __attribute__((packed)) mb2_tag_efi32_ih_t;

typedef struct {

} __attribute__((packed)) mb2_tag_efi64_ih_t;

typedef struct {

} __attribute__((packed)) mb2_tag_load_base_addr_t;
  
// functions
__attribute__((noreturn)) void multiboot2_boot (const char* path, light_framebuffer_t* fb);

#endif // !
