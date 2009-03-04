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

#ifndef __VMM_PROFILER_H__
#define __VMM_PROFILER_H__

#ifdef __V3VEE__

#include <palacios/vmm_rbtree.h>

struct guest_info;


struct v3_profiler {
    uint_t total_exits;

    ullong_t start_time;
    ullong_t end_time;

    uint_t guest_pf_cnt;

    struct rb_root root;
};


void v3_init_profiler(struct guest_info * info);

void v3_profile_exit(struct guest_info * info, uint_t exit_code);

void v3_print_profile(struct guest_info * info);


#endif

#endif
