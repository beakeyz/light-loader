#include "ctx.h"
#include "disk.h"
#include "fs.h"
#include "gfx.h"
#include "memory.h"
#include "stddef.h"
#include "ui/box.h"
#include "ui/button.h"
#include "ui/component.h"
#include "ui/selector.h"
#include <ui/screens/install.h>
#include <font.h>
#include <file.h>

static const char* disk_labels[] = {
  "Disk 0",
  "Disk 1",
  "Disk 2",
  "Disk 3",
  "Disk 4",
  "Disk 5",
  "Disk 6",
  "Disk 7",
  "Disk 8",
  "Disk 9",
};
static const uint32_t max_disks = sizeof(disk_labels) / sizeof(*disk_labels);
static button_component_t* current_device;

#define INSTALL_ERR_MASK        (0x80000000)
#define INSTALL_ERR(a)          (INSTALL_ERR_MASK | (a))

#define INSTALL_ERR_NODISK      INSTALL_ERR(1)
#define INSTALL_ERR_BOOTDEVICE  INSTALL_ERR(2)

static int perform_install();

#define DISK_CHOOSE_LABEL "Which disk do you want to install to?"
#define INSTALLATION_WARNING_LABEL "WARNING: installation on this disk means that all data on it will be wiped! Are you sure you are okay with that?"

static int 
disk_selector_onclick(button_component_t* component)
{
  if (current_device)
    current_device->parent->should_update = true;
  current_device = component;
  return 0;
}

static int
install_btn_onclick(button_component_t* btn)
{
  int error;
  light_ctx_t* ctx;
  light_component_t* parent;

  ctx = get_light_ctx();
  parent = btn->parent;

  if (!ctx->install_confirmed) {
    parent->label = "Please confirm your install!";
    return -1;
  }

  if (!current_device) {
    parent->label = "Please select a device to install to!";
    return -1;
  }

  error = perform_install();

  if (error) {
    parent->label = "Install failed!";
    return error;
  }

  parent->label = "Install successful! Please restart";
  return 0;
}

static int
disk_selector_ondraw(light_component_t* comp)
{
  disk_dev_t* this_dev;
  button_component_t* this_btn;
  uint32_t label_draw_x = 6;
  uint32_t label_draw_y = (comp->height >> 1) - (comp->gfx->current_font->height >> 1);

  this_btn = comp->private;
  this_dev = (disk_dev_t*)this_btn->private;

  gfx_draw_rect(comp->gfx, comp->x, comp->y, comp->width, comp->height, GRAY);

  if (component_is_hovered(comp))
    gfx_draw_rect_outline(comp->gfx, comp->x, comp->y, comp->width, comp->height, BLACK);

  if (this_btn == current_device)
    gfx_draw_rect(comp->gfx, comp->x + 3, comp->y + comp->height - 6, comp->width - 6, 2, GREEN);

  /*
   * Yes, we recompute this every redraw, but that does not happen very often, so
   * its fine =)
   */
  char size_suffix[4];
  size_t disk_size;

  enum {
    Bytes,
    Megabyte,
    Gigabyte,
    Terabyte
  } disk_size_magnitude = Bytes;

  disk_size = this_dev->total_size;

  /* Check if we can downsize to megabytes */
  if (disk_size / Mib <= 1)
    goto draw_labels;

  disk_size_magnitude = Megabyte;
  disk_size /= Mib;

  /* Yay, how about Gigabytes? */
  if (disk_size / Kib <= 1)
    goto draw_labels;

  disk_size_magnitude = Gigabyte;
  disk_size /= Kib;

  /* Yay, how about terabytes? */
  if (disk_size / Kib <= 1)
    goto draw_labels;

  disk_size_magnitude = Terabyte;
  disk_size /= Kib;

draw_labels:

  memset(size_suffix, 0, sizeof(size_suffix));

  switch (disk_size_magnitude) {
    case Bytes:
      memcpy(size_suffix, "Byt", sizeof(size_suffix));
      break;
    case Megabyte:
      memcpy(size_suffix, "Mib", sizeof(size_suffix));
      break;
    case Gigabyte:
      memcpy(size_suffix, "Gib", sizeof(size_suffix));
      break;
    case Terabyte:
      memcpy(size_suffix, "Tib", sizeof(size_suffix));
      break;
  }

  const size_t disk_size_len = strlen(to_string(disk_size));
  const size_t total_str_len = disk_size_len + sizeof(size_suffix);
  char total_str[total_str_len];

  memset(total_str, 0, total_str_len);
  /* Copy the size string */
  memcpy(total_str, to_string(disk_size), disk_size_len);
  memcpy(total_str + disk_size_len, size_suffix, sizeof(size_suffix));

  /* Disk size */
  component_draw_string_at(comp, total_str, label_draw_x + (comp->width) - lf_get_str_width(comp->font, total_str) - 12, label_draw_y, WHITE);

  /* Label */
  component_draw_string_at(comp, comp->label, label_draw_x, label_draw_y, WHITE);

  return 0;
}

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
  button_component_t* current_btn;
  size_t available_device_count;

  current_device = nullptr;
  ctx = get_light_ctx();
  devices = ctx->present_disk_list;
  available_device_count = ctx->present_disk_count <= max_disks ? ctx->present_disk_count : max_disks;

  create_component(root, COMPONENT_TYPE_LABEL, DISK_CHOOSE_LABEL, 24, 52, lf_get_str_width(gfx->current_font, DISK_CHOOSE_LABEL), 16, nullptr);

  for (uint32_t i = 0; i < ctx->present_disk_count; i++) {
    disk_dev_t* dev = devices[i];

    if (!dev)
      continue;

    current_btn = create_button(root, (char*)disk_labels[i], 24, 72 + (36 * i), 256, 32, disk_selector_onclick, disk_selector_ondraw);

    /* Make sure the button knows which device is represents */
    current_btn->private = (uintptr_t)dev;
  }

  create_switch(root, "Confirm my installation", 24, gfx->height - 94, (gfx->width >> 1) - 24, gfx->current_font->height * 2, &ctx->install_confirmed);

  create_button(root, "Install", 24, gfx->height - 28 - 8, gfx->width >> 1, 28, install_btn_onclick, nullptr);

  return 0;
}

/*!
 * @brief: Copy the contents of a file on bootdisk to @partition
 *
 * TODO: Correct error handling
 */
static int
copy_from_bootdisk(disk_dev_t* partition, char* path)
{
  int error;
  void* buffer;
  light_ctx_t* ctx;
  light_file_t* bootdisk_file;
  light_file_t* this_file;

  ctx = get_light_ctx();
  buffer = NULL;
  bootdisk_file = NULL;
  this_file = NULL;

  if (!partition || !partition->filesystem)
    return -1;

  /* Open the bootdisk file */
  bootdisk_file = fopen(path);
  
  if (!bootdisk_file)
    return -1;

  /* Create a path on our partition fs */
  error = partition->filesystem->f_create_path(partition->filesystem, path);

  if (error)
    goto dealloc_and_exit;

  /* Open the file on our partition fs */
  this_file = partition->filesystem->f_open(partition->filesystem, path);

  if (!this_file)
    goto dealloc_and_exit;

  /* Allocate a buffer for our bootdisk file */
  buffer = ctx->f_allocate(bootdisk_file->filesize);

  if (!buffer)
    goto dealloc_and_exit;

  /* Read the contents from the bootdisk file to our buffer */
  error = bootdisk_file->f_readall(bootdisk_file, buffer);

  if (error)
    goto dealloc_and_exit;

  /* Write the buffer to our new file */
  error = this_file->f_write(this_file, buffer, bootdisk_file->filesize, 0);

  if (error)
    goto dealloc_and_exit;

dealloc_and_exit:
  if (buffer)
    ctx->f_deallcoate(buffer, bootdisk_file->filesize);

  if (bootdisk_file)
    bootdisk_file->f_close(bootdisk_file);

  if (this_file)
    this_file->f_close(bootdisk_file);

  return error;
}

/*!
 * @brief: Do the installation on the selected disk
 *
 * TODO: check the selected install disk to see if we didn't boot from that
 * TODO: copy all the files from our root filesystem (The filesystem that the bootloader currently lives on and was booted from) to
 *       the selected install disks partitions. This means moving the bootloader, kernel, ramdisk, ect. to the System partition and
 *       the drivers, apps, ect. will go into the Data partition
 */
static int 
perform_install()
{
  int error;
  disk_dev_t* this;
  disk_dev_t* cur_partition;

  if (!current_device)
    return INSTALL_ERR_NODISK;

  this = (disk_dev_t*)current_device->private;

  if (!this || (this->flags & DISK_FLAG_DID_INSTALL) == DISK_FLAG_DID_INSTALL)
    return INSTALL_ERR_NODISK;

  if (disk_did_boot_from(this))
    return INSTALL_ERR_BOOTDEVICE;

  error = disk_install_partitions(this);

  if (error)
    return error;

  cur_partition = this->next_partition;

  while (cur_partition) {

    /* Check if this is a system partition or a data partition */
    switch (cur_partition->flags & (DISK_FLAG_SYS_PART|DISK_FLAG_DATA_PART)) {
      /* Install a filesystem on the system partition and copy files */
      case DISK_FLAG_SYS_PART:

        /*
         * Install a filesystem on this partition 
         * TODO: let the user choose it's filesystem
         */
        error = disk_install_fs(cur_partition, FS_TYPE_FAT32);

        if (error || !cur_partition->filesystem)
          return error;

        /* 
         * TODO: copy over the needed files for this filesystem 
         * For the system partition, this will be:
         *  - The files on the boot device
         *  - ...
         */

        error = copy_from_bootdisk(cur_partition, "aniva.elf");

        if (error)
          return error;

        error = copy_from_bootdisk(cur_partition, "rdisk.igz");

        if (error)
          return error;

        error = copy_from_bootdisk(cur_partition, "EFI/BOOT/BOOTX64.EFI");

        if (error)
          return error;
        break;
      /* Install a filesystem on the data partition and copy the files we need */
      case DISK_FLAG_DATA_PART:

        /*
         * Install a filesystem on this partition 
         * TODO: let the user choose it's filesystem
         */
        error = disk_install_fs(cur_partition, FS_TYPE_FAT32);

        if (error || !cur_partition->filesystem)
          return error;
        /*
         * TODO: copy over the needed files for this filesystem 
         * For the data partition, that will be:
         *  - All the files that are inside the ramdisk
         *  - ...
         */
        error = cur_partition->filesystem->f_create_path(cur_partition->filesystem, "data.txt");

        if (error)
          return error;
        break;
    }

    cur_partition = cur_partition->next_partition;
  }

  this->flags |= DISK_FLAG_DID_INSTALL;
  this->f_flush(this);
  return 0;
  /*
   * TODO: install =)
   *
   * 1) Clear the entire target disk
   * 2) Impose our own partitioning layout, which looks like:
   *    - 1] Boot partition: FAT32 formatted and contains our bootloader and kernel binaries, resources for our bootloader and kernel configuration
   *    - 2] Data partition: Contains our system files and userspace files (either FAT32, ext2 or our own funky filesystem)
   * 3) Install the filesystems in the partitions that need them
   * 4) Copy the files to our boot partition
   * 5) Copy files to our System partition and create files that we might need
   * 6) If any files are missing, try to download them?? (This would be psycho but mega cool)
   */
}
