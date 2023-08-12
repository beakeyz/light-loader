#include "efiapi.h"
#include "efidef.h"
#include "efierr.h"
#include "heap.h"
#include "stddef.h"
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

void*
efi_allocate(size_t size) 
{
  EFI_STATUS status;
  EFI_PHYSICAL_ADDRESS ret;
  size_t pages = EFI_SIZE_TO_PAGES(size);

  status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, pages, &ret);

  if (status != EFI_SUCCESS)
    return nullptr;

  return (void*)ret;
}

void
efi_deallocate(void* addr, size_t size)
{
  size = EFI_SIZE_TO_PAGES(size);
  BS->FreePages((EFI_PHYSICAL_ADDRESS)addr, size);
}

int
open_protocol(EFI_HANDLE handle, EFI_GUID* guid, void** out)
{
  return (BS->OpenProtocol(handle, guid, out, IH, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL));
}
