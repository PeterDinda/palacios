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
#include <devices/apic_regs.h>
#include <devices/apic.h>

#define MAX_APIC 256

struct icc_bus_internal {
    struct vm_device * apic[MAX_APIC];
};

static struct v3_device_ops dev_ops = {
    .free = NULL,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};

int v3_icc_register_apic(struct v3_vm_info *info, struct vm_device *icc_bus, struct vm_device *apic, uint32_t apic_num)
{
    struct icc_bus_internal * icc = (struct icc_bus_internal *)icc_bus->private_data;

    if (apic_num < MAX_APIC) {
        if (icc->apic[apic_num]) {
            PrintError("Attempt to re-register apic %u\n", apic_num);
            return -1;
        } else {
            icc->apic[apic_num] = apic;
            PrintDebug("Registered apic or ioapic %u\n", apic_num);
            return 0;
        }
    } else {
        PrintError("Too many apics for icc bus!");
        return -1;
    }
}

struct ipi_thunk_data {
    struct vm_device *target;
    uint64_t          val;
} ;

static void icc_force_exit(void *val)
{
     return;
}

int v3_icc_send_ipi(struct vm_device * icc_bus, uint32_t apic_num, uint32_t val) {
    struct icc_bus_internal * internal = (struct icc_bus_internal *)icc_bus->private_data;

    struct int_cmd_reg icr;
    icr.lo = val;

    char *type = NULL, *dest = NULL;
    char foo[8];

    switch (icr.dst_shorthand)
    {
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
    switch (icr.msg_type)
    {
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


    PrintDebug("Sending IPI of type %s and destination type %s from LAPIC %u to LAPIC %u.\n", type, dest, V3_Get_CPU(), apic_num);

    v3_apic_raise_intr(internal->apic[apic_num], val & 0xff);

    V3_Call_On_CPU(apic_num,  icc_force_exit, (void *)(uint64_t)(val & 0xff));

    return 0;
}


static int init_icc_bus_internal_state(struct icc_bus_internal* icc) {
    int i;
    for (i=0;i<MAX_APIC;i++) { icc->apic[i]=0; }
    return  0;
}

static int icc_bus_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    PrintDebug("Creating ICC_BUS\n");
    char * name = v3_cfg_val(cfg, "name");

    struct icc_bus_internal * icc_bus = (struct icc_bus_internal *)V3_Malloc(sizeof(struct icc_bus_internal));

    struct vm_device * dev = v3_allocate_device(name, &dev_ops, icc_bus);

    if (v3_attach_device(vm, dev) == -1) {
        PrintError("Could not attach device %s\n", name);
        return -1;
    }

    init_icc_bus_internal_state(icc_bus);

    return 0;
}



device_register("ICC_BUS", icc_bus_init)
