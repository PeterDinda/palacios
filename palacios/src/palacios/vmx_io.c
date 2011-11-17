/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Andy Gocke <agocke@gmail.com>
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Andy Gocke <agocke@gmail.com>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmx_io.h>
#include <palacios/vmm_io.h>
#include <palacios/vmm.h>
#include <palacios/vmx_handler.h>
#include <palacios/vmm_ctrl_regs.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_decoder.h>

#ifndef V3_CONFIG_DEBUG_IO
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


/* Same as SVM */
static int update_map(struct v3_vm_info * vm, uint16_t port, int hook_read, int hook_write) {
    uint8_t * bitmap = (uint8_t *)(vm->io_map.arch_data);
    int major = port / 8;
    int minor = port % 8;

    if ((hook_read == 0) && (hook_write == 0)) {
	*(bitmap + major) &= ~(0x1 << minor);
    } else {
	*(bitmap + major) |= (0x1 << minor);
    }

    return 0;
}

int v3_init_vmx_io_map(struct v3_vm_info * vm) {
    vm->io_map.update_map = update_map;
    
    vm->io_map.arch_data = V3_VAddr(V3_AllocPages(2));
    memset(vm->io_map.arch_data, 0xff, PAGE_SIZE_4KB * 2);

    v3_refresh_io_map(vm);

    return 0;
}

int v3_deinit_vmx_io_map(struct v3_vm_info * vm) {
    V3_FreePages(V3_PAddr(vm->io_map.arch_data), 2);
    return 0;
}


int v3_handle_vmx_io_in(struct guest_info * core, struct vmx_exit_info * exit_info) {
    struct vmx_exit_io_qual io_qual = *(struct vmx_exit_io_qual *)&(exit_info->exit_qual);;
    struct v3_io_hook * hook = NULL;
    int read_size = 0;

    hook = v3_get_io_hook(core->vm_info, io_qual.port);

    read_size = io_qual.access_size + 1;

    PrintDebug("IN of %d bytes on port %d (0x%x)\n", read_size, io_qual.port, io_qual.port);

    if (hook == NULL) {
	PrintDebug("IN operation on unhooked IO port 0x%x - returning zeros\n", io_qual.port);
	core->vm_regs.rax >>= 8*read_size;
	core->vm_regs.rax <<= 8*read_size;

    } else {
	if (hook->read(core, io_qual.port, &(core->vm_regs.rax), read_size, hook->priv_data) != read_size) {
	    PrintError("Read failure for IN on port %x\n", io_qual.port);
	    return -1;
	}
    }
    

    core->rip += exit_info->instr_len;

    return 0;
}

int v3_handle_vmx_io_ins(struct guest_info * core, struct vmx_exit_info * exit_info) {
    struct vmx_exit_io_qual io_qual = *(struct vmx_exit_io_qual *)&(exit_info->exit_qual);;
    struct v3_io_hook * hook = NULL;
    int read_size = 0;
    addr_t guest_va = exit_info->guest_linear_addr;
    addr_t host_addr = 0;
    int rdi_change = 0;
    uint32_t rep_num = 1;
    struct rflags * flags = (struct rflags *)&(core->ctrl_regs.rflags);

    hook = v3_get_io_hook(core->vm_info, io_qual.port);


    PrintDebug("INS on port 0x%x\n", io_qual.port);

    read_size = io_qual.access_size + 1;

    if (io_qual.rep) {
        struct vmx_exit_io_instr_info instr_info = *(struct vmx_exit_io_instr_info *)&(exit_info->instr_info);

        if (instr_info.addr_size == 0) {
            rep_num = core->vm_regs.rcx & 0xffff;
        } else if(instr_info.addr_size == 1) {
            rep_num = core->vm_regs.rcx & 0xffffffff;
        } else if(instr_info.addr_size == 2) {
            rep_num = core->vm_regs.rcx & 0xffffffffffffffffLL;
        } else {
            PrintDebug("Unknown INS address size!\n");
            return -1;
        }
    }
    
    if (flags->df) {
        rdi_change = -read_size;
    } else {
        rdi_change = read_size;
    }

    PrintDebug("INS size=%d for %ld steps\n", read_size, rep_num);



    if (v3_gva_to_hva(core, guest_va, &host_addr) == -1) {
        PrintError("Could not convert Guest VA to host VA\n");
        return -1;
    }

    do {

	if (hook == NULL) {
	    PrintDebug("INS operation on unhooked IO port 0x%x - returning zeros\n", io_qual.port);
	    
	    memset((char*)host_addr,0,read_size);

	} else {
	    if (hook->read(core, io_qual.port, (char *)host_addr, read_size, hook->priv_data) != read_size) {
		PrintError("Read Failure for INS on port 0x%x\n", io_qual.port);
		return -1;
	    }
	}
	

        host_addr += rdi_change;
        core->vm_regs.rdi += rdi_change;

        if (io_qual.rep) {
            core->vm_regs.rcx--;
        }
        
    } while (--rep_num > 0);


    core->rip += exit_info->instr_len;

    return 0;
}



int v3_handle_vmx_io_out(struct guest_info * core, struct vmx_exit_info * exit_info) {
    struct vmx_exit_io_qual io_qual = *(struct vmx_exit_io_qual *)&(exit_info->exit_qual);
    struct v3_io_hook * hook = NULL;
    int write_size = 0;

    hook =  v3_get_io_hook(core->vm_info, io_qual.port);


    write_size = io_qual.access_size + 1;
    
    PrintDebug("OUT of %d bytes on port %d (0x%x)\n", write_size, io_qual.port, io_qual.port);

    if (hook == NULL) {
	PrintDebug("OUT operation on unhooked IO port 0x%x - ignored\n", io_qual.port);
    } else {  
	if (hook->write(core, io_qual.port, &(core->vm_regs.rax), write_size, hook->priv_data) != write_size) {
	    PrintError("Write failure for out on port %x\n",io_qual.port);
	    return -1;
	}
    }

    core->rip += exit_info->instr_len;

    return 0;
}



int v3_handle_vmx_io_outs(struct guest_info * core, struct vmx_exit_info * exit_info) {
    struct vmx_exit_io_qual io_qual = *(struct vmx_exit_io_qual *)&(exit_info->exit_qual);
    struct v3_io_hook * hook = NULL;
    int write_size;
    addr_t guest_va = exit_info->guest_linear_addr;
    addr_t host_addr;
    int rsi_change;
    uint32_t rep_num = 1;
    struct rflags * flags = (struct rflags *)&(core->ctrl_regs.rflags);

    hook = v3_get_io_hook(core->vm_info, io_qual.port);

    PrintDebug("OUTS on port 0x%x\n", io_qual.port);

    write_size = io_qual.access_size + 1;

    if (io_qual.rep) {
        // Grab the address sized bits of rcx
        struct vmx_exit_io_instr_info instr_info = *(struct vmx_exit_io_instr_info *)&(exit_info->instr_info);

        if (instr_info.addr_size == 0) {
            rep_num = core->vm_regs.rcx & 0xffff;
        } else if(instr_info.addr_size == 1) {
            rep_num = core->vm_regs.rcx & 0xffffffff;
        } else if(instr_info.addr_size == 2) {
            rep_num = core->vm_regs.rcx & 0xffffffffffffffffLL;
        } else {
            PrintDebug("Unknown INS address size!\n");
            return -1;
        }
    }

    if (flags->df) {
        rsi_change = -write_size;
    } else {
        rsi_change = write_size;
    }



    PrintDebug("OUTS size=%d for %ld steps\n", write_size, rep_num);

    if (v3_gva_to_hva(core, guest_va, &host_addr) == -1) {
        PrintError("Could not convert guest VA to host VA\n");
        return -1;
    }

    do {

	if (hook == NULL) {
	    PrintDebug("OUTS operation on unhooked IO port 0x%x - ignored\n", io_qual.port);
	} else {
	    if (hook->write(core, io_qual.port, (char *)host_addr, write_size, hook->priv_data) != write_size) {
		PrintError("Read failure for INS on port 0x%x\n", io_qual.port);
		return -1;
	    }
	}
	

       host_addr += rsi_change;
       core->vm_regs.rsi += rsi_change;

       if (io_qual.rep) {
           --core->vm_regs.rcx;
       }

    } while (--rep_num > 0);


    core->rip += exit_info->instr_len;

    return 0;
}

