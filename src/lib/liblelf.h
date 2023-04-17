#ifndef __LIGHTLOADER_LIBLELF__
#define __LIGHTLOADER_LIBLELF__
#include "light_mainlib.h"
#include "mem/ranges.h"

#define PT_LOAD     0x00000001
#define PT_DYNAMIC  0x00000002
#define PT_INTERP   0x00000003
#define PT_PHDR     0x00000006

#define DT_NULL     0x00000000
#define DT_NEEDED   0x00000001
#define DT_RELA     0x00000007
#define DT_RELASZ   0x00000008
#define DT_RELAENT  0x00000009

#define ABI_SYSV     0x00
#define ARCH_X86_64  0x3e
#define ARCH_X86_32  0x03
#define ARCH_AARCH64 0xb7
#define BITS_LE      0x01
#define ET_DYN       0x0003
#define SHT_RELA     0x00000004
#define R_X86_64_RELATIVE  0x00000008
#define R_AARCH64_RELATIVE 0x00000403

/* Indices into identification array */
#define EI_CLASS    4
#define EI_DATA     5
#define EI_VERSION  6
#define EI_OSABI    7

#define BITS64 64
#define BITS32 32

typedef struct {
    uint64_t base;
    uint64_t length;
    uint64_t permissions;
} elf_range_t;

typedef struct {
    uint32_t section_entry_size;
    uint32_t str_section_idx;
    uint32_t num;
    uint32_t section_offset;
} elf_section_hdr_info_t;

typedef struct {
    uint8_t  ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t hdr_size;
    uint16_t phdr_size;
    uint16_t ph_num;
    uint16_t shdr_size;
    uint16_t sh_num;
    uint16_t shstrndx;
} elf64_hdr_t;

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} elf64_shdr_t;

typedef struct {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} elf64_sym_t;

typedef struct {
    uint8_t  ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t hdr_size;
    uint16_t phdr_size;
    uint16_t ph_num;
    uint16_t shdr_size;
    uint16_t sh_num;
    uint16_t shstrndx;
} elf32_hdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} elf64_phdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} elf32_phdr_t;

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
} elf32_shdr_t;

typedef struct {
    uint64_t r_addr;
    uint32_t r_info;
    uint32_t r_symbol;
    uint64_t r_addend;
} elf64_rela_t;

typedef struct {
    uint64_t d_tag;
    uint64_t d_un;
} elf64_dyn_t;

size_t get_elf_type(uint8_t* elf);
elf_section_hdr_info_t get_header_info (uint8_t* elf);

elf64_phdr_t* get_segment64(uint8_t* elf, elf64_hdr_t* header, uint32_t type);
elf32_phdr_t* get_segment32(uint8_t* elf, elf32_hdr_t* header, uint32_t type);
bool can_elf64_relocate(uint8_t* elf, elf64_hdr_t* header);

LIGHT_STATUS load_elf64(uint8_t* elf_handle, uintptr_t* entry_buffer);

LIGHT_STATUS buffer_load_elf32(uint8_t* elf, uintptr_t* entry_buffer, mem_range_t** ranges, size_t* count);

#endif // !__LIGHTLOADER_LIBLELF__
