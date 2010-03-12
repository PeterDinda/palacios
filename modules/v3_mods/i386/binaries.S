#;  -*- fundamental -*-

/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2010, Jack Lange <jarusl@cs.northwestern.edu>
 * Copyright (c) 2010, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

.data

#define TOSTR(str) #str
#define MOD_DIR(name) TOSTR(./modules/v3_mods/i386/binaries/name)

	
#ifdef CONFIG_V3_MOD_32BIT_TEST
.globl v3_mod_32bit_test_start
v3_mod_32bit_test_start:
.incbin MOD_DIR(test.vo)
.globl v3_mod_32bit_test_stop
v3_mod_32bit_test_stop:
#endif

