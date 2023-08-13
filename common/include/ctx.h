#ifndef __LIGHLOADER_CTX__
#define __LIGHLOADER_CTX__

/*
 * The bootloader context will act as a general way to manage any 
 * bootloader stuff that might be platform-specific, like any exit or
 * system teardown in preperation for controltransfer to the kernel
 */

#include "disk.h"
#include "efiprot.h"
#include <stddef.h>
#include <stdint.h>

typedef struct light_ctx {

  /* The start and end address of the loader in RAM */
  uintptr_t m_loader_image_start;
  uintptr_t m_loader_image_end;

  /* Private platform specific stuff */
  void* private;

  /* Put a reference to the memory map here */
  void* mmap;

  /*
   * Put a reference to the disk structure that we where loaded 
   * from (and thus where the kernel is still chilling) 
   */
  disk_dev_t* disk_handle;
  
  /* Exit the bootloader (Deallocate any shit, prepare final mmap, ect.) in preperation for transfer of control */
  int (*f_exit)();

  int (*f_gather_sys_info)();

  /* General memory allocation for data */
  void* (*f_allocate)(size_t size);
  void (*f_deallcoate)(void* addr, size_t size);

  int (*f_printf)(char* fmt, ...);
} light_ctx_t;

void init_light_ctx(void (*platform_setup)(light_ctx_t* p_ctx));
light_ctx_t* get_light_ctx();

extern light_ctx_t g_light_ctx;
#endif // !__LIGHLOADER_CTX__
