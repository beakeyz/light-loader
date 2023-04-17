#include "liblmath.h"

extern size_t max(size_t a, size_t b);
extern size_t min(size_t a, size_t b);
extern size_t interpolate(uint32_t a, uint32_t b, double scale);
extern size_t int_interpolate(size_t a, size_t b, size_t scale);

// TODO: powers of negatives
size_t pow(size_t a, uint32_t p) {
  size_t ret = a;
  for (size_t i = 1; i < p; i++) {
    ret *= a;
  }
  return ret;
}


// stolen from stackoverflow for the most part :clown:
size_t sqrt(size_t a) {
  uint64_t op  = a;
  uint64_t res = 0;
  uint64_t one = (uint64_t)1 << 62;

  // "one" starts at the highest power of four <= than the argument.
  while (one > op) {
    one >>= 2;
  }

  while (one != 0) {
    if (op >= res + one) {
      op = op - (res + one);
      res = res +  2 * one;
    }
    res >>= 1;
    one >>= 2;
  }

  return res;
}

// Honers method?
double sin(double theta) {

  return 0;
}

double cos(double theta) {

  return 0;
}

double tan(double theta) {

  return 0;
}
