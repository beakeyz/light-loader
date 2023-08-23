#include "ctx.h"
#include "efiapi.h"
#include "efidef.h"
#include "efierr.h"
#include "efilib.h"
#include "heap.h"
#include "rsdp.h"
#include <stdio.h>
#include <sys/ctx.h>
#include <memory.h>

efi_ctx_t*
get_efi_context()
{
  return get_light_ctx()->private;
}

static uint8_t
get_acpi_checksum(void* sdp, size_t size) 
{
  uint8_t checksum = 0;
  uint8_t* check_ptr = sdp;
  for (size_t i = 0; i < size; i++) {
    checksum += check_ptr[i];
  }
  return checksum;
}

static SDT_TYPE
get_sdt_type (EFI_CONFIGURATION_TABLE* table) 
{
  // TODO
  const EFI_GUID RSDP_GUID = ACPI_20_TABLE_GUID;
  const EFI_GUID XSDP_GUID = ACPI_TABLE_GUID;

  if (memcmp(&XSDP_GUID, &table->VendorGuid, sizeof(EFI_GUID)) == 0) {
    if (get_acpi_checksum(table->VendorTable, sizeof(rsdp_t)) == 0) {
      return XSDP;
    }
  }

  if (memcmp(&RSDP_GUID, &table->VendorGuid, sizeof(EFI_GUID)) == 0) {
    if (get_acpi_checksum(table->VendorTable, 20) == 0) {
      return RSDP;
    }

  }

  return UNKNOWN;
}


void
gather_system_pointers(system_ptrs_t* ptrs)
{
  SDT_TYPE current_type;

  if (!ptrs)
    return;

  memset(ptrs, 0, sizeof(*ptrs));

  for (uintptr_t i = 0; i < ST->NumberOfTableEntries; i++) {
    EFI_CONFIGURATION_TABLE* table = &ST->ConfigurationTable[i];
    current_type = get_sdt_type(table);

    switch (current_type) {
      case XSDP:
        if (!ptrs->xsdp)
          ptrs->xsdp = table->VendorTable;
        break;
      case RSDP:
        if (!ptrs->rsdp)
          ptrs->rsdp = table->VendorTable;
        break;
      case UNKNOWN:
        break;
    }

    /* Really no need to search any more */
    if (ptrs->xsdp && ptrs->rsdp)
      break;
  }
}

static uint32_t get_efi_memory_type(UINT32 weird_type) {

  switch (weird_type) {
    case EfiReservedMemoryType:
    case EfiUnusableMemory:
    case EfiMemoryMappedIO:
    case EfiMemoryMappedIOPortSpace:
    case EfiPalCode:
    default:
      return MEMMAP_RESERVED;
    case EfiRuntimeServicesCode:
    case EfiRuntimeServicesData:
    case EfiLoaderCode:
    case EfiLoaderData:
    case EfiBootServicesCode:
    case EfiBootServicesData:
      return MEMMAP_EFI_RECLAIMABLE;
    case EfiACPIReclaimMemory:
      return MEMMAP_ACPI_RECLAIMABLE;
    case EfiACPIMemoryNVS:
      return MEMMAP_ACPI_NVS;
    case EfiConventionalMemory:
      return MEMMAP_USABLE;
  }

  // TODO: find out if this is bad practise
  return MEMMAP_BAD_MEMORY;
}

int
gather_memmap(struct light_ctx* ctx)
{
  EFI_STATUS status;
  size_t size = 0;
  UINTN descriptor_size = 0;
  UINTN key = 0;
  uint32_t version = 0;
  EFI_MEMORY_DESCRIPTOR* mmap = nullptr;

  size_t descriptor_count;

  /* If there was a previous mmap, clear it */
  if (ctx->mmap)
    heap_free(ctx->mmap);

  /* Grab the mmap params */
  status = BS->GetMemoryMap(&size, mmap, &key, &descriptor_size, &version);

  /* allocate memory for the memmap */
  mmap = heap_allocate(size);

  /* Get it again */
  status = BS->GetMemoryMap(&size, mmap, &key, &descriptor_size, &version);

  if (EFI_ERROR(status))
    goto out;

  /* Compute length */
  descriptor_count = ALIGN_UP(size, descriptor_size) / descriptor_size;

  /* Allocate our own mmap structure */
  ctx->mmap = heap_allocate(descriptor_count * sizeof(light_mmap_entry_t));
  ctx->mmap_entries = 0;

  /* Get every descriptor */
  for (uintptr_t i = 0; i < descriptor_count; i++) {
    EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((void*)mmap + i * descriptor_size);

    ctx->mmap[ctx->mmap_entries++] = (light_mmap_entry_t) {
      .size = desc->NumberOfPages * EFI_PAGE_SIZE,
      .paddr = ALIGN_UP(desc->PhysicalStart, EFI_PAGE_SIZE),
      .type = get_efi_memory_type(desc->Type),
      .pad = 0,
    };
  }

out:
  if (mmap)
    heap_free(mmap);

  /* 0 on success */
  return status;
}
