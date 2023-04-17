#include "x86_gdt.h"
#include <mem/pmm.h>


gdt_descriptor_t gdt_struct[] = {
  {0},
  {
    0xffff,
    0x0000,
    0x00,
    0b10011010,
    0x00,
    0x00,
  },
  {
    0xffff,
    0x0000,
    0x00,
    0b10010010,
    0b00000000,
    0x00
  },
  {
    0xffff,
    0x0000,
    0x00,
    0b10011010,
    0b11001111,
    0x00
  },
  {
    0xffff,
    0x0000,
    0x00,
    0b10010010,
    0b11001111,
    0x00
  },
  {
    0x0000,
    0x0000,
    0x00,
    0b10011010,
    0b00100000,
    0x00
  },
  {
    0x0000,
    0x0000,
    0x00,
    0b10010010,
    0b00000000,
    0x00
  }
};

x86_64_gdtr_t g_gdtr = {
  .limit = sizeof(gdt_struct) - 1,
  .ptr = 0
};

void install_gdt() {
  gdt_descriptor_t* cpy = pmm_malloc(sizeof(gdt_struct), MEMMAP_BOOTLOADER_RECLAIMABLE);
  memcpy(cpy, gdt_struct, sizeof(gdt_struct));
  g_gdtr.ptr = (uintptr_t)cpy;
}
