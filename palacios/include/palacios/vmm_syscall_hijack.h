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

#ifndef __VMM_SYSCALL_HIJACK_H__
#define __VMM_SYSCALL_HIJACK_H__


#ifdef __V3VEE__


struct v3_syscall_hook_map {
    /* 512 is an arbitrary number, I'm not sure that
        there is a hard limit on the number of syscalls
        an OS can provide */
    struct v3_syscall_hook * syscall_hooks[512];
};

struct v3_syscall_hook {
    int (*handler)(struct guest_info * core, uint_t syscall_nr, void * priv_data);
    void * priv_data;
};


struct v3_execve_varchunk {
    char ** argv;
    char ** envp;
    uint_t argc;
    uint_t envc;
    uint_t bytes;
    int active;
};
    

int v3_syscall_handler (struct guest_info * core, uint8_t vector, void * priv_data);


int v3_hook_syscall (struct guest_info * core,
    uint_t syscall_nr,
    int (*handler)(struct guest_info * core, uint_t syscall_nr, void * priv_data), 
    void * priv_data);

int v3_hook_passthrough_syscall (struct guest_info * core, uint_t syscall_nr);

int v3_sysopen_handler (struct guest_info * core, uint_t syscall_nr, void * priv_data);
int v3_sysmount_handler (struct guest_info * core, uint_t syscall_nr, void * priv_data);
int v3_sysexecve_handler (struct guest_info * core, uint_t syscall_nr, void * priv_data);


#endif

#endif
