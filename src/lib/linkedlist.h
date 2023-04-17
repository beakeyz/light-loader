#ifndef __LIGHTLOADER_LINKEDLIST__ 
#define __LIGHTLOADER_LINKEDLIST__
#include <lib/light_mainlib.h>

struct linked_list;
struct linked_node;

typedef struct linked_node {
  void* m_data;
  size_t m_data_size;

  struct linked_node* m_next;
} linked_node_t;

typedef void (*LINKEDLIST_DELETE) (
  struct linked_list* this
);

typedef struct linked_list {
  linked_node_t* m_head;
  linked_node_t* m_tail;

  size_t m_entries;

  // null if created on the stack
  LINKEDLIST_DELETE fDelete;
} linked_list_t;

linked_list_t* make_linkedlist();
void delete_linkedlist(linked_list_t* list);

void linkedlist_add_back(linked_list_t* list, void* data, size_t size);
void linkedlist_add_front(linked_list_t* list, void* data, size_t size);

void linkedlist_remove_idx(linked_list_t* list, uint32_t idx);
void linkedlist_remove_data(linked_list_t* list, void* data);

void linkedlist_add_back(linked_list_t* list, void* data, size_t size);
void linkedlist_add_front(linked_list_t* list, void* data, size_t size);

void linkedlist_remove_idx(linked_list_t* list, uint32_t idx);
void linkedlist_remove_data(linked_list_t* list, void* data);

#endif // !
