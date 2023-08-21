#include <elf.h>
#include <elf_common.h>
#include <memory.h>
#include "elf64.h"
#include <lelf.h>
#include "relocations.h"

int 
load_relocatable_elf(void* buffer, size_t size, uintptr_t* entry_ptr, size_t* relocation_count, light_relocation_t** relocations)
{
  Elf64_Ehdr* hdr = buffer;

  if (!memcmp(hdr->e_ident, ELFMAG, SELFMAG))
    return -1;

  if (hdr->e_machine != EM_X86_64)
    return -1;

  *relocation_count = NULL;
  *entry_ptr = hdr->e_entry;

  for (uintptr_t i = 0; i < hdr->e_phnum; i++) {
    Elf64_Phdr* phdr = buffer + hdr->e_phoff + i * hdr->e_phentsize;

    if (phdr->p_type != PT_LOAD)
      continue;

    if (!create_relocation(relocations, (uintptr_t)buffer + phdr->p_offset, phdr->p_paddr, phdr->p_memsz))
        return -1;

    (*relocation_count)++;

    if (*entry_ptr == hdr->e_entry
          && *entry_ptr >= phdr->p_vaddr
          && *entry_ptr < (phdr->p_vaddr + phdr->p_memsz)) {
        *entry_ptr -= phdr->p_vaddr;
        *entry_ptr += phdr->p_paddr;
      }
  }
  
  return 0;
}
