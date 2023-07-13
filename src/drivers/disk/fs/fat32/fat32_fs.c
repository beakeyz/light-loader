#include "fat32_fs.h"
#include "drivers/disk/fs/filesystem.h"
#include "drivers/disk/volume.h"
#include "efiser.h"
#include "frontend/loading_screen.h"
#include "lib/light_mainlib.h"
#include "lib/linkedlist.h"
#include "mem/pmm.h"

// TODO: clean this mess up

// a FAT 8+3 filename consists of a 8 char long name with an 3 char long extention. 
static LIGHT_STATUS transform_fat_filename(char* dest, const char* src) {

  uint32_t i = 0;
  uint32_t src_dot_idx = 0;

  bool found_ext = false;

  // put spaces
  memset(dest, ' ', 11);

  while (src[src_dot_idx + i]) {
    // limit exceed
    if (i >= 11 || (i >= 8 && !found_ext)) {
      return LIGHT_FAIL;
    }

    // we have found the '.' =D
    if (src[i] == '.') {
      found_ext = true; 
      src_dot_idx = i+1;
      i = 0;
    }

    if (found_ext) {
      dest[8 + i] = TO_UPPERCASE(src[src_dot_idx + i]);
      i++;
    } else {
      dest[i] = TO_UPPERCASE(src[i]);
      i++;
    }
  }

  return LIGHT_SUCCESS;
}

// load a cluster from disk
LIGHT_STATUS _load_cluster(FatManager* manager, uint32_t* buffer, uint32_t cluster) {

  uintptr_t usable_clusters_start = (manager->m_fat_bpb.reserved_sector_count * manager->m_fat_bpb.bytes_per_sector); 
  uintptr_t cluster_loc = cluster * sizeof(uint32_t);

  memset(buffer, 0, sizeof(uint32_t));

  LIGHT_STATUS ret = read_volume(manager->m_volume, buffer, usable_clusters_start + cluster_loc, sizeof(uint32_t));

  *buffer &= 0x0fffffff;

  return ret;
}


LIGHT_STATUS _load_clusters(FatManager* manager, void* buffer, uint32_t* clusters, uintptr_t cluster, uintptr_t count) {

  fat_bpb_t* fat_bpt_ptr = &manager->m_fat_bpb;
  size_t cluster_size = manager->m_fat_bpb.sectors_per_cluster * manager->m_fat_bpb.bytes_per_sector;

  for (size_t i = 0; i < count;) {
    uintptr_t cluster_addr = cluster + i;
    size_t cluster_num     = cluster_addr / cluster_size;
    size_t cluster_offset  = cluster_addr % cluster_size;
    size_t cluster_chunk = min(count - i, cluster_size - cluster_offset);

    uintptr_t usable_clusters_start = (uintptr_t)(fat_bpt_ptr->reserved_sector_count + fat_bpt_ptr->fat_num * fat_bpt_ptr->sectors_num_per_fat);
    uintptr_t current_cluster_offset = (usable_clusters_start + (clusters[cluster_num] - 2) * fat_bpt_ptr->sectors_per_cluster) * fat_bpt_ptr->bytes_per_sector;

    LIGHT_STATUS read_status = read_volume(manager->m_volume, buffer + i, current_cluster_offset + cluster_offset, cluster_chunk);

    i += cluster_chunk;
  }

  return LIGHT_SUCCESS;
}

uint32_t* _read_clustr_chain(FatManager* manager, uint32_t cluster, size_t* _chain_size) {
  
  if (cluster < 0x2 || cluster > FAT32_CLUSTER_LIMIT) {
    return 0;
  }

  size_t chain_size = 1;

  loading_screen_set_status("Reading chain");

  uint32_t test = cluster;
  for (; ; chain_size++) {
    manager->fLoadCluster(manager, &test, test);

    if (test < 0x2 || test > FAT32_CLUSTER_LIMIT) {
      break;
    }
  }

  loading_screen_set_status(to_string(chain_size));

  test = cluster;
  uint32_t* clustr_chain = pmm_malloc(sizeof(uint32_t) * chain_size, MEMMAP_BOOTLOADER_RECLAIMABLE);

  for (uint64_t i = 0; i < chain_size; i++) {
    clustr_chain[i] = test;
    manager->fLoadCluster(manager, &test, test);
  }

  *_chain_size = chain_size;

  return clustr_chain;
}

// "open" a FAT dir/file through its relative path (name) and its directory handle
LIGHT_STATUS _open_fat_directory_entry(FatManager* manager, fat_dir_entry_t* directory, fat_dir_entry_t* ret, const char* path) {

  // fuck you, you little asshole
  if (directory == NULL || manager == NULL || path == NULL) {
    return LIGHT_FAIL;
  }

  fat_bpb_t* fat_bpb = &manager->m_fat_bpb;

  uint32_t cluster_number = directory->dir_frst_cluster_lo | (uint32_t)directory->dir_frst_cluster_hi << 16;
  size_t cluster_size = fat_bpb->sectors_per_cluster * fat_bpb->bytes_per_sector;

  size_t cluster_chain_length;
  uint32_t* cluster_chain = manager->fReadClusterChain(manager, cluster_number, &cluster_chain_length);

  size_t cluster_chain_size = cluster_chain_length * cluster_size;

  fat_dir_entry_t* dir_entries = pmm_malloc(cluster_chain_size, MEMMAP_BOOTLOADER_RECLAIMABLE);

  LIGHT_STATUS load_status = manager->fLoadClusters(manager, dir_entries, cluster_chain, 0, cluster_chain_size);    

  pmm_free(cluster_chain, cluster_chain_length * sizeof(uint32_t));

  if (load_status != LIGHT_SUCCESS) {
    g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"OOPSIE\n\r");
    return load_status;
  }

  for (size_t i = 0; i < (cluster_chain_length * cluster_size) / sizeof(fat_dir_entry_t); i++) {
    // filename can not start with '.'
    if (dir_entries[i].dir_name[0] == 0x20) {
      continue;
    }

    if (dir_entries[i].dir_name[0] == 0x00) {
      break;
    }

    uint8_t dir_attributes = dir_entries[i].dir_attr;

    if (dir_attributes == FAT_ATTR_LONG_NAME) {
      // TODO: handle lfn
      fat_lfn_entry_t* fat_lfn_entry = (fat_lfn_entry_t*) &dir_entries[i];
      uint8_t is_first_entry = (fat_lfn_entry->m_sequence_num_alloc_status & 0b01000000);

      // haha since you tried to use an unimplemented feature, you've got gobsmacked =)

    } else if (!(dir_attributes & (1 << 3))) {
      char transformed_filename[11];
      if (transform_fat_filename(transformed_filename, path) == LIGHT_SUCCESS) {
        // "TEST    TXT"
        if (!strncmp(transformed_filename, (const char*)dir_entries[i].dir_name, 11)) {
          *ret = dir_entries[i];
          pmm_free(dir_entries, cluster_chain_size);
          return LIGHT_SUCCESS;
        }
      } 
    }
  }

  pmm_free(dir_entries, cluster_chain_size);
  return LIGHT_FAIL;
}

LIGHT_STATUS init_fat_management(FatManager *manager, light_volume_t* volume) {

  // should be a partition
  manager->m_volume = volume;

  fat_bpb_t fat_bpb = {0};
  if (read_volume(volume, &fat_bpb, 0, sizeof(fat_bpb_t)) != LIGHT_SUCCESS) {
    // yikes
    g_light_info.sys_table->ConOut->OutputString(g_light_info.sys_table->ConOut, (CHAR16*)L"just no\n\r");
    return LIGHT_FAIL;
  }

  // address for the identifier on fat32
  const char* fat32_ident = (((void*)&fat_bpb) + 0x52);

  // address for the identifier on fat32 but fancy
  const char* fat32_ident_fancy = (((void*)&fat_bpb) + 0x03);

  if (strncmp(fat32_ident, "FAT", 3) != 0 && strncmp(fat32_ident_fancy, "FAT32", 5) != 0) {
    // wrong partition format
    return LIGHT_FAIL;
  }

  if (fat_bpb.bytes_per_sector == 0) 
    return LIGHT_FAIL;

  manager->m_fat_bpb = fat_bpb;

  manager->fLoadClusters = _load_clusters;
  manager->fLoadCluster = _load_cluster;
  manager->fReadClusterChain = _read_clustr_chain;
  // manager->fGetClusterChainLength = _get_cluster_chain_length;
  manager->fOpenFatDirectoryEntry = _open_fat_directory_entry;

  return LIGHT_SUCCESS;
}

// **********************************
// * FAT file handle functions      *
// **********************************

LIGHT_STATUS _clean_fat_fhandle (fat_file_handle_t* handle) {

  if (handle != NULL) {
    if (handle->m_cluster_chain != NULL) {
      pmm_free(handle->m_cluster_chain, handle->m_cluster_count * sizeof(uint32_t));
      // if this handle has a manager, it is stored on the heap
      if (handle->m_manager != NULL) {
        pmm_free(handle->m_manager, sizeof(FatManager));
      }
      pmm_free(handle, sizeof(fat_file_handle_t));
      return LIGHT_SUCCESS;
    }
  } 
  return LIGHT_FAIL;
}

// **********************************
// * FAT file interaction functions *
// **********************************

void _fat_read(handle_t* file_handle, void* buffer, uintptr_t location, size_t size) {
  fat_file_handle_t* descriptor = file_handle->file_descriptor;

  memset(buffer, 0, size);

  if (descriptor->m_manager->fLoadClusters(descriptor->m_manager, buffer, descriptor->m_cluster_chain, location, size) == LIGHT_FAIL) {
    // TODO
  }
}

void _fat_close (handle_t* file_handle) {
  fat_file_handle_t* descriptor = file_handle->file_descriptor;
  if (descriptor->CleanFatHandle(descriptor) == LIGHT_FAIL) {
    // TODO
  }
}

handle_t* open_fat32(light_volume_t* volume, const char* path) {

  FatManager* manager;

  /* We don't have a fat32 volume here =( */
  if (volume->fs_type != VOLUME_FSTYPE_FAT32)
    return NULL;

  if (!volume->fat_manager)
    return NULL;

  manager = volume->fat_manager;

  fat_bpb_t fat_bpb = manager->m_fat_bpb;

  // TODO: I need this to be as clean as possible
  uint32_t rds = (fat_bpb.root_entries_count * 32 + (fat_bpb.bytes_per_sector - 1)) / fat_bpb.bytes_per_sector; // NOTE: root_entries_count is zero on FAT32
  // count reserved sectors, root directories and fats as reserved
  uint32_t total_resrv_sect_num = fat_bpb.reserved_sector_count + (fat_bpb.fat_num * fat_bpb.sectors_num_per_fat) + rds;
  uint32_t total_usable_sect_num = fat_bpb.sector_num_fat32 - total_resrv_sect_num;
  uint32_t cluster_count = total_usable_sect_num / fat_bpb.sectors_per_cluster;

  // fatspec =D
  if (cluster_count < 65525) {
    // yikes, wrong partition format again =/
    loading_screen_set_status("Wrong partition format (cc < 65525)");
    return NULL;
  }

  // init at root dir
  fat_dir_entry_t current_dir = {0};

  fat_dir_entry_t* current_dir_ptr;
  fat_dir_entry_t next_dir_entry = {0};
  char path_buffer[FAT32_MAX_FILENAME_LENGTH];
  uint32_t prev_seperator = 0;

  current_dir.dir_frst_cluster_lo = fat_bpb.root_cluster & 0xFFFF;
  current_dir.dir_frst_cluster_hi = fat_bpb.root_cluster >> 16;
  current_dir_ptr = &current_dir;

  uint16_t max_spin = 0xFFFF;

  do {
    for (uint32_t i = 0; i < FAT32_MAX_FILENAME_LENGTH; i++) {
      memset(path_buffer, 0, FAT32_MAX_FILENAME_LENGTH);

      if (path[prev_seperator + i] == '/') {
        memcpy(path_buffer, path + prev_seperator, i);
        prev_seperator += i+1;
        break;
      } else if (path[prev_seperator + i] == 0x00) {
        memcpy(path_buffer, path + prev_seperator, i);
        break;
      }
    }

    if (manager->fOpenFatDirectoryEntry(manager, current_dir_ptr, &next_dir_entry, path_buffer) != LIGHT_SUCCESS) {
      loading_screen_set_status("Failed to open directory, file probably does not exist");
      return NULL;
    }

    loading_screen_set_status("Opended first dir");

    // we could find this shit
    if ((next_dir_entry.dir_attr & (FAT_ATTR_DIRECTORY|FAT_ATTR_VOLUME_ID)) == FAT_ATTR_DIRECTORY) {
      loading_screen_set_status("Next dir entry");
      current_dir = next_dir_entry;
      current_dir_ptr = &current_dir;
    } else {

      loading_screen_set_status("Found the file");

      // found the file
      handle_t* file_handle = pmm_malloc(sizeof(handle_t), MEMMAP_BOOTLOADER_RECLAIMABLE);
      fat_file_handle_t* fat_handle = pmm_malloc(sizeof(fat_file_handle_t), MEMMAP_BOOTLOADER_RECLAIMABLE);

      uint32_t file_cluster = next_dir_entry.dir_frst_cluster_lo | ((uintptr_t)next_dir_entry.dir_frst_cluster_hi << 16); 

      // FAT specific data
      fat_handle->m_manager = manager;
      fat_handle->m_first_cluster_lo = next_dir_entry.dir_frst_cluster_lo;
      fat_handle->m_first_cluster_hi = next_dir_entry.dir_frst_cluster_hi;
      fat_handle->m_filesize_bytes = next_dir_entry.filesize_bytes;
      // fat_handle->m_cluster_count = manager->fGetClusterChainLength(manager, file_cluster);
      loading_screen_set_status(to_string(fat_handle->m_filesize_bytes));
      fat_handle->m_cluster_chain = manager->fReadClusterChain(manager, file_cluster, &fat_handle->m_cluster_count);

      fat_handle->CleanFatHandle = (void*)_clean_fat_fhandle;

      // global file_handle data
      file_handle->volume = manager->m_volume;
      file_handle->file_descriptor = (void*) fat_handle;
      file_handle->file_descriptor_size = sizeof(fat_file_handle_t);
      file_handle->file_size = fat_handle->m_filesize_bytes;
      file_handle->partition_handle = manager->m_volume->partition_handle;

      file_handle->fRead = (void*)_fat_read;
      file_handle->fClose = (void*)_fat_close;

      // this is so aids
      extern LIGHT_STATUS _clean_file_handle(handle_t* handle);
      file_handle->fCleanHandle = _clean_file_handle;

      return file_handle;
    }

    max_spin--;
  } while (max_spin);

  // - detect the end of the path with the target file
  // - create a file_handle_t and stuff it with data =D

  return NULL;
}
