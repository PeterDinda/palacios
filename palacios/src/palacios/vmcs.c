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

#include <palacios/vmcs.h>
#include <palacios/vmx_lowlevel.h>
#include <palacios/vmm.h>



//extern char * exception_names;
//
// Ignores "HIGH" addresses - 32 bit only for now
//


static inline void print_vmcs_field(vmcs_field_t vmcs_index) {
    int len = v3_vmcs_get_field_len(vmcs_index);
    addr_t val;
    
    if (vmcs_read(vmcs_index, &val, len) != VMX_SUCCESS) {
	PrintError("VMCS_READ error for index %x\n", vmcs_index);
	return;
    };
    
    if (len == 2) {
	PrintDebug("%s: %x\n", v3_vmcs_get_field_name(vmcs_index), (uint16_t)val);
    } else if (len == 4) {
	PrintDebug("%s: %x\n", v3_vmcs_get_field_name(vmcs_index), (uint32_t)val);
    } else if (len == 8) {
	PrintDebug("%s: %p\n", v3_vmcs_get_field_name(vmcs_index), (void *)(addr_t)val);
    }
}


static inline void print_vmcs_segments() {
    // see vm_guest.c
}




/*
void print_debug_vmcs_load_guest() {
    const int wordsize = sizeof(addr_t);
    uint64_t temp;
    struct vmcs_segment_access tmp_seg;

    PrintDebug("\n====== Loading Guest State ======\n");
    PRINT_VMREAD("Guest CR0: %x\n", GUEST_CR0, wordsize);
    PRINT_VMREAD("Guest CR3: %x\n", GUEST_CR3, wordsize);
    PRINT_VMREAD("Guest CR4: %x\n", GUEST_CR4, wordsize);
    PRINT_VMREAD("Guest DR7: %x\n", GUEST_DR7, wordsize);

    READ_VMCS_SEG(&tmp_seg,CS,wordsize);
    print_vmcs_segment("CS", &tmp_seg);
    
    READ_VMCS_SEG(&tmp_seg,SS,wordsize);
    print_vmcs_segment("SS", &tmp_seg);

    READ_VMCS_SEG(&tmp,DS,wordsize);
    print_vmcs_segment("DS", &tmp_seg);

    READ_VMCS_SEG(&tmp_seg,ES,wordsize);
    print_vmcs_segment("ES", &tmp_seg);

    READ_VMCS_SEG(&tmp_seg,FS,wordsize);
    print_vmcs_segment("FS", &tmp_seg);

    READ_VMCS_SEG(&tmp_seg,GS,wordsize);
    print_vmcs_segment("GS", &tmp_seg);

    READ_VMCS_SEG(&tmp_seg,TR,wordsize);
    print_vmcs_segment("TR", &tmp_seg);

    READ_VMCS_SEG(&tmp_seg,LDTR,wordsize);
    print_vmcs_segment("LDTR", &tmp_seg);
    
    PrintDebug("\n==GDTR==\n");
    PRINT_VMREAD("GDTR Base: %x\n", GUEST_GDTR_BASE, wordsize);
    PRINT_VMREAD("GDTR Limit: %x\n", GUEST_GDTR_LIMIT, 32);
    PrintDebug("====\n");

    PrintDebug("\n==LDTR==\n");
    PRINT_VMREAD("LDTR Base: %x\n", GUEST_LDTR_BASE, wordsize);
    PRINT_VMREAD("LDTR Limit: %x\n", GUEST_LDTR_LIMIT, 32);
    PrintDebug("=====\n");

    PRINT_VMREAD("Guest RSP: %x\n", GUEST_RSP, wordsize);
    PRINT_VMREAD("Guest RIP: %x\n", GUEST_RIP, wordsize);
    PRINT_VMREAD("Guest RFLAGS: %x\n", GUEST_RFLAGS, wordsize);
    PRINT_VMREAD("Guest Activity state: %x\n", GUEST_ACTIVITY_STATE, 32);
    PRINT_VMREAD("Guest Interruptibility state: %x\n", GUEST_INT_STATE, 32);
    PRINT_VMREAD("Guest pending debug: %x\n", GUEST_PENDING_DEBUG_EXCS, wordsize);

    PRINT_VMREAD("IA32_DEBUGCTL: %x\n", GUEST_IA32_DEBUGCTL, 64);
    PRINT_VMREAD("IA32_SYSENTER_CS: %x\n", GUEST_IA32_SYSENTER_CS, 32);
    PRINT_VMREAD("IA32_SYSTENTER_ESP: %x\n", GUEST_IA32_SYSENTER_ESP, wordsize);
    PRINT_VMREAD("IA32_SYSTENTER_EIP: %x\n", GUEST_IA32_SYSENTER_EIP, wordsize);
    PRINT_VMREAD("IA32_PERF_GLOBAL_CTRL: %x\n", GUEST_IA32_PERF_GLOBAL_CTRL, wordsize);
    PRINT_VMREAD("VMCS Link Ptr: %x\n", VMCS_LINK_PTR, 64);
    // TODO: Maybe add VMX preemption timer and PDTE (Intel 20-8 Vol. 3b)
}

void print_debug_load_host() {
    const int wordsize = sizeof(addr_t);
    uint64_t temp;
    vmcs_segment tmp_seg;

    PrintDebug("\n====== Host State ========\n");
    PRINT_VMREAD("Host CR0: %x\n", HOST_CR0, wordsize);
    PRINT_VMREAD("Host CR3: %x\n", HOST_CR3, wordsize);
    PRINT_VMREAD("Host CR4: %x\n", HOST_CR4, wordsize);
    PRINT_VMREAD("Host RSP: %x\n", HOST_RSP, wordsize);
    PRINT_VMREAD("Host RIP: %x\n", HOST_RIP, wordsize);
    PRINT_VMREAD("IA32_SYSENTER_CS: %x\n", HOST_IA32_SYSENTER_CS, 32);
    PRINT_VMREAD("IA32_SYSENTER_ESP: %x\n", HOST_IA32_SYSENTER_ESP, wordsize);
    PRINT_VMREAD("IA32_SYSENTER_EIP: %x\n", HOST_IA32_SYSENTER_EIP, wordsize);
        
    PRINT_VMREAD("Host CS Selector: %x\n", HOST_CS_SELECTOR, 16);
    PRINT_VMREAD("Host SS Selector: %x\n", HOST_SS_SELECTOR, 16);
    PRINT_VMREAD("Host DS Selector: %x\n", HOST_DS_SELECTOR, 16);
    PRINT_VMREAD("Host ES Selector: %x\n", HOST_ES_SELECTOR, 16);
    PRINT_VMREAD("Host FS Selector: %x\n", HOST_FS_SELECTOR, 16);
    PRINT_VMREAD("Host GS Selector: %x\n", HOST_GS_SELECTOR, 16);
    PRINT_VMREAD("Host TR Selector: %x\n", HOST_TR_SELECTOR, 16);

    PRINT_VMREAD("Host FS Base: %x\n", HOST_FS_BASE, wordsize);
    PRINT_VMREAD("Host GS Base: %x\n", HOST_GS_BASE, wordsize);
    PRINT_VMREAD("Host TR Base: %x\n", HOST_TR_BASE, wordsize);
    PRINT_VMREAD("Host GDTR Base: %x\n", HOST_GDTR_BASE, wordsize);
    PRINT_VMREAD("Host IDTR Base: %x\n", HOSE_IDTR_BASE, wordsize);
}

void print_vmcs_segment(char * name, vmcs_segment* seg)
{
    PrintDebug("\n==VMCS %s Segment==\n",name);
    PrintDebug("\tSelector: %x\n", seg->selector);
    PrintDebug("\tBase Address: %x\n", seg->baseAddr);
    PrintDebug("\tLimit: %x\n", seg->limit);
    PrintDebug("\tAccess: %x\n", seg->access);
}*/

/*
 * Returns the field length in bytes
 */
int v3_vmcs_get_field_len(vmcs_field_t field)
{
    switch(field)
    {
        case VMCS_GUEST_ES_SELECTOR:
        case VMCS_GUEST_CS_SELECTOR:
        case VMCS_GUEST_SS_SELECTOR:
        case VMCS_GUEST_DS_SELECTOR:
        case VMCS_GUEST_FS_SELECTOR:
        case VMCS_GUEST_GS_SELECTOR:
        case VMCS_GUEST_LDTR_SELECTOR:
        case VMCS_GUEST_TR_SELECTOR:
            /* 16 bit host state */
        case VMCS_HOST_ES_SELECTOR:
        case VMCS_HOST_CS_SELECTOR:
        case VMCS_HOST_SS_SELECTOR:
        case VMCS_HOST_DS_SELECTOR:
        case VMCS_HOST_FS_SELECTOR:
        case VMCS_HOST_GS_SELECTOR:
        case VMCS_HOST_TR_SELECTOR:
            return 2;
            /* 64 bit control fields */
        case IO_BITMAP_A_ADDR:
        case IO_BITMAP_A_ADDR_HIGH:
        case IO_BITMAP_B_ADDR:
        case IO_BITMAP_B_ADDR_HIGH:
        case MSR_BITMAPS:
        case MSR_BITMAPS_HIGH:
        case VM_EXIT_MSR_STORE_ADDR:
        case VM_EXIT_MSR_STORE_ADDR_HIGH:
        case VM_EXIT_MSR_LOAD_ADDR:
        case VM_EXIT_MSR_LOAD_ADDR_HIGH:
        case VM_ENTRY_MSR_LOAD_ADDR:
        case VM_ENTRY_MSR_LOAD_ADDR_HIGH:
        case VMCS_EXEC_PTR:
        case VMCS_EXEC_PTR_HIGH:
        case TSC_OFFSET:
        case TSC_OFFSET_HIGH:
        case VIRT_APIC_PAGE_ADDR:
        case VIRT_APIC_PAGE_ADDR_HIGH:
            /* 64 bit guest state fields */
        case VMCS_LINK_PTR:
        case VMCS_LINK_PTR_HIGH:
        case GUEST_IA32_DEBUGCTL:
        case GUEST_IA32_DEBUGCTL_HIGH:
        case GUEST_IA32_PERF_GLOBAL_CTRL:
        case GUEST_IA32_PERF_GLOBAL_CTRL_HIGH:
            return 8;
            /* 32 bit control fields */
        case PIN_VM_EXEC_CTRLS:
        case PROC_VM_EXEC_CTRLS:
        case EXCEPTION_BITMAP:
        case PAGE_FAULT_ERROR_MASK:
        case PAGE_FAULT_ERROR_MATCH:
        case CR3_TARGET_COUNT:
        case VM_EXIT_CTRLS:
        case VM_EXIT_MSR_STORE_COUNT:
        case VM_EXIT_MSR_LOAD_COUNT:
        case VM_ENTRY_CTRLS:
        case VM_ENTRY_MSR_LOAD_COUNT:
        case VM_ENTRY_INT_INFO_FIELD:
        case VM_ENTRY_EXCEPTION_ERROR:
        case VM_ENTRY_INSTR_LENGTH:
        case TPR_THRESHOLD:
            /* 32 bit Read Only data fields */
        case VM_INSTR_ERROR:
        case EXIT_REASON:
        case VM_EXIT_INT_INFO:
        case VM_EXIT_INT_ERROR:
        case IDT_VECTOR_INFO:
        case IDT_VECTOR_ERROR:
        case VM_EXIT_INSTR_LENGTH:
        case VMX_INSTR_INFO:
            /* 32 bit Guest state fields */
        case GUEST_ES_LIMIT:
        case GUEST_CS_LIMIT:
        case GUEST_SS_LIMIT:
        case GUEST_DS_LIMIT:
        case GUEST_FS_LIMIT:
        case GUEST_GS_LIMIT:
        case GUEST_LDTR_LIMIT:
        case GUEST_TR_LIMIT:
        case GUEST_GDTR_LIMIT:
        case GUEST_IDTR_LIMIT:
        case GUEST_ES_ACCESS:
        case GUEST_CS_ACCESS:
        case GUEST_SS_ACCESS:
        case GUEST_DS_ACCESS:
        case GUEST_FS_ACCESS:
        case GUEST_GS_ACCESS:
        case GUEST_LDTR_ACCESS:
        case GUEST_TR_ACCESS:
        case GUEST_INT_STATE:
        case GUEST_ACTIVITY_STATE:
        case GUEST_SMBASE:
        case GUEST_IA32_SYSENTER_CS:
            /* 32 bit host state field */
        case HOST_IA32_SYSENTER_CS:
            return 4;
            /* Natural Width Control Fields */
        case CR0_GUEST_HOST_MASK:
        case CR4_GUEST_HOST_MASK:
        case CR0_READ_SHADOW:
        case CR4_READ_SHADOW:
        case CR3_TARGET_VALUE_0:
        case CR3_TARGET_VALUE_1:
        case CR3_TARGET_VALUE_2:
        case CR3_TARGET_VALUE_3:
            /* Natural Width Read Only Fields */
        case EXIT_QUALIFICATION:
        case IO_RCX:
        case IO_RSI:
        case IO_RDI:
        case IO_RIP:
        case GUEST_LINEAR_ADDR:
            /* Natural Width Guest State Fields */
        case GUEST_CR0:
        case GUEST_CR3:
        case GUEST_CR4:
        case GUEST_ES_BASE:
        case GUEST_CS_BASE:
        case GUEST_SS_BASE:
        case GUEST_DS_BASE:
        case GUEST_FS_BASE:
        case GUEST_GS_BASE:
        case GUEST_LDTR_BASE:
        case GUEST_TR_BASE:
        case GUEST_GDTR_BASE:
        case GUEST_IDTR_BASE:
        case GUEST_DR7:
        case GUEST_RSP:
        case GUEST_RIP:
        case GUEST_RFLAGS:
        case GUEST_PENDING_DEBUG_EXCS:
        case GUEST_IA32_SYSENTER_ESP:
        case GUEST_IA32_SYSENTER_EIP:
            /* Natural Width Host State Fields */
        case HOST_CR0:
        case HOST_CR3:
        case HOST_CR4:
        case HOST_FS_BASE:
        case HOST_GS_BASE:
        case HOST_TR_BASE:
        case HOST_GDTR_BASE:
        case HOST_IDTR_BASE:
        case HOST_IA32_SYSENTER_ESP:
        case HOST_IA32_SYSENTER_EIP:
        case HOST_RSP:
        case HOST_RIP:
     #ifdef __V3_64BIT__
            return 8;
#else
            return 4;
#endif
        default:
            return -1;
    }
}

char* v3_vmcs_get_field_name(vmcs_field_t field)
{   
    switch(field)
    {
        case VMCS_GUEST_ES_SELECTOR:
            return "VMCS_GUEST_ES_SELECTOR";
        case VMCS_GUEST_CS_SELECTOR:
            return "VMCS_GUEST_CS_SELECTOR";
        case VMCS_GUEST_SS_SELECTOR:
            return "VMCS_GUEST_SS_SELECTOR";
        case VMCS_GUEST_DS_SELECTOR:
            return "VMCS_GUEST_DS_SELECTOR";
        case VMCS_GUEST_FS_SELECTOR:
            return "VMCS_GUEST_FS_SELECTOR";
        case VMCS_GUEST_GS_SELECTOR:
            return "VMCS_GUEST_GS_SELECTOR";
        case VMCS_GUEST_LDTR_SELECTOR:
            return "VMCS_GUEST_LDTR_SELECTOR";
        case VMCS_GUEST_TR_SELECTOR:
            return "VMCS_GUEST_TR_SELECTOR";
        case VMCS_HOST_ES_SELECTOR:
            return "VMCS_HOST_ES_SELECTOR";
        case VMCS_HOST_CS_SELECTOR:
            return "VMCS_HOST_CS_SELECTOR";
        case VMCS_HOST_SS_SELECTOR:
            return "VMCS_HOST_SS_SELECTOR";
        case VMCS_HOST_DS_SELECTOR:
            return "VMCS_HOST_DS_SELECTOR";
        case VMCS_HOST_FS_SELECTOR:
            return "VMCS_HOST_FS_SELECTOR";
        case VMCS_HOST_GS_SELECTOR:
            return "VMCS_HOST_GS_SELECTOR";
        case VMCS_HOST_TR_SELECTOR:
            return "VMCS_HOST_TR_SELECTOR";
        case IO_BITMAP_A_ADDR:
            return "IO_BITMAP_A_ADDR";
        case IO_BITMAP_A_ADDR_HIGH:
            return "IO_BITMAP_A_ADDR_HIGH";
        case IO_BITMAP_B_ADDR:
            return "IO_BITMAP_B_ADDR";
        case IO_BITMAP_B_ADDR_HIGH:
            return "IO_BITMAP_B_ADDR_HIGH";
        case MSR_BITMAPS:
            return "MSR_BITMAPS";
        case MSR_BITMAPS_HIGH:
            return "MSR_BITMAPS_HIGH";
        case VM_EXIT_MSR_STORE_ADDR:
            return "VM_EXIT_MSR_STORE_ADDR";
        case VM_EXIT_MSR_STORE_ADDR_HIGH:
            return "VM_EXIT_MSR_STORE_ADDR_HIGH";
        case VM_EXIT_MSR_LOAD_ADDR:
            return "VM_EXIT_MSR_LOAD_ADDR";
        case VM_EXIT_MSR_LOAD_ADDR_HIGH:
            return "VM_EXIT_MSR_LOAD_ADDR_HIGH";
        case VM_ENTRY_MSR_LOAD_ADDR:
            return "VM_ENTRY_MSR_LOAD_ADDR";
        case VM_ENTRY_MSR_LOAD_ADDR_HIGH:
            return "VM_ENTRY_MSR_LOAD_ADDR_HIGH";
        case VMCS_EXEC_PTR:
            return "VMCS_EXEC_PTR";
        case VMCS_EXEC_PTR_HIGH:
            return "VMCS_EXEC_PTR_HIGH";
        case TSC_OFFSET:
            return "TSC_OFFSET";
        case TSC_OFFSET_HIGH:
            return "TSC_OFFSET_HIGH";
        case VIRT_APIC_PAGE_ADDR:
            return "VIRT_APIC_PAGE_ADDR";
        case VIRT_APIC_PAGE_ADDR_HIGH:
            return "VIRT_APIC_PAGE_ADDR_HIGH";
        case VMCS_LINK_PTR:
            return "VMCS_LINK_PTR";
        case VMCS_LINK_PTR_HIGH:
            return "VMCS_LINK_PTR_HIGH";
        case GUEST_IA32_DEBUGCTL:
            return "GUEST_IA32_DEBUGCTL";
        case GUEST_IA32_DEBUGCTL_HIGH:
            return "GUEST_IA32_DEBUGCTL_HIGH";
        case GUEST_IA32_PERF_GLOBAL_CTRL:
            return "GUEST_IA32_PERF_GLOBAL_CTRL";
        case GUEST_IA32_PERF_GLOBAL_CTRL_HIGH:
            return "GUEST_IA32_PERF_GLOBAL_CTRL_HIGH";
        case PIN_VM_EXEC_CTRLS:
            return "PIN_VM_EXEC_CTRLS";
        case PROC_VM_EXEC_CTRLS:
            return "PROC_VM_EXEC_CTRLS";
        case EXCEPTION_BITMAP:
            return "EXCEPTION_BITMAP";
        case PAGE_FAULT_ERROR_MASK:
            return "PAGE_FAULT_ERROR_MASK";
        case PAGE_FAULT_ERROR_MATCH:
            return "PAGE_FAULT_ERROR_MATCH";
        case CR3_TARGET_COUNT:
            return "CR3_TARGET_COUNT";
        case VM_EXIT_CTRLS:
            return "VM_EXIT_CTRLS";
        case VM_EXIT_MSR_STORE_COUNT:
            return "VM_EXIT_MSR_STORE_COUNT";
        case VM_EXIT_MSR_LOAD_COUNT:
            return "VM_EXIT_MSR_LOAD_COUNT";
        case VM_ENTRY_CTRLS:
            return "VM_ENTRY_CTRLS";
        case VM_ENTRY_MSR_LOAD_COUNT:
            return "VM_ENTRY_MSR_LOAD_COUNT";
        case VM_ENTRY_INT_INFO_FIELD:
            return "VM_ENTRY_INT_INFO_FIELD";
        case VM_ENTRY_EXCEPTION_ERROR:
            return "VM_ENTRY_EXCEPTION_ERROR";
        case VM_ENTRY_INSTR_LENGTH:
            return "VM_ENTRY_INSTR_LENGTH";
        case TPR_THRESHOLD:
            return "TPR_THRESHOLD";
        case VM_INSTR_ERROR:
            return "VM_INSTR_ERROR";
        case EXIT_REASON:
            return "EXIT_REASON";
        case VM_EXIT_INT_INFO:
            return "VM_EXIT_INT_INFO";
        case VM_EXIT_INT_ERROR:
            return "VM_EXIT_INT_ERROR";
        case IDT_VECTOR_INFO:
            return "IDT_VECTOR_INFO";
        case IDT_VECTOR_ERROR:
            return "IDT_VECTOR_ERROR";
        case VM_EXIT_INSTR_LENGTH:
            return "VM_EXIT_INSTR_LENGTH";
        case VMX_INSTR_INFO:
            return "VMX_INSTR_INFO";
        case GUEST_ES_LIMIT:
            return "GUEST_ES_LIMIT";
        case GUEST_CS_LIMIT:
            return "GUEST_CS_LIMIT";
        case GUEST_SS_LIMIT:
            return "GUEST_SS_LIMIT";
        case GUEST_DS_LIMIT:
            return "GUEST_DS_LIMIT";
        case GUEST_FS_LIMIT:
            return "GUEST_FS_LIMIT";
        case GUEST_GS_LIMIT:
            return "GUEST_GS_LIMIT";
        case GUEST_LDTR_LIMIT:
            return "GUEST_LDTR_LIMIT";
        case GUEST_TR_LIMIT:
            return "GUEST_TR_LIMIT";
        case GUEST_GDTR_LIMIT:
            return "GUEST_GDTR_LIMIT";
        case GUEST_IDTR_LIMIT:
            return "GUEST_IDTR_LIMIT";
        case GUEST_ES_ACCESS:
            return "GUEST_ES_ACCESS";
        case GUEST_CS_ACCESS:
            return "GUEST_CS_ACCESS";
        case GUEST_SS_ACCESS:
            return "GUEST_SS_ACCESS";
        case GUEST_DS_ACCESS:
            return "GUEST_DS_ACCESS";
        case GUEST_FS_ACCESS:
            return "GUEST_FS_ACCESS";
        case GUEST_GS_ACCESS:
            return "GUEST_GS_ACCESS";
        case GUEST_LDTR_ACCESS:
            return "GUEST_LDTR_ACCESS";
        case GUEST_TR_ACCESS:
            return "GUEST_TR_ACCESS";
        case GUEST_INT_STATE:
            return "GUEST_INT_STATE";
        case GUEST_ACTIVITY_STATE:
            return "GUEST_ACTIVITY_STATE";
        case GUEST_SMBASE:
            return "GUEST_SMBASE";
        case GUEST_IA32_SYSENTER_CS:
            return "GUEST_IA32_SYSENTER_CS";
        case HOST_IA32_SYSENTER_CS:
            return "HOST_IA32_SYSENTER_CS";
        case CR0_GUEST_HOST_MASK:
            return "CR0_GUEST_HOST_MASK";
        case CR4_GUEST_HOST_MASK:
            return "CR4_GUEST_HOST_MASK";
        case CR0_READ_SHADOW:
            return "CR0_READ_SHADOW";
        case CR4_READ_SHADOW:
            return "CR4_READ_SHADOW";
        case CR3_TARGET_VALUE_0:
            return "CR3_TARGET_VALUE_0";
        case CR3_TARGET_VALUE_1:
            return "CR3_TARGET_VALUE_1";
        case CR3_TARGET_VALUE_2:
            return "CR3_TARGET_VALUE_2";
        case CR3_TARGET_VALUE_3:
            return "CR3_TARGET_VALUE_3";
        case EXIT_QUALIFICATION:
            return "EXIT_QUALIFICATION";
        case IO_RCX:
            return "IO_RCX";
        case IO_RSI:
            return "IO_RSI";
        case IO_RDI:
            return "IO_RDI";
        case IO_RIP:
            return "IO_RIP";
        case GUEST_LINEAR_ADDR:
            return "GUEST_LINEAR_ADDR";
        case GUEST_CR0:
            return "GUEST_CR0";
        case GUEST_CR3:
            return "GUEST_CR3";
        case GUEST_CR4:
            return "GUEST_CR4";
        case GUEST_ES_BASE:
            return "GUEST_ES_BASE";
        case GUEST_CS_BASE:
            return "GUEST_CS_BASE";
        case GUEST_SS_BASE:
            return "GUEST_SS_BASE";
        case GUEST_DS_BASE:
            return "GUEST_DS_BASE";
        case GUEST_FS_BASE:
            return "GUEST_FS_BASE";
        case GUEST_GS_BASE:
            return "GUEST_GS_BASE";
        case GUEST_LDTR_BASE:
            return "GUEST_LDTR_BASE";
        case GUEST_TR_BASE:
            return "GUEST_TR_BASE";
        case GUEST_GDTR_BASE:
            return "GUEST_GDTR_BASE";
        case GUEST_IDTR_BASE:
            return "GUEST_IDTR_BASE";
        case GUEST_DR7:
            return "GUEST_DR7";
        case GUEST_RSP:
            return "GUEST_RSP";
        case GUEST_RIP:
            return "GUEST_RIP";
        case GUEST_RFLAGS:
            return "GUEST_RFLAGS";
        case GUEST_PENDING_DEBUG_EXCS:
            return "GUEST_PENDING_DEBUG_EXCS";
        case GUEST_IA32_SYSENTER_ESP:
            return "GUEST_IA32_SYSENTER_ESP";
        case GUEST_IA32_SYSENTER_EIP:
            return "GUEST_IA32_SYSENTER_EIP";
        case HOST_CR0:
            return "HOST_CR0";
        case HOST_CR3:
            return "HOST_CR3";
        case HOST_CR4:
            return "HOST_CR4";
        case HOST_FS_BASE:
            return "HOST_FS_BASE";
        case HOST_GS_BASE:
            return "HOST_GS_BASE";
        case HOST_TR_BASE:
            return "HOST_TR_BASE";
        case HOST_GDTR_BASE:
            return "HOST_GDTR_BASE";
        case HOST_IDTR_BASE:
            return "HOST_IDTR_BASE";
        case HOST_IA32_SYSENTER_ESP:
            return "HOST_IA32_SYSENTER_ESP";
        case HOST_IA32_SYSENTER_EIP:
            return "HOST_IA32_SYSENTER_EIP";
        case HOST_RSP:
            return "HOST_RSP";
        case HOST_RIP:
            return "HOST_RIP";
        default:
            return NULL;
    }
}



