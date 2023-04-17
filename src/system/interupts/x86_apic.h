#ifndef __LIGHTLOADER_APIC__
#define __LIGHTLOADER_APIC__
#include <lib/light_mainlib.h>
#include <system/acpi.h>

// madt structs
typedef struct {
    sdt_t header;
    uint32_t local_controller_addr;
    uint32_t flags;
    char     madt_entries_begin[];
} __attribute__((packed)) madt_t;

typedef struct {
    uint8_t type;
    uint8_t length;
    uint8_t apic_id;
    uint8_t reserved;
    uint32_t address;
    uint32_t gsib;
} __attribute__((packed)) madt_io_apic_t;

typedef struct {
    sdt_t header;
    uint8_t host_address_width;
    uint8_t flags;
    uint8_t reserved[10];
    char  remapping_structures[];
} __attribute__((packed)) dmar_t;

typedef struct {
  bool is_x2;
  bool is_in_int;
  bool is_init;

  size_t max_io_apics;
  madt_io_apic_t** io_apic_ptrs;
} apic_data_t;

bool check_lapic ();
bool check_x2_mode();
LIGHT_STATUS set_x2_mode();

LIGHT_STATUS init_apics();
void apic_ack();

uintptr_t x2_read(uint32_t reg);
void x2_write(uint32_t reg, uintptr_t value);

uint32_t io_apic_read(uintptr_t index, uint32_t reg);
void io_apic_write(uintptr_t index, uint32_t reg, uint32_t value);

uint32_t lapic_read(uint32_t reg);
void lapic_write(uint32_t reg, uint32_t value);

void io_apic_mask_all();

#endif // !__LIGHTLOADER_APIC__
