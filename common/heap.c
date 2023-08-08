
#include "heap.h"
#include "efilib.h"
#include "stddef.h"
#include <stddef.h>
#include <memory.h>

static uint64_t heap_start;
static uint64_t heap_end;
static uint64_t heap_limit;

typedef struct heap_node {
  /* The 'size' of a node is about the total size it encapsulates. To get the size of the data area, subtract this with sizeof(struct heap_node) */
  uint32_t size;
  uint16_t signature;
  uint16_t flags;
  struct heap_node* next;
  uint8_t data[];
} heap_node_t;

#define HEAP_NODE_SIGNATURE 0x1AA5

#define HEAP_NODE_FLAG_USED 0x0001
#define HEAP_NODE_FLAG_

#define INITIAL_HEAP_LIMIT 16 * Mib 

static heap_node_t*
node_from_address(uint64_t address)
{
  heap_node_t* node;

  /* Prevent doing unholy opperations on this address */
  if (!address)
    return nullptr;

  node = (heap_node_t*)(address - sizeof(heap_node_t));

  if (node->signature != HEAP_NODE_SIGNATURE)
    return NULL;

  return node;
}

/*
 * Quick function to debug the heaps function
 * TODO: remove
 */
void
debug_heap()
{
  heap_node_t* start = (heap_node_t*)heap_start;

  ST->ConOut->OutputString(ST->ConOut, (CHAR16*)L"Debugging: \n\r");

  while (start) {

    ST->ConOut->OutputString(ST->ConOut, (CHAR16*)L" - Found a node: ");

    if ((start->flags & HEAP_NODE_FLAG_USED) == 0)
      ST->ConOut->OutputString(ST->ConOut, (CHAR16*)L" Unused\n\r");
    else 
      ST->ConOut->OutputString(ST->ConOut, (CHAR16*)L" Used\n\r");

    start = start->next;
  }
}

void 
init_heap(uint64_t heap_addr, uint64_t heap_size)
{
  /* Setup the variables that keep track of the heaps position */
  heap_start = heap_addr;
  heap_end = heap_addr + heap_size;
  heap_limit = INITIAL_HEAP_LIMIT;

  heap_node_t* start = (heap_node_t*)heap_start;

  start->signature = HEAP_NODE_SIGNATURE;
  start->size = INITIAL_HEAP_LIMIT;
  start->next = nullptr;
  start->flags = NULL;

}

/*!
 * @brief Grab a big block of memory from the pool
 *
 * @size: the amount of bytes to grab
 * @returns: the previous limit
 */
void*
sbrk(unsigned long long size)
{
  void* ret = (void*)(heap_start + heap_limit);

  /* Overflows get slammed */
  if (ret + size >= (void*)heap_end)
    return NULL;

  heap_limit += size;

  return ret;
}

void*
heap_allocate(unsigned long long size)
{
  heap_node_t* node;

  if (!size)
    return nullptr;

  node = (heap_node_t*)heap_start;

  while (node) {

    if ((node->flags & HEAP_NODE_FLAG_USED) == 0 && node->size - sizeof(heap_node_t) >= size) {
      size_t new_node_size = size + sizeof(heap_node_t);
      
      /* If the node fits perfectly, just take it. */
      if (new_node_size == node->size) {
        node->flags |= HEAP_NODE_FLAG_USED;
        return node->data;
      }

      /* We'll need to split it */

      heap_node_t* new_node = (heap_node_t*)(((uint64_t)(node) + node->size) - new_node_size);

      /* Create a new node at the end */
      new_node->next = node->next;
      new_node->size = new_node_size;
      new_node->signature = HEAP_NODE_SIGNATURE;
      new_node->flags = HEAP_NODE_FLAG_USED;

      /* Fixup the old node */
      node->next = new_node;
      node->size -= new_node_size;

      return new_node->data;
    }

    node = node->next;
  }

  return NULL;
}

void
heap_free(void* addr)
{
  uint64_t concurrent_idx;
  heap_node_t* prev;
  heap_node_t* ittr;
  heap_node_t* node = node_from_address((uint64_t)addr);

  if (!node)
    return;

  /* First, merge everything above the current node */
  while (node->next && (node->next->flags & HEAP_NODE_FLAG_USED) == 0) {
    node->size += node->next->size;
    node->next = node->next->next;
  }

  concurrent_idx = 0;
  prev = nullptr;
  ittr = (heap_node_t*)heap_start;

  while (ittr) {

    /* If we have found our node, preemptively break */
    if (ittr == node)
      break;

next:
    prev = ittr;
    ittr = ittr->next;
  }

  if (!ittr || !prev || (prev->flags & HEAP_NODE_FLAG_USED) == HEAP_NODE_FLAG_USED || prev->next != node)
    goto finish;

  prev->size += node->size;
  prev->next = node->next;

  node = prev;

finish:
  /* Clear the used flag */
  node->flags &= ~HEAP_NODE_FLAG_USED;
}
