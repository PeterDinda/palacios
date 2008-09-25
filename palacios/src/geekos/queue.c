/* Northwestern University */
/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */

#include <geekos/queue.h>



void init_queue(struct gen_queue * queue) {
  queue->num_entries = 0;
  INIT_LIST_HEAD(&(queue->entries));
}


struct gen_queue * create_queue() {
  struct gen_queue * tmp_queue = Malloc(sizeof(struct gen_queue));
  init_queue(tmp_queue);
  return tmp_queue;
}

void enqueue(struct gen_queue * queue, void * entry) {
  struct queue_entry * q_entry = Malloc(sizeof(struct queue_entry));

  q_entry->entry = entry;
  list_add_tail(&(q_entry->entry_list), &(queue->entries));
  queue->num_entries++;
}


void * dequeue(struct gen_queue * queue) {
  void * entry_val = 0;
  
  if (!list_empty(&(queue->entries))) {
    struct list_head * q_entry = queue->entries.next;
    struct queue_entry * tmp_entry = list_entry(q_entry, struct queue_entry, entry_list);

    entry_val = tmp_entry->entry;
    list_del(q_entry);
    Free(tmp_entry);
  }

  return entry_val;
}
