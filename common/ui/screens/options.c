#include "gfx.h"
#include "stddef.h"
#include "ui/box.h"
#include "ui/button.h"
#include <ui/screens/options.h>

int 
construct_optionsscreen(light_component_t** root, light_gfx_t* gfx)
{
  create_box(root, nullptr, 8, 32, gfx->width - 16, gfx->height - 38, 0, true, LIGHT_GRAY);

  return 0;
}

