#ifndef __LIGHTLOADER_ACPI__
#define __LIGHTLOADER_ACPI__

#include <lib/light_mainlib.h>


// this structure will contain all the pointers to the funnie structures
typedef struct {
  void* rsdp;
  void* xsdp;
} sdt_ptrs_t;

typedef enum {
  RSDP,
  XSDP,
  OTHER
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

uint8_t get_acpi_checksum(void* sdp, size_t size);

SDT_TYPE get_sdt_type (EFI_CONFIGURATION_TABLE* table);

sdt_ptrs_t get_sdtp (void);

void* get_rsdt_table (const char* sig, uint32_t idx);

#endif // !__LIGHTLOADER_ACPI__
