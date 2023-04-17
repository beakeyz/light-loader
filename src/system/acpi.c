#include "acpi.h"

uint8_t get_acpi_checksum(void* sdp, size_t size) {
  // TODO
  uint8_t checksum = 0;
  uint8_t* check_ptr = sdp;
  for (size_t i = 0; i < size; i++) {
    checksum += check_ptr[i];
  }
  return checksum;
}

SDT_TYPE get_sdt_type (EFI_CONFIGURATION_TABLE* table) {
  // TODO
  const EFI_GUID RSDP_GUID = ACPI_20_TABLE_GUID;
  const EFI_GUID XSDP_GUID = ACPI_TABLE_GUID;

  if (memcmp(&XSDP_GUID, &table->VendorGuid, sizeof(EFI_GUID)) == 0) {
    if (get_acpi_checksum(table->VendorTable, sizeof(rsdp_t)) == 0) {
      //g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[INFO] Found xsdp!\n\r");
      return XSDP;
    }
  }

  if (memcmp(&RSDP_GUID, &table->VendorGuid, sizeof(EFI_GUID)) == 0) {
    if (get_acpi_checksum(table->VendorTable, 20) == 0) {
      //g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[INFO] Found rsdp!\n\r");
      return RSDP;
    }

  }

  //g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[INFO] Found other pointer!\n\r");
  return OTHER;
}

sdt_ptrs_t get_sdtp (void) {
  sdt_ptrs_t t = {NULL, NULL};

  for (size_t i = 0; i < g_light_info.sys_table->NumberOfTableEntries; i++) {
    // let's once again just hope for the best
    EFI_CONFIGURATION_TABLE* current_cfg_table_ptr = &g_light_info.config_table[i];

    // what kind of sdt (system descriptor table) are we pointing to? (rsdp, xsdp)
    // - check guid
    // - check checksum
    SDT_TYPE current_type = get_sdt_type(current_cfg_table_ptr);

    // put both our pointers in the sdt_ptrs_t struct and return

    switch (current_type) {
      case XSDP:
        t.xsdp = (void*)current_cfg_table_ptr->VendorTable;
        //g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[INFO] set xsdp!\n\r");
        break;
      case RSDP:
        t.rsdp = (void*)current_cfg_table_ptr->VendorTable;
        //g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[INFO] set rsdp!\n\r");
        break;
      case OTHER:
        break;
    }

    if (t.rsdp && t.xsdp) {
      //g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[INFO] returned!\n\r");
      break;
    }
  }

  return t;
}

void* get_rsdt_table (const char* sig, uint32_t idx) {

  sdt_ptrs_t ptrs = get_sdtp();
  rsdp_t* rsdp = NULL;
  rsdt_t* table = NULL;
  size_t entries = 0;

  if (ptrs.xsdp != NULL) {
    // yeey we have an extended pointer
    rsdp = (rsdp_t*)ptrs.xsdp;
    table = (rsdt_t*)(uintptr_t)rsdp->xsdt_addr;

    entries = (table->header.length - sizeof(sdt_t)) / 8;
  } else if (ptrs.rsdp != NULL) {
    // we are poor =(
    rsdp = (rsdp_t*)ptrs.rsdp;
    table = (rsdt_t*)(uintptr_t)rsdp->rsdt_addr;

    entries = (table->header.length - sizeof(sdt_t)) / 4;
  } else {
    // we are extremely poor =((
    return NULL;
  }

  if (rsdp == NULL || table == NULL) {
    // wtf
    return NULL;
  }
  
  uint32_t count = 0;
  for (size_t i = 0; i < entries; i++) {
    sdt_t* current_table = (ptrs.xsdp != NULL) ? (sdt_t*)(uintptr_t)((uint64_t*)table->ptrs_start)[i] : (sdt_t*)(uintptr_t)((uint32_t*)table->ptrs_start)[i]; 

    // idk if it is a good idea to match our loop i against the param idx =/
    #define SIG_LENGHT 4
    if (memcmp(current_table->signature, sig, SIG_LENGHT) == 0 && get_acpi_checksum(current_table, current_table->length) == 0 && count++ == idx) {

      //g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[INFO] found table!\n\r");
      return current_table;
    }

  }

 // g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"[INFO] table not found!\n\r");
  return NULL;
}
