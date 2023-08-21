#include "gfx.h"
#include "stddef.h"
#include "ui/box.h"
#include "ui/button.h"
#include <ui/screens/home.h>

int 
construct_homescreen(light_component_t** root)
{
  light_gfx_t* gfx;
  get_light_gfx(&gfx);

  create_box(root, nullptr, 8, 32, gfx->width - 16, gfx->height - 38, 0, true, LIGHT_GRAY);

  create_box(root, nullptr, 100, 100, 100, 100, 0, true, WHITE);

  create_button(root, "Test", 230, 100, 100, 32, nullptr, nullptr);

  return 0;
}
