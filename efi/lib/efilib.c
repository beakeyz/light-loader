#include "efiapi.h"
#include "efidef.h"
#include "efierr.h"
#include "heap.h"
#include <efilib.h>

EFI_SYSTEM_TABLE         *ST;
EFI_BOOT_SERVICES        *BS;
EFI_RUNTIME_SERVICES     *RT;
EFI_HANDLE               IH;

size_t
locate_handle_with_buffer(EFI_LOCATE_SEARCH_TYPE type, EFI_GUID guid, size_t* size, EFI_HANDLE** handle_list)
{
  EFI_STATUS status;

  if (!handle_list || !size)
    return 0;

  status = BS->LocateHandle(type, &guid, NULL, size, *handle_list);

  if (status == EFI_BUFFER_TOO_SMALL) {
    *handle_list = heap_allocate(*size);

    if (!(*handle_list))
      return 0;

    status = BS->LocateHandle(type, &guid, NULL, size, *handle_list);

    if (EFI_ERROR(status))
      heap_free(*handle_list);
  }

  if (EFI_ERROR(status))
    return 0;

  return (*size / sizeof(EFI_HANDLE));
}
