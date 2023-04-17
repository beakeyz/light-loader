#include "x86_idt.h"

static idt_entry_t idt_table[256];
static idtr_t idtr;

void dummy_isr() {

}

void init_idt() {
  // Yea right
}
