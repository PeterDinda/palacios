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


struct v3_mod_info {
    int (*init)(char * args);
    int (*exit)();
} __attribute__((packed));


#define register_module(init_fn, exit_fn)                               \
    static struct v3_mod_info _v3_mod                                   \
    __attribute__((used))                                               \
        __attribute__((unused, __section__(".v3_linkage"),              \
                       aligned(8)))                                     \
        = {init_fn, exit_fn};


/*
 * Module interface API
 */


int v3_mod_test(void);
int v3_var_test;

int v3_printk(const char * fmt, ...);
