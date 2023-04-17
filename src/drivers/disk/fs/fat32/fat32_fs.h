#ifndef __LIGHTLOADER_FAT32__
#define __LIGHTLOADER_FAT32__
#include "drivers/disk/fs/filesystem.h"
#include "drivers/disk/volume.h"
#include <lib/light_mainlib.h>

#define FAT_SECTOR_SIZE 512

#define FAT32_MAX_FILENAME_LENGTH 261

#define FAT32_CLUSTER_LIMIT 0xfffffef

#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN 0x02
#define FAT_ATTR_SYSTEM 0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE 0x20
#define FAT_ATTR_LONG_NAME (FAT_ATTR_READ_ONLY | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM | FAT_ATTR_VOLUME_ID)


struct _FatManager;

typedef struct {
  union {
    // joined bpb for fat16 and fat32
    // following fatspec instructions
    struct {
      uint8_t jmp_boot[3];
      char oem_name[8];
      uint16_t bytes_per_sector;
      uint8_t sectors_per_cluster;
      uint16_t reserved_sector_count;
      uint8_t fat_num;
      uint16_t root_entries_count;
      uint16_t sector_num_fat16;
      uint8_t media_type;
      uint16_t sectors_per_fat16;
      uint16_t sectors_per_track;
      uint16_t heads_num;
      uint32_t hidden_sector_num;
      uint32_t sector_num_fat32;
      uint32_t sectors_num_per_fat;
      uint16_t ext_flags;
      uint16_t fs_version;
      uint32_t root_cluster;
      uint16_t fs_info_sector;
      uint16_t mbr_copy_sector;
      uint8_t reserved[12];
      uint8_t drive_number;

      uint8_t reserved1;
      uint8_t signature;
      uint32_t volume_id;
      char volume_label[11];
      char system_type[8];
    } __attribute__((packed));
    uint8_t content[FAT_SECTOR_SIZE];
  };
} __attribute__((packed)) fat_bpb_t;

typedef struct {
  uint8_t dir_name[11];
  uint8_t dir_attr;
  uint8_t dir_ntres;
  uint8_t dir_crt_time_tenth;
  uint16_t dir_crt_time;
  uint16_t dir_crt_date;
  uint16_t dir_last_acc_date;
  uint16_t dir_frst_cluster_hi;
  uint16_t dir_wrt_time;
  uint16_t dir_wrt_date;
  uint16_t dir_frst_cluster_lo;
  uint32_t filesize_bytes;
} __attribute__((packed)) fat_dir_entry_t;

// TODO: long filename
typedef struct {
  uint8_t m_sequence_num_alloc_status;
  char m_file_chars_one[10];
  uint8_t m_attributes;
  uint8_t m_type;
  uint8_t m_dos_checksum;
  char m_file_chars_two[12]; 
  uint16_t m_first_cluster;
  char m_file_chars_three[4];
} __attribute__((packed)) fat_lfn_entry_t;

// TODO: just ew

struct _fat_file_handle;

// should free memory of the FatManager, bufferd clusters and just delete structure itself
typedef LIGHT_STATUS (*CLEAN_FAT_FHANDLE) (
  struct _fat_file_handle* handle
);

typedef struct _fat_file_handle {
  struct _FatManager* m_manager; // pointer to the manager that handled this file

  uint16_t m_first_cluster_lo;
  uint16_t m_first_cluster_hi;

  uint32_t m_filesize_bytes;

  uint32_t* m_cluster_chain;
  uint32_t m_cluster_count;

  CLEAN_FAT_FHANDLE CleanFatHandle;

} fat_file_handle_t;

//
// FatManager
//

typedef LIGHT_STATUS (*LOAD_CLUSTER) (
  struct _FatManager* _fat_manager,
  uint32_t* buffer,
  uint32_t cluster_num
);

typedef LIGHT_STATUS (*LOAD_CLUSTERS) (
  struct _FatManager* _fat_manager,
  void* buffer,
  uint32_t* clusters,
  uintptr_t cluster,
  uintptr_t count
);

typedef uint32_t* (*READ_CLUSTER_CHAIN) (
  struct _FatManager* _fat_manager,
  uint32_t cluster,
  size_t chain_size
);

typedef size_t (*GET_CLUSTER_CHAIN_LENGTH) (
  struct _FatManager* _fat_manager,
  uint32_t cluster
);

typedef LIGHT_STATUS (*OPEN_FAT_DIRECTORY_ENTRY) (
  struct _FatManager* _FatManager,
  fat_dir_entry_t* directory,
  fat_dir_entry_t* ret,
  const char* path
);

// for now, managers will be done in this CamelCase-like codeconvention.
// I will look into a project-wide refactor, but that is going to take a while =/
typedef struct _FatManager {
  light_volume_t* m_volume;
  fat_bpb_t m_fat_bpb;

  LOAD_CLUSTER fLoadCluster;
  LOAD_CLUSTERS fLoadClusters;
  READ_CLUSTER_CHAIN fReadClusterChain;
  GET_CLUSTER_CHAIN_LENGTH fGetClusterChainLength;
  OPEN_FAT_DIRECTORY_ENTRY fOpenFatDirectoryEntry;
} FatManager;

LIGHT_STATUS init_fat_management(FatManager* manager, light_volume_t* volume);
handle_t* open_fat32(light_volume_t* volume, const char* path);

#endif // !__LIGHTLOADER_FAT32__
