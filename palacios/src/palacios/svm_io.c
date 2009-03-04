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

#include <palacios/svm_io.h>
#include <palacios/vmm_io.h>
#include <palacios/vmm_ctrl_regs.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vm_guest_mem.h>

#ifndef DEBUG_IO
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif





// This should package up an IO request and call vmm_handle_io
int v3_handle_svm_io_in(struct guest_info * info) {
    vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t *)(info->vmm_data));
    //  vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));
    struct svm_io_info * io_info = (struct svm_io_info *)&(ctrl_area->exit_info1);

    struct v3_io_hook * hook = v3_get_io_hook(info, io_info->port);
    int read_size = 0;

    if (hook == NULL) {
	PrintError("Hook Not present for in on port %x\n", io_info->port);
	// error, we should not have exited on this port
	return -1;
    }


    if (io_info->sz8) { 
	read_size = 1;
    } else if (io_info->sz16) {
	read_size = 2;
    } else if (io_info->sz32) {
	read_size = 4;
    }

    PrintDebug("IN of %d bytes on port %d (0x%x)\n", read_size, io_info->port, io_info->port);

    if (hook->read(io_info->port, &(info->vm_regs.rax), read_size, hook->priv_data) != read_size) {
	// not sure how we handle errors.....
	PrintError("Read Failure for in on port %x\n", io_info->port);
	return -1;
    }

    info->rip = ctrl_area->exit_info2;

    return 0;
}





/* We might not handle wrap around of the RDI register correctly...
 * In that if we do wrap around the effect will manifest in the higher bits of the register
 */
int v3_handle_svm_io_ins(struct guest_info * info) {
    vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t *)(info->vmm_data));
    vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));
  
    struct svm_io_info * io_info = (struct svm_io_info *)&(ctrl_area->exit_info1);
  
    struct v3_io_hook * hook = v3_get_io_hook(info, io_info->port);
    int read_size = 0;
    addr_t dst_addr = 0;
    uint_t rep_num = 1;
    ullong_t mask = 0;
    struct v3_segment *theseg = &(info->segments.es); // default is ES
    addr_t inst_ptr;


    // This is kind of hacky...
    // direction can equal either 1 or -1
    // We will multiply the final added offset by this value to go the correct direction
    int direction = 1;
    struct rflags * flags = (struct rflags *)&(guest_state->rflags);  

    if (flags->df) {
	direction = -1;
    }


    if (hook == NULL) {
	PrintError("Hook Not present for ins on port %x\n", io_info->port);
	// error, we should not have exited on this port
	return -1;
    }



    if (guest_va_to_host_va(info, get_addr_linear(info, info->rip, &(info->segments.cs)), &inst_ptr) == -1) {
	PrintError("Can't access instruction\n");
	return -1;
    }

    while (is_prefix_byte(*((char*)inst_ptr))) {
	switch (*((char*)inst_ptr)) { 
	    case PREFIX_CS_OVERRIDE:
		theseg = &(info->segments.cs);
		break;
	    case PREFIX_SS_OVERRIDE:
		theseg = &(info->segments.ss);
		break;
	    case PREFIX_DS_OVERRIDE:
		theseg = &(info->segments.ds);
		break;
	    case PREFIX_ES_OVERRIDE:
		theseg = &(info->segments.es);
		break;
	    case PREFIX_FS_OVERRIDE:
		theseg = &(info->segments.fs);
		break;
	    case PREFIX_GS_OVERRIDE:
		theseg = &(info->segments.gs);
		break;
	    default:
		break;
	}
	inst_ptr++;
    }


    PrintDebug("INS on  port %d (0x%x)\n", io_info->port, io_info->port);

    if (io_info->sz8) {
	read_size = 1;
    } else if (io_info->sz16) {
	read_size = 2;
    } else if (io_info->sz32) {
	read_size = 4;
    } else {
	PrintError("io_info Invalid Size\n");
	return -1;
    }

  
    if (io_info->addr16) {
	mask = 0xffff;
    } else if (io_info->addr32) {
	mask = 0xffffffff;
    } else if (io_info->addr64) {
	mask = 0xffffffffffffffffLL;
    } else {
	// This value should be set depending on the host register size...
	mask = get_gpr_mask(info);

	PrintDebug("INS io_info invalid address size, mask=0x%p, io_info=0x%p\n",
		   (void *)(addr_t)mask, (void *)(addr_t)(io_info));
	// PrintDebug("INS Aborted... Check implementation\n");
	//return -1;
    }

    if (io_info->rep) {
	//    rep_num = info->vm_regs.rcx & mask;
	rep_num = info->vm_regs.rcx;
    }


    PrintDebug("INS size=%d for %d steps\n", read_size, rep_num);

    while (rep_num > 0) {
	addr_t host_addr;
	dst_addr = get_addr_linear(info, info->vm_regs.rdi & mask, theseg);
    
	PrintDebug("Writing 0x%p\n", (void *)dst_addr);

	if (guest_va_to_host_va(info, dst_addr, &host_addr) == -1) {
	    // either page fault or gpf...
	    PrintError("Could not convert Guest VA to host VA\n");
	    return -1;
	}

	if (hook->read(io_info->port, (char*)host_addr, read_size, hook->priv_data) != read_size) {
	    // not sure how we handle errors.....
	    PrintError("Read Failure for ins on port %x\n", io_info->port);
	    return -1;
	}

	info->vm_regs.rdi += read_size * direction;

	if (io_info->rep)
	    info->vm_regs.rcx--;
    
	rep_num--;
    }


    info->rip = ctrl_area->exit_info2;

    return 0;
}

int v3_handle_svm_io_out(struct guest_info * info) {
    vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t *)(info->vmm_data));
    //  vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));
    struct svm_io_info * io_info = (struct svm_io_info *)&(ctrl_area->exit_info1);

    struct v3_io_hook * hook = v3_get_io_hook(info, io_info->port);
    int write_size = 0;

    if (hook == NULL) {
	PrintError("Hook Not present for out on port %x\n", io_info->port);
	// error, we should not have exited on this port
	return -1;
    }


    if (io_info->sz8) { 
	write_size = 1;
    } else if (io_info->sz16) {
	write_size = 2;
    } else if (io_info->sz32) {
	write_size = 4;
    }

    PrintDebug("OUT of %d bytes on  port %d (0x%x)\n", write_size, io_info->port, io_info->port);

    if (hook->write(io_info->port, &(info->vm_regs.rax), write_size, hook->priv_data) != write_size) {
	// not sure how we handle errors.....
	PrintError("Write Failure for out on port %x\n", io_info->port);
	return -1;
    }

    info->rip = ctrl_area->exit_info2;

    return 0;
}


/* We might not handle wrap around of the RSI register correctly...
 * In that if we do wrap around the effect will manifest in the higher bits of the register
 */

int v3_handle_svm_io_outs(struct guest_info * info) {
    vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t *)(info->vmm_data));
    vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));

  
    struct svm_io_info * io_info = (struct svm_io_info *)&(ctrl_area->exit_info1);
  
    struct v3_io_hook * hook = v3_get_io_hook(info, io_info->port);
    int write_size = 0;
    addr_t dst_addr = 0;
    uint_t rep_num = 1;
    ullong_t mask = 0;
    addr_t inst_ptr;
    struct v3_segment * theseg = &(info->segments.es); // default is ES

    // This is kind of hacky...
    // direction can equal either 1 or -1
    // We will multiply the final added offset by this value to go the correct direction
    int direction = 1;
    struct rflags * flags = (struct rflags *)&(guest_state->rflags);  

    if (flags->df) {
	direction = -1;
    }


    if (hook == NULL) {
	PrintError("Hook Not present for outs on port %x\n", io_info->port);
	// error, we should not have exited on this port
	return -1;
    }

    PrintDebug("OUTS on  port %d (0x%x)\n", io_info->port, io_info->port);

    if (io_info->sz8) { 
	write_size = 1;
    } else if (io_info->sz16) {
	write_size = 2;
    } else if (io_info->sz32) {
	write_size = 4;
    }


    if (io_info->addr16) {
	mask = 0xffff;
    } else if (io_info->addr32) {
	mask = 0xffffffff;
    } else if (io_info->addr64) {
	mask = 0xffffffffffffffffLL;
    } else {
	// This value should be set depending on the host register size...
	mask = get_gpr_mask(info);

	PrintDebug("OUTS io_info invalid address size, mask=0%p, io_info=0x%p\n",
		   (void *)(addr_t)mask, (void *)(addr_t)io_info);
	// PrintDebug("INS Aborted... Check implementation\n");
	//return -1;
	// should never happen
	//PrintDebug("Invalid Address length\n");
	//return -1;
    }

    if (io_info->rep) {
	rep_num = info->vm_regs.rcx & mask;
    }

  


    if (guest_va_to_host_va(info,get_addr_linear(info,info->rip,&(info->segments.cs)),&inst_ptr)==-1) {
	PrintError("Can't access instruction\n");
	return -1;
    }

    while (is_prefix_byte(*((char*)inst_ptr))) {
	switch (*((char*)inst_ptr)) { 
	    case PREFIX_CS_OVERRIDE:
		theseg = &(info->segments.cs);
		break;
	    case PREFIX_SS_OVERRIDE:
		theseg = &(info->segments.ss);
		break;
	    case PREFIX_DS_OVERRIDE:
		theseg = &(info->segments.ds);
		break;
	    case PREFIX_ES_OVERRIDE:
		theseg = &(info->segments.es);
		break;
	    case PREFIX_FS_OVERRIDE:
		theseg = &(info->segments.fs);
		break;
	    case PREFIX_GS_OVERRIDE:
		theseg = &(info->segments.gs);
		break;
	    default:
		break;
	}
	inst_ptr++;
    }

    PrintDebug("OUTS size=%d for %d steps\n", write_size, rep_num);

    while (rep_num > 0) {
	addr_t host_addr;

	dst_addr = get_addr_linear(info, (info->vm_regs.rsi & mask), theseg);
    
	if (guest_va_to_host_va(info, dst_addr, &host_addr) == -1) {
	    // either page fault or gpf...
	}

	if (hook->write(io_info->port, (char*)host_addr, write_size, hook->priv_data) != write_size) {
	    // not sure how we handle errors.....
	    PrintError("Write Failure for outs on port %x\n", io_info->port);
	    return -1;
	}

	info->vm_regs.rsi += write_size * direction;

	if (io_info->rep) {
	    info->vm_regs.rcx--;
	}

	rep_num--;
    }


    info->rip = ctrl_area->exit_info2;


    return 0;
}
