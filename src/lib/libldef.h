#ifndef __LIGHTLOADER_LIBLDEF__
#define __LIGHTLOADER_LIBLDEF__

// TODO: convert typedefs (yuckie)
typedef signed              char    int8_t;
typedef unsigned            char    uint8_t;
typedef signed              short   int16_t;
typedef unsigned            short   uint16_t;
typedef signed              int     int32_t;
typedef unsigned            int     uint32_t;
typedef long signed         int     int64_t;
typedef long unsigned       int     uint64_t;
typedef unsigned            int     uint_t;

typedef char*                       va_list;
typedef uint64_t                    size_t;
typedef int64_t                     ssize_t;
typedef uint64_t                    uintptr_t;
typedef int                         bool;

#define asm __asm__
#define asm_v __asm__ volatile

#define bool int
#define true 1
#define false 0

//#define native_ptr (sizeof(void*) == 8) ? ulong_t : uword_t

#define Kib 1024
#define Mib Kib * Kib
#define Gib Mib * Kib
#define Tib Gib * Kib

#endif // !__LIGHTLOADER_LIBLDEF__
