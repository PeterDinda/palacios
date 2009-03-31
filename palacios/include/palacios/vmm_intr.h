/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_INTR_H_
#define __VMM_INTR_H_


#ifdef __V3VEE__

#include <palacios/vmm_types.h>
#include <palacios/vmm_list.h>



typedef enum {INVALID_INTR, EXTERNAL_IRQ, NMI, SOFTWARE_INTR, VIRTUAL_INTR} intr_type_t;

struct guest_info;
struct v3_interrupt;



struct v3_irq_hook {
    int (*handler)(struct guest_info * info, struct v3_interrupt * intr, void * priv_data);
    void * priv_data;
};





struct v3_intr_state {

    struct list_head controller_list;

    uint_t irq_pending;
    uint_t irq_vector;

    /* some way to get the [A]PIC intr */
    struct v3_irq_hook * hooks[256];
  
};



void v3_init_interrupt_state(struct guest_info * info);


int v3_raise_irq(struct guest_info * info, int irq);
int v3_lower_irq(struct guest_info * info, int irq);



struct intr_ctrl_ops {
    int (*intr_pending)(void * private_data);
    int (*get_intr_number)(void * private_data);
    int (*raise_intr)(void * private_data, int irq);
    int (*lower_intr)(void * private_data, int irq);
    int (*begin_irq)(void * private_data, int irq);
};



void v3_register_intr_controller(struct guest_info * info, struct intr_ctrl_ops * ops, void * state);

int v3_intr_pending(struct guest_info * info);
uint_t v3_get_intr_number(struct guest_info * info);
intr_type_t v3_get_intr_type(struct guest_info * info);

int v3_injecting_intr(struct guest_info * info, uint_t intr_num, intr_type_t type);

/*
  int start_irq(struct vm_intr * intr);
  int end_irq(struct vm_intr * intr, int irq);
*/



int v3_hook_irq(struct guest_info * info, 
		uint_t irq,
		int (*handler)(struct guest_info * info, struct v3_interrupt * intr, void * priv_data),
		void  * priv_data);

int v3_hook_passthrough_irq(struct guest_info *info, uint_t irq);



#endif // !__V3VEE__



#endif
