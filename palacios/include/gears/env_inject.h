/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2012, Kyle C. Hale <kh@u.northwestern.edu> 
 * Copyright (c) 2012, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __ENV_INJECT_H__
#define __ENV_INJECT_H__

int v3_insert_env_inject (void *ginfo, char ** strings, int num_strings, char * bin_name);

#ifdef __V3VEE__

struct v3_env_injects {
    struct list_head env_inject_list;
};


struct v3_env_inject_info {
    char ** env_vars;
    int num_env_vars;
    struct list_head inject_node;
    char * bin_name;
};

int v3_remove_env_inject (struct v3_vm_info * vm, struct v3_env_inject_info * inject);

#endif  /* ! __V3VEE__ */

#endif
