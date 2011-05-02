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
 * Copyright (c) 2008, Andy Gocke <agocke@gmail.com>
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Andy Gocke <agocke@gmail.com>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

.data

#ifdef V3_CONFIG_VMX
.globl v3_vmxassist_start
v3_vmxassist_start:
.incbin V3_CONFIG_VMXASSIST_PATH
.globl v3_vmxassist_end
v3_vmxassist_end:
#endif


.globl v3_vgabios_start
v3_vgabios_start:
.incbin V3_CONFIG_VGABIOS_PATH
.global v3_vgabios_end
v3_vgabios_end:

.globl v3_rombios_start
v3_rombios_start:
.incbin V3_CONFIG_ROMBIOS_PATH
.globl v3_rombios_end
v3_rombios_end:


#ifdef V3_CONFIG_USE_PXE_BIOS
.globl pxebios_start
pxebios_start:
.incbin V3_CONFIG_PXEBIOS_PATH
.globl pxebios_end
pxebios_end:
#endif


/* temporary module */
/*
.globl symmod_start
symmod_start:
.incbin "./palacios/src/modules/test.ko"
.globl symmod_end
symmod_end:*/

