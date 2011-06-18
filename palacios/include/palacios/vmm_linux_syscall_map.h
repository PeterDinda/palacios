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

#ifndef __VMM_LINUX_SYSCALL_MAP_H__
#define __VMM_LINUX_SYSCALL_MAP_H__

#ifdef __V3VEE__

#include <palacios/vmm_types.h>

char * get_linux_syscall_name32 (uint_t syscall_nr);
char * get_linux_syscall_name64 (uint_t syscall_nr);

#endif

#endif
