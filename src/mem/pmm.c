#include "pmm.h"
#include "drivers/disk/fs/filesystem.h"
#include "efidef.h"
#include "lib/liblmath.h"
#include "lib/light_mainlib.h"

mmap_struct_t g_mmap_struct;

static uint32_t _get_efi_memory_type(UINT32);
static LIGHT_STATUS _add_mmap_entry(mmap_descriptor_t*, size_t*, uintptr_t, size_t, uint32_t);
static LIGHT_STATUS _merge_mmap_entries (mmap_descriptor_t* orig_map, size_t* entries);
static LIGHT_STATUS _new_mmap_descriptor(mmap_descriptor_t* map, size_t* size, uintptr_t addr, uintptr_t len, uint32_t type);
static const char *_get_memmap_str_type(uint32_t type);

meminfo_t mmap_get_info(size_t mmap_count, mmap_descriptor_t *mmap) {
  meminfo_t ret = {0, 0};

  for (uint64_t i = 0; i < mmap_count; ++i) {
    mmap_descriptor_t disc = mmap[i];

    if (disc.base < ONE_MB_MARK) {
      if (disc.base + disc.limit > ONE_MB_MARK) {
        // uhm, what is where?
        size_t lo_amount = ONE_MB_MARK - disc.base;
        size_t up_amount = disc.limit - lo_amount;
        ret.lo += lo_amount;
        ret.up += up_amount;
      } else {
        ret.lo += disc.limit;
      }
    } else {
      ret.up += disc.limit;
    }
  }

  return ret;
}
/*
dump the efi memmap into our pointer
*/
LIGHT_STATUS init_memmap() {

  memset(&g_mmap_struct, 0, sizeof(g_mmap_struct));
  UINTN key = 0;

  g_light_info.boot_services->GetMemoryMap(&g_light_info.efi_memmap_size, g_light_info.efi_memmap, &key, &g_light_info.efi_desc_size, &g_light_info.efi_desc_version);
  
  g_mmap_struct.max_mmap_entries = (g_light_info.efi_memmap_size / g_light_info.efi_desc_size) + 512;

  g_light_info.efi_memmap_size += SMALL_PAGE_SIZE;

  // TODO: make this type of errorhandling more verbose
  // allocate for our efi-obtained mmap
  if (g_light_info.boot_services->AllocatePool(EfiLoaderData, g_light_info.efi_memmap_size, (void **)&g_light_info.efi_memmap))
    return LIGHT_FAIL;
  // allocate for our custom mmap
  if (g_light_info.boot_services->AllocatePool(EfiLoaderData, g_mmap_struct.max_mmap_entries * sizeof(mmap_descriptor_t), (void **)&g_mmap_struct.lightloader_mmap))
    return LIGHT_FAIL;
  // allocate for the original mmap
  if (g_light_info.boot_services->AllocatePool(EfiLoaderData, g_mmap_struct.max_mmap_entries * sizeof(mmap_descriptor_t), (void **)&g_mmap_struct.original_mmap))
    return LIGHT_FAIL;

  if (g_light_info.boot_services->GetMemoryMap(&g_light_info.efi_memmap_size, g_light_info.efi_memmap, &key, &g_light_info.efi_desc_size, &g_light_info.efi_desc_version)) {
    g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[WARNING] Uhm, guys?\n\r");
    return LIGHT_FAIL;
  }

  size_t entries = (g_light_info.efi_memmap_size / g_light_info.efi_desc_size);

  for (size_t i = 0; i < entries; ++i) {
    EFI_MEMORY_DESCRIPTOR *cur_desc = (EFI_MEMORY_DESCRIPTOR*)((void *)(g_light_info.efi_memmap) + i * g_light_info.efi_desc_size);

    uint32_t mem_type = _get_efi_memory_type(cur_desc->Type);

    uint64_t addr = cur_desc->PhysicalStart;
    uint64_t length = cur_desc->NumberOfPages * SMALL_PAGE_SIZE;

    if (mem_type == MEMMAP_USABLE && addr + length >= FOUR_GB_MARK) {

      if (addr < FOUR_GB_MARK) {
        size_t delta = FOUR_GB_MARK - addr;
        length -= delta;

        g_mmap_struct.lightloader_mmap[g_mmap_struct.lightloader_mmap_entries].base = addr;
        g_mmap_struct.lightloader_mmap[g_mmap_struct.lightloader_mmap_entries].limit = delta;
        g_mmap_struct.lightloader_mmap[g_mmap_struct.lightloader_mmap_entries].type = mem_type;

        addr = FOUR_GB_MARK;
        g_mmap_struct.lightloader_mmap_entries++;
      }

      mem_type = MEMMAP_EFI_RECLAIMABLE;
      continue;
    }


    g_mmap_struct.lightloader_mmap[g_mmap_struct.lightloader_mmap_entries].base = addr;
    g_mmap_struct.lightloader_mmap[g_mmap_struct.lightloader_mmap_entries].limit = length;
    g_mmap_struct.lightloader_mmap[g_mmap_struct.lightloader_mmap_entries].type = mem_type;
    g_mmap_struct.lightloader_mmap[g_mmap_struct.lightloader_mmap_entries].zero = 0;

    g_mmap_struct.lightloader_mmap_entries++;
  }

  pmm_clean_entries_mmap(g_mmap_struct.lightloader_mmap, &g_mmap_struct.lightloader_mmap_entries);

  g_mmap_struct.original_mmap_entries = g_mmap_struct.lightloader_mmap_entries;
  memcpy(g_mmap_struct.original_mmap, g_mmap_struct.lightloader_mmap, g_mmap_struct.lightloader_mmap_entries * sizeof(mmap_descriptor_t));  

  LIGHT_STATUS mark_status = pmm_mark_in_mmap(g_mmap_struct.lightloader_mmap, &g_mmap_struct.lightloader_mmap_entries, (uintptr_t)__image_base, g_light_info.lightloader_size, MEMMAP_BOOTLOADER_RECLAIMABLE, 0);
  if (mark_status != LIGHT_SUCCESS) {
    g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[ERROR] Something went wrong while marking mmap entries!\n\r");
  }

  return mark_status;
}

/*
Be cool
*/
LIGHT_STATUS pmm_clean_entries_mmap(mmap_descriptor_t* map, size_t* _entries) {
  size_t entries = *_entries;

  for (size_t i = 0; i < entries; i++) {
    if (map[i].type == MEMMAP_USABLE) {
      for (size_t j = 0; j < entries; j++) {
        if (j == i)
          continue;

        size_t current_base = map[i].base;
        size_t current_limit = map[i].limit;
        size_t current_end_base = current_base + current_limit;
        
        size_t compare_base = map[j].base;
        size_t compare_limit = map[j].limit;
        size_t compare_end_base = compare_base + compare_limit;

        // TODO: idk if this sucks lol
        // The compare entry is inside our current entry, great
        if (compare_end_base > current_base && compare_end_base < current_end_base) {
          // sanity check i guess
          if (compare_base < current_base) {
            map[j].limit -= compare_end_base - current_base;
          }
        }
      }
    }
  }

  LIGHT_STATUS s = _merge_mmap_entries(map, &entries);
  if (s != LIGHT_SUCCESS) {
    g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[ERROR] Something went wrong while merging mmap entries!\n\r");
    return s;
  }

  *_entries = entries;
  return LIGHT_SUCCESS;
}

LIGHT_STATUS pmm_mark_in_mmap(mmap_descriptor_t* map, size_t* mmap_size, uint64_t addr, size_t len, uint32_t mem_desc_type, uint32_t old_desc_type) {

  size_t size = *mmap_size;

  for (size_t i = 0; i < size; i++) {
    
    mmap_descriptor_t* current = &map[i];
    if (old_desc_type != 0 && current->type == old_desc_type) {

      size_t current_end = addr + len;
      bool is_in_desired_range = (current->base >= addr && current->base + current->limit <= current_end);

      if (is_in_desired_range) {

        // alright, now we have to create a new entry ;-;

        if (_new_mmap_descriptor(map, &size, addr, len, mem_desc_type) == LIGHT_SUCCESS) {
          pmm_clean_entries_mmap(map, &size);
          *mmap_size = size;
          return LIGHT_SUCCESS;
        }
      }
    }
  }
  
  LIGHT_STATUS status = _new_mmap_descriptor(map, &size, addr, len, mem_desc_type);
  if (status != LIGHT_SUCCESS) {
    g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[DEBUG] Failed to mark in memmap!\n\r");
    return status;
  }

  pmm_clean_entries_mmap(map, &size);
  *mmap_size = size;
  return LIGHT_SUCCESS;
}

void pmm_debug_mmap() {

  for (uintptr_t i = 0; i < g_mmap_struct.lightloader_mmap_entries; i++) {
    mmap_descriptor_t desc = g_mmap_struct.lightloader_mmap[i];


    light_log(L"Memory map entry: ");
    uint32_t type = desc.type;

    switch (type) {
      case MEMMAP_USABLE:
        light_log(L"Usable ");
        break;
      case MEMMAP_ACPI_NVS:
        light_log(L"ACPI nvs ");
        break;
      case MEMMAP_RESERVED:
        light_log(L"Reserved ");
        break;
      case MEMMAP_BAD_MEMORY:
        light_log(L"Bad mem ");
        break;
      case MEMMAP_ACPI_RECLAIMABLE:
        light_log(L"ACPI reclaim ");
        break;
      case MEMMAP_EFI_RECLAIMABLE:
        light_log(L"EFI reclaim ");
        break;
      default:
        light_log(L"Unknown ");
    }

    light_log(to_string(desc.base));
    light_log(" -> ");
    light_log(to_string(desc.base + desc.limit));
    light_log(L"\n\r");
  }
}

/*
  Deallocate from the lightloader_mmap
*/
void *pmm_free(void *ptr, size_t len) {
  mmap_descriptor_t* map = g_mmap_struct.lightloader_mmap;
  pmm_mark_in_mmap(map, &g_mmap_struct.lightloader_mmap_entries, (size_t)ptr, ALIGN_UP(len, SMALL_PAGE_SIZE), MEMMAP_USABLE, 0); 
  return ptr;
}

/*
  Allocate inside the lightloader_mmap
*/
void *pmm_malloc(size_t len, uint32_t type) {

  mmap_descriptor_t* map = g_mmap_struct.lightloader_mmap;
  size_t entries = g_mmap_struct.lightloader_mmap_entries;

  // can't allocate 0 bytes =/
  if (len == 0) {
    return NULL;
  }

  len = ALIGN_UP(len, 0x1000);

  for (size_t i = entries-1; i >= 0; i--) {
    if (map[i].type != MEMMAP_USABLE) {
      continue;
    }

    size_t current_base = map[i].base;
    size_t current_end = current_base + map[i].limit;

    if (current_end > FOUR_GB_MARK) {
      if (current_base > FOUR_GB_MARK) {
        continue;
      }
      current_end = FOUR_GB_MARK;
    }

    // Page align our alloc_base
    size_t allocation_base = ALIGN_DOWN(current_end - len, 0x1000);

    if (allocation_base < current_base) {
      continue;
    }

    void* ptr = (void*)allocation_base;

    pmm_mark_in_mmap(map, &g_mmap_struct.lightloader_mmap_entries, allocation_base, current_end - allocation_base, type, MEMMAP_USABLE);

    memset(ptr, 0, len);
    return ptr;
  } 

  return NULL;
}

// TODO: some way to convert our efi-memmap to a generic memmap that, lets say, multiboot can use
// void convert_to_general_memmap([params]);
// void reclaim_efi_mem ([params])

// STATIC STUFF (EW)
static LIGHT_STATUS _new_mmap_descriptor(mmap_descriptor_t* map, size_t* _size, uintptr_t addr, uintptr_t len, uint32_t type) {

  size_t size = *_size;
  
  size_t current_end = addr + len;

  for (size_t i = 0; i < size; i++) {
    size_t compare_base = map[i].base;
    size_t compare_end = compare_base + map[i].limit;

    if (addr <= compare_base && addr + len >= compare_end) {
      map[i] = map[size + 1];
      size--;
      i--;
      continue;
    }

    if (addr <= compare_base && current_end < compare_end && current_end > compare_base) {
      size_t delta = current_end - compare_base; 
      map[i].limit -= delta;
      map[i].base += delta;
      continue;
    }

    if (addr > compare_base && current_end >= compare_end && addr < compare_end) {
      size_t delta = compare_end - addr;
      map[i].limit -= delta;
      continue;
    }

    if (addr > compare_base && current_end < compare_end) {
      size_t top_delta = compare_end - addr;
      size_t bottom_delta = compare_end - current_end;
      map[i].limit -= top_delta;

      mmap_descriptor_t* new_resulting = &map[size++];
      new_resulting->base = compare_end;
      new_resulting->limit = bottom_delta;
      new_resulting->type = map[i].type;
      new_resulting->zero = 0;
      continue;
    }
  } 

  mmap_descriptor_t* new = &map[size++];
  new->base = addr;
  new->limit = len;
  new->type = type;
  new->zero = 0;

  *_size = size;
  return LIGHT_SUCCESS;
}

static uint32_t _get_efi_memory_type(UINT32 weird_type) {

  switch (weird_type) {
  case EfiReservedMemoryType:
  case EfiRuntimeServicesCode:
  case EfiRuntimeServicesData:
  case EfiUnusableMemory:
  case EfiMemoryMappedIO:
  case EfiMemoryMappedIOPortSpace:
  case EfiPalCode:
  case EfiLoaderCode:
  case EfiLoaderData:
  default:
    return MEMMAP_RESERVED;
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

// TODO: find out this crap
static LIGHT_STATUS _merge_mmap_entries (mmap_descriptor_t* orig_map, size_t* entries) {

  size_t _entries = *entries;

  for (size_t i = 0; i < _entries; i++) {
    mmap_descriptor_t* one = &orig_map[i];
    mmap_descriptor_t* two = &orig_map[i+1];

    // FIXME: allow merging for every type?
    bool should_merge = (one->type == MEMMAP_USABLE || one->type == MEMMAP_EFI_RECLAIMABLE || one->type == MEMMAP_BOOTLOADER_RECLAIMABLE);

    if (should_merge && one->type == two->type) {

      // both seem fine
      if (two->base == one->base + one->limit || two->base == one->base + one->limit + 1) {
        
        one->limit += two->limit;

        //g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[INFO] Merging mmap entry!\n\r");

        // shift entries
        for (size_t j = i + 1; j < _entries; j++) {
          orig_map[j] = orig_map[j+1];
        }
        _entries--;
        i--;
      }
    }
  }
  
  *entries = _entries;
  return LIGHT_SUCCESS;
}

mmap_descriptor_t* get_raw_efi_memmap(size_t* entries) {

  if (g_light_info.did_ditch_efi_services) {

    mmap_descriptor_t* ret = g_mmap_struct.original_mmap;

    // Processing?


    return ret;
  }

  return NULL;
}

void exit_boot_services() {

  EFI_STATUS status;
  EFI_MEMORY_DESCRIPTOR tmp[1];
  EFI_MEMORY_DESCRIPTOR* copy;
  g_light_info.efi_memmap_size = sizeof(tmp);
  UINTN key;
  // get mmap
  g_light_info.boot_services->GetMemoryMap(&g_light_info.efi_memmap_size, tmp, &key, &g_light_info.efi_desc_size, &g_light_info.efi_desc_version);

  g_light_info.efi_memmap_size += SMALL_PAGE_SIZE;

  status = g_light_info.boot_services->FreePool(g_light_info.efi_memmap);
  if (status) {
    // yikes
    light_log(L"FUCK2");
    for (;;) {}
  }

  status = g_light_info.boot_services->AllocatePool(EfiLoaderData, g_light_info.efi_memmap_size, (void**)&g_light_info.efi_memmap);
  if (status) {
    // yikes
    light_log(L"FUCK3");
    for (;;) {}
  }

  status = g_light_info.boot_services->AllocatePool(EfiLoaderData, g_light_info.efi_memmap_size, (void**)&copy);
  if (status) {
    // yikes
    light_log(L"FUCK4");
    for (;;) {}
  }

  status = g_light_info.boot_services->GetMemoryMap(&g_light_info.efi_memmap_size, g_light_info.efi_memmap, &key, &g_light_info.efi_desc_size, &g_light_info.efi_desc_version);
  if (status) {
    // yikes
    light_log(L"FUCK5");
    for (;;) {}
  }

  status = g_light_info.boot_services->ExitBootServices(g_light_info.image_handle, key);
  if (status != EFI_SUCCESS) {
    // yikes
    light_log(L"FUCK6");
    for (;;) {}
  }

  asm volatile ("cld" ::: "memory");
  asm volatile ("cli" ::: "memory");

  g_light_info.did_ditch_efi_services = true;
}

static const char *_get_memmap_str_type(uint32_t type) {
    switch (type) {
        case MEMMAP_USABLE:
            return "Usable RAM";
        case MEMMAP_RESERVED:
            return "Reserved";
        case MEMMAP_ACPI_RECLAIMABLE:
            return "ACPI reclaimable";
        case MEMMAP_ACPI_NVS:
            return "ACPI NVS";
        case MEMMAP_BAD_MEMORY:
            return "Bad memory";
        case MEMMAP_FRAMEBUFFER:
            return "Framebuffer";
        case MEMMAP_BOOTLOADER_RECLAIMABLE:
            return "Bootloader reclaimable";
        case MEMMAP_KRNL:
            return "Kernel/Modules";
        case MEMMAP_EFI_RECLAIMABLE:
            return "EFI reclaimable";
        default:
            return "???";
    }
}
