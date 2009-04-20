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

#include <palacios/vmm_config.h>
#include <palacios/vmm.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_msr.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_profiler.h>
#include <palacios/vmm_mem.h>
#include <palacios/vmm_hypercall.h>


#include <devices/serial.h>
#include <devices/keyboard.h>
#include <devices/8259a.h>
#include <devices/8254.h>
#include <devices/nvram.h>
#include <devices/generic.h>
#include <devices/ide.h>
#include <devices/ram_cd.h>
#include <devices/ram_hd.h>
#include <devices/bochs_debug.h>
#include <devices/os_debug.h>
#include <devices/apic.h>
#include <devices/io_apic.h>
#include <devices/para_net.h>
#include <devices/pci.h>
#include <devices/i440fx.h>
#include <devices/piix3.h>


#include <palacios/vmm_host_events.h>

#define USE_GENERIC 1





static int setup_memory_map(struct guest_info * info, struct v3_vm_config * config_ptr);
static int setup_devices(struct guest_info * info, struct v3_vm_config * config_ptr);
static struct vm_device *  configure_generic(struct guest_info * info, struct v3_vm_config * config_ptr);




static int passthrough_mem_write(addr_t guest_addr, void * src, uint_t length, void * priv_data) {

    return length;
    //  memcpy((void*)guest_addr, src, length);
    PrintDebug("Write of %d bytes to %p\n", length, (void *)guest_addr);
    PrintDebug("Write Value = %p\n", (void *)*(addr_t *)src);
    
    return length;
}



int v3_config_guest(struct guest_info * info, struct v3_vm_config * config_ptr) {
    extern v3_cpu_arch_t v3_cpu_type;

    // Amount of ram the Guest will have, rounded to a 4K page boundary
    info->mem_size = config_ptr->mem_size & ~(addr_t)0xfff;

    // Initialize the subsystem data strutures
    v3_init_time(info);
    v3_init_io_map(info);
    v3_init_msr_map(info);
    v3_init_interrupt_state(info);
    v3_init_dev_mgr(info);
    v3_init_host_events(info);
    
    v3_init_decoder(info);
    
    v3_init_hypercall_map(info);

    
    // Initialize the memory map
    v3_init_shadow_map(info);
    
    
    if ((v3_cpu_type == V3_SVM_REV3_CPU) && 
	(config_ptr->enable_nested_paging == 1)) {
	PrintDebug("Guest Page Mode: NESTED_PAGING\n");
	info->shdw_pg_mode = NESTED_PAGING;
    } else {
	PrintDebug("Guest Page Mode: SHADOW_PAGING\n");
	v3_init_shadow_page_state(info);
	info->shdw_pg_mode = SHADOW_PAGING;
    }
    
    // Initial CPU operating mode
    info->cpu_mode = REAL;
    info->mem_mode = PHYSICAL_MEM;
    
    // Configure the memory map for the guest
    if (setup_memory_map(info, config_ptr) == -1) {
	PrintError("Setting up guest memory map failed...\n");
	return -1;
    }
    
    // Configure the devices for the guest
    setup_devices(info, config_ptr);
    
    if (config_ptr->enable_profiling) {
	info->enable_profiler = 1;
	v3_init_profiler(info);
    } else {
	info->enable_profiler = 0;
    }

    //v3_hook_io_port(info, 1234, &IO_Read, NULL, info);
    
    // Setup initial cpu register state
    info->rip = 0xfff0;
    info->vm_regs.rsp = 0x0;
  
    
    return 0;
}


/* TODO:
 * The amount of guest memory is stored in info->mem_size
 * We need to make sure the memory map extends to cover it
 */
static int setup_memory_map(struct guest_info * info, struct v3_vm_config * config_ptr) {
    PrintDebug("Setting up memory map (memory size=%dMB)\n", (uint_t)(info->mem_size / (1024 * 1024)));
    
    // VGA frame buffer
    if (1) {
	if (v3_add_shadow_mem(info, 0xa0000, 0xc0000, 0xa0000) == -1) {
	    PrintError("Could not map VGA framebuffer\n");
	    return -1;
	}
    } else {
	v3_hook_write_mem(info, 0xa0000, 0xc0000, 0xa0000,  passthrough_mem_write, NULL);
    }  

#define VGABIOS_START 0x000c0000
#define ROMBIOS_START 0x000f0000

    /* layout vgabios */
    {
	addr_t vgabios_dst = v3_get_shadow_addr(&(info->mem_map.base_region), VGABIOS_START);
	memcpy(V3_VAddr((void *)vgabios_dst), config_ptr->vgabios, config_ptr->vgabios_size);	
    }
    
    /* layout rombios */
    {
	addr_t rombios_dst = v3_get_shadow_addr(&(info->mem_map.base_region), ROMBIOS_START);
	memcpy(V3_VAddr((void *)rombios_dst), config_ptr->rombios, config_ptr->rombios_size);
    }

#ifdef CRAY_XT
    {
#define SEASTAR_START 0xffe00000 
#define SEASTAR_END 0xffffffff 
	// Map the Seastar straight through
	if (v3_add_shadow_mem(info, SEASTAR_START, SEASTAR_END, SEASTAR_START) == -1) {
	    PrintError("Could not map through the seastar\n");
	    return -1;
	}
    }
#endif    

    print_shadow_map(info);

    return 0;
}



static int setup_devices(struct guest_info * info, struct v3_vm_config * config_ptr) {

    struct vm_device * ide = NULL;
    struct vm_device * ramdisk = NULL;
    
    struct vm_device * pci = NULL;
    struct vm_device * northbridge = NULL;
    struct vm_device * southbridge = NULL;

    struct vm_device * nvram = NULL;
    struct vm_device * pic = v3_create_pic();
    struct vm_device * keyboard = v3_create_keyboard();
    struct vm_device * pit = v3_create_pit(); 
    struct vm_device * bochs_debug = v3_create_bochs_debug();
    struct vm_device * os_debug = v3_create_os_debug();
    struct vm_device * apic = v3_create_apic();
    struct vm_device * ioapic = v3_create_io_apic(apic);
    struct vm_device * para_net = v3_create_para_net();


    //struct vm_device * serial = v3_create_serial();
    struct vm_device * generic = NULL;


    int use_generic = USE_GENERIC;

    if (config_ptr->enable_pci == 1) {
	pci = v3_create_pci();
	northbridge = v3_create_i440fx(pci);
	southbridge = v3_create_piix3(pci);
	ide = v3_create_ide(pci, southbridge);
    } else {
	ide = v3_create_ide(NULL, NULL);
    }



    nvram = v3_create_nvram(ide);

    if (config_ptr->use_ram_cd == 1) {
	PrintDebug("Creating Ram CD\n");
	ramdisk = v3_create_ram_cd(ide, 0, 0, 
				   (addr_t)(config_ptr->ramdisk), 
				   config_ptr->ramdisk_size);
    } else if (config_ptr->use_ram_hd == 1) {
	PrintDebug("Creating Ram HD\n");
	ramdisk = v3_create_ram_hd(ide, 0, 0, 
				   (addr_t)(config_ptr->ramdisk), 
				   config_ptr->ramdisk_size);
    }
    
    
    if (use_generic) {
	generic = configure_generic(info, config_ptr);
    }





    v3_attach_device(info, pic);
    v3_attach_device(info, pit);
    v3_attach_device(info, keyboard);
    // v3_attach_device(info, serial);
    v3_attach_device(info, bochs_debug);
    v3_attach_device(info, os_debug);

    v3_attach_device(info, apic);
    v3_attach_device(info, ioapic);

    v3_attach_device(info, para_net);

    if (config_ptr->enable_pci == 1) {
	PrintDebug("Attaching PCI\n");
	v3_attach_device(info, pci);
	PrintDebug("Attaching Northbridge\n");
	v3_attach_device(info, northbridge);
	PrintDebug("Attaching Southbridge\n");
	v3_attach_device(info, southbridge);
    }

    PrintDebug("Attaching IDE\n");
    v3_attach_device(info, ide);

    if (ramdisk != NULL) {
	v3_attach_device(info, ramdisk);
    }

    if (use_generic) {
	// Important that this be attached last!
	v3_attach_device(info, generic);
    }

    // This should go last because it contains the hardware state
    v3_attach_device(info, nvram);
    
    PrintDebugDevMgr(info);

    return 0;
}



static struct vm_device *  configure_generic(struct guest_info * info, struct v3_vm_config * config_ptr) {
    PrintDebug("Creating Generic Device\n");
    struct vm_device * generic = v3_create_generic();
    
    // Make the DMA controller invisible
    v3_generic_add_port_range(generic, 0x00, 0x07, GENERIC_PRINT_AND_IGNORE);   // DMA 1 channels 0,1,2,3 (address, counter)
    v3_generic_add_port_range(generic, 0xc0, 0xc7, GENERIC_PRINT_AND_IGNORE);   // DMA 2 channels 4,5,6,7 (address, counter)
    v3_generic_add_port_range(generic, 0x87, 0x87, GENERIC_PRINT_AND_IGNORE);   // DMA 1 channel 0 page register
    v3_generic_add_port_range(generic, 0x83, 0x83, GENERIC_PRINT_AND_IGNORE);   // DMA 1 channel 1 page register
    v3_generic_add_port_range(generic, 0x81, 0x81, GENERIC_PRINT_AND_IGNORE);   // DMA 1 channel 2 page register
    v3_generic_add_port_range(generic, 0x82, 0x82, GENERIC_PRINT_AND_IGNORE);   // DMA 1 channel 3 page register
    v3_generic_add_port_range(generic, 0x8f, 0x8f, GENERIC_PRINT_AND_IGNORE);   // DMA 2 channel 4 page register
    v3_generic_add_port_range(generic, 0x8b, 0x8b, GENERIC_PRINT_AND_IGNORE);   // DMA 2 channel 5 page register
    v3_generic_add_port_range(generic, 0x89, 0x89, GENERIC_PRINT_AND_IGNORE);   // DMA 2 channel 6 page register
    v3_generic_add_port_range(generic, 0x8a, 0x8a, GENERIC_PRINT_AND_IGNORE);   // DMA 2 channel 7 page register
    v3_generic_add_port_range(generic, 0x08, 0x0f, GENERIC_PRINT_AND_IGNORE);   // DMA 1 misc registers (csr, req, smask,mode,clearff,reset,enable,mmask)
    v3_generic_add_port_range(generic, 0xd0, 0xde, GENERIC_PRINT_AND_IGNORE);   // DMA 2 misc registers
    
    
    
    
    // Make the Serial ports invisible 
    
    v3_generic_add_port_range(generic, 0x3f8, 0x3f8+7, GENERIC_PRINT_AND_IGNORE);      // COM 1
    v3_generic_add_port_range(generic, 0x2f8, 0x2f8+7, GENERIC_PRINT_AND_IGNORE);      // COM 2
    

      

    v3_generic_add_port_range(generic, 0x3e8, 0x3e8+7, GENERIC_PRINT_AND_IGNORE);      // COM 3
    v3_generic_add_port_range(generic, 0x2e8, 0x2e8+7, GENERIC_PRINT_AND_IGNORE);      // COM 4

      
      

    // Make the PCI bus invisible (at least it's configuration)
    
    //v3_generic_add_port_range(generic, 0xcf8, 0xcf8, GENERIC_PRINT_AND_IGNORE); // PCI Config Address
    //v3_generic_add_port_range(generic, 0xcfc, 0xcfc, GENERIC_PRINT_AND_IGNORE); // PCI Config Data
    
    
    
#if 0
    if (!use_ramdisk) {
	// Monitor the IDE controllers (very slow)
	v3_generic_add_port_range(generic, 0x170, 0x178, GENERIC_PRINT_AND_PASSTHROUGH); // IDE 1
	v3_generic_add_port_range(generic, 0x376, 0x377, GENERIC_PRINT_AND_PASSTHROUGH); // IDE 1
    }
      

    v3_generic_add_port_range(generic, 0x1f0, 0x1f8, GENERIC_PRINT_AND_PASSTHROUGH); // IDE 0
    v3_generic_add_port_range(generic, 0x3f6, 0x3f7, GENERIC_PRINT_AND_PASSTHROUGH); // IDE 0
#endif
      
      
#if 0
    
    // Make the floppy controllers invisible
    
    v3_generic_add_port_range(generic, 0x3f0, 0x3f2, GENERIC_PRINT_AND_IGNORE); // Primary floppy controller (base,statusa/statusb,DOR)
    v3_generic_add_port_range(generic, 0x3f4, 0x3f5, GENERIC_PRINT_AND_IGNORE); // Primary floppy controller (mainstat/datarate,data)
    v3_generic_add_port_range(generic, 0x3f7, 0x3f7, GENERIC_PRINT_AND_IGNORE); // Primary floppy controller (DIR)
    v3_generic_add_port_range(generic, 0x370, 0x372, GENERIC_PRINT_AND_IGNORE); // Secondary floppy controller (base,statusa/statusb,DOR)
    v3_generic_add_port_range(generic, 0x374, 0x375, GENERIC_PRINT_AND_IGNORE); // Secondary floppy controller (mainstat/datarate,data)
    v3_generic_add_port_range(generic, 0x377, 0x377, GENERIC_PRINT_AND_IGNORE); // Secondary floppy controller (DIR)
    
#endif

#if 1

    // Make the parallel port invisible
      
    v3_generic_add_port_range(generic, 0x378, 0x37f, GENERIC_PRINT_AND_IGNORE);

#endif

#if 1

    // Monitor graphics card operations
    
    v3_generic_add_port_range(generic, 0x3b0, 0x3bb, GENERIC_PRINT_AND_PASSTHROUGH);
    v3_generic_add_port_range(generic, 0x3c0, 0x3df, GENERIC_PRINT_AND_PASSTHROUGH);
      
#endif


#if 1
    // Make the ISA PNP features invisible
    
    v3_generic_add_port_range(generic, 0x274, 0x277, GENERIC_PRINT_AND_IGNORE);
    v3_generic_add_port_range(generic, 0x279, 0x279, GENERIC_PRINT_AND_IGNORE);
    v3_generic_add_port_range(generic, 0xa79, 0xa79, GENERIC_PRINT_AND_IGNORE);
#endif


#if 1
    // Monitor any network card (realtek ne2000) operations 
    v3_generic_add_port_range(generic, 0xc100, 0xc1ff, GENERIC_PRINT_AND_PASSTHROUGH);
#endif



    //  v3_generic_add_port_range(generic, 0x378, 0x400, GENERIC_PRINT_AND_IGNORE);
    

    return generic;
}
