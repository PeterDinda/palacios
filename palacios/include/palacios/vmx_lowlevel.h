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

#ifndef __VMX_LOWLEVEL_H__
#define __VMX_LOWLEVEL_H__

#ifdef __V3VEE__


#define VMX_SUCCESS         0 
#define VMX_FAIL_INVALID    1
#define VMX_FAIL_VALID      2


#define VMWRITE_OP  ".byte 0x0f,0x79,0xc1;"           /* [eax],[ecx] */
#define VMREAD_OP   ".byte 0x0f,0x78,0xc1;"           /* [eax],[ecx] */
 
#define VMXON_OP    ".byte 0xf3,0x0f,0xc7,0x30;"         /*  [eax] */


static int inline v3_enable_vmx(addr_t host_state) {
    int ret;
    __asm__ __volatile__ (
			  VMXON_OP
			  "setnaeb %0;"
			  : "=q"(ret)
			  : "a"(host_state), "0"(ret)
			  : "memory"
			  );

    if (ret) {
	return -1;
    } 

    return 0;
}





static int inline vmcs_write(addr_t vmcs_index, addr_t value) {
    int ret_valid = 0;
    int ret_invalid = 0;

    __asm__ __volatile__ (
			  VMWRITE_OP
			  "seteb %0;"  // fail valid    (ZF=1)
			  "setnaeb %1;" // fail invalid (CF=1)
			  : "=q"(ret_valid), "=q"(ret_invalid)
			  : "a"(vmcs_index), "c"(&value), "0"(ret_valid), "1"(ret_invalid)
			  : "memory"
			  );

    if (ret_valid) {
	return VMX_FAIL_VALID;
    } else if (ret_invalid) {
	return VMX_FAIL_INVALID;
    }

    return VMX_SUCCESS;
}



static int inline vmcs_read(addr_t vmcs_index,  void * dst, int len) {
    addr_t val = 0;
    int ret_valid = 0;
    int ret_invalid = 0;

    __asm__ __volatile__ (
			  VMREAD_OP
			  "seteb %0;"  // fail valid    (ZF=1)
			  "setnaeb %1;" // fail invalid (CF=1)
			  : "=q"(ret_valid), "=q"(ret_invalid), "=c"(val)
			  : "a"(vmcs_index), "0"(ret_valid), "1"(ret_invalid)
			  : "memory"
			  );

    if (ret_valid) {
	return VMX_FAIL_VALID;
    } else if (ret_invalid) {
	return VMX_FAIL_INVALID;
    }

    return VMX_SUCCESS;
}




#endif

#endif
