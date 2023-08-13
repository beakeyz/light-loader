#ifndef __LIGHTLOADER_STDIO__
#define __LIGHTLOADER_STDIO__

#include <ctx.h>

#define printf(...) get_light_ctx()->f_printf(__VA_ARGS__)

#endif // !__LIGHTLOADER_STDIO__
