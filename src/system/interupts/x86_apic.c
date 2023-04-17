#include "x86_apic.h"
#include "lib/light_mainlib.h"
#include "mem/pmm.h"
#include "system/acpi.h"
#include "system/cpu.h"

static apic_data_t apic_data;

static void inc_max_io_apics(void*);
static void add_ioptr(void*);

typedef void (*io_callback)(void*);

static void loop_madt(madt_t*, io_callback);

bool check_lapic () {
  uint32_t eax, ebx, ecx, edx;
  if (cpuid(0x1, 0, &eax, &ebx, &ecx, &edx)) {
    if (edx & (1 << 9)) {
      return true;
    }
  }

  return false;
}

bool check_x2_mode() {
  uint32_t eax, ebx, ecx, edx;
  if (cpuid(0x1, 0, &eax, &ebx, &ecx, &edx)) {
    if (ecx & (1 << 21)) {
      dmar_t* funnie_check = get_rsdt_table("DMAR", 0);
      if (funnie_check && (funnie_check->flags & (1 << 0)) && (funnie_check->flags & (1 << 1))) {
        return false;
      }
      return true;
    }
  }
  return false;
}

LIGHT_STATUS set_x2_mode() {
  if (!check_x2_mode()) {
    return LIGHT_FAIL;
  }

  uint64_t ia32_apic_base = rdmsr(0x1b);
  ia32_apic_base |= (1 << 10);
  wrmsr(0x1b, ia32_apic_base);

  apic_data.is_x2 = true;

  return LIGHT_SUCCESS;
}

// (io apics)
LIGHT_STATUS init_apics() {
  if (apic_data.is_init)
    return LIGHT_FAIL;

  madt_t* madt = get_rsdt_table("APIC", 0);

  if (!madt) {
    apic_data.is_init = true;
    return LIGHT_FAIL;
  }

  loop_madt(madt, inc_max_io_apics);

  apic_data.io_apic_ptrs = pmm_malloc(apic_data.max_io_apics * sizeof(madt_io_apic_t*), MEMMAP_BOOTLOADER_RECLAIMABLE);
  apic_data.max_io_apics = 0;

  loop_madt(madt, add_ioptr);

  io_apic_mask_all();

  return LIGHT_SUCCESS;
}

void apic_ack() {
  if (apic_data.is_x2) {
    x2_write(0x00, 0x00);
    return;
  }
  lapic_write(0x00, 0x00);
}

// TODO
uintptr_t x2_read(uint32_t reg) {
  return 0;
}
void x2_write(uint32_t reg, uintptr_t value) {

}

uint32_t io_apic_read(uintptr_t index, uint32_t reg) {
  uintptr_t base = (uintptr_t)apic_data.io_apic_ptrs[index]->address;
  mmout32(base, reg);
  return mmin32(base + 16);
}

void io_apic_write(uintptr_t index, uint32_t reg, uint32_t value) {
  uintptr_t base = (uintptr_t)apic_data.io_apic_ptrs[index]->address;
  mmout32(base, reg);
  mmout32(base + 16, value);
}

uint32_t lapic_read(uint32_t reg) {
  size_t lapic_mmio_base = (size_t)(rdmsr(0x1b) & 0xfffff000);
  return mmin32(lapic_mmio_base + reg);
}

void lapic_write(uint32_t reg, uint32_t value) {
  size_t lapic_mmio_base = (size_t)(rdmsr(0x1b) & 0xfffff000);
  mmout32(lapic_mmio_base + reg, value);
}

void io_apic_mask_all() {
  for (size_t i = 0; i < apic_data.max_io_apics; i++) {
    uint32_t gsi_count = ((io_apic_read(i, 1) & 0xff0000) >> 16) + 1;
    for (size_t j = 0; j < gsi_count; j++) {
      uintptr_t haha = j * 2 + 16;
      io_apic_write(i, haha, (1 << 16));
      io_apic_write(i, haha + 1, 0);
    }
  } 
}

// dumb?
static void loop_madt(madt_t* ptr, io_callback callback) {
  for (uint8_t* p = (uint8_t*)(ptr->madt_entries_begin); (uintptr_t)p < ((uintptr_t)ptr + ptr->header.length); p += *(p + 1)) {
    if (*p) {
      callback((void*)p);
    }
  }
}

static void inc_max_io_apics(void* madt_ptr) {
  apic_data.max_io_apics++;
}

static void add_ioptr(void* madt_ptr) {
  apic_data.io_apic_ptrs[apic_data.max_io_apics++] = madt_ptr;
}
