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

#define MAX_APICS 256


struct ipi_thunk_data {
    struct vm_device * target;
    uint64_t val;
};

struct int_cmd_reg {
    union {
        uint64_t val;

        struct {
            uint32_t lo;
            uint32_t hi;
        } __attribute__((packed));

        struct {
            uint_t vec           : 8;
            uint_t msg_type      : 3;
            uint_t dst_mode      : 1;
            uint_t del_status    : 1;
            uint_t rsvd1         : 1;
            uint_t lvl           : 1;
            uint_t trig_mode     : 1;
            uint_t rem_rd_status : 2;
            uint_t dst_shorthand : 2;
            uint64_t rsvd2       : 36;
            uint32_t dst         : 8;
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));




struct apic_data {
    struct guest_info * core;
    struct v3_icc_ops * ops;
    
    void * priv_data;
    int present;
};


struct icc_bus_state {
    struct apic_data apics[MAX_APICS];
};

static struct v3_device_ops dev_ops = {
    .free = NULL,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};




int v3_icc_send_irq(struct vm_device * icc_bus, uint8_t apic_num, uint32_t irq_num) {
    struct icc_bus_state * state = (struct icc_bus_state *)icc_bus->private_data;
    struct apic_data * apic = &(state->apics[apic_num]);    


    struct int_cmd_reg icr;
    icr.lo = irq_num;


    char * type = NULL;
    char * dest = NULL;
    char foo[8];

    switch (icr.dst_shorthand) {
	case 0x0:
	    sprintf(foo, "%d", icr.dst);
	    dest = foo;
	    break;
	case 0x1:
	    dest = "(self)";
	    break;
	case 0x2:
	    dest = "(broadcast inclusive)";
	    break;
	case 0x3:
	    dest = "(broadcast)";
	    break;
    }

    switch (icr.msg_type) {
	case 0x0:
	    type = "";
	    break;
	case 0x4:
	    type = "(NMI)";
	    break;
	case 0x5:
	    type = "(INIT)";
	    break;
	case 0x6:
	    type = "(Startup)";
	    break;
    }


    PrintDebug("Sending IPI of type %s and destination type %s from LAPIC %u to LAPIC %u.\n", 
	       type, dest, V3_Get_CPU(), apic_num);

    apic->ops->raise_intr(apic->core, irq_num & 0xff, apic->priv_data);

    //V3_Call_On_CPU(apic_num,  icc_force_exit, (void *)(uint64_t)(val & 0xff));

    return 0;
}



/* THIS IS A BIG ASSUMPTION: APIC PHYSID == LOGID == CORENUM */

int v3_icc_register_apic(struct guest_info  * core, struct vm_device * icc_bus, 
			 uint8_t apic_num, struct v3_icc_ops * ops, void * priv_data) {
    struct icc_bus_state * icc = (struct icc_bus_state *)icc_bus->private_data;
    struct apic_data * apic = &(icc->apics[apic_num]);

    if (apic->present == 1) {
	PrintError("Attempt to re-register apic %u\n", apic_num);
	return -1;
    }
    
    apic->present = 1;
    apic->priv_data = priv_data;
    apic->core = core;
    apic->ops = ops;
   
    PrintDebug("Registered apic%u\n", apic_num);

    return 0;
}




static int icc_bus_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    PrintDebug("Creating ICC_BUS\n");

    char * name = v3_cfg_val(cfg, "name");

    struct icc_bus_state * icc_bus = (struct icc_bus_state *)V3_Malloc(sizeof(struct icc_bus_state));
    memset(icc_bus, 0, sizeof(struct icc_bus_state));

    struct vm_device * dev = v3_allocate_device(name, &dev_ops, icc_bus);

    if (v3_attach_device(vm, dev) == -1) {
        PrintError("Could not attach device %s\n", name);
        return -1;
    }

    return 0;
}



device_register("ICC_BUS", icc_bus_init)
