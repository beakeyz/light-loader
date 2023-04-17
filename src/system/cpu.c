#include "cpu.h"
#include "lib/light_mainlib.h"

void out8(uint16_t port, uint8_t value) {
  
  asmv ("outb %%al, %1"  :: "a"(value), "Nd"(port) : "memory");
}
void out16(uint16_t port, uint16_t value) {
  asmv ("outw %%ax, %1" :: "a"(port), "Nd"(value) : "memory");
}
void out32(uint16_t port, uint32_t value) {
//  TODO
//  asmv ("outl %%eax, %1" :: "a"(port), "Nd"(value) : "memory");
}

uint32_t mmin32 (uintptr_t addr) {
  uint32_t ret;
  asm volatile (
      "movl (%1), %0"
      : "=r" (ret)
      : "r"  (addr)
      : "memory"
  );
  return ret;
}

void mmout32 (uintptr_t addr, uint32_t value) {
  asmv (
      "movl %1, (%0)"
        :
        : "r" (addr), "ir" (value)
        : "memory"
    );
}

uint8_t in8(uint16_t port) {
  uint8_t ret;
  asmv ("inb %1, %%al" : "=a"(ret) : "Nd"(port) : "memory");
  return ret;
}
uint16_t in16(uint16_t port) {
  uint16_t ret;
  asmv ("inw %1, %%ax" : "=a"(ret) : "Nd"(port) : "memory");
  return ret;
}
uint32_t in32(uint16_t port) {
  // TODO
//  uint32_t ret;
//  asmv ("inl %1, %%eax" : "=a"(ret) : "Nd"(port) : "memory");
//  return ret;
  return 0;
}

bool cpuid (uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
  uint32_t cpuid_max;
  asmv ("cpuid" : "=a" (cpuid_max) : "a" (leaf & 0x80000000) : "ebx", "ecx", "edx");

  if (leaf > cpuid_max)
    return false;

  asmv ("cpuid" : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx) : "a" (leaf), "c" (subleaf));

  return true;
}

void wrmsr (uint32_t msr, uintptr_t value) {
  uint32_t edx = value >> 32;
  uint32_t eax = (uint32_t)value;
  asm volatile ("wrmsr"
                  :
                  : "a" (eax), "d" (edx), "c" (msr)
                  : "memory");
}
uintptr_t rdmsr(uint32_t msr) {
  uint32_t edx, eax;
  asm volatile ("rdmsr"
                  : "=a" (eax), "=d" (edx)
                  : "c" (msr)
                  : "memory");
  return ((uint64_t)edx << 32) | eax;
}

uintptr_t rdtsc () {
  uint32_t eax;
  uint32_t edx;

  asmv ("rdtsc" : "=a"(eax), "=d"(edx) : );

  uintptr_t cycles = ((uintptr_t)edx << 32) | eax;
  return cycles;
}

// ha ha
void delay (size_t amount) {
  uintptr_t old = rdtsc();

  while (rdtsc() < (old + amount));
}
