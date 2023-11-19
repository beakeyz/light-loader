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

static int __fat32_load_clusters(light_fs_t* fs, void* buffer, fat_file_t* file, uint32_t start, size_t count);
static int __fat32_load_cluster(light_fs_t* fs, uint32_t* buffer, uint32_t cluster);

typedef struct fat_private {
  fat_bpb_t bpb;
  uint32_t root_directory_sectors;
  uint32_t total_reserved_sectors;
  uint32_t total_usable_sectors;
  uint32_t first_cluster_offset;
  uint32_t cluster_count;
  uint32_t cluster_size;
  uintptr_t usable_clusters_start;
  fat_dir_entry_t root_entry;

  uint32_t flags;
} fat_private_t;

int 
fat32_probe(light_fs_t* fs, disk_dev_t* device)
{
  int error;
  fat_private_t* private;
  fat_bpb_t bpb = { 0 };

  error = device->f_read(device, &bpb, sizeof(bpb), 0);

  if (error)
    return error;

  // address for the identifier on fat32
  const char* fat32_ident = (((void*)&bpb) + 0x52);

  // address for the identifier on fat32 but fancy
  const char* fat32_ident_fancy = (((void*)&bpb) + 0x03);

  if (strncmp(fat32_ident, "FAT", 3) != 0 && strncmp(fat32_ident_fancy, "FAT32", 5) != 0)
    return -1;

  if (!bpb.bytes_per_sector)
    return -2;

  private = heap_allocate(sizeof(fat_private_t));

  memset(private, 0, sizeof(fat_private_t));

  private->root_directory_sectors = (bpb.root_entries_count * 32 + (bpb.bytes_per_sector - 1)) / bpb.bytes_per_sector;
  private->total_reserved_sectors = bpb.reserved_sector_count + (bpb.fat_num * bpb.sectors_num_per_fat) + private->root_directory_sectors;
  private->total_usable_sectors = bpb.sector_num_fat32 - private->total_reserved_sectors;
  private->cluster_count = private->total_usable_sectors / bpb.sectors_per_cluster;
  private->cluster_size = bpb.sectors_per_cluster * bpb.bytes_per_sector;
  private->first_cluster_offset = bpb.reserved_sector_count * bpb.bytes_per_sector;
  private->usable_clusters_start = (bpb.reserved_sector_count + (bpb.fat_num * bpb.sectors_num_per_fat));

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

  if (cluster > p->cluster_count)
    return -1;

  error = fs->device->f_read(fs->device, buffer, sizeof(*buffer), p->first_cluster_offset + (cluster * sizeof(*buffer)));

  if (error)
    return error;

  /* Mask any fucky bits to comply with FAT32 standards */
  *buffer &= 0x0fffffff;

  return 0;
}

/*!
 * @brief: Set the value of a cluster in the FAT
 */
static int
__fat32_set_cluster(light_fs_t* fs, uint32_t value, uint32_t cluster)
{
  int error;
  fat_private_t* p;

  p = fs->private;

  if (cluster > p->cluster_count)
    return -1;

  /* Mask any fucky bits to comply with FAT32 standards */
  value &= 0x0fffffff;

  return fs->device->f_write(fs->device, &value, sizeof(value), p->first_cluster_offset + (cluster * sizeof(value)));
}

/*!
 * @brief: Read one entire cluster into a local buffer
 */
static int
__fat32_read_cluster(light_fs_t* fs, void* buffer, uint32_t cluster)
{
  int error;
  uintptr_t current_cluster_offset;

  fat_private_t* p;

  if (!fs || !fs->private || cluster < 2)
    return -1;

  p = fs->private;
  current_cluster_offset = (p->usable_clusters_start + (cluster - 2) * p->bpb.sectors_per_cluster) * p->bpb.bytes_per_sector;
  error = fs->device->f_read(fs->device, buffer, p->cluster_size, current_cluster_offset);

  if (error)
    return -2;

  return 0;
}

/*!
 * @brief: Write one entrie cluster to disk
 * 
 * The size of @buffer must be at least ->cluster_size 
 * this size is assumed
 */
static int
__fat32_write_cluster(light_fs_t* fs, void* buffer, uint32_t cluster)
{
  int error;
  uintptr_t current_cluster_offset;

  fat_private_t* p;

  if (!fs || !fs->private || cluster < 2)
    return -1;

  p = fs->private;
  current_cluster_offset = (p->usable_clusters_start + (cluster - 2) * p->bpb.sectors_per_cluster) * p->bpb.bytes_per_sector;
  error = fs->device->f_write(fs->device, buffer, p->cluster_size, current_cluster_offset);

  if (error)
    return -2;

  return 0;
}

/*!
 * @brief: Finds the index of a free cluster
 *
 * @returns: 0 on success, anything else on failure (TODO: real ierror API)
 * @buffer: buffer where the index gets put into
 */
static int
__fat32_find_free_cluster(light_fs_t* fs, uint32_t* buffer)
{
  int error;
  uint32_t current_index;
  uint32_t cluster_buff;
  fat_private_t* p;

  p = fs->private;
  /* Start at index 2, cuz the first 2 clusters are weird lol */
  current_index = 2;
  cluster_buff = 0;

  while (current_index < p->cluster_count) {

    /* Try to load the value of the cluster at our current index into the value buffer */
    error = __fat32_load_cluster(fs, &cluster_buff, current_index);

    if (error)
      return error;

    /* Zero in a cluster index means its free / unused */
    if (cluster_buff == 0)
      break;

    current_index++;
  }

  /* Could not find a free cluster, WTF */
  if (current_index >= p->cluster_count)
    return -1;

  /* Place the index into the buffer */
  *buffer = current_index;

  return 0;
}

static uint32_t __fat32_dir_entry_get_start_cluster(fat_dir_entry_t* e)
{
  return ((uint32_t)e->dir_frst_cluster_lo & 0xffff) | ((uint32_t)e->dir_frst_cluster_hi << 16);
}

static uint32_t __fat32_file_get_start_cluster(fat_file_t* ff)
{
  if (!ff->cluster_chain)
    return 0;

  return ff->cluster_chain[0];
}

/*!
 * @brief: Find the last cluster of a fat directory entry
 *
 * @returns: 0 on success, anything else on failure
 */
static int 
__fat32_find_end_cluster(light_fs_t* fs, uint32_t start_cluster, uint32_t* buffer)
{
  int error;
  fat_private_t* p;
  uint32_t previous_value;
  uint32_t value_buffer;

  p = fs->private;
  value_buffer = start_cluster;
  previous_value = value_buffer;

  while (true) {
    /* Load one cluster */
    error = __fat32_load_cluster(fs, &value_buffer, value_buffer);

    if (error)
      return error;

    /* Reached end? */
    if (value_buffer < 0x2 || value_buffer >= FAT32_CLUSTER_LIMIT)
      break;

    previous_value = value_buffer;
  }

  /* previous_value holds the index to the last cluster at this point. Ez pz */
  *buffer = previous_value;

  return 0;
}

/*!
 * @brief: Add a new directory entry in a directory
 *
 * This function checks to see if the specified 'directory' is also considered a directory
 * by FAT32 standards
 *
 * We simply find the last dir entry inside this directory and see if we have space in this cluster to append another
 * entry. If not, we'll expand the cluster chain by one in order to fit our new entry
 *
 * TODO: test this routine
 */
static int 
__fat32_dir_append_entry(light_fs_t* fs, fat_dir_entry_t* dir, fat_dir_entry_t* entry)
{
  int error;
  fat_private_t* p;
  void* first_data_buffer;
  uint32_t first_cluster;
  uint32_t last_cluster;
  uint32_t current_dir_entry_idx;
  uint32_t potential_dir_entry_count;
  fat_dir_entry_t* current_dir_entry;

  if (!dir || (dir->dir_attr & FAT_ATTR_DIRECTORY) != FAT_ATTR_DIRECTORY)
    return -1;

  p = fs->private;

  error = __fat32_find_end_cluster(fs, __fat32_dir_entry_get_start_cluster(dir), &last_cluster);

  if (error)
    return -2;

  /* Yay, we found the last data cluster. Lets cache it rq (Should prob allocate on the heap to avoid stack smashing) */
  uint8_t cluster_buffer[p->cluster_size];

  error = __fat32_read_cluster(fs, &cluster_buffer, last_cluster);

  if (error)
    return error;

  current_dir_entry_idx = 0;
  potential_dir_entry_count = p->cluster_size / sizeof(fat_dir_entry_t);

  /* Loop over the dir entries in this cluster until we find one without a name (meaning that one marks the end) */
  while (current_dir_entry_idx < potential_dir_entry_count) {

    current_dir_entry = (fat_dir_entry_t*)(&cluster_buffer[current_dir_entry_idx * sizeof(fat_dir_entry_t)]);

    if (current_dir_entry->dir_name[0] == 0x00)
      break;

    current_dir_entry_idx++;
  }

  first_cluster = NULL;
  error = __fat32_find_free_cluster(fs, &first_cluster);

  if (error)
    return error;

  /* This buffer will contain the first cluster for our new dir entry */
  first_data_buffer = heap_allocate(p->cluster_size);
  memset(first_data_buffer, 0, p->cluster_size);

  /* Add some data to this mofo */
  if ((entry->dir_attr & FAT_ATTR_DIRECTORY) == FAT_ATTR_DIRECTORY) {
    /* Take the buffer */
    fat_dir_entry_t* new_cluster_buffer = (fat_dir_entry_t*)first_data_buffer;

    /* Set the current dir entry */
    new_cluster_buffer[0].dir_attr = FAT_ATTR_DIRECTORY;
    memset(&new_cluster_buffer[0].dir_name, ' ', 11);
    new_cluster_buffer[0].dir_name[0] = '.';
    new_cluster_buffer[0].filesize_bytes = 0;
    new_cluster_buffer[0].dir_frst_cluster_lo = first_cluster & 0xffff;
    new_cluster_buffer[0].dir_frst_cluster_hi = (first_cluster >> 16) & 0xffff;

    /* Set the previous dir entry */
    new_cluster_buffer[1].dir_attr = FAT_ATTR_DIRECTORY;
    memset(&new_cluster_buffer[1].dir_name, ' ', 11);
    new_cluster_buffer[1].dir_name[0] = '.';
    new_cluster_buffer[1].dir_name[1] = '.';
    new_cluster_buffer[1].filesize_bytes = 0;
    new_cluster_buffer[1].dir_frst_cluster_lo = dir->dir_frst_cluster_lo;
    new_cluster_buffer[1].dir_frst_cluster_hi = dir->dir_frst_cluster_hi;
    entry->filesize_bytes = 0;
  } else {
    /* Set the filesize for a generic file */
    entry->filesize_bytes = 0;
  }

  /* Write our initial data into this cluster */
  error = __fat32_write_cluster(fs, first_data_buffer, first_cluster);

  /* Free the buffer after we're done with it */
  heap_free(first_data_buffer);
  
  if (error)
    return error;

  /* Make sure the first data buffer is also marked as the last, since there is currently only one buffer */
  error = __fat32_set_cluster(fs, FAT32_CLUSTER_EOF, first_cluster);

  if (error)
    return error;

  entry->dir_frst_cluster_lo = first_cluster & 0xffff;
  entry->dir_frst_cluster_hi = (first_cluster >> 16) & 0xffff;

  /*
   * Can we steal from this cluster or do we have to append a new one?
   */
  if (current_dir_entry_idx < potential_dir_entry_count) {
    /* Copy the entry into our cluster */
    memcpy(current_dir_entry, entry, sizeof(fat_dir_entry_t));

    /* (TODO) Write the cluster back to disk (and sync?) */
    error = __fat32_write_cluster(fs, cluster_buffer, last_cluster);
  } else {
    printf("FUCK we have to allocate a new cluster for this fucko");
    for (;;) {}
    return -5;
  }

  return error;
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
  
  if (start_cluster < 0x2 || start_cluster >= FAT32_CLUSTER_LIMIT)
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

    if (buffer < 0x2 || buffer >= FAT32_CLUSTER_LIMIT)
      break;

    length++;
  }

  /* Set correct lengths */
  ffile->cluster_chain_length = length;

  /* Files can have a length of zero? */
  if (!length)
    return 0;

  /* Reset buffer */
  buffer = start_cluster;

  /* Allocate chain */
  ffile->cluster_chain = heap_allocate(length * sizeof(uint32_t));

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

  if (!fs || !fs->private || !file || !file->cluster_chain)
    return -1;

  p = fs->private;

  index = 0;

  while (index < count) {
    current_offset = start + index;
    current_index = current_offset / p->cluster_size;
    current_deviation = current_offset % p->cluster_size;

    current_delta = count - index;

    /* Limit the delta to the size of a cluster, except if we try to get an unaligned size smaller than 'cluster_size' */
    if (current_delta > p->cluster_size - current_deviation)
      current_delta = p->cluster_size - current_deviation;

    current_cluster_offset = (p->usable_clusters_start + (file->cluster_chain[current_index] - 2) * p->bpb.sectors_per_cluster) * p->bpb.bytes_per_sector;

    error = fs->device->f_read(fs->device, buffer + index, current_delta, current_cluster_offset + current_deviation);

    if (error)
      return -2;

    index += current_delta;
  }

  return 0;
}

/*!
 * @brief: Transform a regular path into a FAT compliant path
 *
 * @returns: Wether we found a file extention or not
 */
static bool
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
      return found_ext;
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

  return found_ext;
}

static int 
__fat32_open_dir_entry(light_fs_t* fs, fat_dir_entry_t* current, fat_dir_entry_t* out, char* rel_path, uint32_t* offset)
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

  /* Dir entries can't have a chain length of zero here lol */
  if (error || !tmp_ffile.cluster_chain_length)
    goto fail_dealloc_cchain;

  clusters_size = tmp_ffile.cluster_chain_length * p->cluster_size;
  dir_entries_count = clusters_size / sizeof(fat_dir_entry_t);

  dir_entries = heap_allocate(clusters_size);

  error = __fat32_load_clusters(fs, dir_entries, &tmp_ffile, 0, clusters_size);

  if (error)
    goto fail_dealloc_dir_entries;

  (void)transform_fat_filename(transformed_buffer, rel_path);

  /* Loop over all the directory entries and check if any paths match ours */
  for (uint32_t i = 0; i < dir_entries_count; i++) {
    fat_dir_entry_t entry = dir_entries[i];

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
        if (offset)
          *offset = i * sizeof(fat_dir_entry_t);
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

static int
__fat32_file_write(struct light_file* file, void* buffer, size_t size, uintptr_t offset)
{
  int error;
  bool did_overflow;
  uint32_t max_offset;
  uint32_t overflow_delta;
  uint32_t overflow_clusters;
  uint32_t write_clusters;
  uint32_t write_start_cluster;
  uint32_t start_cluster_index;
  uint32_t cluster_internal_offset;
  uint32_t file_start_cluster;
  uint32_t file_end_cluster;
  fat_private_t* f_priv;
  fat_file_t* ffile;

  ffile = file->private;

  if (!ffile)
    return -1;

  f_priv = file->parent_fs->private;
  max_offset = ffile->cluster_chain_length * f_priv->cluster_size - 1;
  overflow_delta = 0;

  if (offset + size > max_offset)
    overflow_delta = (offset + size) - max_offset;

  did_overflow = false;

  /* Calculate how many clusters we might need to allocate extra */
  overflow_clusters = overflow_delta / f_priv->cluster_size;
  /* Calculate in how many clusters we need to write */
  write_clusters = ALIGN_UP(size, f_priv->cluster_size) / f_priv->cluster_size;
  /* Calculate the first cluster where we need to write */
  write_start_cluster = ALIGN_DOWN(offset, f_priv->cluster_size) / f_priv->cluster_size;
  /* Calculate the start offset within the first cluster */
  cluster_internal_offset = offset % f_priv->cluster_size;
  /* Find the starting cluster */
  file_start_cluster = __fat32_file_get_start_cluster(ffile);

  /* Find the last cluster of this file */
  error = __fat32_find_end_cluster(file->parent_fs, file_start_cluster, &file_end_cluster);

  if (error)
    return error;

  uint32_t next_free_cluster;

  /* Allocate the overflow clusters */
  while (overflow_clusters) {
    next_free_cluster = NULL;

    /* Find a free cluster */
    error = __fat32_find_free_cluster(file->parent_fs, &next_free_cluster);

    if (error)
      return error;

    /* Add this cluster to the end of this files chain */
    __fat32_set_cluster(file->parent_fs, next_free_cluster, file_end_cluster);
    __fat32_set_cluster(file->parent_fs, FAT32_CLUSTER_EOF, next_free_cluster);

    did_overflow = true;
    file_end_cluster = next_free_cluster;
    overflow_clusters--;
  }

  /* Only re-cache the cluster chain if we made changes to the clusters */
  if (did_overflow) {
    /* Reset cluster_chain data for this file */
    heap_free(ffile->cluster_chain);
    ffile->cluster_chain = NULL;
    ffile->cluster_chain_length = NULL;

    /* Re-cache the cluster chain */
    error = __fat32_cache_cluster_chain(file->parent_fs, file, file_start_cluster);

    if (error)
      return error;

    file->filesize = ffile->cluster_chain_length * f_priv->cluster_size;
  }

  /*
   * We can now perform a nice write opperation on this files data
   */
  start_cluster_index = write_start_cluster / ffile->cluster_chain_length;
  uint8_t* cluster_buffer = heap_allocate(f_priv->cluster_size);

  /* Current offset: the offset into the current cluster */
  uint32_t current_offset = cluster_internal_offset;
  /* Current delta: how much have we already written */
  uint32_t current_delta = 0;
  /* Write size: how many bytes are in this write */
  uint32_t write_size;
  
  /* FIXME: this is prob slow asf. We could be smarter about our I/O */
  for (uint32_t i = 0; i < write_clusters; i++) {

    if (current_delta >= size)
      break;

    /* This is the cluster we need to write shit to */
    uint32_t this_cluster = ffile->cluster_chain[start_cluster_index+i];

    /* Figure out the write size */
    write_size = size - current_delta;

    /* Write size exceeds the size of our clusters */
    if (write_size > f_priv->cluster_size - current_offset)
      write_size = f_priv->cluster_size - current_offset;

    /* Read the current content into our buffer here */
    __fat32_read_cluster(file->parent_fs, cluster_buffer, this_cluster);

    /* Copy the bytes over */
    memcpy(&cluster_buffer[current_offset], &buffer[current_delta], write_size);

    /* And write back the changes we've made */
    __fat32_write_cluster(file->parent_fs, cluster_buffer, this_cluster);

    current_delta += write_size;
    /* After the first write, any subsequent write will always start at the start of a cluster */
    current_offset = 0;
  }

  fat_dir_entry_t* dir_entry = (fat_dir_entry_t*)((uintptr_t)cluster_buffer + ffile->direntry_cluster_offset);

  if (__fat32_read_cluster(file->parent_fs, cluster_buffer, ffile->direntry_cluster))
    goto free_and_exit;

  /* Add the size to this file */
  dir_entry->filesize_bytes += size;

  __fat32_write_cluster(file->parent_fs, cluster_buffer, ffile->direntry_cluster);

free_and_exit:
  heap_free(cluster_buffer);
  return 0;
}

static int 
__fat32_file_read(struct light_file* file, void* buffer, size_t size, uintptr_t offset)
{
  /* Yay, raw reads */
  return __fat32_load_clusters(file->parent_fs, buffer, file->private, offset, size);
}

static int 
__fat32_file_readall(struct light_file* file, void* buffer)
{
  size_t total_size;
  fat_private_t* p;
  fat_file_t* ffile = file->private;

  if (!ffile)
    return 1;

  p = file->parent_fs->private;
  total_size = ffile->cluster_chain_length * p->cluster_size;

  return __fat32_load_clusters(file->parent_fs, buffer, ffile, 0, total_size);
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
  file->parent_fs = fs;
  p = fs->private;

  uintptr_t current_idx = 0;
  fat_dir_entry_t current = p->root_entry;

  /* Do opening lmao */

  for (uintptr_t i = 0; i < path_size; i++) {

    /* Stop either at the end, or at any '/' char */
    if (path_buffer[i] != '/' && path_buffer[i] != '\0')
      continue;

    /* Place a null-byte */
    path_buffer[i] = '\0';

    ffile->direntry_cluster = __fat32_dir_entry_get_start_cluster(&current); 

    /* NOTE: Offset is the offset from the first cluster of a dir entry */
    error = __fat32_open_dir_entry(fs, &current, &current, &path_buffer[current_idx], &ffile->direntry_cluster_offset);

    ffile->direntry_cluster += (ffile->direntry_cluster_offset) / p->cluster_size;
    ffile->direntry_cluster_offset %= p->cluster_size;

    if (error)
      goto fail_and_deallocate;

    /*
     * If we found our file (its not a directory) we can populate the file object and return it
     */
    if ((current.dir_attr & (FAT_ATTR_DIRECTORY|FAT_ATTR_VOLUME_ID)) != FAT_ATTR_DIRECTORY) { 

      /* Make sure the file knows about its cluster chain */
      error = __fat32_cache_cluster_chain(fs, file, __fat32_dir_entry_get_start_cluster(&current));

      /* Only if we failed to read a file with bytes lmao */
      if (error && current.filesize_bytes)
        goto fail_and_deallocate;

      file->filesize = current.filesize_bytes;
      file->f_read = __fat32_file_read;
      file->f_readall = __fat32_file_readall;
      file->f_write = __fat32_file_write;
      file->f_close = __fat32_file_close;

      break;
    }

    /* Set the current index if we have successfuly 'switched' directories */
    current_idx = i + 1;

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

static int
fat32_create_path(light_fs_t* fs, const char* path)
{
  /* Include 0x00 byte */
  int error;
  bool is_dir;
  const size_t path_size = strlen(path) + 1;

  if (path_size >= FAT32_MAX_FILENAME_LENGTH)
    return -3;

  char path_buffer[path_size];
  fat_private_t* p;

  memcpy(path_buffer, path, path_size);

  p = fs->private;
  is_dir = false;

  uintptr_t current_idx = 0;
  fat_dir_entry_t current = p->root_entry;

  /* Do opening lmao */
  for (uintptr_t i = 0; i < path_size; i++) {

    /* Stop either at the end, or at any '/' char */
    if (path_buffer[i] != '/' && path_buffer[i] != '\0')
      continue;

    /* We are working with a directory! */
    if (path_buffer[i] == '/')
      is_dir = true;

    /* Place a null-byte */
    path_buffer[i] = '\0';

    /* Keep opening shit until we fail lmao */
    error = __fat32_open_dir_entry(fs, &current, &current, &path_buffer[current_idx], NULL);

    /* yay, this one does not exist! */
    if (error) {

      /* We're in a directory right now, so let's just copy the attributes of our bby */
      fat_dir_entry_t new_entry = { 0 };

      (void)transform_fat_filename((char*)new_entry.dir_name, &path_buffer[current_idx]);
      //memset(new_entry.dir_name, ' ', sizeof new_entry.dir_name);
      //memcpy(new_entry.dir_name, &path_buffer[current_idx], sizeof new_entry.dir_name);

      if (is_dir)
        new_entry.dir_attr |= FAT_ATTR_DIRECTORY;

      error = __fat32_dir_append_entry(fs, &current, &new_entry);

      if (error)
        return error;
      
      /* Copy the 'new_entry' data into the current entry */
      current = new_entry;
    }

    /* Set the current index if we have successfuly 'switched' directories */
    current_idx = i + 1;

    /*
     * Place back the slash
     */
    path_buffer[i] = '/';

    /* Make sure we don't believe everything is a dir */
    is_dir = false;
  }

  return 0;
}

int 
fat32_close(light_fs_t* fs, light_file_t* file)
{
  fat_file_t* ff;

  ff = file->private;

  /* Deallocate caches */
  heap_free(ff->cluster_chain);
  /* Deallocate file */
  heap_free(ff);
  /* Deallocate */
  heap_free(file);
  return 0;
}

/*
 * Straight from the Microsoft FAT spec
 * at: https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf
 */
struct disksize_clustercount_mapping {
  /* Disk size in 512 byte sectors */
  uint32_t DiskSize;
  /* Sectors per FAT32 cluster */
  uint8_t SecPerClusVal;
};

/* This too xD */
static struct disksize_clustercount_mapping DskTableFAT32 [] = {
  {66600, 0}, /* disks up to 32.5 MB, the 0 value for SecPerClusVal trips an error */
  {532480, 1}, /* disks up to 260 MB, .5k cluster */
  {16777216, 8}, /* disks up to 8 GB, 4k cluster */
  {33554432, 16}, /* disks up to 16 GB, 8k cluster */
  {67108864, 32}, /* disks up to 32 GB, 16k cluster */
  {0xFFFFFFFF, 64}/* disks greater than 32GB, 32k cluster */
};
static uint32_t dsk_table_mapping_count = sizeof(DskTableFAT32) / sizeof(*DskTableFAT32);

/*!
 * @brief: 
 * TODO:
 */
int 
fat32_install(light_fs_t* fs, disk_dev_t* device)
{
  uint32_t sectors_per_cluster;
  uint32_t sector_size;
  uint32_t cluster_count;
  uint32_t fat_size_sectors;
  uintptr_t current_disksize;
  fat_bpb_t bpb;
  void* fat;
  fat_dir_entry_t root_entry;

  memset(&bpb, 0, sizeof(bpb));
  memset(&root_entry, 0, sizeof(root_entry));

  sector_size = 512;

  /* Write both identifiers, just for fun */
  memcpy("FAT32 ", bpb.system_type, 7);
  memcpy("FAT32", bpb.oem_name, 6);
  memcpy("FAT", bpb.reserved, 4);

  /* Scan for the best sector -> cluster ratio */
  for (uint32_t i = 0; i < dsk_table_mapping_count; i++) {
    /* NOTE: Make sure DiskSize is 64-bit */
    current_disksize = (uintptr_t)DskTableFAT32[i].DiskSize * sector_size;

    /* If we've reached the last mapping, we'll just use that */
    if (device->total_size > current_disksize && i+1 != dsk_table_mapping_count)
      continue;
    
    bpb.sectors_per_cluster = DskTableFAT32[i].SecPerClusVal;
    break;
  }

  /* Fuck, could not find a mapping =/ */
  if (!bpb.sectors_per_cluster)
    return -1;

  bpb.bytes_per_sector = sector_size;
  /* TODO: make sure 2+Tib devices don't fuck us here */
  bpb.sector_num_fat32 = device->total_size / sector_size;
  /* NOTE: For FAT32 this is zero, but FAT12 and FAT16 do use this field */
  bpb.root_entries_count = 0;

  cluster_count = bpb.sector_num_fat32 / bpb.sectors_per_cluster;
  /* TODO: recursively do this calculation to minimize wasted space */
  fat_size_sectors = (cluster_count * sizeof(uint32_t)) / sector_size;

  bpb.fat_num = 1;
  bpb.sectors_num_per_fat = fat_size_sectors;
  /* ??? */
  bpb.reserved_sector_count = bpb.sectors_per_cluster;
  bpb.root_cluster = 2;
  /* Disable FAT mirroring and enable the 0th FAT */
  bpb.ext_flags = 0;
  bpb.ext_flags |= (1 << 7);

  if ((device->flags & DISK_FLAG_REMOVABLE) == DISK_FLAG_REMOVABLE)
    bpb.media_type = 0xF0;
  else 
    bpb.media_type = 0xF8;

  bpb.fs_version = 0;
  bpb.fs_info_sector = 1;
  /* Make bios happy */
  bpb.content[510] = 0x55;
  bpb.content[511] = 0xAA;

  /*
  private->root_directory_sectors = (bpb.root_entries_count * 32 + (bpb.bytes_per_sector - 1)) / bpb.bytes_per_sector;
  private->total_reserved_sectors = bpb.reserved_sector_count + (bpb.fat_num * bpb.sectors_num_per_fat) + private->root_directory_sectors;
  private->total_usable_sectors = bpb.sector_num_fat32 - private->total_reserved_sectors;
  private->cluster_count = private->total_usable_sectors / bpb.sectors_per_cluster;
  private->cluster_size = bpb.sectors_per_cluster * bpb.bytes_per_sector;
  private->first_cluster_offset = bpb.reserved_sector_count * bpb.bytes_per_sector;
  private->usable_clusters_start = (bpb.reserved_sector_count + (bpb.fat_num * bpb.sectors_num_per_fat));
  */

  /* TODO: zero FATs */
  uint32_t cluster_size = bpb.sectors_per_cluster * bpb.bytes_per_sector;
  uint32_t fats_size = fat_size_sectors * bpb.bytes_per_sector;

  /* Clear the FATs */
  for (uint32_t i = 0; i < bpb.fat_num; i++)
    device->f_write_zero(device, fats_size, (bpb.reserved_sector_count * bpb.bytes_per_sector) + (i * fats_size));

  /* Mask any fucky bits to comply with FAT32 standards */
  uint32_t eof_value = FAT32_CLUSTER_EOF & 0x0fffffff;

  /* Make sure the root dir entry does not die lol */
  device->f_write(device, &eof_value, sizeof(eof_value), (bpb.reserved_sector_count * bpb.bytes_per_sector) + (bpb.root_cluster * sizeof(eof_value)));

  /* Reallocate in order to clear the root cluster */
  uint8_t* zeros = heap_allocate(cluster_size);
  memset(zeros, 0, cluster_size);

  /* Make sure there is nothing at cluster 2 */
  uint32_t current_cluster_offset = (bpb.reserved_sector_count + (bpb.fat_num * bpb.sectors_num_per_fat)) * bpb.bytes_per_sector;
  device->f_write(device, zeros, cluster_size, current_cluster_offset);

  heap_free(zeros);

  /*
   * What to do:
   * - Create a bpb for this filesystem
   * - Calculate a nice cluster size for this device
   * - Create the FATs
   * - Create a root directory entry
   */

  /* Write the bpb to the first sector of the device */
  return device->f_write(device, &bpb, sizeof(bpb), 0);
}

light_fs_t fat32_fs = 
{
  .fs_type = FS_TYPE_FAT32,
  .f_probe = fat32_probe,
  .f_open = fat32_open,
  .f_close = fat32_close,
  .f_create_path = fat32_create_path,
  .f_install = fat32_install,
};

void 
init_fat_fs()
{
  register_fs(&fat32_fs);
}
