
/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2012, Jack Lange <jarusl@cs.pitt.edu> 
 * Copyright (c) 2012, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.pitt.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmm_extensions.h>
#include <palacios/vmm_io.h>
#include <palacios/vmm_cpuid.h>
#include <palacios/vm_guest.h>


#define VMWARE_CPUID_LEAF 0x40000000
#define VMWARE_MAGIC 0x564D5868
#define VMWARE_IO_PORT  0x5658

#define VMWARE_IO_VERSION 10
#define VMWARE_IO_GETHZ 45


static int io_read(struct guest_info * core, uint16_t port, void * dst, uint_t length, void * priv_data) {
    uint64_t cpu_hz = V3_CPU_KHZ() * 1000;
    uint32_t magic = (uint32_t)(core->vm_regs.rax);
    uint32_t cmd = (uint32_t)(core->vm_regs.rcx);

    PrintError("VMWARE IO READ of size %d (command=%d)\n", length, cmd);

    
    if (magic != VMWARE_MAGIC) {
	PrintError("Invalid VMWARE MAgic number in Persona interface, ignoring for now\n");
	return length;
    }
    
    if (cmd == VMWARE_IO_GETHZ) {
	// EAX Takes low bytes
	// EBX takes high bytes
	core->vm_regs.rax = cpu_hz & 0x00000000ffffffffLL;
	core->vm_regs.rbx = (cpu_hz >> 32) & 0x00000000ffffffffLL;
    } else {
	PrintError("Unhandled VMWARE IO operation\n");
	return -1;
    }

    return length;
}


static int io_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * priv_data) {

    PrintError("VMWARE IO PORT WRITE\n");
    return -1;
}


static int vmware_cpuid_handler(struct guest_info * core, uint32_t cpuid, 
				uint32_t * eax, uint32_t * ebx, 
				uint32_t * ecx, uint32_t * edx, 
				void * priv_data) {

    // Don't Care (?)
    *eax = 0;

    // Set VMWARE Vendor string in EBX,ECX,EDX
    memcpy(ebx, "VMwa", 4);
    memcpy(ecx, "reVM", 4);
    memcpy(edx, "ware", 4);
    
    return 0;
}



static int vmware_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg, void ** priv_data) {

    V3_Print("Using VMWARE virtualization persona\n");

    v3_cpuid_add_fields(vm, 0x00000001, 
			0, 0, 
			0, 0, 
			0x80000000, 0x80000000,
			0, 0
			);


    v3_hook_io_port(vm, VMWARE_IO_PORT, 
		    io_read, io_write, 
		    NULL);

    v3_hook_cpuid(vm, VMWARE_CPUID_LEAF, 
		  vmware_cpuid_handler, NULL);
			 

    // hook io port 
    // set CPUID hypervisor enabled
    // set VMWare CPUID 

    
    return 0;


}




static struct v3_extension_impl vmware_impl = {
    .name = "VMWARE_IFACE",
    .init = vmware_init,
    .deinit = NULL,
    .core_init = NULL,
    .core_deinit = NULL,
    .on_entry = NULL,
    .on_exit = NULL
};



register_extension(&vmware_impl);
