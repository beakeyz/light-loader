/*
 * Decompression stuff taken right from the aniva kernel
 */

#include "ctx.h"
#include "efilib.h"
#include "heap.h"
#include <stddef.h>
#include <memory.h>
#include <fs/tarfs/decompress.h>

#define HUFFMAN_CACHE_SIZE 0x8000
#define HUFFMAN_CACHE_MASK 0x7FFF

struct huffman_cache {
  /*
   * The RFC 1951 reference specifies a maximum distance of 32K bytes. This
   * is subsequently how far we need to be able to 'look back' into our stuff.
   */
  uintptr_t cache_ptr;
  uint8_t cache[HUFFMAN_CACHE_SIZE];
};

typedef struct {
  uintptr_t m_start_addr;
  uintptr_t m_end_addr;
  uint8_t* m_current; /* Itterator pointer in the compressed data */
  uint8_t* m_out; /* Buffer to our uncompressed data */
  uint8_t* m_current_out; /* Itterator in our uncompressed buffer */
  /* Buffers we will use to read bits */
  uint32_t m_bit_buffer;
  uint32_t m_bit_buffer_bits_count;

  struct huffman_cache* m_cache;
} decompress_ctx_t;

struct gzip_compressed_header {
  uint8_t compression_method;
  uint8_t flags;
  uint32_t mtime;
  uint8_t xflags;
  uint8_t os;
  uint16_t extra_bytes;
  char* name;
  char* comment;
  uint16_t crc16; /* CRC at the beginning of the compressed block */
  uint32_t crc32; /* CRC at the end of the compressed block */
};

#define MAX_NAME_LEN    128
#define MAX_COMMENT_LEN 256

struct huffman_table {
  uint16_t counts[16];
  uint16_t alphabet[288];
};

static int handle_non_compressed(decompress_ctx_t* ctx);
static int huffman_inflate(decompress_ctx_t* ctx, struct huffman_table* lengths, struct huffman_table* dists);
static int dynamic_huffman_inflate(decompress_ctx_t* ctx);
static int generic_inflate(decompress_ctx_t* ctx);
static void c_write(decompress_ctx_t* ctx, uint8_t data);
static void c_write_from(decompress_ctx_t* ctx, uint32_t offset);

/*
 * These tables contain the distances and lengths that we use to 
 * decode the huffman encoded huffman trees (Yea, mega funky), or
 * formally, the blocks which are compressed using fixed huffman coding.
 */
struct huffman_table fixed_lengths = { 0 };
struct huffman_table fixed_distances = { 0 };
bool fixed_tables_built = false;

/* TODO: Find a place to dynamically allocate for this cache */
/* another TODO: just implement a kernel cache manager where we can create such cache -_- */
struct huffman_cache default_cache = { 0, { 0 } };

static void 
fill_huffman_table(struct huffman_table* table, const uint8_t* lengths, size_t size) 
{
  uint16_t offsets[16] = { 0 };
  uint32_t count = 0;

  /* Zero */
  memset(table, 0, sizeof(struct huffman_table));

  for (size_t i = 0; i < size; i++) {
      table->counts[lengths[i]]++; 
  }

  table->counts[0] = 0;

  /* NOTE: Dangerous assert */
  // ASSERT_MSG(table->counts[0] == 0, "Huffman table count missmatch! (counts[0] != 0)");

  for (uint8_t i = 0; i < 16; i++) {
    offsets[i] = count;
    count += table->counts[i];
  }

  for (size_t i = 0; i < size; i++) {
    if (lengths[i])
      table->alphabet[offsets[lengths[i]]++] = i;
  }
}

static void 
build_fixed_tables() 
{

  if (fixed_tables_built)
    return;

  fixed_tables_built = true;

  const uint16_t fixed_lengths_size = 288;
  const uint8_t fixed_distances_size = 30;

  uint8_t l[fixed_lengths_size];
  for (uint16_t i = 0; i < 144; i++) {
    l[i] = 8;
  }
  for (uint16_t i = 144; i < 256; i++) {
    l[i] = 9;
  }
  for (uint16_t i = 256; i < 280; i++) {
    l[i] = 7;
  }
  for (uint16_t i = 280; i < fixed_lengths_size; i++) {
    l[i] = 8;
  }

  fill_huffman_table(&fixed_lengths, l, fixed_lengths_size);

  for (uint32_t i = 0; i < fixed_distances_size; i++) {
    l[i] = 5;
  }

  fill_huffman_table(&fixed_distances, l, fixed_distances_size);
}

static void 
destroy_gzip_header(struct gzip_compressed_header* header) 
{
  heap_free(header->comment);
  heap_free(header->name);
}

static void 
c_reset_bit_buffer(decompress_ctx_t* ctx) 
{
  ctx->m_bit_buffer = 0;
  ctx->m_bit_buffer_bits_count = 0;
}

/*
 * When we want to read a byte, we need to reset the bit buffer in order to preserve alignment
 */
static inline uint8_t 
c_read(decompress_ctx_t* ctx) 
{
  if ((uintptr_t)ctx->m_current > ctx->m_end_addr)
    return 0;

  return *ctx->m_current++;
}

/*
 * Write our uncompressed data, one byte at a time
 */
static void 
c_write(decompress_ctx_t* ctx, uint8_t data) 
{
  /* Ensure the pointer stays inbetween the bounds of our buffer */
  ctx->m_cache->cache[ctx->m_cache->cache_ptr++] = data;
  ctx->m_cache->cache_ptr &= HUFFMAN_CACHE_MASK;

  *ctx->m_current_out++ = data;
}

static void 
c_write_from(decompress_ctx_t* ctx, uint32_t offset) 
{
  /* Make a pointer that is always inside our buffer */
  const size_t overflown_ptr = ctx->m_cache->cache_ptr + HUFFMAN_CACHE_SIZE;
  const uint8_t lookback_data = ctx->m_cache->cache[(overflown_ptr - offset) & HUFFMAN_CACHE_MASK];

  c_write(ctx, lookback_data);
}

static uint16_t 
c_read16(decompress_ctx_t* ctx) 
{
  uint16_t a = c_read(ctx);
  uint16_t b = c_read(ctx);
  return (b << 8) | (a << 0);
}

static uint32_t 
c_read32(decompress_ctx_t* ctx) 
{
  uint32_t a = c_read(ctx);
  uint32_t b = c_read(ctx);
  uint32_t c = c_read(ctx);
  uint32_t d = c_read(ctx);
  return (d << 24) | (c << 16) | (b << 8) | (a << 0);;
}

static uint8_t 
c_read_bit(decompress_ctx_t* ctx) 
{
  if (ctx->m_bit_buffer_bits_count == 0) {
    /* Fill the buffer */
    ctx->m_bit_buffer = c_read(ctx);

    /* We fill the buffer with 8 bits */
    ctx->m_bit_buffer_bits_count = 8;
  }

  uint8_t ret = ctx->m_bit_buffer & 1;

  ctx->m_bit_buffer >>= 1;

  ctx->m_bit_buffer_bits_count--;

  return ret;
}

static uint32_t 
c_read_bits(decompress_ctx_t* ctx, uint8_t count) 
{
  uint32_t ret = 0;
  for (uint32_t i = 0; i < count; i++) {
    /* Load bit into the ith place of the buffer */
    ret |= (c_read_bit(ctx) << i);
  }

  return ret;
}

/*
 * This function basicaly walks the huffman tree to find the 'letter' for a certain value
 */
static uint32_t 
c_read_symbol(decompress_ctx_t* ctx, struct huffman_table* table) 
{
  int count = 0;
  int current = 0;
  for (uint32_t i = 1; current >= 0; i++) {
    /* Prepare for next bit and read it */
    current = (current << 1) | c_read_bit(ctx);
    /* Go backwards in the tree */
    current -= table->counts[i];
    /* Add to total distance */
    count += table->counts[i];
  }
  return table->alphabet[count + current];
}

static int
handle_non_compressed(decompress_ctx_t* ctx) 
{
  c_reset_bit_buffer(ctx);

  /* Non-compressed blocks start with these two words */
  uint16_t len = c_read16(ctx);
  uint16_t nlen = c_read16(ctx);

  uint16_t check_len = len & 0xFFFF;
  uint16_t check_nlen = ~nlen & 0xFFFF;

  /* Check validety */
  if (check_len != check_nlen)
    return -1;

  /* Just write this data to the out buffer */
  for (uintptr_t i = 0; i < len; i++) {
    c_write(ctx, c_read(ctx));
  }

  return 0;
}

static int 
huffman_inflate(decompress_ctx_t* ctx, struct huffman_table* lengths, struct huffman_table* dists) 
{

  /* Length codes as represented by the RFC1951, section 3.2.5 */
  static const uint16_t __length_codes[] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51,
    59, 67, 83, 99, 115, 131, 163, 195, 227, 258
  };

  /* Bitcounts for the length codes as represented by the RFC1951, section 3.2.5 */
  static const uint16_t __lbits[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,
    4, 5, 5, 5, 5, 0
  };

  /* Distance codes as represented by the RFC1951, section 3.2.5 */
  static const uint16_t __distances[] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385,
    513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
  };

  /* Bitcounts for the distances as represented by the RFC1951, section 3.2.5 */
  static const uint16_t __dbits[] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10,
    10, 11, 11, 12, 12, 13, 13
  };

  for (;;) {
    const uint32_t symb = c_read_symbol(ctx, lengths);

    if (symb == 256) {
      break;
    } else if (symb < 256) {
      c_write(ctx, symb);
    } else {
      uint32_t fixed_symb = symb - 257;
      uint32_t length = c_read_bits(ctx, __lbits[fixed_symb]) + __length_codes[fixed_symb];
      uint32_t dist = c_read_symbol(ctx, dists);
      uint32_t offset = c_read_bits(ctx, __dbits[dist]) + __distances[dist];

      for (uint32_t i = 0; i < length; i++) {
        c_write_from(ctx, offset);
      }
    }
  }

  return 0;
}

static int 
dynamic_huffman_inflate(decompress_ctx_t* ctx) 
{
  static const uint8_t clens[] = {
      16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
  };

  uint8_t dyn_lengths[320] = { 0 };

  uint32_t dyn_literals  = 257 + c_read_bits(ctx, 5);
  uint32_t dyn_distances = 1 + c_read_bits(ctx, 5);
  uint32_t dyn_code_lens  = 4 + c_read_bits(ctx, 4);

  for (uint32_t i = 0; i < dyn_code_lens; i++) {
    dyn_lengths[clens[i]] = c_read_bits(ctx, 3);
  }

  struct huffman_table dyn_codes;

  fill_huffman_table(&dyn_codes, dyn_lengths, 19);

  for (uint32_t i = 0; i < dyn_literals + dyn_distances;) {
    int symb = c_read_symbol(ctx, &dyn_codes);

    uint32_t repeat_count = 0;

    switch (symb) {
      case 16:
        /* 16: copy the prev code length 3 - 6 times */
        symb = dyn_lengths[i-1];
        repeat_count = 3 + c_read_bits(ctx, 2);
        break;
      case 17:
        /* 17: repeat a code length of 0 for 3 - 10 times */
        symb = 0;
        repeat_count = 3 + c_read_bits(ctx, 3);
        break;
      case 18:
        /* 18: repeat a code length of 0 for 11 - 138 times */
        symb = 0;
        repeat_count = 11 + c_read_bits(ctx, 7);
        break;
      default:
        /* 0 - 15: length is equal to the symbol */
        repeat_count = 1;
        break;
    }

    /* While repeat_count is non-null, keep copying lengths */
    while (repeat_count--) {
      dyn_lengths[i++] = symb;
    }
  }

  struct huffman_table dyn_len_table;
  struct huffman_table dyn_dist_table;

  fill_huffman_table(&dyn_len_table, dyn_lengths, dyn_literals);
  fill_huffman_table(&dyn_dist_table, dyn_lengths + dyn_literals, dyn_distances);

  return huffman_inflate(ctx, &dyn_len_table, &dyn_dist_table);
}

int generic_inflate(decompress_ctx_t* ctx) 
{
  c_reset_bit_buffer(ctx);

  /* Build the fixed tables for the huffman tables if they are not built already */
  build_fixed_tables();

  int error = 0;

  for (;;) {
    bool last = c_read_bit(ctx);
    uint8_t block_type = c_read_bits(ctx, 2);

    switch (block_type) {
      /* Not compressed */
      case 0x00:
        error = handle_non_compressed(ctx);
        break;
      /* fixed huffman code */
      case 0x01:
        error = huffman_inflate(ctx, &fixed_lengths, &fixed_distances);
        break;
      /* dynamic huffman code */
      case 0x02:
        error = dynamic_huffman_inflate(ctx);
        break;
      /* Invalid */
      case 0x03:
      default:
        error = -1;
        break;
    }

    if (last || error)
      break;
  }

  return error;
}

#define GZIP_MAGIC_BYTE0 0x1F
#define GZIP_MAGIC_BYTE1 0x8B

#define GZIP_DEFLATE_CM 0x08

/* These flags encode to different 'optional' headers in the block */
#define GZIP_FLAG_TEXT 0x01
#define GZIP_FLAG_HCRC 0x02
#define GZIP_FLAG_EXTR 0x04
#define GZIP_FLAG_NAME 0x08
#define GZIP_FLAG_COMM 0x10

static size_t 
cram_find_decompressed_size(void* start, size_t size) 
{
  decompress_ctx_t dummy_ctx = { 0 };

  dummy_ctx.m_current = (uint8_t*)((uintptr_t)start + size) - 8;
  dummy_ctx.m_end_addr = (uintptr_t)start + size;

  uint32_t crc32 = c_read32(&dummy_ctx);

  /* TODO: validate crc32? */
  (void)crc32;

  return c_read32(&dummy_ctx);
}

/*!
 * @brief: Decompress a compressed buffer into a buffer allocated by this function
 *
 * @returns: The size of the decompressed buffer
 */
size_t 
tarfs_decompress(void* c_buffer, size_t c_size, void** out_buffer)
{
  size_t decompressed_size;
  light_ctx_t* lctx;

  decompressed_size = cram_find_decompressed_size(c_buffer, c_size);

  if (!decompressed_size)
    return 0;

  lctx = get_light_ctx();

  /* FIXME: deallocate this if this function fails */
  *out_buffer = lctx->f_allocate(decompressed_size);

  if (!(*out_buffer))
    return 0;

  decompress_ctx_t ctx = {
    .m_start_addr = (uintptr_t)c_buffer,
    .m_end_addr = (uintptr_t)c_buffer + c_size,
    .m_current = (uint8_t*)c_buffer,
    .m_out = *out_buffer,
    .m_current_out = *out_buffer,
    .m_bit_buffer = 0,
    .m_bit_buffer_bits_count = 0,
    .m_cache = &default_cache,
  };

  if (c_read(&ctx) != GZIP_MAGIC_BYTE0)
    return 0;

  if (c_read(&ctx) != GZIP_MAGIC_BYTE1)
    return 0;

  /* TODO: find a practical reason to store the header */
  struct gzip_compressed_header header = { 0 };

  /* Magic word matches, we can proceed with the decompression */

  header.compression_method = c_read(&ctx);
  header.flags = c_read(&ctx);
  header.mtime = c_read32(&ctx);
  header.xflags = c_read(&ctx);
  header.os = c_read(&ctx);

  /* Can't compress blocks with this that aren't encoded with DEFLATE */
  if (header.compression_method != GZIP_DEFLATE_CM) {
    return 0;
  }

  /* Extra bytes we can discard */
  if (header.flags & GZIP_FLAG_EXTR) {
    header.extra_bytes = c_read16(&ctx);
    for (uintptr_t i = 0; i < header.extra_bytes; i++)
      c_read(&ctx);
  }

  if (header.flags & GZIP_FLAG_NAME) {
    header.name = heap_allocate(MAX_NAME_LEN);
    memset(header.name, 0, MAX_NAME_LEN);

    uintptr_t idx = 0;
    uint8_t c;
    while ((c = c_read(&ctx)) != 0x00) {
      if (idx < MAX_NAME_LEN)
        header.name[idx++] = c;
    }
  }

  if (header.flags & GZIP_FLAG_COMM) {
    header.comment = heap_allocate(MAX_COMMENT_LEN);
    memset(header.comment, 0, MAX_COMMENT_LEN);

    uintptr_t idx = 0;
    uint8_t c;
    while ((c = c_read(&ctx)) != 0x00) {
      if (idx < MAX_COMMENT_LEN)
        header.comment[idx++] = c;
    }
  }

  if (header.flags & GZIP_FLAG_HCRC) {
    header.crc16 = c_read16(&ctx);
  }

  destroy_gzip_header(&header);

  if (generic_inflate(&ctx))
    return 0;

  return decompressed_size;
}
