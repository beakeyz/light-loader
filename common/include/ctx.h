#ifndef __LIGHLOADER_CTX__
#define __LIGHLOADER_CTX__

/*
 * The bootloader context will act as a general way to manage any 
 * bootloader stuff that might be platform-specific, like any exit or
 * system teardown in preperation for controltransfer to the kernel
 */

#include "disk.h"
#include "key.h"
#include "mouse.h"
#include "rsdp.h"
#include <stdint.h>
#include <boot/boot.h>

struct light_mmap_entry;

#define CTX_INSTALL_CHECK_PATH "res/install.yes"

typedef struct light_ctx {

  /* The start and end address of the loader in RAM */
  uintptr_t m_loader_image_start;
  uintptr_t m_loader_image_end;

  /* Private platform specific stuff */
  void* private;

  /* Do we still have firmware support? (Only really applicable for EFI) */
  bool has_fw;
  /* Has the user confirmed that they want to install to disk permanently? */
  bool install_confirmed;

  /* Put a reference to the memory map here */
  struct light_mmap_entry* mmap;
  size_t mmap_entries;

  /* Either the rsdp or the xsdp */
  system_ptrs_t sys_ptrs;

  /* How should we boot */
  light_boot_config_t light_bcfg;

  /*
   * Put a reference to the disk structure that we where loaded 
   * from (and thus where the kernel is still chilling) 
   * (EFI gives us the handle to a partition, so TODO rename this variable)
   */
  disk_dev_t* disk_handle;

  /* Array of all the 'disk' drives we found on the system */
  disk_dev_t** present_volume_list;
  uint32_t present_volume_count;
  
  /* Exit the bootloader (Deallocate any shit, prepare final mmap, ect.) in preperation for transfer of control */
  int (*f_fw_exit)();
  int (*f_shutdown)();
  int (*f_get_random_num)(uint8_t* b_out, uint32_t b_len);

  /* Mmap, syspointers, ect. */
  int (*f_gather_sys_info)();

  /* General memory allocation for data */
  void* (*f_allocate)(size_t size);
  void (*f_deallcoate)(void* addr, size_t size);

  int (*f_printf)(char* fmt, ...);

  bool (*f_has_keyboard)();
  bool (*f_has_mouse)();

  void (*f_init_keyboard)();
  void (*f_init_mouse)();

  int (*f_get_keypress)(light_key_t* key);
  int (*f_get_mousepos)(light_mousepos_t* pos);

} light_ctx_t;

void init_light_ctx(void (*platform_setup)(light_ctx_t* p_ctx));
light_ctx_t* get_light_ctx();

bool light_is_installed();

extern light_ctx_t g_light_ctx;
#endif // !__LIGHLOADER_CTX__
