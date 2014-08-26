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

#include <palacios/vm_guest_mem.h>


static int pre_config_pc_core(struct guest_info * info, v3_cfg_tree_t * cfg) { 

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

    v3_cfg_tree_t * bios_tree = NULL;
    v3_cfg_tree_t * bios_list = NULL;

    bios_tree = v3_cfg_subtree(cfg, "bioses");


    if (!v3_cfg_val(bios_tree, "disable_vgabios")) {

#if defined(V3_CONFIG_SEABIOS) || defined(V3_CONFIG_BOCHSBIOS)

#define VGABIOS_START 0x000c0000
        /* layout vgabios */
        {
            extern uint8_t v3_vgabios_start[];
            extern uint8_t v3_vgabios_end[];
            void * vgabios_dst = 0;

            if (v3_gpa_to_hva(&(vm->cores[0]), VGABIOS_START, (addr_t *)&vgabios_dst) == -1) {
                PrintError(vm, VCORE_NONE, "Could not find VGABIOS destination address\n");
                return -1;
            }

            V3_Print(vm,VCORE_NONE,"Mapping VGA BIOS of %llu bytes at address %p\n", (uint64_t)(v3_vgabios_end-v3_vgabios_start), (void*)VGABIOS_START);
            memcpy(vgabios_dst, v3_vgabios_start, v3_vgabios_end - v3_vgabios_start);	
        }

#endif

    }


    if (!v3_cfg_val(bios_tree, "disable_rombios")) {

        /* layout rombios */
        {
            extern uint8_t v3_rombios_start[];
            extern uint8_t v3_rombios_end[];
            void * rombios_dst = 0;



            if (v3_gpa_to_hva(&(vm->cores[0]), V3_CONFIG_BIOS_START, (addr_t *)&rombios_dst) == -1) {
                PrintError(vm, VCORE_NONE, "Could not find ROMBIOS destination address\n");
                return -1;
            }

            V3_Print(vm,VCORE_NONE,"Mapping BIOS of %llu bytes at address %p\n", (uint64_t)(v3_rombios_end-v3_rombios_start), (void*)V3_CONFIG_BIOS_START);
            memcpy(rombios_dst, v3_rombios_start, v3_rombios_end - v3_rombios_start);

#ifdef V3_CONFIG_SEABIOS
            // SEABIOS is also mapped into end of 4GB region
            if (v3_add_shadow_mem(vm, V3_MEM_CORE_ANY, 
                        0xfffe0000, 0xffffffff,
                        (addr_t)V3_PAddr(rombios_dst)) == -1) {
                PrintError(vm, VCORE_NONE, "Error mapping SEABIOS to end of memory\n");
                return -1;
            }
            V3_Print(vm,VCORE_NONE,"Additionally mapping SEABIOS of %llu bytes at address %p\n", (uint64_t)(v3_rombios_end-v3_rombios_start), (void*)0xfffe0000);
#endif

        }
    }

    bios_list = v3_cfg_subtree(bios_tree, "bios");

    while (bios_list) {
        char * id = v3_cfg_val(bios_list, "file");
        char * addr = v3_cfg_val(bios_list, "address");
        uint64_t file_ptr = 0;
        void * dest = NULL;
        struct v3_cfg_file * file = NULL;

        if (!id) {
            PrintError(vm, VCORE_NONE, "Could not find bios file\n");
            continue;
        }

        if (!addr) {
            PrintError(vm, VCORE_NONE, "Could not find bios address\n");
            continue;
        }

        file = v3_cfg_get_file(vm, id);
        if (!file) {
            PrintError(vm, VCORE_NONE, "Invalid BIOS file: %s\n", id);
            continue;
        }

        file_ptr = atox(addr);

        V3_Print(vm, VCORE_NONE, "Copying BIOS ROM (%s) to %p (size=%lld)\n", 
                 id,
                 (void*)file_ptr,
                 file->size);

        if (v3_gpa_to_hva(&(vm->cores[0]), (addr_t)file_ptr, (addr_t *)&dest) == -1) {
            PrintError(vm, VCORE_NONE, "Could not find BIOS (%s) destination address\n", id);
            continue;
        }

        memcpy((void*)dest, file->data, file->size);

        V3_Print(vm, VCORE_NONE, "Moving on to next BIOS file\n");
        bios_list = v3_cfg_next_branch(bios_list);
    }

    return 0;
}

