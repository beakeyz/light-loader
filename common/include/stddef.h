#ifndef __LIGHTLOADER_STDDEF__
#define __LIGHTLOADER_STDDEF__

#include <stdint.h>

#define asm __asm__
#define nullptr (void*)0
#define NULL 0
#define true 1
#define false 0

#define ALWAYS_INLINE inline __attribute__((always_inline))
#define NOINLINE __attribute__((noinline))
#define NORETURN __attribute__((noreturn))
#define NAKED __attribute__((naked))
#define SECTION(__sect) __attribute__((section(__sect)))
#define USED __attribute__((used))
#define UNUSED __attribute__((unused))

// ascii encoding
#define TO_UPPERCASE(c) ((c >= 'a' && c <= 'z') ? c - 0x20 : c)
#define TO_LOWERCASE(c) ((c >= 'A' && c <= 'Z') ? c + 0x20 : c)

#define Kib 1024
#define Mib (Kib * Kib)
#define Gib (Mib * Kib)
#define Tib (Gib * Kib)

#endif // !__LIGHTLOADER_STDDEF__
