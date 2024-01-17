#include "boot/multiboot.h"
#include "ctx.h"
#include "efilib.h"
#include "elf.h"
#include <lelf.h>
#include "elf64.h"
#include "file.h"
#include "gfx.h"
#include "heap.h"
#include "relocations.h"
#include "rsdp.h"
#include <memory.h>
#include <boot/boot.h>
#include <stdint.h>
#include <stdio.h>
#include <elf.h>

/* TODO: rename the kernelfile to fit the kernel name */
const char* default_kernel_path = "aniva.elf";
const char* default_ramdisk_path = "rdisk.igz";

extern __attribute__((noreturn)) void multiboot2_boot_entry(uint32_t entry, uint32_t ranges, uint32_t ranges_count, uint32_t new_stub_location, uint32_t multiboot_addr, uint32_t multiboot_magic);
extern char multiboot2_bootstub[];
extern char multiboot2_bootstub_end[];

static void
panic(char* msg)
{
  light_gfx_t* gfx;
  get_light_gfx(&gfx);

  gfx_clear_screen(gfx);

  /* Print n switch */
  printf(" * Lightloader panic: ");
  printf(msg);
  gfx_switch_buffers(gfx);

  for (;;) {}
}

static struct multiboot_header*
find_mb_header(void* buffer)
{
  struct multiboot_header* header;

  for (uintptr_t i = 0; i < MULTIBOOT_SEARCH; i += MULTIBOOT_HEADER_ALIGN) {
    header = (struct multiboot_header*)((uintptr_t)buffer + i);

    if (header->magic == MULTIBOOT2_HEADER_MAGIC)
      goto validate;
  }

  return NULL;

validate:
  /* If this checksum is non-zero, the header is invalid */
  if (header->magic + header->header_length + header->architecture + header->checksum)
    return NULL;

  return header;
}

static struct multiboot_header_tag* 
get_multiboot_header_tag(struct multiboot_header* header, uint32_t type)
{
  for (struct multiboot_header_tag *tag = (struct multiboot_header_tag*)(header + 1); // header + 1 to skip the header struct.
    tag < (struct multiboot_header_tag *)((uintptr_t)header + header->header_length) && tag->type != MULTIBOOT_HEADER_TAG_END;
    tag = (struct multiboot_header_tag *)((uintptr_t)tag + ALIGN_UP(tag->size, MULTIBOOT_TAG_ALIGN))) {

    if (tag->type == type)
      return tag;
  }

  return nullptr;
}

static multiboot_uint32_t get_multiboot_mmap_type(uint32_t light_loader_type) {

  switch (light_loader_type) {
    /* TODO: what other types do we just mark as usable? */
    case MEMMAP_EFI_RECLAIMABLE:
    case MEMMAP_BOOTLOADER_RECLAIMABLE:
    case MEMMAP_USABLE:
      return MULTIBOOT_MEMORY_AVAILABLE;
    case MEMMAP_BAD_MEMORY:
      return MULTIBOOT_MEMORY_BADRAM;
    case MEMMAP_ACPI_RECLAIMABLE:
      return MULTIBOOT_MEMORY_ACPI_RECLAIMABLE;
    case MEMMAP_ACPI_NVS:
      return MULTIBOOT_MEMORY_NVS;
  }

  return MULTIBOOT_MEMORY_RESERVED;
}

static multiboot_memory_map_t*
get_multiboot_memorymap(size_t* entry_count, light_ctx_t* ctx)
{
  uintptr_t i = 0;
  multiboot_memory_map_t* ret;
  uintptr_t needed_address;
  light_mmap_entry_t* light_mmap;

  if (!entry_count)
    return nullptr;

  *entry_count = 0;

  light_mmap = ctx->mmap;
  needed_address = 0;
  ret = heap_allocate(sizeof(multiboot_memory_map_t) * ctx->mmap_entries);

  memset(ret, 0, ctx->mmap_entries * sizeof(multiboot_memory_map_t));

  while(i < ctx->mmap_entries) {
    light_mmap_entry_t* current = &light_mmap[i];
    uint64_t j = 1;

    if (!current->type)
      goto skip;

    while (i + j < ctx->mmap_entries) {
      light_mmap_entry_t* check = &light_mmap[i + j];

      if (current->type != check->type)
        break;

      current->size += check->size;

      j++;
    }

    ret[*entry_count].addr = current->paddr;
    ret[*entry_count].len = current->size;
    ret[*entry_count].type = get_multiboot_mmap_type(current->type);
    ret[*entry_count].zero = 0;

    (*entry_count)++;

skip:
    i += j;
  }

  return ret;
}

static multiboot_memory_map_t*
collapse_mmap(multiboot_memory_map_t* map, size_t* entries)
{
  uint64_t i = 0;
  multiboot_memory_map_t* ret;
  size_t total_entries;

  if (!map || !entries || !(*entries))
    return nullptr;

  total_entries = *entries;
  ret = heap_allocate(total_entries * sizeof(multiboot_memory_map_t));

  if (!ret)
    return nullptr;

  memset(ret, 0, total_entries * sizeof(multiboot_memory_map_t));

  *entries = 0;

  while (i < total_entries) {
    multiboot_memory_map_t current = map[i];
    uintptr_t j = 1;

    while (i + j < total_entries) {
      multiboot_memory_map_t next = map[i + j];

      if (current.type != next.type)
        break;

      current.len += next.len;

      j++;
    }
    
    /* Copy over the current entry */
    memcpy(&ret[*entries], &current, sizeof(multiboot_memory_map_t));

    i+=j;
    (*entries)++;
  }

  return ret;
}

static uintptr_t
relocate_ramdisk(light_relocation_t** relocations, void* current_buffer, size_t size)
{
  uintptr_t highest;

  if (!relocations || !(*relocations))
    return NULL;

  highest = highest_relocation_addr(*relocations);

  if (!highest)
    return NULL;

  if (!create_relocation(relocations, (uintptr_t)current_buffer, highest, size)) 
    return NULL;

  return highest;
}

/*!
 * @brief Create a reallocation for the bootstub
 *
 * Returns: the target address of the relocation
 */
static uintptr_t 
relocate_bootstub()
{
  size_t bootstub_size = (size_t)multiboot2_bootstub_end - (size_t)&multiboot2_bootstub;
  void* new_bootstub_location = heap_allocate(bootstub_size);

  if (!new_bootstub_location)
    return NULL;

  memcpy(new_bootstub_location, multiboot2_bootstub, bootstub_size);

  return (uintptr_t)new_bootstub_location;
}

#define MULTIBOOT_DEFAULT_LOAD_BASE 0x10000
#define MULTIBOOT_BUFF_SIZE 0x2000

uintptr_t multiboot_buffer;
uintptr_t current_multiboot_tag;

/*!
 * @brief Allocate a buffer for multiboot info and create a reallocation for it
 *
 * Returns: the target address of the relocation
 */
static uintptr_t
prepare_multiboot_buffer(light_relocation_t** relocations)
{
  uintptr_t multiboot_reloc_addr;

  if (!relocations || !(*relocations))
    return NULL;

  current_multiboot_tag = 8;
  multiboot_buffer = (uintptr_t)heap_allocate(MULTIBOOT_BUFF_SIZE);

  if (!multiboot_buffer)
    return NULL;

  /* FIXME: this causes all sorts of problems */
  /*
  multiboot_reloc_addr = MULTIBOOT_DEFAULT_LOAD_BASE;

  if (relocation_does_overlap(relocations, multiboot_reloc_addr,  MULTIBOOT_BUFF_SIZE))
    */
  multiboot_reloc_addr = highest_relocation_addr(*relocations);

  if (!create_relocation(relocations, (uintptr_t)multiboot_buffer, multiboot_reloc_addr, MULTIBOOT_BUFF_SIZE))
    return NULL;

  return multiboot_reloc_addr;
}

static void* 
add_multiboot_tag(uint32_t type, uint32_t size)
{
  struct multiboot_tag* new_tag = (struct multiboot_tag*)(multiboot_buffer + current_multiboot_tag);

  memset(new_tag, 0, size);

  new_tag->type = type;
  new_tag->size = size;

  current_multiboot_tag += ALIGN_UP(size, MULTIBOOT_TAG_ALIGN);

  return new_tag;
}

static uint32_t 
get_first_bit_offset(uint32_t dword)
{
  uint32_t pos = 0;

  while (pos < 31 && ((dword >> pos) & 1) == 0)
    pos++;

  return pos;
}

void 
__attribute__((noreturn))
boot_context_configuration(light_ctx_t* ctx)
{
  int error;
  size_t relocation_count = NULL;
  light_relocation_t* relocations = NULL;
  light_file_t* kernel_file = nullptr;
  light_file_t* ramdisk_file = nullptr;
  size_t kernel_size = NULL;
  size_t ramdisk_size = NULL;
  uintptr_t kernel_entry = NULL;
  void* kernel_buffer = nullptr;
  void* ramdisk_buffer = nullptr;
  struct multiboot_header* mb_header = nullptr;
  Elf64_Ehdr* elf_header = nullptr;
  light_boot_config_t* config = &ctx->light_bcfg;
  light_gfx_t* gfx = NULL;

  uintptr_t new_bootstub_loc = NULL;
  uintptr_t new_multiboot_loc = NULL;
  uintptr_t new_ramdisk_loc = NULL;

  /* First, make sure we have valid files here */
  if (!config->kernel_file)
    config->kernel_file = (char*)default_kernel_path;
  if (!config->ramdisk_file)
    config->ramdisk_file = (char*)default_ramdisk_path;

  /* Get the GFX in preperation for multiboot stuff */
  get_light_gfx(&gfx);

  /* Clear the framebuffer here so that panics get displayed properly */
  gfx_clear_screen(gfx);

  /* Open the kernel file */
  kernel_file = fopen(config->kernel_file);

  if (!kernel_file)
    panic("Could not find kernel file!");

  /* Allocate a buffer for the kernel file */
  kernel_buffer = heap_allocate(kernel_file->filesize);

  if (!kernel_buffer)
    panic("Could not allocate buffer to fit the kernel!");

  /* Read the entire kernel file */
  error = kernel_file->f_readall(kernel_file, kernel_buffer);

  if (error)
    panic("Could not read the kernel!");

  /* Grab the size rq */
  kernel_size = kernel_file->filesize;

  /* Close the kernel file for kicks and giggles */
  kernel_file->f_close(kernel_file);

  /* Find the kernels mb header */
  mb_header = find_mb_header(kernel_buffer);

  if (!mb_header)
    panic("Could not find a valid Multiboot2 header in the kernel file!");
  
  /* Create relocations for the kernel */
  error = load_relocatable_elf(kernel_buffer, kernel_size, &kernel_entry, &relocation_count, &relocations);

  if (error)
    panic("Failed to load kernel ELF!");

  ramdisk_file = fopen(config->ramdisk_file);

  if (!ramdisk_file)
    panic("Failed to open kernel ramdisk!");

  ramdisk_buffer = heap_allocate(ramdisk_file->filesize);

  if (!ramdisk_buffer)
    panic("Failed to allocate buffer for the ramdisk!");

  error = ramdisk_file->f_readall(ramdisk_file, ramdisk_buffer);

  if (error)
    panic("Failed to read the ramdisk file!");

  ramdisk_size = ramdisk_file->filesize;

  /* Close the ramdisk for kicks and giggles */
  ramdisk_file->f_close(ramdisk_file);

  /* Relocate our bootstub */
  new_bootstub_loc = relocate_bootstub();

  if (!new_bootstub_loc)
    panic("Couldn't relocate bootstub (NOMEM?)");

  /* Prepare our multiboot buffer */
  new_multiboot_loc = prepare_multiboot_buffer(&relocations);

  if (!new_multiboot_loc)
    panic("Failed to prepare multiboot buffers");

  new_ramdisk_loc = relocate_ramdisk(&relocations, ramdisk_buffer, ramdisk_size);

  if (!new_ramdisk_loc)
    panic("Failed to relocate the ramdisk!");

  /* With the first tag we will identify ourselves */
  const char* loader_name = "__LightLoader__";
  struct multiboot_tag_string* name_tag = add_multiboot_tag(MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME, sizeof(struct multiboot_tag_string) + strlen(loader_name));

  memcpy(name_tag->strn, loader_name, strlen(loader_name) + 1);

  /* Create a framebuffer tag if the kernel wants one */
  if (get_multiboot_header_tag(mb_header, MULTIBOOT_HEADER_TAG_FRAMEBUFFER)) {
    struct multiboot_tag_framebuffer* fb_tag = add_multiboot_tag(MULTIBOOT_TAG_TYPE_FRAMEBUFFER, sizeof(struct multiboot_tag_framebuffer));

    fb_tag->common.framebuffer_addr = gfx->phys_addr;
    fb_tag->common.framebuffer_width = gfx->width;
    fb_tag->common.framebuffer_height = gfx->height;
    fb_tag->common.framebuffer_pitch = gfx->stride * (gfx->bpp >> 3);
    fb_tag->common.framebuffer_bpp = gfx->bpp;

    fb_tag->framebuffer_red_mask_size = 8;
    fb_tag->framebuffer_green_mask_size = 8;
    fb_tag->framebuffer_blue_mask_size = 8;
    
    fb_tag->framebuffer_red_field_position = get_first_bit_offset(gfx->red_mask);
    fb_tag->framebuffer_green_field_position = get_first_bit_offset(gfx->green_mask);
    fb_tag->framebuffer_blue_field_position = get_first_bit_offset(gfx->blue_mask);

    fb_tag->common.framebuffer_type = 0;
  }

  /* Add the ramdisk */
  if (new_ramdisk_loc) {
    struct multiboot_tag_module* ramdisk_mod = add_multiboot_tag(MULTIBOOT_TAG_TYPE_MODULE, sizeof(struct multiboot_tag_module));

    ramdisk_mod->mod_end = new_ramdisk_loc + ramdisk_size;
    ramdisk_mod->mod_start = new_ramdisk_loc;
  }

  /* Add the system pointers */
  if (ctx->sys_ptrs.xsdp) {
    struct multiboot_tag_new_acpi* new_acpi = add_multiboot_tag(MULTIBOOT_TAG_TYPE_ACPI_NEW, sizeof(struct multiboot_tag_new_acpi) + sizeof(uint64_t));

    memset(new_acpi->rsdp, 0, sizeof(uint64_t));
    *(uint64_t*)new_acpi->rsdp = (uint64_t)ctx->sys_ptrs.xsdp;
  } 

  if (ctx->sys_ptrs.rsdp) {
    struct multiboot_tag_old_acpi* old_acpi = add_multiboot_tag(MULTIBOOT_TAG_TYPE_ACPI_OLD, sizeof(struct multiboot_tag_old_acpi) + sizeof(uint64_t));

    memset(old_acpi->rsdp, 0, sizeof(uint64_t));
    *(uint64_t*)old_acpi->rsdp = (uint64_t)ctx->sys_ptrs.rsdp;
  }

  error = ctx->f_fw_exit();

  if (error)
    panic("Failed to exit the firmware!");

  /* Convert our lightloader mmap to a multiboot one and create the memory map tag */
  size_t entry_count = NULL;
  size_t final_entry_count = NULL;
  multiboot_memory_map_t* final_mmap;
  multiboot_memory_map_t* mmap = get_multiboot_memorymap(&entry_count, ctx);

  /* This would be fucked */
  if (!mmap || !entry_count)
    panic("Failed to create multiboot memory map");

  /* Cache the entry count */
  final_entry_count = entry_count;

  /* This is not very needed, but just a nice  */
  multiboot_memory_map_t* collapsed_mmap = collapse_mmap(mmap, &entry_count);

  final_mmap = mmap;

  /* If we could collapse, use the collapsed mmap */
  if (collapsed_mmap && entry_count) {
    final_mmap = collapsed_mmap;
    final_entry_count = entry_count;
  }

  struct multiboot_tag_mmap* mmap_tag = add_multiboot_tag(MULTIBOOT_TAG_TYPE_MMAP, sizeof(struct multiboot_tag_mmap) + sizeof(multiboot_memory_map_t) * final_entry_count);

  mmap_tag->entry_size = sizeof(multiboot_memory_map_t);
  mmap_tag->entry_version = 0;

  for (uintptr_t i = 0; i < final_entry_count; i++) {
    mmap_tag->entries[i] = final_mmap[i];
  }

  struct multiboot_tag* end_tag = add_multiboot_tag(0, sizeof(struct multiboot_tag));

  /* Call our muiltiboot bootstub =) */
  multiboot2_boot_entry(
      (uint32_t)kernel_entry,
      (uint32_t)(uintptr_t)relocations,
      (uint32_t)relocation_count,
      (uint32_t)(uintptr_t)new_bootstub_loc,
      (uint32_t)new_multiboot_loc,
      MULTIBOOT2_BOOTLOADER_MAGIC
      );

  panic("Returned from the kernel somehow!");
}
