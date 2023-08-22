#include "gfx.h"
#include "stddef.h"
#include "ui/box.h"
#include "ui/button.h"
#include <ui/screens/home.h>
#include <ctx.h>

/*
 * Click handler to boot the default bootconfig
 */
int boot_default_config(button_component_t* component)
{
  light_gfx_t* gfx;
  get_light_gfx(&gfx);

  /* Reset the file paths to null in order to ensure the default paths */
  gfx->ctx->light_bcfg.kernel_file = NULL;
  gfx->ctx->light_bcfg.ramdisk_file = NULL;

  gfx->flags |= GFX_FLAG_SHOULD_EXIT_FRONTEND;
  return 0;
}

int 
construct_homescreen(light_component_t** root, light_gfx_t* gfx)
{
  create_box(root, nullptr, 8, 32, gfx->width - 16, gfx->height - 38, 0, true, LIGHT_GRAY);

  create_box(root, nullptr, 100, 100, 100, 100, 0, true, WHITE);

  create_button(root, "Test", 230, 100, 100, 32, nullptr, nullptr);

  return 0;
}
