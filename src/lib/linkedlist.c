#include "linkedlist.h"
#include <mem/pmm.h>

void delete_linkedlist(linked_list_t* list) {
  // walk entries and free them
  // free list if it was created on the heap

  linked_node_t* next = list->m_tail;
  while (next != NULL) {

    pmm_free(next, sizeof(linked_node_t));

    next = next->m_next;
  }
  
  // TODO: check if the list was created on the heap
}

linked_list_t* make_linkedlist() {
  
  linked_list_t* list = pmm_malloc(sizeof(linked_list_t), MEMMAP_BOOTLOADER_RECLAIMABLE);

  list->fDelete = delete_linkedlist;
  list->m_entries = 0;
  list->m_head = NULL;
  list->m_tail = NULL;
    
  return list;
}

void linkedlist_add_back(linked_list_t* list, void* data, size_t size) {
  
  linked_node_t* node = pmm_malloc(sizeof(linked_node_t), MEMMAP_BOOTLOADER_RECLAIMABLE);
  node->m_data = data;
  node->m_data_size = size;
  node->m_next = NULL;

  if (list->m_tail == NULL && list->m_head == NULL) {
    list->m_tail = node;
    list->m_head = node;
  } else {
    node->m_next = list->m_tail;
    list->m_tail = node;
  }
}

void linkedlist_add_front(linked_list_t* list, void* data, size_t size) {

  linked_node_t* node = pmm_malloc(sizeof(linked_node_t), MEMMAP_BOOTLOADER_RECLAIMABLE);
  node->m_data = data;
  node->m_data_size = size;
  node->m_next = NULL;

  if (list->m_tail == NULL && list->m_head == NULL) {
    list->m_tail = node;
    list->m_head = node;
  } else {
    list->m_head->m_next = node;
    list->m_head = node;
    // node has no next in this case
  }
}

void linkedlist_remove_idx(linked_list_t* list, uint32_t idx);
void linkedlist_remove_data(linked_list_t* list, void* data);