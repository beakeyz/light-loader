#ifndef __LIGHTLOADER_MULTIBOOT2_BOOT__ 
#define __LIGHTLOADER_MULTIBOOT2_BOOT__
#include "drivers/display/framebuffer.h"
  
// functions
__attribute__((noreturn)) void multiboot2_boot (const char* path, light_framebuffer_t* fb);

#endif // !
