/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Kyle C. Hale <kh@u.norhtwestern.edu>
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vm_guest.h>
#include <palacios/vm_guest_mem.h>

#include <gears/process_environment.h>

static struct v3_execve_varchunk var_dump;


/* KCH: currently only checks if we can perform a user-mode write
   return 1 on success */
static int v3_gva_can_access(struct guest_info * core, addr_t gva) {

    v3_reg_t guest_cr3 = 0;
    pf_error_t access_type;
    pt_access_status_t access_status;

    access_type.write = 1;
    access_type.user = 1;

    if (core->mem_mode == PHYSICAL_MEM) {
        return -1;
    }

    if (core->shdw_pg_mode == SHADOW_PAGING) {
        guest_cr3 = core->shdw_pg_state.guest_cr3;
    } else {
        guest_cr3 = core->ctrl_regs.cr3;
    }

    // guest is in paged mode
    switch (core->cpu_mode) {
    case PROTECTED:
        if (v3_check_guest_pt_32(core, guest_cr3, gva, access_type, &access_status) == -1) {
            return -1;
        }
        break;
    case PROTECTED_PAE:
        if (v3_check_guest_pt_32pae(core, guest_cr3, gva, access_type, &access_status) == -1) {
            return -1;
        }
        break;
    case LONG:
    case LONG_32_COMPAT:
    case LONG_16_COMPAT:
        if (v3_check_guest_pt_64(core, guest_cr3, gva, access_type, &access_status) == -1) {
            return -1;
        }
        break;
    default:
        return -1;
    }

    if (access_status != PT_ACCESS_OK) {
        return 0;
    } else {
        return 1;
    }
}

static int v3_copy_chunk_guest32(struct guest_info * core, addr_t gva, uint_t argcnt, uint_t envcnt) {

    int ret = 0, i = 0;
    addr_t hva;
    uint32_t tmp_args[var_dump.argc];
    uint32_t tmp_envs[var_dump.envc];

    PrintDebug(core->vm_info, core, "Initiating copy into guest (32bit)\n");
    
    ret = v3_gva_to_hva(core, get_addr_linear(core, gva, &(core->segments.ds)), &hva);
    if (ret == -1) {
        PrintDebug(core->vm_info, core, "Error translating gva in v3_copy_chunk_2guest\n");
        return -1;
    }
    
    // copy the env strings (we're moving top-down through the stack)
    char * host_cursor = (char*) hva;
    uint32_t guest_cursor = (uint32_t) gva;
    host_cursor -= strlen(var_dump.envp[i]) + 1;
    guest_cursor -= strlen(var_dump.envp[i]) + 1;
    while (i < var_dump.envc) {
        //PrintDebug(core->vm_info, core, "Copying envvar#%d: %s\n", i, var_dump.envp[i]);
        strcpy(host_cursor, var_dump.envp[i]);
        tmp_envs[i] = guest_cursor;
        i++;
        if (i != var_dump.envc) { 
            host_cursor -= strlen(var_dump.envp[i]) + 1;
            guest_cursor -= strlen(var_dump.envp[i]) + 1;
        }
    }
        
    // then the arg strings
    i = 0;
    host_cursor -= strlen(var_dump.argv[i]) + 1;
    guest_cursor -= strlen(var_dump.argv[i]) + 1;
    while (i < var_dump.argc) {
        //PrintDebug(core->vm_info, core, "Copying arg #%d: %s\n", i, var_dump.argv[i]);
        strcpy(host_cursor, var_dump.argv[i]);
        tmp_args[i] = guest_cursor;
        i++;
        if (i != var_dump.argc) {
            host_cursor -= strlen(var_dump.argv[i]) + 1;
            guest_cursor -= strlen(var_dump.argv[i]) + 1;
        }
    }

    
    // padding
    host_cursor--;  
    guest_cursor--;
    while ((long)host_cursor % 4) {
        *host_cursor = 0;
        host_cursor--;
        guest_cursor--;
    }

    // null ptr
    host_cursor -= 4;
    guest_cursor -= 4;
    *((uint32_t*)host_cursor) = 0;

    host_cursor -= 4;
    guest_cursor -= 4;
    for (i = 0; i < var_dump.envc; i++) {
       *((uint32_t*)host_cursor) = tmp_envs[i];
        host_cursor -= 4;
        guest_cursor -= 4;
    }

    core->vm_regs.rdx = guest_cursor + 4;
    
    *((uint32_t*)host_cursor) = 0;
    host_cursor -= 4;
    guest_cursor -= 4;
    for (i = 0; i < var_dump.argc; i++) {
        *((uint32_t*)host_cursor) = tmp_args[i];
        host_cursor -= 4;
        guest_cursor -= 4;
    }

    core->vm_regs.rcx = guest_cursor + 4;

    // free up our temporary storage in the VMM
    for (i = 0; i < var_dump.argc; i++) {
        V3_Free(var_dump.argv[i]);
    }
    for (i = 0; i < var_dump.envc; i++) {
        V3_Free(var_dump.envp[i]);
    }
    
    V3_Free(var_dump.envp);
    V3_Free(var_dump.argv);
    return 0;
}


static int v3_copy_chunk_vmm32(struct guest_info * core, const char ** argstrs, const char ** envstrs, uint_t argcnt, uint_t envcnt) {

    addr_t envp, argv;
    uint_t argc = 0, envc = 0, bytes = 0;
    char * cursor;

    PrintDebug(core->vm_info, core, "Initiating copy into vmm\n");

    int ret = v3_gva_to_hva(core, get_addr_linear(core, core->vm_regs.rdx, &(core->segments.ds)), &envp);
    if (ret == -1) {
        PrintDebug(core->vm_info, core, "Error translating address in rdx\n");
        return 0;
    }

    ret = v3_gva_to_hva(core, get_addr_linear(core, core->vm_regs.rcx, &(core->segments.ds)), &argv);
    if (ret == -1) {
        PrintDebug(core->vm_info, core, "Error translating address in rcx\n");
        return 0;
    }
    
    cursor = (char*)argv;
    while (*((uint32_t*)cursor) != 0) {
        addr_t argvn;
        ret = v3_gva_to_hva(core, get_addr_linear(core, (addr_t)*((uint32_t*)cursor), &(core->segments.ds)), &argvn);
        if (ret == -1) {
            PrintDebug(core->vm_info, core, "Error translating address for argvn\n");
        }
        argc++;
        cursor += 4;
    } 

    /* account for new args */
    argc += argcnt;
    var_dump.argv = (char**)V3_Malloc(sizeof(char*)*argc);

    if (!var_dump.argv) {
	PrintError(core->vm_info, core, "Cannot allocate in copying\n");
	return -1;
    }

    var_dump.argc = argc;
    bytes += sizeof(uint32_t)*argc;

    cursor = (char*)argv;
    int i = 0;
    while (*((uint32_t*)cursor) != 0) {
        addr_t argvn;
        ret = v3_gva_to_hva(core, get_addr_linear(core, (addr_t)*((uint32_t*)cursor), &(core->segments.ds)), &argvn);
        if (ret == -1) {
            PrintDebug(core->vm_info, core, "Error translating argvn address\n");
        }
    
        /* malloc room for the string */
        char * tmpstr = (char*)V3_Malloc(strlen((char*)argvn) + 1);

	if (!tmpstr) {
	    PrintError(core->vm_info, core, "Cannot allocate temporary\n");
	    return -1;
	}

        /* copy the pointer */
        var_dump.argv[i] = tmpstr; 

        /* copy the string */
	// this is guaranteed to alwys null terminate tmpstr
        strncpy(tmpstr, (char*)argvn, strlen((char*)argvn) + 1);

        i++;
        cursor += 4;
        bytes += strlen((char*)argvn) + 1;
    }

    /* stick in new arg strings */
    int j = 0;
    while (j < argcnt) {
        char * tmpstr = (char*)V3_Malloc(strlen(argstrs[j]) + 1);

	if (!tmpstr) {
	    PrintError(core->vm_info, core, "Cannot allocate temp string\n");
	    return -1;
	}

	// will always null-terminate tmpstr
        strncpy(tmpstr, argstrs[i], strlen(argstrs[j]) + 1);
        var_dump.argv[i] = tmpstr;
        bytes += strlen(argstrs[j]) + 1;
        i++; j++;
    }
        
    
    cursor = (char*)envp;
    while (*((uint32_t*)cursor) != 0) {
        addr_t envpn;
        ret = v3_gva_to_hva(core, get_addr_linear(core, (addr_t)*((uint32_t*)cursor), &(core->segments.ds)), &envpn);
        if (ret == -1) {
            PrintDebug(core->vm_info, core, "Error translating address for envpn\n");
        }
        envc++;
        cursor += 4;
    } 

    envc += envcnt;
    var_dump.envp = (char**)V3_Malloc(sizeof(char*)*envc);

    if (!var_dump.envp) {
	PrintError(core->vm_info, core, "Cannot allocate var dump\n");
	return -1;
    }

    var_dump.envc = envc;
    bytes += sizeof(uint32_t)*envc;

    cursor = (char*)envp;
    i = 0;
    while (*((uint32_t*)cursor) != 0) {
        addr_t envpn;
        ret = v3_gva_to_hva(core, get_addr_linear(core, (addr_t)*((uint32_t*)cursor), &(core->segments.ds)), &envpn);
        if (ret == -1) {
            PrintDebug(core->vm_info, core, "Error translating address for envpn\n");
        }
        
        /* malloc room for the string */
        char * tmpstr = (char*)V3_Malloc(strlen((char*)envpn) + 1);

	if (!tmpstr) {
	    PrintError(core->vm_info, core, "Cannot allocate temp string\n");
	    return -1;
	}
        
        /* copy the pointer */
        var_dump.envp[i] = tmpstr;

        /* deepcopy the string */
	// will always null-terminate tmpstr
        strncpy(tmpstr, (char*)envpn, strlen((char*)envpn) + 1);
        i++;    
        cursor += 4;
        bytes += strlen((char*)envpn) + 1; 
    }

    /* put in our new env strings */
    j = 0;
    while (j < envcnt) {
        char * tmpstr = (char*)V3_Malloc(strlen(envstrs[j]) + 1);

	if (!tmpstr) {
	    PrintError(core->vm_info, core, "Cannot allocate temp string\n");
	    return -1;
	}
	// will always null-terminate tmpstr
        strncpy(tmpstr, envstrs[j], strlen(envstrs[j]) + 1);
        var_dump.envp[i] = tmpstr;
        bytes += strlen(envstrs[j]) + 1;
        i++; j++;
    }


    /* account for padding for strings
       and 2 null pointers */
    bytes += (bytes % 4) + 8;
    var_dump.bytes = bytes;
    return bytes;
}


static int v3_inject_strings32 (struct guest_info * core, const char ** argstrs, const char ** envstrs, uint_t argcnt, uint_t envcnt) {

    addr_t inject_gva;
    uint_t bytes_needed = 0;

    /* copy out all of the arguments and the environment to the VMM */
    if ((bytes_needed = v3_copy_chunk_vmm32(core, argstrs, envstrs, argcnt, envcnt)) == -1) {
        PrintDebug(core->vm_info, core, "Error copying out environment and arguments\n");
        return -1;
    }

    PrintDebug(core->vm_info, core, "environment successfully copied into VMM\n");
    
    inject_gva = v3_prepare_guest_stack(core, bytes_needed);
    if (!inject_gva) {
        PrintDebug(core->vm_info, core, "Not enough space on user stack\n");
        return -1;
    }

    v3_copy_chunk_guest32(core, inject_gva, argcnt, envcnt);

    return 0;
}


static int v3_copy_chunk_guest64(struct guest_info * core, addr_t gva, uint_t argcnt, uint_t envcnt) {

    int ret = 0, i = 0;
    addr_t hva;
    uint64_t tmp_args[var_dump.argc];
    uint64_t tmp_envs[var_dump.envc];

    PrintDebug(core->vm_info, core, "Initiating copy into guest (64bit)\n");
    
    ret = v3_gva_to_hva(core, get_addr_linear(core, gva, &(core->segments.ds)), &hva);
    if (ret == -1) {
        PrintDebug(core->vm_info, core, "Error translating gva in v3_copy_chunk_2guest64\n");
        return -1;
    }
    
    char * host_cursor = (char*) hva;
    uint64_t guest_cursor = (uint64_t) gva;
    host_cursor -= strlen(var_dump.envp[i]) + 1;
    guest_cursor -= strlen(var_dump.envp[i]) + 1;
    while (i < var_dump.envc) {
        //PrintDebug(core->vm_info, core, "Copying envvar#%d: %s\n", i, var_dump.envp[i]);
        strcpy(host_cursor, var_dump.envp[i]);
        tmp_envs[i] = guest_cursor;
        i++;
        if (i != var_dump.envc) { 
            host_cursor -= strlen(var_dump.envp[i]) + 1;
            guest_cursor -= strlen(var_dump.envp[i]) + 1;
        }
    }
        
    i = 0;
    host_cursor -= strlen(var_dump.argv[i]) + 1;
    guest_cursor -= strlen(var_dump.argv[i]) + 1;
    while (i < var_dump.argc) {
        //PrintDebug(core->vm_info, core, "Copying arg #%d: %s\n", i, var_dump.argv[i]);
        strcpy(host_cursor, var_dump.argv[i]);
        tmp_args[i] = guest_cursor;
        i++;
        if (i != var_dump.argc) {
            host_cursor -= strlen(var_dump.argv[i]) + 1;
            guest_cursor -= strlen(var_dump.argv[i]) + 1;
        }
    }

    // padding
    host_cursor--;  
    guest_cursor--;
    while ((long)host_cursor % 8) {
        *host_cursor = 0;
        host_cursor--;
        guest_cursor--;
    }

    // one null ptr
    host_cursor -= 8;
    guest_cursor -= 8;
    *((uint64_t*)host_cursor) = 0;

    host_cursor -= 8;
    guest_cursor -= 8;
    for (i = 0; i < var_dump.envc; i++) {
       *((uint64_t*)host_cursor) = tmp_envs[i];
        host_cursor -= 8;
        guest_cursor -= 8;
    }

    core->vm_regs.rdx = guest_cursor + 8;

    *((uint64_t*)host_cursor) = 0;
    host_cursor -= 8;
    guest_cursor -= 8;
    for (i = 0; i < var_dump.argc; i++) {
        *((uint64_t*)host_cursor) = tmp_args[i];
        host_cursor -= 8;
        guest_cursor -= 8;
    }

    core->vm_regs.rcx = guest_cursor + 8;

    for (i = 0; i < var_dump.argc; i++) {
        V3_Free(var_dump.argv[i]);
    }
    for (i = 0; i < var_dump.envc; i++) {
        V3_Free(var_dump.envp[i]);
    }
    
    V3_Free(var_dump.envp);
    V3_Free(var_dump.argv);
    return 0;
}


static int v3_copy_chunk_vmm64(struct guest_info * core, const char ** argstrs, const char ** envstrs, uint_t argcnt, uint_t envcnt) {

    addr_t envp, argv;
    uint_t argc = 0, envc = 0, bytes = 0;
    char * cursor;

    PrintDebug(core->vm_info, core, "Initiating copy into vmm\n");

    int ret = v3_gva_to_hva(core, get_addr_linear(core, core->vm_regs.rdx, &(core->segments.ds)), &envp);
    if (ret == -1) {
        PrintDebug(core->vm_info, core, "Error translating address in rdx\n");
        return 0;
    }

    ret = v3_gva_to_hva(core, get_addr_linear(core, core->vm_regs.rcx, &(core->segments.ds)), &argv);
    if (ret == -1) {
        PrintDebug(core->vm_info, core, "Error translating address in rcx\n");
        return 0;
    }
    
    cursor = (char*)argv;
    while (*((uint64_t*)cursor) != 0) {
        addr_t argvn;
        ret = v3_gva_to_hva(core, get_addr_linear(core, (addr_t)*((uint64_t*)cursor), &(core->segments.ds)), &argvn);
        if (ret == -1) {
            PrintDebug(core->vm_info, core, "Error translating address for argvn\n");
        }
        argc++;
        cursor += 8;
    } 
    
    /* account for new strings */
    argc += argcnt;
    var_dump.argv = (char**)V3_Malloc(sizeof(char*)*argc);

    if (!var_dump.argv) {
	PrintError(core->vm_info, core, "Cannot allocate var dump\n");
	return -1;
    }

    var_dump.argc = argc;
    bytes += sizeof(char*)*argc;

    cursor = (char*)argv;
    int i = 0;
    while (*((uint64_t*)cursor) != 0) {
        addr_t argvn;
        ret = v3_gva_to_hva(core, get_addr_linear(core, (addr_t)*((uint64_t*)cursor), &(core->segments.ds)), &argvn);
        if (ret == -1) {
            PrintDebug(core->vm_info, core, "Error translating argvn address\n");
        }
    
        /* malloc room for the string */
        char * tmpstr = (char*)V3_Malloc(strlen((char*)argvn) + 1);

	if (!tmpstr) {
	    PrintError(core->vm_info, core, "Cannot allocate temp string\n");
	    return -1;
	}

        /* copy the pointer */
        var_dump.argv[i] = tmpstr; 

        /* copy the string */
	// will always null-terminate tmpstr
        strncpy(tmpstr, (char*)argvn, strlen((char*)argvn) + 1);
        i++;
        cursor += 8;
        bytes += strlen((char*)argvn) + 1;
    }
        
    /* stick in new arg strings */
    int j = 0;
    while (j < argcnt) {
        char * tmpstr = (char*)V3_Malloc(strlen(argstrs[j]) + 1);

	if (!tmpstr) {
	    PrintError(core->vm_info, core, "Cannot allocate temp string\n");
	    return -1;
	}

	// will always null-terminate tmpstr
        strncpy(tmpstr, argstrs[j], strlen(argstrs[j]) + 1);
        var_dump.argv[i] = tmpstr;
        bytes += strlen(argstrs[j]) + 1;
        i++; j++;
    }


    cursor = (char*)envp;
    while (*((uint64_t*)cursor) != 0) {
        addr_t envpn;
        ret = v3_gva_to_hva(core, get_addr_linear(core, (addr_t)*((uint64_t*)cursor), &(core->segments.ds)), &envpn);
        if (ret == -1) {
            PrintDebug(core->vm_info, core, "Error translating address for envpn\n");
        }
        envc++;
        cursor += 8;
    } 

    envc += envcnt;
    var_dump.envp = (char**)V3_Malloc(sizeof(char*)*envc);

    if (!var_dump.envp) {
	PrintError(core->vm_info, core, "Cannot allocate var dump\n");
	return -1;
    }

    var_dump.envc = envc;
    bytes += sizeof(uint64_t)*(envc);


    cursor = (char*)envp;
    i = 0;
    while (*((uint64_t*)cursor) != 0) {
        addr_t envpn;
        ret = v3_gva_to_hva(core, get_addr_linear(core, (addr_t)*((uint64_t*)cursor), &(core->segments.ds)), &envpn);
        if (ret == -1) {
            PrintDebug(core->vm_info, core, "Error translating address for envpn\n");
        }
        
        /* malloc room for the string */
        char * tmpstr = (char*)V3_Malloc(strlen((char*)envpn) + 1);

	if (!tmpstr) {
	    PrintError(core->vm_info, core, "Cannot allocate temp string\n");
	    return -1;
	}
        
        /* copy the pointer */
        var_dump.envp[i] = tmpstr;

        /* deepcopy the string */
	// will always null-terminate tmpstr
        strncpy(tmpstr, (char*)envpn, strlen((char*)envpn) + 1);
        i++;    
        cursor += 8;
        bytes += strlen((char*)envpn) + 1; 
    }

    /* stick in new env strings */
    j = 0;
    while (j < envcnt) {
        char * tmpstr = (char*)V3_Malloc(strlen(envstrs[j]) + 1);

	if (!tmpstr) {
	    PrintError(core->vm_info, core, "Cannot allocate temp string\n");
	    return -1;
	}
	// will always null-terminate tmpstr
        strncpy(tmpstr, envstrs[i], strlen(envstrs[j]) + 1);
        var_dump.envp[i] = tmpstr;
        bytes += strlen(envstrs[j]) + 1;
        i++; j++;
    } 


    /* account for padding for strings
       and 2 null pointers */
    bytes += (bytes % 8) + 16;
    var_dump.bytes = bytes;
    return bytes;
}


static int v3_inject_strings64 (struct guest_info * core, const char ** argstrs, const char ** envstrs, uint_t argcnt, uint_t envcnt) {

    addr_t inject_gva;
    uint_t bytes_needed = 0;

    /* copy out all of the arguments and the environment to the VMM */
    if ((bytes_needed = v3_copy_chunk_vmm64(core, argstrs, envstrs, argcnt, envcnt)) == -1) {
        PrintDebug(core->vm_info, core, "Error copying out environment and arguments\n");
        return -1;
    }

    PrintDebug(core->vm_info, core, "environment successfully copied into VMM\n");
    
    inject_gva = v3_prepare_guest_stack(core, bytes_needed);
    if (!inject_gva) {
        PrintDebug(core->vm_info, core, "Not enough space on user stack\n");
        return -1;
    }

    v3_copy_chunk_guest64(core, inject_gva, argcnt, envcnt);
    return 0;
}


addr_t v3_prepare_guest_stack (struct guest_info * core, uint_t bytes_needed) {

    /* TODO: check if we've injected a page fault to get more stack space */

    // do we have enough room between esp and the next page boundary?
    uint_t rem_bytes = 4096 - (core->vm_regs.rsp % 4096);

    if (rem_bytes >= bytes_needed) {
        return (addr_t)core->vm_regs.rsp;
    } else {
        // not enough room, find out how many pages we need (ceiling)
        uint_t num_pages = (bytes_needed + 4095) / 4096;
        
        // check if num_pages are user & writable
        int i = 0;
        int pages_ok = 1;
        addr_t gva = core->vm_regs.rsp + rem_bytes;
        for (; i < num_pages; i++, gva -= 4096) {
            if (!v3_gva_can_access(core, gva)) {
                pages_ok = 0;
            }
        }

        if (pages_ok) {
            return (addr_t)core->vm_regs.rsp;
        } else {
    
            /*
            // inject a page fault
            pf_error_t fault_type = {
                .write = 1,
                .user = 1
            };

            // hoping Linux will allocate all pages in between gva and esp 
            v3_inject_guest_pf(core, gva - (num_pages*4096), fault_type);
            */
            return -1;
        }
    }
}


/* TODO: give these next to functions the ability to copy into guest stack */
int v3_replace_arg (struct guest_info * core, uint_t argnum, const char * newval) { 

    return 0;
}


int v3_replace_env (struct guest_info * core, const char * envname, const char * newval) {

    return 0;
}


int v3_inject_strings (struct guest_info * core, const char ** argstrs, const char ** envstrs, uint_t argcnt, uint_t envcnt) {
    
    if (core->cpu_mode == LONG) {
        if (v3_inject_strings64(core, argstrs, envstrs, argcnt, envcnt) == -1) {
            PrintDebug(core->vm_info, core, "Error injecting strings into environment (64)\n");
            return -1;
        }
    } else {
        if (v3_inject_strings32(core, argstrs, envstrs, argcnt, envcnt) == -1) {
            PrintDebug(core->vm_info, core, "Error injecting strings into environment (32)\n");
            return -1;
        }
    }

    return 0;
}
