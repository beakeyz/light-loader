#include "ctx.h"
#include "ui/box.h"
#include "ui/component.h"
#include <ui/screens/install.h>

/*
 * Items that need to be on the install screen:
 *  - A selector for which disk to install to
 *  - Switches to select:
 *     o If we want to wipe the entire disk and repartition
 *     o If we want to use a single partition
 *     o If we ...
 *  - a BIG button to confirm installation
 */
int 
construct_installscreen(light_component_t** root, light_gfx_t* gfx)
{
  light_ctx_t* ctx;
  disk_dev_t** devices;

  ctx = get_light_ctx();
  devices = ctx->present_disk_list;

  for (uint32_t i = 0; i < ctx->present_disk_count; i++) {
    disk_dev_t* dev = devices[i];

    if (!dev)
      continue;

    create_component(root, COMPONENT_TYPE_LABEL, "Disk device", 24, 72 + (36 * i), 82, 32, nullptr);
  }

  return 0;
}
