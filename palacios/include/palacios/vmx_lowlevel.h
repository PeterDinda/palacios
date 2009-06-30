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

// vmfail macro
#define CHECK_VMXFAIL(ret_valid, ret_invalid)	\
    if (ret_valid) {				\
        return VMX_FAIL_VALID;			\
    } else if (ret_invalid) {			\
        return VMX_FAIL_INVALID;		\
    }

/* Opcode definitions for all the VM instructions */

#define VMCLEAR_OPCODE  ".byte 0x66,0xf,0x67;" /* reg=/6 */
#define VMRESUME_OPCODE ".byte 0x0f,0x01,0xc3;"
#define VMPTRLD_OPCODE  ".byte 0x0f,0xc7;" /* reg=/6 */
#define VMPTRST_OPCODE  ".byte 0x0f,0xc7;" /* reg=/7 */
#define VMREAD_OPCODE   ".byte 0x0f,0x78;"
#define VMWRITE_OPCODE  ".byte 0x0f,0x79;"
#define VMXOFF_OPCODE   ".byte 0x0f,0x01,0xc4;"
#define VMXON_OPCODE    ".byte 0xf3,0x0f,0xc7;" /* reg=/6 */


/* Mod/rm definitions for intel registers/memory */
#define EAX_ECX_MODRM   ".byte 0xc1;"
// %eax with /6 reg
#define EAX_06_MODRM    ".byte 0x30;"
// %eax with /7 reg
#define EAX_07_MODRM    ".byte 0x38;"



static inline int v3_enable_vmx(uint64_t host_state) {
    uint8_t ret_invalid = 0;

    __asm__ __volatile__ (
                VMXON_OPCODE
                EAX_06_MODRM
                "setnaeb %0;" // fail invalid (CF=1)
                : "=q"(ret_invalid)
                : "a"(&host_state),"0"(ret_invalid)
                : "memory");

    if (ret_invalid) {
        return VMX_FAIL_INVALID;
    } else {
        return VMX_SUCCESS;
    }
}

// No vmcall necessary - is only executed by the guest

static inline int vmcs_clear(uint64_t addr) {
    uint8_t ret_valid = 0;
    uint8_t ret_invalid = 0;

    __asm__ __volatile__ (
            VMCLEAR_OPCODE
            EAX_06_MODRM
            "seteb %0;" // fail valid (ZF=1)
            "setnaeb %1;" // fail invalid (CF=1)
            : "=q"(ret_valid), "=q"(ret_invalid)
            : "a"(&addr), "0"(ret_valid), "1"(ret_invalid)
            : "memory");

    CHECK_VMXFAIL(ret_valid, ret_invalid);
  
    return VMX_SUCCESS;
}


static inline int vmcs_resume() {
    uint8_t ret_valid = 0;
    uint8_t ret_invalid = 0;

    __asm__ __volatile__ (
                VMRESUME_OPCODE
                "seteb %0;"
                "setnaeb %1;"
                : "=q"(ret_valid), "=q"(ret_invalid)
                : "0"(ret_valid), "1"(ret_invalid)
                : "memory");

    CHECK_VMXFAIL(ret_valid, ret_invalid);

    return VMX_SUCCESS;
}


static inline int vmcs_load(vmcs_t * vmcs_ptr) {
    uint64_t addr = (uint64_t)vmcs_ptr;
    uint8_t ret_valid = 0;
    uint8_t ret_invalid = 0;
    
    __asm__ __volatile__ (
                VMPTRLD_OPCODE
                EAX_06_MODRM
                "seteb %0;" // fail valid (ZF=1)
                "setnaeb %1;"  // fail invalid (CF=1)
                : "=q"(ret_valid), "=q"(ret_invalid)
                : "a"(&addr), "0"(ret_valid), "1"(ret_invalid)
                : "memory");
    
    CHECK_VMXFAIL(ret_valid, ret_invalid);

    return VMX_SUCCESS;
}

static inline int vmcs_store(vmcs_t * vmcs_ptr) {
    uint64_t addr = (uint64_t)vmcs_ptr;

    __asm__ __volatile__ (
               VMPTRSRT_OPCODE
               EAX_07_MODRM
               :
               : "a"(&addr)
               : "memory");

    return VMX_SUCCESS;
}

/* According to Intel, vmread will return an architecure sized type - be sure that
 * dst is at least 64-bits in IA-32e and 32 otherwise */
static inline int vmcs_read(addr_t vmcs_index, void * dst) {
    addr_t val = 0;
    uint8_t ret_valid = 0;
    uint8_t ret_invalid = 0;

    __asm__ __volatile__ (  
                VMREAD_OPCODE
                EAX_ECX_MODRM
                "seteb %0;" // fail valid
                "setnaeb %1;" // fail invalid
                : "=q"(ret_valid), "=q"(ret_invalid), "=c"(val) // Use ECX
                : "a" (vmcs_index), "0"(ret_valid), "1"(ret_invalid)
                : "memory"
                );

    CHECK_VMXFAIL(ret_valid, ret_invalid);

    // TODO: Fix this, will have to do a cast because dst will be variable length
    *dst = val;

    return VMX_SUCCESS;
}

static inline int vmcs_write(addr_t vmcs_index, addr_t value) {
    uint8_t ret_valid = 0;
    uint8_t ret_invalid = 0;

    __asm__ __volatile__ (
                VMWRITE_OPCODE
                EAX_ECX_MODRM
                "seteb %0;" // fail valid (ZF=1)
                "setnaeb %1;" // fail invalid (CF=1)
                : "=q" (ret_valid), "=q" (ret_invalid)
                : "a" (vmcs_index), "c"(value), "0"(ret_valid), "1"(ret_invalid)
                : "memory");

    CHECK_VMXFAIL(ret_valid, ret_invalid);

    return VMX_SUCCESS;
}

static inline int vmx_off() {
    uint8_t ret_valid = 0;
    uint8_t ret_invalid = 0;

    __asm__ __volatile__ (
                VMXOFF_OPCODE
                "seteb %0;"
                "setnaeb %1;"
                : "=q"(ret_valid), "=q"(ret_invalid)
                : "0"(ret_valid), "1"(ret_invalid)
                : "memory");

    CHECK_VMXFAIL(ret_valid, ret_invalid);

    return VMX_SUCCESS;
}

#endif

#endif
