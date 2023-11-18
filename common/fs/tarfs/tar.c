#include "file.h"
#include "fs.h"
#include "heap.h"
#include <memory.h>
#include <fs/tarfs/decompress.h>
#include <fs/tarfs/tar.h>

typedef struct tarfs_file_priv {
  void* buffer;
} tarfs_file_priv_t;

typedef struct tarfs_priv {
  void* start;
  size_t size;
  light_file_t* file;
} tarfs_priv_t;

typedef uint8_t* tar_block_start_t;

typedef enum TAR_TYPE {
  TAR_TYPE_FILE = '0',
  TAR_TYPE_HARD_LINK = '1',
  TAR_TYPE_SYMLINK = '2',
  TAR_TYPE_CHAR_DEV = '3',
  TAR_TYPE_BLK_DEV = '4',
  TAR_TYPE_DIR = '5',
  TAR_TYPE_FIFO = '6',
} TAR_TYPE_t;

#define TAR_USTAR_ALIGNMENT 512

static uintptr_t 
decode_tar_ascii(uint8_t* str, size_t length) 
{
  uintptr_t ret = 0;
  uint8_t* c = str;

  /* Itterate to null byte */
  for (uintptr_t i = 0; i < length; i++) {
    ret *= 8;
    ret += *c - '0';
    c++;
  }

  return ret;
}

static uintptr_t 
apply_tar_alignment(uintptr_t val) 
{
  return (((val + TAR_USTAR_ALIGNMENT - 1) / TAR_USTAR_ALIGNMENT) + 1) * TAR_USTAR_ALIGNMENT;
}

static int 
ramfs_read(light_file_t* node, void* buffer, size_t size, uintptr_t offset) 
{
  if (!node || !buffer || !size)
    return -1;

  return 0;
}

static int tar_file_read(light_file_t* file, void* buffer, size_t* size, uintptr_t offset)
{
  size_t target_size;

  if (!file || !buffer || !size)
    return -1;

  if (!(*size))
    return -2;

  target_size = *size;
  *size = 0;

  if (offset >= file->filesize)
    return -3;

  /* Make sure we never read outside of the file */
  if (offset + target_size > file->filesize)
    target_size -= (offset + target_size) - file->filesize;

  /* ->m_buffer points to the actual data inside ramfs, so just copy it out of the ro region */
  memcpy(buffer, (void*)((uint64_t)((tarfs_file_priv_t*)(file->private))->buffer + offset), target_size);

  /* Adjust the read size */
  *size = target_size;

  return 0;
}

int tar_file_close(light_fs_t* fs, light_file_t* file)
{
  /* Nothing to be done */
  return 0;
}

static light_file_t* 
tarfs_find(light_fs_t* fs, char* name) 
{
  tarfs_priv_t* fs_priv;
  tar_file_t current_file = { 0 };
  uintptr_t current_offset = 0;
  size_t filesize;
  size_t name_len = strlen(name);

  tarfs_file_priv_t* priv;
  light_file_t* file;

  if (!name || !name_len)
    return nullptr;

  fs_priv = fs->private;

  while (current_offset < fs_priv->size) {
    memcpy(&current_file, fs_priv->start + current_offset, sizeof(current_file));

    filesize = decode_tar_ascii(current_file.size, 11);

    if (!memcmp(current_file.fn, name, name_len))
      goto next;

    switch (current_file.type) {
      case TAR_TYPE_FILE:
        /* Create file early to catch vnode grabbing issues early */
        priv = heap_allocate(sizeof(tarfs_file_priv_t));

        if (!priv)
          return nullptr;

        file = heap_allocate(sizeof(light_file_t));

        if (!file) {
          heap_free(priv);
          return nullptr;
        }

        memset(file, 0, sizeof(*file));

        file->private = priv;
        file->parent_fs = fs;
        file->filesize = filesize;

        priv->buffer = (void*)((uintptr_t)fs_priv->start + current_offset + TAR_USTAR_ALIGNMENT);

        return file;
      default:
        return nullptr;
    }

next:
    current_offset += apply_tar_alignment(filesize);
  }

  return nullptr;
}


light_fs_t* 
create_tarfs(light_file_t* file)
{
  tarfs_priv_t* priv = NULL;
  light_fs_t* fs = NULL;
  void* file_buffer = NULL;
  size_t decomp_size = NULL;

  file_buffer = heap_allocate(file->filesize);

  if (!file_buffer)
    return nullptr;

  priv = heap_allocate(sizeof(*priv));

  if (!priv)
    goto dealloc_and_exit;

  fs = heap_allocate(sizeof(*fs));

  if (!fs)
    goto dealloc_and_exit;

  memset(fs, 0, sizeof(*fs));
  memset(priv, 0, sizeof(*priv));

  priv->file = file;

  fs->device = nullptr;
  fs->next = nullptr;
  fs->private = priv;

  fs->f_open = tarfs_find;
  fs->f_close = tar_file_close;

  /* When a filesystems type is tarfs, we can assume the priv field to be a file */
  fs->fs_type = FS_TYPE_TARFS;

  /* Read the conents of the file into our nice buffer */
  file->f_readall(file, file_buffer);

  /* Is enforcing success here a good idea? */
  decomp_size = tarfs_decompress(file_buffer, file->filesize, &priv->start);

  if (!decomp_size)
    goto dealloc_and_exit;

  heap_free(file_buffer);

  priv->size = decomp_size;

  return fs;

dealloc_and_exit:
  if (fs)
    heap_free(fs);
  if (priv)
    heap_free(priv);
  if (file_buffer)
    heap_free(file_buffer);

  return nullptr;
}

