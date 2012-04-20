/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Kyle C. Hale <kh@u.northwestern.edu> 
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __CODE_INJECT_H__
#define __CODE_INJECT_H__

int v3_insert_code_inject (void *ginfo, void *code, int size, char *bin_file, 
                           int is_dyn, int is_exec_hooked, int func_offset);

#ifdef __V3VEE__

#define E_NEED_PF -2

#define MMAP_SIZE 86
#define MUNMAP_SIZE 22
#define VMMCALL_SIZE 10

#define PAGES_BACK 50
#define ELF_MAG_SIZE 4
#define NO_MMAP 0
#define MMAP_COMPLETE 1

struct v3_code_injects {
    struct list_head code_inject_list;
    struct list_head hooked_code_injects;
    int active;
};


// TODO: adjust size of boolean members
struct v3_code_inject_info {

    // pointer to ELF and its size
    void *code;
    int code_size;


    // indicates this is a hooked inject
    int is_exec_hooked;
    char * bin_file;

    // important offsets to ELF sections
    // for the injected code
    int func_offset;
    int got_offset;
    int plt_offset;


    int is_dyn;
    addr_t code_region_gva;
    // continuation-style function for
    // page fault handling
    struct v3_cont *cont;


    // the following are for saving context
    char *old_code;
    struct v3_gprs regs;
    struct v3_ctrl_regs ctrl_regs;
    uint64_t rip;

    struct list_head inject_node;

    int in_progress;
};

struct v3_cont {
    addr_t check_addr;
    int (*cont_func)(struct guest_info * core, struct v3_code_inject_info * inject,
                     addr_t check);
};

int v3_remove_code_inject(struct v3_vm_info * vm, struct v3_code_inject_info * inject);
int v3_do_inject(struct guest_info * core, struct v3_code_inject_info * inject, int mmap_state);
int v3_do_static_inject(struct guest_info * core, struct v3_code_inject_info * inject, 
                        int mmap_state, addr_t region_gva);
int v3_handle_guest_inject(struct guest_info * core, void * priv_data);

#endif  /* ! __V3VEE__ */

#endif
