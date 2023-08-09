#ifndef __LIGHLOADER_CTX__
#define __LIGHLOADER_CTX__

/*
 * The bootloader context will act as a general way to manage any 
 * bootloader stuff that might be platform-specific, like any exit or
 * system teardown in preperation for controltransfer to the kernel
 */

#include <stddef.h>
typedef struct light_ctx {
  
  /* Exit the bootloader (Deallocate any shit, prepare final mmap, ect.) in preperation for transfer of control */
  int (*f_exit)();

  /* General memory allocation for data */
  void* (*f_allocate)(size_t size);
  void (*f_deallcoate)(void* addr, size_t size);
} light_ctx_t;

void init_light_ctx(void (*platform_setup)(light_ctx_t* p_ctx));

light_ctx_t* get_light_ctx();

extern light_ctx_t g_light_ctx;

#endif // !__LIGHLOADER_CTX__
