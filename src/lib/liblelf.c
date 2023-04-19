#include "liblelf.h"
#include "lib/light_mainlib.h"
#include "mem/pmm.h"
#include "mem/ranges.h"

size_t get_elf_type(uint8_t* elf) {

  elf64_hdr_t* header = (void*)elf;

  if (strncmp((char*)header->ident, "\177ELF", 4)) {
    light_log(L"No vaild ELF ident!\n\r");
    return 0;
  }

  if (header->machine == ARCH_X86_64 || header->machine == ARCH_AARCH64) {
    return BITS64;
  } else if (header->machine == ARCH_X86_32) {
    return BITS32;
  }

  return 0;
}

elf_section_hdr_info_t get_header_info (uint8_t* elf) {
  elf64_hdr_t* header = (void*)elf;

  elf_section_hdr_info_t ret = {0};

  if (memcmp(header->ident, "\177ELF", 4)) {
    // uhm
    return ret;
  }

  ret.num = header->sh_num;
  ret.section_entry_size = header->shdr_size;
  ret.section_offset = header->shstrndx;
  ret.str_section_idx = header->shoff;

  return ret;
}

elf64_phdr_t* get_segment64(uint8_t* elf, elf64_hdr_t* header, uint32_t type) {
  if (header->phdr_size < sizeof(elf64_phdr_t)) {
    return NULL;
  }

  for (uint16_t i = 0; i < header->ph_num; i++) {
    elf64_phdr_t* pheader = (void*)elf + (header->phoff + i * header->phdr_size);
    if (pheader->p_type == type) {
      return pheader;
    }
  }
  return NULL;
}

elf32_phdr_t* get_segment32(uint8_t* elf, elf32_hdr_t* header, uint32_t type) {
  if (header->phdr_size < sizeof(elf64_phdr_t)) {
    return NULL;
  }

  for (uint16_t i = 0; i < header->ph_num; i++) {
    elf32_phdr_t* pheader = (void*)elf + (header->phoff + i * header->phdr_size);
    if (pheader->p_type == type) {
      return pheader;
    }
  }
  return NULL;
}

bool can_elf64_relocate(uint8_t* elf, elf64_hdr_t* header) {

  elf64_phdr_t* program_header = get_segment64(elf, header, PT_DYNAMIC);
      
  const uint16_t section_count = program_header->p_filesz / sizeof(elf64_dyn_t);

  for (uint16_t j = 0; j < section_count; j++) {
    elf64_dyn_t* dynamic = (void*)elf +  (program_header->p_offset + (j * sizeof(elf64_dyn_t)));

    if (dynamic->d_tag == DT_RELA) {
      return true;
    }
  }
  return false;
}

LIGHT_STATUS buffer_load_elf32(uint8_t* elf, uintptr_t* entry_buffer, mem_range_t** ranges, size_t* count) {

  // validate elf
  elf32_hdr_t* header = (void*)elf;

  *ranges = NULL;
  *count = 0;
  *entry_buffer = header->entry;

  for (uint16_t i = 0; i < header->ph_num; i++) {
    elf32_phdr_t* section_header = (void*)elf + (header->phoff + i * header->phdr_size);


    if (section_header->p_type == PT_LOAD) {
      *count += 1;
    }
  }

  if (*count == 0) {
    light_log("No PT_LOAD phdrs found!\n\r");
    return LIGHT_FAIL;
  }

  size_t idx = 0;
  uintptr_t ranges_size = 0;
  *ranges = pmm_malloc(sizeof(mem_range_t) * (*count), MEMMAP_BOOTLOADER_RECLAIMABLE);
  
  for (uint16_t i = 0; i < header->ph_num; i++) {
    elf32_phdr_t* section_header = (void*)elf + (header->phoff + i * header->phdr_size);

    if (section_header->p_type == PT_LOAD) {

      void* buffer = pmm_malloc(section_header->p_memsz, MEMMAP_BOOTLOADER_RECLAIMABLE);
      memcpy(buffer, elf + section_header->p_offset, section_header->p_filesz);

      mem_range_t range = load_range((uintptr_t)buffer, section_header->p_paddr, section_header->p_memsz);

      LIGHT_STATUS load_status = load_range_into_chain(ranges, &ranges_size, &range);

      if (load_status == LIGHT_FAIL) {
        light_log(L"Failed to load range into chain while buffering ELF!");
        return LIGHT_FAIL;
      }

      if (*entry_buffer == header->entry
          && *entry_buffer >= section_header->p_vaddr
          && *entry_buffer < (section_header->p_vaddr + section_header->p_memsz)) {
        *entry_buffer -= section_header->p_vaddr;
        *entry_buffer += section_header->p_paddr;
      }

      idx++;
    }

  }

  return LIGHT_SUCCESS;
}

LIGHT_STATUS buffer_load_elf64(uint8_t* elf, uintptr_t* entry_buffer, mem_range_t** ranges, size_t* count) {
  // validate elf
  elf64_hdr_t* header = (void*)elf;

  *ranges = NULL;
  *entry_buffer = header->entry;

  for (uint16_t i = 0; i < header->ph_num; i++) {
    elf64_phdr_t* section_header = (void*)elf + (header->phoff + i * header->phdr_size);


    if (section_header->p_type == PT_LOAD) {
      *count += 1;
    }
  }

  if (*count == 0) {
    light_log("No PT_LOAD phdrs found!\n\r");
    return LIGHT_FAIL;
  }

  size_t idx = 0;
  uintptr_t ranges_size = 0;
  *ranges = pmm_malloc(sizeof(mem_range_t) * (*count), MEMMAP_BOOTLOADER_RECLAIMABLE);
  
  for (uint16_t i = 0; i < header->ph_num; i++) {
    elf64_phdr_t* section_header = (void*)elf + (header->phoff + i * header->phdr_size);

    if (section_header->p_type == PT_LOAD) {

      void* buffer = pmm_malloc(section_header->p_memsz, MEMMAP_BOOTLOADER_RECLAIMABLE);
      memcpy(buffer, elf + section_header->p_offset, section_header->p_filesz);

      mem_range_t range = load_range((uintptr_t)buffer, section_header->p_paddr, section_header->p_memsz);

      LIGHT_STATUS load_status = load_range_into_chain(ranges, &ranges_size, &range);

      if (load_status == LIGHT_FAIL) {
        light_log(L"Failed to load range into chain while buffering ELF!");
        return LIGHT_FAIL;
      }

      if (*entry_buffer == header->entry
          && *entry_buffer >= section_header->p_vaddr
          && *entry_buffer < (section_header->p_vaddr + section_header->p_memsz)) {
        *entry_buffer -= section_header->p_vaddr;
        *entry_buffer += section_header->p_paddr;
      }

      idx++;
    }

  }

  return LIGHT_SUCCESS;
}

