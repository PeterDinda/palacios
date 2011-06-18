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
#include <palacios/vmm_intr.h>
#include <palacios/vmm_syscall_hijack.h>
#include <palacios/vmm_mpi_accel.h>
#include <palacios/vmm_process_environment.h>
#include <palacios/vmm_execve_hook.h>


int v3_init_mpi_accel (struct guest_info * core) {
    //binfile = "./envtest";
    //args[1] = "LD_PRELOAD=./libcwrap.so";

    v3_hook_swintr(core, 0x80, v3_syscall_handler, NULL);
    v3_hook_syscall(core, 11, v3_sysexecve_handler, NULL);
    v3_hook_executable(core, "./envtest", v3_mpi_preload_handler, NULL);

    return 0;
}


int v3_deinit_mpi_accel (struct guest_info * core) {

    return 0;
}


int v3_mpi_preload_handler (struct guest_info * core, void * priv_data) {

    char * a[3];
    a[0] = "TEST=HITHERE";
    a[1] = "TEST2=/blah/blah/blah";
    a[2] = "LD_PRELOAD=./libcwrap.so";

    int ret = v3_inject_strings(core, (const char**)NULL, (const char**)a, 0, 3);
    if (ret == -1) {
        PrintDebug("Error injecting strings in execve handler\n");
        return -1;
    }

    return 0;
}


