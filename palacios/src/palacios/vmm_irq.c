#if 0
#include <palacios/vmm_irq.h>
#include <palacios/vmm.h>

void init_irq_map(struct vmm_irq_map * map) {
  map->head = NULL;
  map->num_hooks = 0;
}


int add_irq_hook(struct vmm_irq_map * map, struct vmm_irq_hook * hook) {
  if (!(map->head)) {
    map->head = hook;
    map->num_hooks = 1;
    return 0;
  } else if (map->head->irq > hook->irq) {
    hook->next = map->head;

    map->head->prev = hook;
    map->head = hook;
    map->num_hooks++;

    return 0;
  } else {
    struct vmm_irq_hook * tmp_hook = map->head;
    while ((tmp_hook->next) &&
	   (tmp_hook->next->irq <= hook->irq)) {
      tmp_hook = tmp_hook->next;
    }

    if (tmp_hook->irq == hook->irq) {
      return -1;
    } else {
      hook->prev = tmp_hook;
      hook->next = tmp_hook->next;

      if (tmp_hook->next) {
	tmp_hook->next->prev = hook;
      }

      tmp_hook->next = hook;

      map->num_hooks++;
      return 0;
    }
  }
  return -1;
}


int remove_irq_hook(struct vmm_irq_map * map, struct vmm_irq_hook * hook) {
  if (map->head == hook) {
    map->head = hook->next;
  } else if (hook->prev) {
    hook->prev->next = hook->next;
  } else {
    return -1;
  }

  if (hook->next) {
    hook->next->prev = hook->prev;
  }

  map->num_hooks--;

  return 0;
}


int hook_irq(struct vmm_irq_map * map, uint_t irq, 
	     int(*handler)(uint_t irq, void * private_data), 
	     void * private_data) {

  struct vmm_irq_hook * hook = NULL;
  VMMMalloc(struct vmm_irq_hook *, hook, sizeof(struct vmm_irq_hook));

  if (!hook) {
    // big problems
    return -1;
  }

  hook->irq = irq;
  hook->handler = handler;
  hook->private_data = private_data;
  hook->next = NULL;
  hook->prev = NULL;
  
  if (add_irq_hook(map, hook) != 0) {
    VMMFree(hook);
    return -1;
  }

  return 0;
}


int unhook_irq(struct vmm_irq_map * map, uint_t irq) {
  struct vmm_irq_hook * hook = get_irq_hook(map, irq);

  if (!hook) {
    return -1;
  }

  remove_irq_hook(map, hook);
  return 0;
}


struct vmm_irq_hook * get_irq_hook(struct vmm_irq_map * map, uint_t irq) {
  struct vmm_irq_hook * tmp_hook = map->head;
  
  while (tmp_hook) {
    if (tmp_hook->irq == irq) {
      return tmp_hook;
    }
    tmp_hook = tmp_hook->next;
  }
  return NULL;
}


#endif
