#ifndef __LIGHTLOADER_CPU__
#define __LIGHTLOADER_CPU__
#include <lib/light_mainlib.h>

void out8(uint16_t port, uint8_t value);
void out16(uint16_t port, uint16_t value);
void out32(uint16_t port, uint32_t value);

uint32_t mmin32 (uintptr_t addr);
void mmout32 (uintptr_t addr, uint32_t value);

uint8_t in8(uint16_t port);
uint16_t in16(uint16_t port);
uint32_t in32(uint16_t port);

bool cpuid (uint32_t leaf, uint32_t subleaf,
          uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);

void wrmsr (uint32_t msr, uintptr_t value);
uintptr_t rdmsr(uint32_t msr);


uintptr_t rdtsc ();
void delay (size_t amount);

#endif // !__LIGHTLOADER_CPU__
