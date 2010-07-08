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

#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vm_guest.h>
#include <devices/icc_bus.h>
#include <devices/apic_regs.h>


#define MAX_APICS 256

#ifndef CONFIG_DEBUG_ICC_BUS
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


void v3_force_exit() {
}

struct ipi_thunk_data {
    struct vm_device * target;
    uint64_t val;
};



struct apic_data {
    struct guest_info * core;
    struct v3_icc_ops * ops;
    
    void * priv_data;
    int present;
};


struct icc_bus_state {
    struct apic_data apics[MAX_APICS];
    
    uint32_t         ioapic_id;
};

static struct v3_device_ops dev_ops = {
    .free = NULL,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};


static char *shorthand_str[] = { 
    "(no shorthand)",
    "(self)",
    "(all)",
    "(all-but-me)",
 };

static char *deliverymode_str[] = { 
    "(fixed)",
    "(lowest priority)",
    "(SMI)",
    "(reserved)",
    "(NMI)",
    "(INIT)",
    "(Start Up)",
    "(reserved)",
};



static int deliver(uint32_t src_apic, struct apic_data *dest_apic, struct int_cmd_reg *icr, struct icc_bus_state * state) {

    switch (icr->del_mode) {						

	case 0:  //fixed
	case 1: // lowest priority
	    PrintDebug("icc_bus: delivering to core %u\n",dest_apic->core->cpu_id); 
	    dest_apic->ops->raise_intr(dest_apic->core, icr->vec, dest_apic->priv_data); 
	    if (src_apic!=state->ioapic_id && dest_apic->core->cpu_id != src_apic) { 
		PrintDebug("icc_bus: non-local core, forcing it to exit\n"); 
		// TODO: do what the print says
	    }							
	    break;							
	    
	case 2:   //SMI			
	    PrintError("icc_bus: SMI delivery is unsupported\n");	
	    return -1;						
	    break;							
	    
	case 3:  //reserved						
	case 7:
	    PrintError("icc_bus: Reserved delivery mode 3 is unsupported\n"); 
	    return -1;						
	    break;							

	case 4:  //NMI					
	    PrintError("icc_bus: NMI delivery is unsupported\n"); 
	    return -1;						
	    break;							

	case 5: //INIT
	    PrintError("icc_bus: INIT delivery is unsupported\n"); 
	    return -1;						
	    break;							

	case 6: //Start Up
	    PrintError("icc_bus: Startup Delivery is unsupported\n"); 
	    return -1;						
	    break;							
    }

    return 0;
} 



int v3_icc_send_ipi(struct vm_device * icc_bus, uint32_t src_apic, uint64_t icr_data) {

    PrintDebug("icc_bus: icc_bus=%p, src_apic=%u, icr_data=%llx\n",icc_bus,src_apic,icr_data);

    struct int_cmd_reg *icr = (struct int_cmd_reg *)&icr_data;
    struct icc_bus_state * state = (struct icc_bus_state *)icc_bus->private_data;

    // initial sanity checks
    if (src_apic>=MAX_APICS || (!state->apics[src_apic].present && src_apic!=state->ioapic_id)) { 
	PrintError("icc_bus: Apparently sending from unregistered apic id=%u\n",src_apic);
	return -1;
    }
    if (icr->dst_mode==0  && !state->apics[icr->dst].present) { 
	PrintError("icc_bus: Attempted send to unregistered apic id=%u\n",icr->dst);
	return -1;
    }
    
    struct apic_data * dest_apic =  &(state->apics[icr->dst]);


    PrintDebug("icc_bus: IPI %s %u from %s %u to %s %u (icr=0x%llx)\n",
	       deliverymode_str[icr->del_mode], icr->vec, src_apic==state->ioapic_id ? "ioapic" : "apic",
	       src_apic, shorthand_str[icr->dst_shorthand], icr->dst,icr->val);




    switch (icr->dst_shorthand) {

	case 0:  // no shorthand
	    if (deliver(src_apic,dest_apic,icr,state)) { 
		return -1;
	    }
	    break;

	case 1:  // self
	    if (icr->dst==state->ioapic_id) { 
		PrintError("icc_bus: ioapic attempting to send to itself\n");
		return -1;
	    }
	    if (deliver(src_apic,dest_apic,icr,state)) { 
		return -1;
	    }
	    break;

	case 2: 
	case 3: { // all and all-but-me
	    int i;
	    for (i=0;i<MAX_APICS;i++) { 
		dest_apic=&(state->apics[i]);
		if (dest_apic->present && (i!=src_apic || icr->dst_shorthand==2)) { 
		    if (deliver(src_apic,dest_apic,icr,state)) { 
			return -1;
		    }
		}
	    }
	}
	    break;
    }

    return 0;
}



/* THIS IS A BIG ASSUMPTION: APIC PHYSID == LOGID == CORENUM */

int v3_icc_register_apic(struct guest_info  * core, struct vm_device * icc_bus, 
			 uint8_t apic_num, struct v3_icc_ops * ops, void * priv_data) {
    struct icc_bus_state * icc = (struct icc_bus_state *)icc_bus->private_data;
    struct apic_data * apic = &(icc->apics[apic_num]);

    if (apic->present == 1) {
	PrintError("icc_bus: Attempt to re-register apic %u\n", apic_num);
	return -1;
    }
    
    apic->present = 1;
    apic->priv_data = priv_data;
    apic->core = core;
    apic->ops = ops;
   
    PrintDebug("icc_bus: Registered apic %u\n", apic_num);

    return 0;
}


int v3_icc_register_ioapic(struct v3_vm_info *vm, struct vm_device * icc_bus, uint8_t apic_num)
{
    struct icc_bus_state * icc = (struct icc_bus_state *)icc_bus->private_data;

    if (icc->ioapic_id) { 
	PrintError("icc_bus: Attempt to register a second ioapic!\n");
	return -1;
    }

    icc->ioapic_id=apic_num;

    PrintDebug("icc_bus: Registered ioapic %u\n", apic_num);
    

    return 0;
}



static int icc_bus_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    PrintDebug("icc_bus: Creating ICC_BUS\n");

    char * name = v3_cfg_val(cfg, "name");

    struct icc_bus_state * icc_bus = (struct icc_bus_state *)V3_Malloc(sizeof(struct icc_bus_state));
    memset(icc_bus, 0, sizeof(struct icc_bus_state));

    struct vm_device * dev = v3_allocate_device(name, &dev_ops, icc_bus);

    if (v3_attach_device(vm, dev) == -1) {
        PrintError("icc_bus: Could not attach device %s\n", name);
        return -1;
    }

    return 0;
}



device_register("ICC_BUS", icc_bus_init)
