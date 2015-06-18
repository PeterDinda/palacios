/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2015, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm_bios.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_xml.h>



int v3_setup_bioses(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) 
{

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

            V3_Print(vm,VCORE_NONE,"Mapping VGA BIOS of %llu bytes at gpa %p (hva %p)\n", (uint64_t)(v3_vgabios_end-v3_vgabios_start), (void*)VGABIOS_START,vgabios_dst);
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

            V3_Print(vm,VCORE_NONE,"Mapping BIOS of %llu bytes at gpa %p (hva %p)\n", (uint64_t)(v3_rombios_end-v3_rombios_start), (void*)V3_CONFIG_BIOS_START,rombios_dst);
            memcpy(rombios_dst, v3_rombios_start, v3_rombios_end - v3_rombios_start);

#ifdef V3_CONFIG_SEABIOS
	    
#define SEABIOS_HIGH_START 0xfffe0000
#define SEABIOS_HIGH_END   0xffffffff

	    if (v3_get_mem_region(vm,V3_MEM_CORE_ANY, SEABIOS_HIGH_START)) {
		// it is already mapped, we are done
		// note it's mapped to the same place as the low memory copy
		// so it's now pointing to the fresh copy
		V3_Print(vm,VCORE_NONE,"BIOS is already mapped\n");
	    } else {
		if (v3_add_shadow_mem(vm, V3_MEM_CORE_ANY, 
				      SEABIOS_HIGH_START, SEABIOS_HIGH_END,
				      (addr_t)V3_PAddr(rombios_dst)) == -1) {
		    PrintError(vm, VCORE_NONE, "Error mapping SEABIOS to end of memory\n");
		    return -1;
		}
		V3_Print(vm,VCORE_NONE,"BIOS  mapped\n");
            }
	    
	    V3_Print(vm,VCORE_NONE,"Additionally mapping SEABIOS of %llu bytes at gpa %p\n", (uint64_t)(v3_rombios_end-v3_rombios_start), (void*)SEABIOS_HIGH_START);
#endif

        }

	{
	    void *ba;

// traditional BIOS data area (and IVT, etc).  Technically only 0x400-0x4ff is the BDA
#define BDA_START   0x0
#define BDA_END     0xfff

// Extended BDA and EBDA2.  Technically not standardized.  Usually at the
// end of the 640K chunk.  We're just using that last page to play it save
#define EBDA_START 0x9f000
#define EBDA_END   0x9ffff

	    if (v3_gpa_to_hva(&(vm->cores[0]), BDA_START, (addr_t *)&ba) == -1) {
                PrintError(vm, VCORE_NONE, "Could not find BDA destination address\n");
                return -1;
            }
	    
	    memset(ba,0,(BDA_END-BDA_START+1));

	    V3_Print(vm,VCORE_NONE,"Cleared BDA %p (HVA %p) through %p\n",(void*)BDA_START,ba,(void*)BDA_END);

	    if (v3_gpa_to_hva(&(vm->cores[0]), EBDA_START, (addr_t *)&ba) == -1) {
                PrintError(vm, VCORE_NONE, "Could not find EBDA destination address\n");
                return -1;
            }
	    
	    memset(ba,0,(EBDA_END-EBDA_START+1));

	    V3_Print(vm,VCORE_NONE,"Cleared EBDA %p (HVA %p) through %p\n",(void*)EBDA_START,ba,(void*)EBDA_END);
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

    // NOW BLOW AWAY BDA and EBDA here
    // possibly also reset CMOS


    return 0;
}
