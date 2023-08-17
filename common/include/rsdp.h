#ifndef __LIGHTLOADER_RSDP__
#define __LIGHTLOADER_RSDP__

#include <stdint.h>

typedef struct system_ptrs {
  void* rsdp;
  void* xsdp;
} system_ptrs_t;

typedef enum {
  RSDP,
  XSDP,
  UNKNOWN
} SDT_TYPE;

// these god dawmn structures
typedef struct {
    char     signature[4];
    uint32_t length;
    uint8_t  rev;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_rev;
    char     creator_id[4];
    uint32_t creator_rev;
} __attribute__((packed)) sdt_t;

typedef struct {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  rev;
    uint32_t rsdt_addr;
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t  ext_checksum;
    uint8_t  reserved[3];
} __attribute__((packed)) rsdp_t;

typedef struct rsdt {
    sdt_t header;
    char ptrs_start[];
} __attribute__((packed)) rsdt_t;

#endif // !__LIGHTLOADER_RSDP__
