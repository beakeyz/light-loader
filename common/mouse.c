#include "mouse.h"
#include "ctx.h"
#include <memory.h>

light_mousepos_t previous_mousepos;
uint32_t _x_limit, _y_limit;

void
init_mouse()
{
  light_ctx_t* ctx = get_light_ctx();

  memset(&previous_mousepos, 0, sizeof(previous_mousepos));
  _x_limit = _y_limit = 0;

  if (!ctx || !ctx->f_init_mouse)
    return;

  ctx->f_init_mouse();
}

bool
has_mouse()
{
  light_ctx_t* ctx = get_light_ctx();

  if (!ctx || !ctx->f_has_mouse)
    return false;

  /* Call to the platform specific probing function */
  return ctx->f_has_mouse();
}

void
get_previous_mousepos(light_mousepos_t* pos)
{
  if (pos)
    *pos = previous_mousepos;
}

void
reset_mousepos(uint32_t x, uint32_t y, uint32_t x_limit, uint32_t y_limit)
{
  previous_mousepos.x = x;
  previous_mousepos.y = y;
  previous_mousepos.btn_flags = 0;

  /* Make sure the limit is set correctly */
  _x_limit = x_limit;
  _y_limit = y_limit;
}
