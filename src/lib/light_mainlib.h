#ifndef __LIGHT_TYPES__
#define __LIGHT_TYPES__

// I hate these headers, but I am too lazy atm to re-write them 
#include <lib/libldef.h>
#include <efi.h>

// TODO: remove
#define IF(thing) if(thing) {
#define ELSE } else {
#define ELIF(thing) } else if(thing) {
#define ENDIF }

#define asm __asm__
#define asmv __asm__ volatile

// ascii encoding 
#define TO_UPPERCASE(c) ((c >= 'a' && c <= 'z') ? c - 0x20 : c)
#define TO_LOWERCASE(c) ((c >= 'A' && c <= 'Z') ? c + 0x20 : c)

typedef enum {
  LIGHT_FAIL = 0,
  LIGHT_FAIL_WITH_WARNING = 1,
  LIGHT_SUCCESS = 2,
} LIGHT_STATUS;

// forward referencing (I know my code style is messy, I am just confused and selfconcous =[ )
struct mmap_struct;
struct _FatManager;

// TODO: move efi_memmap to the g_mmap_struct structure?
typedef struct {
  EFI_SYSTEM_TABLE* sys_table;
  EFI_BOOT_SERVICES* boot_services; 
  EFI_RUNTIME_SERVICES* runtime_services;
  EFI_CONFIGURATION_TABLE* config_table;

  EFI_HANDLE image_handle;

  EFI_HANDLE boot_drive_handle;

  EFI_MEMORY_DESCRIPTOR* efi_memmap; // the memory map that we create after we exit boot_services gets stored here
  UINTN efi_memmap_size;
  UINTN efi_desc_size;
  UINT32 efi_desc_version;

  size_t lightloader_size;
  size_t system_ticks;

  bool did_ditch_efi_services;

  bool has_sse;

  // can be casted to (mmap_struct_t*)
  struct mmap_struct* mmap_data;
  struct _FatManager* g_boot_volume_manager;

} light_info_t;

extern char __image_base[];
extern char __image_end[];

// TODO: memory functions
LIGHT_STATUS init_light_lib(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE* syst);

extern void exit_boot_services(); // pmm.c

#define string const char*
#define _string char*

size_t strlen(string str);

// 1 = 2 ?
int strcmp (string str1, string str2);
int strncmp(const char *s1, const char *s2, size_t n);

// and then there where two
_string strcpy (_string dest, string src);

// mem shit

int memcmp (const void* dest, const void* src, size_t size);
void *memcpy(void * restrict dest, const void * restrict src, size_t length);
// Problematic rn
//void *memmove(void *dest, const void *src, size_t n);
void *memset(void *data, int value, size_t length);
void *memchr(const void *s, int c, size_t n);

// different kinds of number-to-string formating

string to_string (uint64_t val);

void light_log(const short unsigned int* msg);

void hang();

extern light_info_t g_light_info;
#endif // !__LIGHT_TYPES__
