#include <palacios/vmm_queue.h>



void init_queue(struct gen_queue * queue) {
  queue->num_entries = 0;
  INIT_LIST_HEAD(&(queue->entries));
}


struct gen_queue * create_queue() {
  struct gen_queue * tmp_queue = V3_Malloc(sizeof(struct gen_queue));
  init_queue(tmp_queue);
  return tmp_queue;
}

void enqueue(struct gen_queue * queue, addr_t entry) {
  struct queue_entry * q_entry = V3_Malloc(sizeof(struct queue_entry));

  q_entry->entry = entry;
  list_add_tail(&(q_entry->entry_list), &(queue->entries));
  queue->num_entries++;
}


addr_t dequeue(struct gen_queue * queue) {
  addr_t entry_val = 0;

  if (!list_empty(&(queue->entries))) {
    struct list_head * q_entry = queue->entries.next;
    struct queue_entry * tmp_entry = list_entry(q_entry, struct queue_entry, entry_list);

    entry_val = tmp_entry->entry;
    list_del(q_entry);
    V3_Free(tmp_entry);
  }

  return entry_val;
}
