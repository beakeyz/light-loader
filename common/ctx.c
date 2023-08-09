#include <ctx.h>
#include <memory.h>

light_ctx_t g_light_ctx = { 0 };

void
init_light_ctx(void (*platform_setup)(light_ctx_t* p_ctx))
{
  /* Clear that bitch */
  memset(&g_light_ctx, 0, sizeof(g_light_ctx));

  platform_setup(&g_light_ctx);
}

/* Idc about safety, this is my bootloader bitches */
light_ctx_t*
get_light_ctx()
{
  return &g_light_ctx;
}
