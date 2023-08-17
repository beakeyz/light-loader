#include "key.h"
#include <ctx.h>

void
init_keyboard()
{
  light_ctx_t* ctx = get_light_ctx();

  if (!ctx || !ctx->f_init_keyboard)
    return;

  ctx->f_init_keyboard();
}

bool
has_keyboard()
{
  light_ctx_t* ctx = get_light_ctx();

  if (!ctx || !ctx->f_has_keyboard)
    return false;

  /* Call to the platform specific probing function */
  return ctx->f_has_keyboard();
}

void
get_keyboard_flags(uint32_t* flags)
{
  (void)flags;
}
