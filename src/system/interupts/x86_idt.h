#ifndef __LIGHTLOADER_X86_IDT__ 
#define __LIGHTLOADER_X86_IDT__
#include <lib/light_mainlib.h>

typedef struct  {
    uint16_t limit;
    uint64_t ptr;
} __attribute__((packed)) idtr_t;

typedef struct  {
    uint16_t offset_lo;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_hi;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

void init_idt();

#endif // !
