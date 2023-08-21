#ifndef __LIGHTLOADER_GDT__
#define __LIGHTLOADER_GDT__


#include <stdint.h>
typedef struct {
  uint16_t limit;
  uint16_t base_low;
  uint8_t base_mid;
  uint8_t access;
  uint8_t g;
  uint8_t base_high;
} __attribute__((packed)) gdt_descriptor_t;

typedef struct {
  uint16_t limit;
  uintptr_t ptr;
} __attribute__((packed)) x86_64_gdtr_t;

typedef struct {
  uint16_t limit;
  uintptr_t ptr;
  uintptr_t ptr_high;
} __attribute__((packed)) i386_gdtr_t;

#endif // !__LIGHTLOADER_GDT__
