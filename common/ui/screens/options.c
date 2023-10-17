#include "file.h"
#include "gfx.h"
#include "stddef.h"
#include "ui/box.h"
#include "ui/button.h"
#include <ui/screens/options.h>

int test_onclick(button_component_t* comp)
{
  /* Try to create the path /User on the boot disk */
  int error = fcreate("test.txt");

  if (error)
    comp->parent->label = "Fuck you lmao";
  else
    comp->parent->label = "Yay, Success";
  return 0;
}

int open_test_onclick(button_component_t* comp)
{
  /* Try to create the path /User on the boot disk */
  light_file_t* error = fopen("test.txt");

  if (error)
    comp->parent->label = "Yay, Success";
  else
    comp->parent->label = "Fuck you lmao";
  return 0;
}

/*
 * Options we need to implement:
 *  - resolution
 *  - background image / color
 *  - keyboard layout
 *  - mouse sens
 *  - kernel options
 *     
 */
int 
construct_optionsscreen(light_component_t** root, light_gfx_t* gfx)
{

  create_button(root, "Test fat", 50, 50, 156, 26, test_onclick, nullptr);
  create_button(root, "Open Test fat", 50, 80, 156, 26, open_test_onclick, nullptr);

  /* TODO: ... */
  return 0;
}

