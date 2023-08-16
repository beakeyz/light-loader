#include "fs/fat/fat32.h"
#include <file.h>
#include "fs.h"
#include <heap.h>
#include <disk.h>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>

#define FAT_FLAG_NO_LFN 0x00000001
#define FAT_FLAG_EXFAT  0x00000002

typedef struct fat_private {
  fat_bpb_t bpb;
  uint32_t root_directory_sectors;
  uint32_t total_reserved_sectors;
  uint32_t total_usable_sectors;
  uint32_t cluster_count;
  uint32_t cluster_size;
  uintptr_t usable_clusters_start;
  fat_dir_entry_t root_entry;

  uint32_t flags;
} fat_private_t;

static int
__fat32_file_write(struct light_file* file, void* buffer, size_t size, uintptr_t offset)
{
  return 0;
}

static int 
__fat32_file_read(struct light_file* file, void* buffer, size_t size, uintptr_t offset)
{
  return 0;
}

static int 
__fat32_file_readall(struct light_file* file, void* buffer)
{
  return 0;
}

static int
__fat32_file_close(struct light_file* file)
{
  fat_file_t* ffile;

  ffile = file->private;

  /* Clear the cluster chain */
  if (ffile->cluster_chain)
    heap_free(ffile->cluster_chain);

  /* Clear the rest of the file */
  heap_free(file->private);
  heap_free(file);
  return 0;
}

int 
fat32_probe(light_fs_t* fs, disk_dev_t* device)
{
  int error;
  fat_private_t* private;
  fat_bpb_t bpb = { 0 };

  error = device->f_bread(device, &bpb, 1, device->first_sector);

  if (error)
    return error;

  // address for the identifier on fat32
  const char* fat32_ident = (((void*)&bpb) + 0x52);

  // address for the identifier on fat32 but fancy
  const char* fat32_ident_fancy = (((void*)&bpb) + 0x03);

  if (strncmp(fat32_ident, "FAT", 3) != 0 && strncmp(fat32_ident_fancy, "FAT32", 5) != 0)
    return -1;

  if (!bpb.bytes_per_sector)
    return -1;

  private = heap_allocate(sizeof(fat_private_t));

  memset(private, 0, sizeof(fat_private_t));

  private->root_directory_sectors = (bpb.root_entries_count * 32 + (bpb.bytes_per_sector - 1)) / bpb.bytes_per_sector;
  private->total_reserved_sectors = bpb.reserved_sector_count + (bpb.fat_num * bpb.sectors_num_per_fat) + private->root_directory_sectors;
  private->total_usable_sectors = bpb.sector_num_fat32 - private->total_reserved_sectors;
  private->cluster_count = private->total_usable_sectors / bpb.sectors_per_cluster;
  private->cluster_size = bpb.sectors_per_cluster * bpb.bytes_per_sector;
  private->total_usable_sectors = bpb.reserved_sector_count * bpb.bytes_per_sector;
  private->usable_clusters_start = (uintptr_t)(bpb.reserved_sector_count + (bpb.fat_num * bpb.sectors_num_per_fat));

  /* TODO: support lfn */
  private->flags |= FAT_FLAG_NO_LFN;

  /* Not FAT32 */
  if (private->cluster_count < 65525) {
    heap_free(private);
    return -1;
  }

  /*
   * Make sure the root entry is setup correctly
   */
  private->root_entry = (fat_dir_entry_t){
    .dir_frst_cluster_lo = bpb.root_cluster & 0xFFFF,
    .dir_frst_cluster_hi = (bpb.root_cluster >> 16) & 0xFFFF,
    .dir_attr = FAT_ATTR_DIRECTORY,
    .dir_name = "/",
  };

  memcpy(&private->bpb, &bpb, sizeof(fat_bpb_t));

  fs->private = private;

  return 0;
}

/*!
 * @brief Load a singular clusters value
 *
 * fs: the filesystem object to opperate on
 * buffer: the buffer for the clusters value
 * cluster: the 'index' of the cluster on disk
 */
static int
__fat32_load_cluster(light_fs_t* fs, uint32_t* buffer, uint32_t cluster)
{
  int error;
  fat_private_t* p;

  p = fs->private;
  error = fs->device->f_read(fs->device, buffer, sizeof(*buffer), p->total_usable_sectors + (cluster * sizeof(*buffer)));

  if (error)
    return error;

  /* Mask any fucky bits to comply with FAT32 standards */
  *buffer &= 0x0fffffff;

  return 0;
}

/*!
 * @brief Caches the entire cluster chain at start_cluster into the files private data
 *
 * fs: filesystem lmao
 * file: the file to store the chain to
 * start_cluster: cluster number on disk to start caching
 */
static int
__fat32_cache_cluster_chain(light_fs_t* fs, light_file_t* file, uint32_t start_cluster)
{
  int error;
  size_t length;
  uint32_t buffer;
  fat_file_t* ffile;
  
  if (start_cluster < 0x2 || start_cluster > FAT32_CLUSTER_LIMIT)
    return -1;

  if (!file || !file->private)
    return -2;

  length = 1;
  buffer = start_cluster;
  ffile = file->private;

  if (ffile->cluster_chain)
    return -3;

  /* Count the clusters */
  while (true) {
    error = __fat32_load_cluster(fs, &buffer, buffer);

    if (error)
      return error;

    if (buffer < 0x2 || buffer > FAT32_CLUSTER_LIMIT)
      break;

    length++;
  }

  /* Reset buffer */
  buffer = start_cluster;

  /* Allocate chain */
  ffile->cluster_chain = heap_allocate(length * sizeof(uint32_t));

  /* Set correct lengths */
  ffile->cluster_chain_length = length;

  /* Loop and store the clusters */
  for (uint32_t i = 0; i < length; i++) {
    ffile->cluster_chain[i] = buffer;

    error = __fat32_load_cluster(fs, &buffer, buffer);

    if (error)
      return error;
  }
  
  return 0;
}

static int
__fat32_load_clusters(light_fs_t* fs, void* buffer, fat_file_t* file, uint32_t start, size_t count)
{
  int error;
  uintptr_t current_index;
  uintptr_t current_offset;
  uintptr_t current_deviation;
  uintptr_t current_delta;
  uintptr_t current_cluster_offset;
  uintptr_t index;

  fat_private_t* p;

  if (!fs->private || !file->cluster_chain)
    return -1;

  p = fs->private;

  index = 0;

  while (index < count) {
    current_offset = start + index;
    current_index = current_offset / p->cluster_size;
    current_deviation = current_offset % p->cluster_size;

    current_delta = count - index;

    /* Limit the delta to the size of a cluster, except if we try to get an unaligned size */
    if (current_delta > p->cluster_size - current_deviation)
      current_delta = p->cluster_size - current_deviation;

    current_cluster_offset = (p->usable_clusters_start + (file->cluster_chain[current_index] - 2) * p->bpb.sectors_per_cluster) * p->bpb.bytes_per_sector;

    error = fs->device->f_read(fs->device, buffer + index, current_delta, current_cluster_offset + current_deviation);

    index += current_delta;
  }

  return 0;
}

static int
transform_fat_filename(char* dest, const char* src) 
{

  uint32_t i = 0;
  uint32_t src_dot_idx = 0;

  bool found_ext = false;

  // put spaces
  memset(dest, ' ', 11);

  while (src[src_dot_idx + i]) {
    // limit exceed
    if (i >= 11 || (i >= 8 && !found_ext)) {
      return -1;
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

  return 0;
}

static int 
__fat32_open_dir_entry(light_fs_t* fs, fat_dir_entry_t* current, fat_dir_entry_t* out, char* rel_path)
{
  int error;
  /* We always use this buffer if we don't support lfn */
  char transformed_buffer[11];
  uint32_t cluster_num;

  size_t dir_entries_count;
  size_t clusters_size;
  fat_dir_entry_t* dir_entries;

  fat_private_t* p;
  light_file_t tmp = { 0 };
  fat_file_t tmp_ffile = { 0 };

  if (!out || !rel_path)
    return -1;

  /* Make sure we open a directory, not a file */
  if ((current->dir_attr & FAT_ATTR_DIRECTORY) == 0)
    return -2;

  p = fs->private;
  cluster_num = current->dir_frst_cluster_lo | ((uint32_t)current->dir_frst_cluster_hi << 16);
  tmp.private = &tmp_ffile;

  error = __fat32_cache_cluster_chain(fs, &tmp, cluster_num);

  if (error)
    goto fail_dealloc_cchain;

  clusters_size = tmp_ffile.cluster_chain_length * p->cluster_size;
  dir_entries_count = clusters_size / sizeof(fat_dir_entry_t);

  dir_entries = heap_allocate(clusters_size);

  error = __fat32_load_clusters(fs, dir_entries, &tmp_ffile, 0, clusters_size);

  if (error)
    goto fail_dealloc_dir_entries;

  error = transform_fat_filename(transformed_buffer, rel_path);

  /* Only fail if we don't support lfn here. If this fails while we support lfn, we can just use the rel_path as-is */
  if (error && (p->flags & FAT_FLAG_NO_LFN) == FAT_FLAG_NO_LFN)
    goto fail_dealloc_dir_entries;

  printf("Tried to get dir entries");
  /* Loop over all the directory entries and check if any paths match ours */
  for (uint32_t i = 0; i < dir_entries_count; i++) {
    fat_dir_entry_t entry = dir_entries[i];

    printf((char*)entry.dir_name);

    /* Space kinda cringe */
    if (entry.dir_name[0] == ' ')
      continue;

    /* End */
    if (entry.dir_name[0] == 0x00) {
      error = -3;
      break;
    }

    if (entry.dir_attr == FAT_ATTR_LONG_NAME) {
      /* TODO: support
       */
      error = -4;
      continue;
    } else {

      /* This our file/directory? */
      if (strncmp(transformed_buffer, (const char*)entry.dir_name, 11) == 0) {
        *out = entry;
        error = 0;
        break;
      }
    }
  }

fail_dealloc_dir_entries:
  heap_free(dir_entries);
fail_dealloc_cchain:
  heap_free(tmp_ffile.cluster_chain);
  return error;
}

light_file_t*
fat32_open(light_fs_t* fs, char* path)
{
  /* Include 0x00 byte */
  int error;
  const size_t path_size = strlen(path) + 1;

  if (path_size >= FAT32_MAX_FILENAME_LENGTH)
    return nullptr;

  char path_buffer[path_size];
  fat_private_t* p;
  light_file_t* file = heap_allocate(sizeof(light_file_t));
  fat_file_t* ffile = heap_allocate(sizeof(fat_file_t));

  memset(file, 0, sizeof(*file));
  memset(ffile, 0, sizeof(*ffile));

  memcpy(path_buffer, path, path_size);

  file->private = ffile;
  p = fs->private;

  fat_dir_entry_t current = p->root_entry;

  /* Do opening lmao */

  for (uintptr_t i = 0; i < path_size; i++) {

    /* Stop either at the end, or at any '/' char */
    if (path_buffer[i] != '/' && path_buffer[i] != '\0')
      continue;

    /* Place a null-byte */
    path_buffer[i] = '\0';

    error = __fat32_open_dir_entry(fs, &current, &current, path_buffer);

    if (error)
      goto fail_and_deallocate;

    printf("opening thing");
    /*
     * If we found our file (its not a directory) we can populate the file object and return it
     */
    if ((current.dir_attr & (FAT_ATTR_DIRECTORY|FAT_ATTR_VOLUME_ID)) != FAT_ATTR_DIRECTORY) {

      /* Make sure the file knows about its cluster chain */
      error = __fat32_cache_cluster_chain(fs, file, (current.dir_frst_cluster_lo | (current.dir_frst_cluster_hi << 16)));

      if (error)
        goto fail_and_deallocate;

      file->filesize = current.filesize_bytes;
      file->f_read = __fat32_file_read;
      file->f_readall = __fat32_file_readall;
      file->f_write = __fat32_file_write;
      file->f_close = __fat32_file_close;

      break;
    }

    /*
     * Place back the slash
     */
    path_buffer[i] = '/';
  }

  return file;

fail_and_deallocate:
  heap_free(file);
  heap_free(ffile);
  return nullptr;
}

int 
fat32_close(light_fs_t* fs, light_file_t* file)
{
  /* Deallocate caches */
  /* Deallocate file */
  /* Deallocate */
  return 0;
}

light_fs_t fat32_fs = 
{
  .fs_type = FS_TYPE_FAT32,
  .f_probe = fat32_probe,
  .f_open = fat32_open,
  .f_close = fat32_close,
};

void 
init_fat_fs()
{
  register_fs(&fat32_fs);
}
