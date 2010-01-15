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


static int pre_config_pc_core(struct guest_info * info, v3_cfg_tree_t * cfg) { 


    info->cpu_mode = REAL;
    info->mem_mode = PHYSICAL_MEM;


    info->vm_regs.rdi = 0;
    info->vm_regs.rsi = 0;
    info->vm_regs.rbp = 0;
    info->vm_regs.rsp = 0;
    info->vm_regs.rbx = 0;
    info->vm_regs.rdx = 0;
    info->vm_regs.rcx = 0;
    info->vm_regs.rax = 0;

    return 0;
}

static int post_config_pc_core(struct guest_info * info, v3_cfg_tree_t * cfg) { 

    v3_print_mem_map(info->vm_info);
    return 0;
}

static int post_config_pc(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {

#define VGABIOS_START 0x000c0000
#define ROMBIOS_START 0x000f0000
    
    /* layout vgabios */
    {
	extern uint8_t v3_vgabios_start[];
	extern uint8_t v3_vgabios_end[];
	
	addr_t vgabios_dst = v3_get_shadow_addr(&(vm->mem_map.base_region), VGABIOS_START);
	memcpy(V3_VAddr((void *)vgabios_dst), v3_vgabios_start, v3_vgabios_end - v3_vgabios_start);	
    }
    
    /* layout rombios */
    {
	extern uint8_t v3_rombios_start[];
	extern uint8_t v3_rombios_end[];

	addr_t rombios_dst = v3_get_shadow_addr(&(vm->mem_map.base_region), ROMBIOS_START);
	memcpy(V3_VAddr((void *)rombios_dst), v3_rombios_start, v3_rombios_end - v3_rombios_start);
    }

    return 0;
}

