#include "file.h"
#include "gfx.h"
#include "heap.h"
#include "stddef.h"
#include "ui/box.h"
#include "ui/button.h"
#include <stdio.h>
#include <font.h>
#include <ui/screens/options.h>

int test_onclick(button_component_t* comp)
{
  /* Try to create the path /User on the boot disk */
  int error = fcreate("test.txt");

  if (error)
    comp->parent->label = "Fuck you lmao";
  else
    comp->parent->label = "Yay, Success";

  /* Try to create the path /User on the boot disk */
  light_file_t* file = fopen("test.txt");

  if (error)
    comp->parent->label = "Yay, Success";
  else
    comp->parent->label = "Fuck you lmao";

  char* test_str = "This is a test, yay";

  /* Write into the file */
  error = fwrite(file, test_str, 20, 0);

  if (error)
    comp->parent->label = "Could not write =/";
  else
    comp->parent->label = "yay";

  return 0;
}

int open_test_onclick(button_component_t* comp)
{
  /* Try to create the path /User on the boot disk */
  light_file_t* file = fopen("test.txt");

  if (file) {
    char* buffer = heap_allocate(128);

    fread(file, buffer, 20, 0);

    comp->parent->label = buffer;
  } else
    comp->parent->label = "Fuck you lmao";

  return 0;
}

bool sicko = false;

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
  sicko = false;

  create_button(root, "Test fat", 24, 50, 156, 26, test_onclick, nullptr);
  create_button(root, "Open Test fat", 24, 80, 156, 26, open_test_onclick, nullptr);

  create_switch(root, "Fuc you", 24, 140, 245, gfx->current_font->height * 2, &sicko);

  /* TODO: ... */
  return 0;
}

