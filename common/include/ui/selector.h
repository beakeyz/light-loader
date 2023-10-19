#ifndef __LIGHTLOADER_UI_SELECTOR__
#define __LIGHTLOADER_UI_SELECTOR__

#include <image.h>
#include "disk.h"
#include "ui/component.h"

struct selector_entry;

typedef struct selector_component {
  light_component_t* parent;

  uint32_t entry_count;
  uint32_t selected_index;
  const char** options;
} selector_component_t;

enum SELECTOR_TYPE {
  SELECTOR_TYPE_DISK = 0,
  SELECTOR_TYPE_PARTITION,
  SELECTOR_TYPE_COLOR,
  SELECTOR_TYPE_RESOLUTION,
};

typedef struct selector_entry {
  light_image_t* icon;
  enum SELECTOR_TYPE selector_type;
  const char* option;

  union {
    disk_dev_t* dev;
    void* p;
  } private;
} selector_entry_t;

selector_component_t* create_selector(light_component_t** link, const char* label, uint32_t x, uint32_t y, enum SELECTOR_TYPE selector_type, uint32_t option_count, const char** options);

#endif // !__LIGHTLOADER_UI_SELECTOR__
