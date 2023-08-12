#include "ctx.h"
#include <sys/ctx.h>

efi_ctx_t*
get_efi_context()
{
  return get_light_ctx()->private;
}
