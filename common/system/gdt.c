#include "heap.h"
#include <system/gdt.h>
#include <memory.h>

/*
 * We need to prepare a GDT for the bootstub, so it can use that when it 
 * jumps back to protected mode
 */
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
  .ptr = (uintptr_t)&gdt_struct,
};
