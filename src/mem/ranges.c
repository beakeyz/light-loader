#include "ranges.h"
#include "lib/light_mainlib.h"
#include <lib/libldef.h>
#include <stddef.h>

bool does_range_overlap(mem_range_t* one, mem_range_t* two) {
  size_t abs_target_diff = one->m_target - two->m_target;
  bool one_in_two = one->m_target >= two->m_target && (two->m_range_size) > abs_target_diff;
  bool two_in_one = two->m_target >= one->m_target && (one->m_range_size) > abs_target_diff;
  return (!one_in_two) && (!two_in_one);
}

mem_range_t load_range(uintptr_t current, uintptr_t target, size_t size) {
  mem_range_t ret = {
    .m_target = target,
    .m_current_location = current,
    .m_range_size = size,
  };
  return ret;
}

LIGHT_STATUS load_range_into_chain(mem_range_t** chain, size_t* chain_lenght, mem_range_t* range) {

  if (!range || range->m_range_size == 0) {
    return LIGHT_FAIL;
  }

  size_t __chain_lenght = *chain_lenght;

  //for (uintptr_t i = 0; i < __chain_lenght; i++) {
  //  if (does_range_overlap(chain[i], range)) {
  //    return LIGHT_FAIL;
  //  }
 // }

  (*chain)[__chain_lenght].m_current_location = range->m_current_location;
  (*chain)[__chain_lenght].m_target = range->m_target;
  (*chain)[__chain_lenght].m_range_size = range->m_range_size;
  __chain_lenght++;

  *chain_lenght = __chain_lenght;
  return LIGHT_SUCCESS;
}

