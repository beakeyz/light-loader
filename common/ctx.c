#include "file.h"
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

/*!
 * @brief: Check if the light loader is marked as installed
 *
 * In order to quickly verify if we are installed, we check the existance of the
 * install.yes file in the res folder of the root directory of the boot volume
 */
bool 
light_is_installed()
{
  light_file_t* file;

  file = fopen(CTX_INSTALL_CHECK_PATH);

  if (file) {
    file->f_close(file);
    return true;
  }

  return false;
}
