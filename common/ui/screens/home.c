#include "gfx.h"
#include "stddef.h"
#include "ui/box.h"
#include "ui/button.h"
#include "ui/component.h"
#include <ui/screens/home.h>
#include <ctx.h>
#include <font.h>

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

#define WELCOME_LABEL "Welcome to the Light Loader!"

/*
 * Construct the homescreen
 *
 * Positioning will be done manually, which is the justification for the amount of random numbers in this
 * funciton. This makes the development of screens rather unscalable, but it is nice and easy / simple
 * 
 * I try to be explicit in the use of these coordinates, but that does not make it any more scalable, just
 * a little easier to see the logic in them
 */
int 
construct_homescreen(light_component_t** root, light_gfx_t* gfx)
{
  create_box(root, nullptr, 8, 32, gfx->width - 16, gfx->height - 38, 0, true, LIGHT_GRAY);

  create_component(root, COMPONENT_TYPE_LABEL, WELCOME_LABEL, 10, 34, lf_get_str_width(gfx->current_font, WELCOME_LABEL), gfx->current_font->height, nullptr);

  create_button(root, "Boot default", 8 + 8, gfx->height - (38 + 8), 120, 34, boot_default_config, nullptr);
  return 0;
}
