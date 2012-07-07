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
#include <palacios/vm_guest.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_extensions.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm_hypercall.h>
#include <palacios/vmcb.h>

#include <gears/code_inject.h> 
#include <gears/execve_hook.h>
#include <gears/sw_intr.h>

#include "elf.h"

struct v3_code_injects code_injects;

static char mmap_code[] =   "\xb8\xc0\x00\x00\x00\x31\xdb\xb9"
                            "\x00\x00\x10\x00\xba\x01\x00\x00"
                            "\x00\xbd\x02\x00\x00\x00\x09\xea"
                            "\xbd\x04\x00\x00\x00\x09\xea\xbe"
                            "\x02\x00\x00\x00\xbd\x20\x00\x00"
                            "\x00\x09\xee\xbf\xff\xff\xff\xff"
                            "\x31\xed\xcd\x80\x89\xc3\xb9\x00"
                            "\x00\x10\x00\xc7\x00\xef\xbe\xad"
                            "\xde\x05\x00\x10\x00\x00\x81\xe9"
                            "\x00\x10\x00\x00\x75\xed\xb8\x00"
                            "\xf0\x00\x00\x0f\x01\xd9";

static char munmap_code[] = "\xb8\x5b\x00\x00\x00\xb9\x00\x00"
                            "\x10\x00\xcd\x80\x89\xc3\xb8\x03"
                            "\xf0\x00\x00\x0f\x01\xd9";

static char vmmcall_code[] = "\x48\xc7\xc0\x02\xf0\x00\x00\x0f"
                             "\x01\xd9";

static const char elf_magic[] = {0x7f, 'E', 'L', 'F'};


/*
 * the presence of this is kind of a hack, and exists because
 * when one of the below hypercall handlers is invoked, we don't
 * have an elegant way of deciding which inject queue (normal or exec-hooked)
 * to pull the first element from, so we have this place marker
 *
 * This could be ugly with more than one core...
 */
static struct v3_code_inject_info * current_inject;


static int free_code_inject (struct v3_vm_info * vm, struct v3_code_inject_info * inject) {
    list_del(&(inject->inject_node));
    V3_Free(inject);
    return 0;
}


/* 
 * helper function to save a chunk of code in an inject object's state and
 * overwrite it with something else (mostly for injecting hypercalls)
 */
static int v3_plant_code (struct guest_info * core, struct v3_code_inject_info * inject,
                          char * hva, char * code, uint_t size) {
    int i;

    // first back up old code
    inject->old_code = (char*)V3_Malloc(size);

    if (!inject->old_code) {
	PrintError("Cannot allocate in planting code\n");
	return -1;
    }

    for (i = 0; i < size; i++)
        inject->old_code[i] = *(hva + i);

    // overwrite
    for (i = 0; i < size; i++)
        *(hva + i) = *(code + i);

    return 0;
}


static int v3_restore_pre_mmap_state (struct guest_info * core, struct v3_code_inject_info * inject) {
    int ret;
    addr_t rip_hva, mmap_gva;

    if ((mmap_gva = (addr_t)core->vm_regs.rbx) < 0) {
        PrintError("Error running mmap in guest: v3_restore_pre_mmap_state\n");
        return -1;
    }

    inject->code_region_gva = mmap_gva;

	ret = v3_gva_to_hva(core, 
						get_addr_linear(core, (addr_t)inject->rip, &(core->segments.cs)),
						&rip_hva);
    if (ret == -1) {
		PrintError("Error translating RIP address: v3_restore_pre_mmap_state\n");
		return -1;
    }

    // restore the code overwritten by mmap code
    memcpy((void*)rip_hva, (void*)inject->old_code, MMAP_SIZE);
    V3_Free(inject->old_code);

    v3_do_static_inject(core, inject, MMAP_COMPLETE, mmap_gva);
    return 0;
}


static int v3_restore_pre_inject_state (struct guest_info * core, struct v3_code_inject_info * inject) {
    int ret;
    addr_t rip_hva;

    // restore original register state at int 80
    memcpy(&core->vm_regs, &inject->regs, sizeof(struct v3_gprs));
    memcpy(&core->ctrl_regs, &inject->ctrl_regs, sizeof(struct v3_ctrl_regs));

	ret = v3_gva_to_hva(core, 
						get_addr_linear(core, (addr_t)inject->rip, &(core->segments.cs)),
						&rip_hva);
    if (ret == -1) {
		PrintError("Error translating RIP address: v3_pre_inject_state\n");
		return -1;
    }

    // increment original rip by 2 to skip the int 80
    core->rip = inject->rip + 2;
    return 0;
}


/* 
 * This function completes stage 1 of the inject. It is invoked when code to
 * mmap space for the real code has been injected and has completed. This mmap
 * code will hypercall back into Placios, getting us here.
 */
static int mmap_init_handler (struct guest_info * core, unsigned int hcall_id, void * priv_data) {
    struct v3_code_inject_info * inject = current_inject;
    v3_restore_pre_mmap_state(core, inject);
    return 0;
}


/* 
 * This function is stage 3 of the injection process. It is invoked when the injected code 
 * has run to completeion and run a hypercall at its tail to get back into the
 * VMM. After this, it only remains to unmap the space we injected it into (the
 * 4th and final stage)
 */
static int inject_code_finish (struct guest_info * core, unsigned int hcall_id, void * priv_data) {
    struct v3_code_inject_info * inject = current_inject;
    addr_t hva;

    // is the original int 80 page still paged in?
    if (v3_gva_to_hva(core, 
                        get_addr_linear(core, (addr_t)inject->rip, &(core->segments.cs)),
                        &hva) == -1) {
        PrintError("No mapping in shadow page table: inject_code_finish\n");
        return -1;
    }

    inject->old_code = V3_Malloc(MUNMAP_SIZE);

    if (!inject->old_code) {
        PrintError("Problem mallocing old code segment\n");
        return -1;
    }

    // save old code and overwrite with munmap
    v3_plant_code(core, inject, (char*)hva, munmap_code, MUNMAP_SIZE);

    // set rbx with gva of code region
    core->vm_regs.rbx = inject->code_region_gva;

    // set rip back
    core->rip = inject->rip;
    return 0;
}


//
// this is 4th and final stage of the code injection process. It is invoked after code
// has been injected to run the munmap system call on our previosuly allocated
// memory chunk. It results in the clean
// up and removal of the current inject's structures and state, and its
// removal from any injection queues
// 
static int munmap_finish (struct guest_info * core, unsigned int hcall_id, void * priv_data) {
    struct v3_code_inject_info * inject = current_inject;
    int i = 0;
    addr_t hva;

    if (core->vm_regs.rbx < 0) {
        PrintError("Problem munmapping injected code\n");
        return -1;
    }

    if (v3_gva_to_hva(core, 
                        get_addr_linear(core, (addr_t)inject->rip, &(core->segments.cs)),
                        &hva) == -1) {
        PrintError("No mapping in shadow page table: inject_code_finish\n");
        return -1;
    }

    for (i = 0; i < MUNMAP_SIZE; i++) 
        *(char*)(hva + i) = *(char*)(inject->old_code + i);

    V3_Free(inject->old_code);

    v3_restore_pre_inject_state(core, inject);

    // clean up
    v3_remove_code_inject(core->vm_info, inject);
    current_inject = NULL;
    
    // raise the original int 80 again, causing an exec
    return v3_raise_swintr(core, SW_INTR_SYSCALL_VEC);
}


/* 
 * This function is comprises stage 2 of the injection process. Here, the
 * injected code is copied one page at a time. Each time a new page must be
 * copied, Palacios injects a page fault for it to bring it into the guest and
 * host page tables. The fault address will be somewhere in our previously
 * mmap'd region, but we will jump back to the same RIP every time, which
 * contains the hypercall that invokes this function.
 */
static int mmap_pf_handler (struct guest_info * core, unsigned int hcall_id, void * priv_data) {
    struct v3_code_inject_info * inject = current_inject;
    pf_error_t err;
    int i, offset = core->vm_regs.rbx;
    addr_t hva, gva = core->vm_regs.rcx;
    memset((void*)&err, 0, sizeof(pf_error_t));

    // was page fault handled by guest kernel?
	if (v3_gva_to_hva(core, 
						get_addr_linear(core, gva, &(core->segments.ds)),
						&hva) == -1) {
        PrintError("No mapping in shadow page table: mmap_pf_handler\n");
        return -1;
    }
    
    if (offset >= inject->code_size) {
        core->rip = gva - offset + inject->func_offset;

        // restore registers (here, really just for sane ebp/esp)
        memcpy(&core->vm_regs, &inject->regs, sizeof(struct v3_gprs));
        memcpy(&core->ctrl_regs, &inject->ctrl_regs, sizeof(struct v3_ctrl_regs));

        if (v3_gva_to_hva(core, 
                            get_addr_linear(core, inject->rip, &(core->segments.cs)),
                            &hva) == -1) {
            PrintError("No mapping for old RIP in shadow page table: mmap_pf_handler: %p\n", (void*)inject->rip);
            return -1;
        }

	// restore the hypercall with original int 80 code
	for (i = 0; i < VMMCALL_SIZE; i++) 
	    *(char*)(hva + i) = *(char*)(inject->old_code + i);

        V3_Free(inject->old_code);

        if (v3_gva_to_hva(core, 
                            get_addr_linear(core, core->rip, &(core->segments.cs)),
                            &hva) == -1) {
            PrintError("No mapping for new RIP in shadow page table: mmap_pf_handler: %p\n", (void*)core->rip);
            return -1;
        }

        return 0;
    }

    // copy the next page of code
    for (i = 0; i < PAGE_SIZE; i++) 
        *(char*)(hva + i) = *(char*)(inject->code + offset + i);


    core->vm_regs.rbx += PAGE_SIZE;
    core->vm_regs.rcx += PAGE_SIZE;

    // to account for rip being incremented by hcall handler
    core->rip -= VMMCALL_SIZE;

    // inject the page fault for next page
    err.user = 1;
    err.write = 1;
    v3_inject_guest_pf(core, gva + PAGE_SIZE, err);

    return 0;
}


static int init_code_inject (struct v3_vm_info * vm, v3_cfg_tree_t * cfg, void ** priv_data) {
    struct v3_code_injects * injects = &code_injects;
    INIT_LIST_HEAD(&(injects->code_inject_list));
    INIT_LIST_HEAD(&(injects->hooked_code_injects));

    injects->active = 1;

    current_inject = NULL;

    v3_register_hypercall(vm, 0xf000, mmap_init_handler, NULL);
    v3_register_hypercall(vm, 0xf001, inject_code_finish, NULL);
    v3_register_hypercall(vm, 0xf002, mmap_pf_handler, NULL);
    v3_register_hypercall(vm, 0xf003, munmap_finish, NULL);
    return 0;
}


static int deinit_code_inject (struct v3_vm_info * vm, void * priv_data) {
    struct v3_code_injects * injects = &code_injects;
    struct v3_code_inject_info * inject = NULL;
    struct v3_code_inject_info * tmp = NULL;

    list_for_each_entry_safe(inject, tmp, &(injects->code_inject_list), inject_node) {
        free_code_inject(vm, inject); 
    }

    list_for_each_entry_safe(inject, tmp, &(injects->hooked_code_injects), inject_node) {
        free_code_inject(vm, inject); 
    }

    v3_remove_hypercall(vm, 0xf000);
    v3_remove_hypercall(vm, 0xf001);
    v3_remove_hypercall(vm, 0xf002);
    v3_remove_hypercall(vm, 0xf003);
    return 0;
}




/* KCH currently unused */
/* this dynamic linking stuff will eventually be moved out of this file... */
static addr_t v3_get_dyn_entry (struct guest_info * core, addr_t elf_gva, addr_t elf_hva, 
                                    int section_code) {
    ElfW(Ehdr) *ehdr;
    ElfW(Phdr) *phdr, *phdr_cursor;
    ElfW(Dyn) *dyn = NULL;
    int i, j, num_dyn;
    addr_t hva;

    ehdr = (ElfW(Ehdr)*)elf_hva;
    phdr = (ElfW(Phdr)*)(elf_hva + ehdr->e_phoff);
    phdr_cursor = phdr;

    //PrintDebug("num phdrs: %d\n", ehdr->e_phnum);
    for (i = 0; i < ehdr->e_phnum; i++, phdr_cursor++) {
        if (phdr_cursor->p_type == PT_DYNAMIC) {
            num_dyn = phdr_cursor->p_filesz / sizeof(ElfW(Dyn));
            dyn = (ElfW(Dyn)*)(elf_hva + phdr_cursor->p_offset);

            // make sure this addr is paged in 
            if (v3_gva_to_gpa(core, elf_gva + phdr_cursor->p_offset, &hva) == -1) {
                PrintError("Dynamic segment isn't paged in\n");
                return 0;
            }

            for (j = 0; j < num_dyn; j++, dyn++) {
                if (dyn->d_tag == section_code) {
                    switch (section_code) {
                        case DT_STRSZ:
                        case DT_SYMENT:
                        case DT_PLTREL:
                            return (addr_t)dyn->d_un.d_val;
                        default:
                            return (addr_t)dyn->d_un.d_ptr;
                    }
                }
            }
            break;
        }
    }
    return 0;
}


static int v3_do_resolve (struct guest_info * core, addr_t elf_gva, addr_t elf_hva) {

    addr_t got_gva, symtab_gva, strtab_gva;

    if ((got_gva = v3_get_dyn_entry(core, elf_gva, elf_hva, DT_PLTGOT)) == 0) {
        PrintError("Problem getting at PLTGOT in v3_do_resolve\n");
        return -1;
    }


    if ((strtab_gva = v3_get_dyn_entry(core, elf_gva, elf_hva, DT_STRTAB)) == 0) {
        PrintError("Problem getting at PLTGOT in v3_do_resolve\n");
        return -1;
    }

    if ((symtab_gva = v3_get_dyn_entry(core, elf_gva, elf_hva, DT_SYMTAB)) == 0) {
        PrintError("Problem getting at PLTGOT in v3_do_resolve\n");
        return -1;
    }


    PrintDebug("Got gva: %p\n", (void*)got_gva);
    PrintDebug("Symtab gva: %p\n", (void*)symtab_gva);
    PrintDebug("Strtab gva: %p\n", (void*)strtab_gva);
    return 0;
}

static int v3_do_cont (struct guest_info * core, struct v3_code_inject_info * inject,  addr_t check) {

    addr_t hva;
    pf_error_t err_code;
    int ret;

    ret = v3_gva_to_gpa(core, check, &hva);

    // page fault wasn't handled by kernel??
    if (ret == -1) {
        PrintError("ERROR: no mapping in guest page table!\n");
        return -1;
    }

	ret = v3_gva_to_hva(core, 
						get_addr_linear(core, check, &(core->segments.cs)),
						&hva);

    // this should never happen...
	if (ret == -1) {
        PrintError("ERROR: no mapping in shadow page table\n");
        return -1;
	}
    
    if (strncmp(elf_magic, (char*)hva, ELF_MAG_SIZE) != 0) {

        check -= PAGE_SIZE;
        inject->cont->check_addr = check;
        inject->cont->cont_func = v3_do_cont;

        memset((void*)&err_code, 0, sizeof(pf_error_t));
        err_code.user = 1;

        if (v3_inject_guest_pf(core, check, err_code) < 0) {
            PrintError("Problem injecting pf\n");
            return -1;
        }

        return E_NEED_PF;
    }

    PrintDebug("Found ELF!\n");
    V3_Free(inject->cont);
    inject->cont = NULL;
    return v3_do_resolve(core, check, hva);
}
 

/*
 * mmap_state: 0 = no inject space in procces yet
 *             1 = code segment space mmap'd, still need data
 *             2 = code & data segments mmap'd, ready to inject real code
 *
 */
//
// return  E_NEED_PF up the call stack to signal page fault injection
// (so rip doesn't get incremented and sw_intr doesn't get injected
//
int v3_do_inject (struct guest_info * core, struct v3_code_inject_info * inject, int mmap_state) {
	addr_t rip_hva, elf_hva, elf_gva;
	int ret = 0, i = 0;
    pf_error_t err_code;

    memset((void*)&err_code, 0, sizeof(pf_error_t));
	
	ret = v3_gva_to_hva(core, 
						get_addr_linear(core, (addr_t)core->rip, &(core->segments.cs)),
						&rip_hva);
	if (ret == -1) {
		PrintError("Error translating RIP address in v3_do_inject\n");
		return -1;
	}

    elf_gva = (addr_t)(core->rip & 0xfffffffffffff000);

    for (i = 0; i < PAGES_BACK; i++, elf_gva -= PAGE_SIZE) {
    
        ret = v3_gva_to_hva(core, 
                            get_addr_linear(core, elf_gva, &(core->segments.cs)),
                            &elf_hva);

        // need to page in
        if (ret == -1) {

            PrintDebug("Found a page we need to fault in\n");
            inject->cont = (struct v3_cont *)V3_Malloc(sizeof(struct v3_cont));

	    if (!inject->cont) {
		PrintError("Cannot allocate in doing inject\n");
		return -1;
	    }

            ret = v3_gva_to_gpa(core, elf_gva, &elf_hva);

            if (ret == -1) {
                PrintDebug("no mapping in guest page table\n");
            }

            inject->cont->check_addr = elf_gva;
            inject->cont->cont_func = v3_do_cont;
            err_code.user = 1;

            PrintDebug("Injecting pf for addr: %p\n", (void*) elf_gva);

            if (v3_inject_guest_pf(core, elf_gva, err_code) < 0) {
                PrintError("Problem injecting pf\n");
                return -1;
            }

            return E_NEED_PF;
        }

        if (strncmp(elf_magic, (char*)elf_hva, ELF_MAG_SIZE) == 0) {
            PrintDebug("Found elf_magic!\n");
            break;
        }

    }


    V3_Free(inject->cont);
    inject->cont = NULL;
    return v3_do_resolve(core, elf_gva, elf_hva);

    PrintDebug("Planting code\n");
    v3_plant_code(core, inject, (char*)rip_hva, mmap_code, MMAP_SIZE);

    PrintDebug("Saving register context\n");
    PrintDebug("First 8 bytes 0x%lx\n", *(long*)rip_hva);
    /* may need to save v3_ctrl registers too... */
    memcpy(&inject->regs, &core->vm_regs, sizeof(struct v3_gprs));
    inject->rip = core->rip;

    /* jump to injected code */
    PrintDebug("Jumping to injected code\n");
	return 0;
}


/*
 * mmap_state: NO_MMAP = no inject space mmap'd in procces yet
 *             MMAP_COMPLETE = mmap complete, time to do real inject
 *
 */
int v3_do_static_inject (struct guest_info * core, struct v3_code_inject_info * inject,
                         int mmap_state, addr_t region_gva) {
	addr_t rip_hva;
	int ret;

	
	ret = v3_gva_to_hva(core, 
						get_addr_linear(core, (addr_t)core->rip, &(core->segments.cs)),
						&rip_hva);
	if (ret == -1) {
		PrintError("Error translating RIP address: v3_do_static_inject\n");
		return -1;
	}

    switch (mmap_state) { 
        case NO_MMAP: 
        {
            // inject mmap code
            v3_plant_code(core, inject, (char*)rip_hva, mmap_code, MMAP_SIZE);

            // save registers (gprs and ctrl regs, and rip)
            memcpy(&inject->regs, &core->vm_regs, sizeof(struct v3_gprs));
            memcpy(&inject->ctrl_regs, &core->ctrl_regs, sizeof(struct v3_ctrl_regs));
            inject->rip = core->rip;

            // jump to mmap code, and squash original swintr
            return E_NEED_PF;
        }
        case MMAP_COMPLETE:
        {
            pf_error_t err_code;
            memset((void*)&err_code, 0, sizeof(pf_error_t));

            ret = v3_gva_to_hva(core, 
                                get_addr_linear(core, (addr_t)inject->rip, &(core->segments.cs)),
                                &rip_hva);
            if (ret == -1) {
                PrintError("Error translating RIP address: v3_do_static_inject\n");
                return -1;
            }

            // inject hypercall code
            v3_plant_code(core, inject, (char*)rip_hva, vmmcall_code, VMMCALL_SIZE);

            /* store current copy offset in rbx, fault gva in rcx */
            core->vm_regs.rbx = 0;
            core->vm_regs.rcx = region_gva;

            err_code.user = 1;
            err_code.write = 1;

            // inject the first page fault for the code block
            if (v3_inject_guest_pf(core, region_gva, err_code) < 0) {
                PrintError("Problem injecting page fault in v3_do_static_inject\n");
                return -1;
            }

            // returning here will run hypercall 0xf002
            // This will get us back in v3_mmap_pf_handler
            core->rip = inject->rip;
            return 0;
        }
        default:
            PrintError("Invalid mmap state\n");
            return -1;
    }
	return 0;
}


/*
 * This function is invoked in one of two ways:
 * 1. A syscall has been intercepted and we've popped off the next pending
 * inject
 * 2. An exec has been intercepted and we've popped off the next hooked inject
 *
 */
int v3_handle_guest_inject (struct guest_info * core, void * priv_data) {
    struct v3_code_inject_info * inject = (struct v3_code_inject_info *)priv_data;

    /* eventually this should turn into a mutex lock */
    if (current_inject) {
        PrintError("An inject is already in progress\n");
        return -1;
    } else {
        current_inject = inject;
        inject->in_progress = 1;
    }

    if (!inject->is_dyn) {
        return v3_do_static_inject(core, inject, 0, (addr_t)NULL);
    } else {
        if (inject->cont) 
           return inject->cont->cont_func(core, inject, inject->cont->check_addr);
        else 
           return v3_do_inject(core, inject, 0);
    }

    return 0;
}


int v3_insert_code_inject (void * ginfo, void * code, int size, 
                           char * bin_file, int is_dyn, int is_exec_hooked, int func_offset) {
    struct v3_code_injects * injects = &code_injects;
    struct v3_vm_info * vm = (struct v3_vm_info *)ginfo;
    struct v3_code_inject_info * inject;

    if (!injects->active) {
        PrintError("Code injection has not been initialized\n");
        return -1;
    }

    inject = V3_Malloc(sizeof(struct v3_code_inject_info));
    if (!inject) {
        PrintError("Error allocating inject info in v3_insert_code_inject\n");
        return -1;
    }

    memset(inject, 0, sizeof(struct v3_code_inject_info));

    inject->code = code;
    inject->code_size = size;
    inject->is_dyn = is_dyn;
    inject->func_offset = func_offset;
    inject->bin_file = bin_file;
    inject->is_exec_hooked = is_exec_hooked;

    if (is_exec_hooked) {
        v3_hook_executable(vm, bin_file, v3_handle_guest_inject, (void*)inject);
        list_add_tail(&(inject->inject_node), &(injects->hooked_code_injects));
    } else {
        list_add_tail(&(inject->inject_node), &(injects->code_inject_list));
    }

    return 0;
}


int v3_remove_code_inject (struct v3_vm_info * vm, struct v3_code_inject_info * inject) {

    PrintDebug("Removing and freeing code inject\n");
    if (inject->is_exec_hooked) {
        if (v3_unhook_executable(vm, inject->bin_file) < 0) {
            PrintError("Problem unhooking executable in v3_remove_code_inject\n");
            return -1;
        }
    }

    free_code_inject(vm, inject);
    return 0;
}


static struct v3_extension_impl code_inject_impl = {
	.name = "code_inject",
	.init = init_code_inject,
	.deinit = deinit_code_inject,
	.core_init = NULL,
	.core_deinit = NULL,
	.on_entry = NULL,
	.on_exit = NULL
};
register_extension(&code_inject_impl);

