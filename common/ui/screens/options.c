#include "file.h"
#include "gfx.h"
#include "heap.h"
#include "stddef.h"
#include "ui/box.h"
#include "ui/button.h"
#include "ui/input_box.h"
#include <stdio.h>
#include <font.h>
#include <ui/screens/options.h>

struct light_option l_options[] = {
  {
    "Use kterm",
    LOPTION_TYPE_BOOL,
    false,
    { 0, }
  },
  {
    "Create ENV file on install",
    LOPTION_TYPE_BOOL,
    false,
    { 0, }
  },
  {
    "Use ramdisk",
    LOPTION_TYPE_BOOL,
    false,
    { 0, }
  },
  {
    "Test option two",
    LOPTION_TYPE_STRING,
    (uintptr_t)"Test string lmao",
    { 0, }
  },
};

static uint32_t l_options_len = sizeof(l_options) / sizeof(l_options[0]);

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
  uint32_t current_y = 52;

  /*
   * Loop over all the options and add them to the option screen
   */
  for (uint32_t i = 0; i < l_options_len; i++) {
    switch (l_options[i].type) {
      case LOPTION_TYPE_BOOL:
        l_options[i].btn = create_switch(root, l_options[i].name, 24, current_y, 312, 26, (bool*)&l_options[i].value);

        current_y += l_options[i].btn->parent->height + (l_options[i].btn->parent->height / 2);
        break;
      case LOPTION_TYPE_STRING:
        l_options[i].input_box = create_inputbox(root, l_options[i].name, (char*)l_options[i].value, 24, current_y, 312, 26);

        current_y += l_options[i].input_box->parent->height + (l_options[i].input_box->parent->height / 2);
        break;

    }
  }

  return 0;
}

