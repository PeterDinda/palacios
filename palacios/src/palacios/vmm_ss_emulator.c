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

#include <palacios/vmm.h>
#include <palacios/vmm_emulator.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmcb.h>
#include <palacios/vmm_ctrl_regs.h>


#ifndef CONFIG_DEBUG_EMULATOR
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


int v3_init_emulator(struct guest_info * info) {


    emulator->tf_enabled = 0;

    return 0;
}










static int set_stepping(struct guest_info * info) {
    vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));
    ctrl_area->exceptions.db = 1;

    info->emulator.tf_enabled = ((struct rflags *)&(info->ctrl_regs.rflags))->tf;

    ((struct rflags *)&(info->ctrl_regs.rflags))->tf = 1;

    return 0;
}


static int unset_stepping(struct guest_info * info) {
    vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));
    ctrl_area->exceptions.db = 0;

    ((struct rflags *)&(info->ctrl_regs.rflags))->tf = info->emulator.tf_enabled;

    if (info->emulator.tf_enabled) {
	// Inject breakpoint exception into guest
    }

    return 0;

}


int v3_emulate_memory_read(struct guest_info * info, addr_t read_gva, 
			   int (*read)(addr_t read_addr, void * dst, uint_t length, void * priv_data), 
			   addr_t read_gpa, void * private_data) {
    struct basic_instr_info instr_info;
    uchar_t instr[15];
    int ret;
    addr_t data_addr_offset = PAGE_OFFSET(read_gva);
 
    PrintDebug("Emulating Read\n");

    if (info->mem_mode == PHYSICAL_MEM) { 
	ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    } else { 
	ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    }

    if (ret == -1) {
	PrintError("Could not read guest memory\n");
	return -1;
    }

#ifdef CONFIG_DEBUG_EMULATOR
    PrintDebug("Instr (15 bytes) at %p:\n", (void *)(addr_t)instr);
    PrintTraceMemDump(instr, 15);
#endif  


    if (v3_basic_mem_decode(info, (addr_t)instr, &instr_info) == -1) {
	PrintError("Could not do a basic memory instruction decode\n");
	V3_Free(data_page);
	return -1;
    }

    // Read the data directly onto the emulated page
    ret = read(read_gpa, (void *)(data_page->page_addr + data_addr_offset), instr_info.op_size, private_data);
    if ((ret == -1) || ((uint_t)ret != instr_info.op_size)) {
	PrintError("Read error in emulator\n");
	return -1;
    }

 

    // setup_code_page(info, instr, &instr_info);
    set_stepping(info);

    info->emulator.running = 1;
    info->run_state = VM_EMULATING;
    info->emulator.instr_length = instr_info.instr_length;

    return 0;
}



int v3_emulate_memory_write(struct guest_info * info, addr_t write_gva,
			    int (*write)(addr_t write_addr, void * src, uint_t length, void * priv_data), 
			    addr_t write_gpa, void * private_data) {

    struct basic_instr_info instr_info;
    uchar_t instr[15];
    int ret;
    addr_t data_addr_offset = PAGE_OFFSET(write_gva);
    int i;

    PrintDebug("Emulating Write for instruction at 0x%p\n", (void *)(addr_t)(info->rip));

    if (info->mem_mode == PHYSICAL_MEM) { 
	ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    } else { 
	ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    }


  
    if (v3_basic_mem_decode(info, (addr_t)instr, &instr_info) == -1) {
	PrintError("Could not do a basic memory instruction decode\n");
	V3_Free(write_op);
	V3_Free(data_page);
	return -1;
    }

    if (instr_info.has_rep==1) { 
	PrintDebug("Emulated instruction has rep\n");
    }


    if (info->emulator.running == 0) {
	//    setup_code_page(info, instr, &instr_info);
	set_stepping(info);
	info->emulator.running = 1;
	info->run_state = VM_EMULATING;
	info->emulator.instr_length = instr_info.instr_length;
    }

    return 0;
}


// end emulation
int v3_emulation_exit_handler(struct guest_info * info) {
  
    unset_stepping(info);


    PrintDebug("returning from emulation\n");

    return 0;
}
