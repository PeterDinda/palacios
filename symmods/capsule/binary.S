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

#define IDENT(str) str
#define TOSTR(str) #str 
#define XSTR(str) TOSTR(str)
#define MOD_PATH(dir, name)  TOSTR(dir/name)
#define MOD_NM(name) TOSTR(name)


mod_start:
.incbin MOD_PATH(MOD_DIR,MOD_OBJ)
mod_stop:

mod_name:
.ascii MOD_NM(MOD_NAME)


.section _v3_capsules, "a"
IDENT(#ifdef __x86_64__)
.quad mod_name 
.quad mod_start
.quad mod_stop 
.byte MOD_TYPE
.byte MOD_ARCH
.space 2
IDENT(#else)
.long mod_name
.space 4
.long mod_start
.space 4
.long mod_stop
.space 4
.byte MOD_TYPE 
.byte MOD_ARCH
.space 2
IDENT(#endif)
