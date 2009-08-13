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


#include <palacios/vmm_intr.h>
#include <palacios/vmm.h>

#include <palacios/vm_guest.h>
#include <palacios/vmm_ctrl_regs.h>

#include <palacios/vmm_lock.h>

#ifndef CONFIG_DEBUG_INTERRUPTS
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif




struct intr_controller {
    struct intr_ctrl_ops * ctrl_ops;
    
    void * priv_data;
    struct list_head ctrl_node;
};


void v3_init_interrupt_state(struct guest_info * info) {

    info->intr_state.irq_pending = 0;
    info->intr_state.irq_started = 0;
    info->intr_state.irq_vector = 0;

    INIT_LIST_HEAD(&(info->intr_state.controller_list));

    v3_lock_init(&(info->intr_state.irq_lock));

    memset((uchar_t *)(info->intr_state.hooks), 0, sizeof(struct v3_irq_hook *) * 256);
}

void v3_register_intr_controller(struct guest_info * info, struct intr_ctrl_ops * ops, void * state) {
    struct intr_controller * ctrlr = (struct intr_controller *)V3_Malloc(sizeof(struct intr_controller));

    ctrlr->priv_data = state;
    ctrlr->ctrl_ops = ops;

    list_add(&(ctrlr->ctrl_node), &(info->intr_state.controller_list));

}




static inline struct v3_irq_hook * get_irq_hook(struct guest_info * info, uint_t irq) {
    V3_ASSERT(irq <= 256);
    return info->intr_state.hooks[irq];
}


int v3_hook_irq(struct guest_info * info, 
		uint_t irq,
		int (*handler)(struct guest_info * info, struct v3_interrupt * intr, void * priv_data),
		void  * priv_data) 
{
    struct v3_irq_hook * hook = (struct v3_irq_hook *)V3_Malloc(sizeof(struct v3_irq_hook));

    if (hook == NULL) { 
	return -1; 
    }

    if (get_irq_hook(info, irq) != NULL) {
	PrintError("IRQ %d already hooked\n", irq);
	return -1;
    }

    hook->handler = handler;
    hook->priv_data = priv_data;
  
    info->intr_state.hooks[irq] = hook;

    if (V3_Hook_Interrupt(info, irq)) { 
	PrintError("hook_irq: failed to hook irq %d\n", irq);
	return -1;
    } else {
	PrintDebug("hook_irq: hooked irq %d\n", irq);
	return 0;
    }
}



static int passthrough_irq_handler(struct guest_info * info, struct v3_interrupt * intr, void * priv_data) {
    PrintDebug("[passthrough_irq_handler] raise_irq=%d (guest=0x%p)\n", 
	       intr->irq, (void *)info);

    return v3_raise_irq(info, intr->irq);
}

int v3_hook_passthrough_irq(struct guest_info * info, uint_t irq) {
    int rc = v3_hook_irq(info, irq, passthrough_irq_handler, NULL);

    if (rc) { 
	PrintError("guest_irq_injection: failed to hook irq 0x%x (guest=0x%p)\n", irq, (void *)info);
	return -1;
    } else {
	PrintDebug("guest_irq_injection: hooked irq 0x%x (guest=0x%p)\n", irq, (void *)info);
	return 0;
    }
}





int v3_deliver_irq(struct guest_info * info, struct v3_interrupt * intr) {
    PrintDebug("v3_deliver_irq: irq=%d state=0x%p, \n", intr->irq, (void *)intr);
  
    struct v3_irq_hook * hook = get_irq_hook(info, intr->irq);

    if (hook == NULL) {
	PrintError("Attempting to deliver interrupt to non registered hook(irq=%d)\n", intr->irq);
	return -1;
    }
  
    return hook->handler(info, intr, hook->priv_data);
}





int v3_raise_virq(struct guest_info * info, int irq) {
    struct v3_intr_state * intr_state = &(info->intr_state);
    int major = irq / 8;
    int minor = irq % 8;

    intr_state->virq_map[major] |= (1 << minor);
   
    return 0;
}

int v3_lower_virq(struct guest_info * info, int irq) {
    struct v3_intr_state * intr_state = &(info->intr_state);
    int major = irq / 8;
    int minor = irq % 8;

    intr_state->virq_map[major] &= ~(1 << minor);

    return 0;
}


int v3_lower_irq(struct guest_info * info, int irq) {
    struct intr_controller * ctrl = NULL;
    struct v3_intr_state * intr_state = &(info->intr_state);

    //    PrintDebug("[v3_lower_irq]\n");
    addr_t irq_state = v3_lock_irqsave(intr_state->irq_lock);

    list_for_each_entry(ctrl, &(intr_state->controller_list), ctrl_node) {
	ctrl->ctrl_ops->lower_intr(ctrl->priv_data, irq);
    }
 
    v3_unlock_irqrestore(intr_state->irq_lock, irq_state);

    return 0;
}

int v3_raise_irq(struct guest_info * info, int irq) {
    struct intr_controller * ctrl = NULL;
    struct v3_intr_state * intr_state = &(info->intr_state);

    //  PrintDebug("[v3_raise_irq (%d)]\n", irq);
    addr_t irq_state = v3_lock_irqsave(intr_state->irq_lock);

    list_for_each_entry(ctrl, &(intr_state->controller_list), ctrl_node) {
	ctrl->ctrl_ops->raise_intr(ctrl->priv_data, irq);
    }

    v3_unlock_irqrestore(intr_state->irq_lock, irq_state);

    return 0;
}



v3_intr_type_t v3_intr_pending(struct guest_info * info) {
    struct v3_intr_state * intr_state = &(info->intr_state);
    struct intr_controller * ctrl = NULL;
    int ret = V3_INVALID_INTR;
    int i = 0;

    //  PrintDebug("[intr_pending]\n");
    addr_t irq_state = v3_lock_irqsave(intr_state->irq_lock);

    // VIRQs have priority
    for (i = 0; i < MAX_IRQ / 8; i++) {
	if (intr_state->virq_map[i] != 0) {   
	    ret = V3_VIRTUAL_IRQ;
	    break;
	}
    }

    if (ret == V3_INVALID_INTR) {
	list_for_each_entry(ctrl, &(intr_state->controller_list), ctrl_node) {
	    if (ctrl->ctrl_ops->intr_pending(ctrl->priv_data) == 1) {
		ret = V3_EXTERNAL_IRQ;
		break;
	    }
	}
    }

    v3_unlock_irqrestore(intr_state->irq_lock, irq_state);

    return ret;
}


uint32_t v3_get_intr(struct guest_info * info) {
    struct v3_intr_state * intr_state = &(info->intr_state);
    struct intr_controller * ctrl = NULL;
    uint_t ret = 0;
    int i = 0;
    int j = 0;

    addr_t irq_state = v3_lock_irqsave(intr_state->irq_lock);    

    // virqs have priority
    for (i = 0; i < MAX_IRQ / 8; i++) {
	if (intr_state->virq_map[i] != 0) {
	    for (j = 0; j < 8; j++) {
		if (intr_state->virq_map[i] & (1 << j)) {
		    ret = (i * 8) + j;
		    break;
		}
	    }
	    break;
	}
    }

    if (!ret) {
	list_for_each_entry(ctrl, &(intr_state->controller_list), ctrl_node) {
	    if (ctrl->ctrl_ops->intr_pending(ctrl->priv_data)) {
		uint_t intr_num = ctrl->ctrl_ops->get_intr_number(ctrl->priv_data);
		
		//	PrintDebug("[get_intr_number] intr_number = %d\n", intr_num);
		ret = intr_num;
		break;
	    }
	}
    }

    v3_unlock_irqrestore(intr_state->irq_lock, irq_state);

    return ret;
}

/*
intr_type_t v3_get_intr_type(struct guest_info * info) {
    struct v3_intr_state * intr_state = &(info->intr_state);
    struct intr_controller * ctrl = NULL;
    intr_type_t type = V3_INVALID_INTR;

    addr_t irq_state = v3_lock_irqsave(intr_state->irq_lock);  

    list_for_each_entry(ctrl, &(intr_state->controller_list), ctrl_node) {
	if (ctrl->ctrl_ops->intr_pending(ctrl->priv_data) == 1) {
	    //PrintDebug("[get_intr_type] External_irq\n");
	    type = V3_EXTERNAL_IRQ;	    
	    break;
	}
    }

#ifdef CONFIG_DEBUG_INTERRUPTS
    if (type == V3_INVALID_INTR) {
	PrintError("[get_intr_type] Invalid_Intr\n");
    }
#endif

    v3_unlock_irqrestore(intr_state->irq_lock, irq_state);

    return type;
}
*/





int v3_injecting_intr(struct guest_info * info, uint_t intr_num, v3_intr_type_t type) {
    struct v3_intr_state * intr_state = &(info->intr_state);

    if (type == V3_EXTERNAL_IRQ) {
	struct intr_controller * ctrl = NULL;

	addr_t irq_state = v3_lock_irqsave(intr_state->irq_lock); 

	//	PrintDebug("[injecting_intr] External_Irq with intr_num = %x\n", intr_num);
	list_for_each_entry(ctrl, &(intr_state->controller_list), ctrl_node) {
	    ctrl->ctrl_ops->begin_irq(ctrl->priv_data, intr_num);
	}

	v3_unlock_irqrestore(intr_state->irq_lock, irq_state);
    }

    return 0;
}
