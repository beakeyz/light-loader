#include "light_mainlib.h"
#include "mem/pmm.h"
#include <system/cpu.h>

light_info_t g_light_info;

size_t strlen (const char* str) {
    size_t s = 0;
    while (str[s] != 0)
        s++;
    return s;
}

int strcmp (const char * str1, const char *str2) {
    while (*str1 == *str2 && (*str1))
    {
        str1++;
        str2++;
    }
    return *(const unsigned char *)str1 - *(const unsigned char *)str2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c1 = s1[i], c2 = s2[i];
        if (c1 != c2)
            return c1 < c2 ? -1 : 1;
        if (!c1)
            return 0;
    }

    return 0;
}
_string strcpy (_string dest, string src) {
    size_t s = 0;
    while (src[s] != '\0') {
        dest[s] = src[s];
        s++;
    }
    return dest;
}

// quite a lot of allocations, idk man =/
void *memset(void *src, int value, size_t len) {
  uint8_t *d = src;
  for (size_t i = len; i > 0; i--) {
    *d++ = value;
  }
  return src;
}

void *memcpy(void *dst, const void *src, size_t len) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  while (len-- > 0) {
    d[len] = s[len];
  }
  return dst;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  int32_t delta = 0;
  const uint8_t *ss1 = (const uint8_t *)s1;
  const uint8_t *ss2 = (const uint8_t *)s2;
  for (size_t i = 0; i < n; i++) {
    if (ss1[i] != ss2[i]) {
      delta += (ss1[i] - ss2[i]);
    }
  }
  return delta;
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *copy = (const unsigned char *)s;
    for (size_t i = 0; i < n; i++)
    {
        if (copy[i] == c)
        {
            return (void *)(copy + i);
        }
    }

    return NULL;
}

// funnies
// NOTE: temp: only for debugging
// TODO: remove this crap everywhere when the io gets an upgrade
// Credit goes to: https://github.com/dreamos82/Dreamos64/

// quite the buffer
static char to_str_buff[128 * 2];

string to_string(uint64_t val) {
    memset(to_str_buff, 0, sizeof(to_str_buff));
    uint8_t size = 0;
    uint64_t size_test = val;
    while (size_test / 10 > 0) {
        size_test /= 10;
        size++;
    }

    uint8_t index = 0;
    
    while (val / 10 > 0) {
        uint8_t remain = val % 10;
        val /= 10;
        to_str_buff[size - index] = remain + '0';
        index++;
    }
    uint8_t remain = val % 10;
    to_str_buff[size - index] = remain + '0';
    to_str_buff[size + 1] = 0;
    return to_str_buff;
}

LIGHT_STATUS init_light_lib(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *syst) {

  // zero the structure first
  memset(&g_light_info, 0, sizeof(g_light_info));

  // the god (goated ong ong fr on my mother)
  g_light_info.sys_table = syst;

  // service 1
  g_light_info.boot_services = syst->BootServices;

  // service 2
  g_light_info.runtime_services = syst->RuntimeServices;

  // no plz don't go ;-;
  g_light_info.did_ditch_efi_services = false;

  // where am I?
  g_light_info.image_handle = image_handle;

  // acpi info and stuff
  g_light_info.config_table = syst->ConfigurationTable;

  // I need this
  g_light_info.lightloader_size = ALIGN_UP((uintptr_t)__image_end - (uintptr_t)__image_base, SMALL_PAGE_SIZE);

  // Let's just hope this does not die ;-;
  void* mmap_data = &g_mmap_struct;
  g_light_info.mmap_data = mmap_data;

  g_light_info.has_sse = false;

  // SSE check as per the INTEL manual
  uint32_t eax;
  uint32_t ebx;
  uint32_t ecx;
  uint32_t edx;
  cpuid(0x1, 0, &eax, &ebx, &ecx, &edx);

  const int32_t CHECKBITS = 0x080000 | 0x100000;

  if ((ecx & CHECKBITS) == CHECKBITS) {
    g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"has sse\n\r");
    g_light_info.has_sse = true;
  }

  return LIGHT_SUCCESS;
}

void light_log(const short unsigned int* msg) {
  g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)msg);
}

void hang() {
  for (;;) {}
} 
