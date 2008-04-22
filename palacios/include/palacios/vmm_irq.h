#ifndef __VMM_IRQ_H
#define __VMM_IRQ_H

#if 0
#include <palacios/vmm_types.h>


struct vmm_irq_hook;




struct vmm_irq_hook {
  uint_t irq;
  void * private_data;

  int(*handler)(uint_t irq, void * private_data);

  struct vmm_irq_hook *next, *prev;
};


void init_irq_map(struct vmm_irq_map * map);


int hook_irq(struct vmm_irq_map * map, uint_t irq, 
	     int(*handler)(uint_t irq, void * private_data), 
	     void * private_data);


int unhook_irq(struct vmm_irq_map * map, uint_t irq);

struct vmm_irq_hook * get_irq_hook(struct vmm_irq_map * map, uint_t irq);

#endif
#endif
