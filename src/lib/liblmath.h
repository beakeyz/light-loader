#ifndef __LIGHTLOADER_LIBLMATH__
#define __LIGHTLOADER_LIBLMATH__
#include <lib/libldef.h>

#define PI 3.14159265

// fdlibm hihi
#define __HI(x) *(1+(int*)&x)
#define __LO(x) *((int*)&x)

// oneliners can be inline
inline size_t max(size_t a, size_t b) { return (a > b) ? a : b; }
inline size_t min(size_t a, size_t b) { return (a < b) ? a : b; }

inline size_t interpolate(uint32_t a, uint32_t b, double scale) { return b + (int32_t)(a - b) * scale; }

// scale is from 0 to 1024 in this case
inline size_t int_interpolate(size_t a, size_t b, size_t scale) { return (a*(1024-scale) + b * scale) >> 10; }

// TODO: implement
size_t pow (size_t a, uint32_t pow);
size_t sqrt (size_t a);

double sin(double theta);
double cos(double theta);
double tan(double theta);

#endif
