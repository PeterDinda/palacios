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


static const char * vmcs_field_to_str(vmcs_field_t field);

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
	PrintDebug("%s: %x\n", vmcs_field_to_str(vmcs_index), (uint16_t)val);
    } else if (len == 4) {
	PrintDebug("%s: %x\n", vmcs_field_to_str(vmcs_index), (uint32_t)val);
    } else if (len == 8) {
	PrintDebug("%s: %p\n", vmcs_field_to_str(vmcs_index), (void *)(addr_t)val);
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
int v3_vmcs_get_field_len(vmcs_field_t field) {
    switch(field)  {
	/* 16 bit Control Fields */
        case VMCS_GUEST_ES_SELECTOR:
        case VMCS_GUEST_CS_SELECTOR:
        case VMCS_GUEST_SS_SELECTOR:
        case VMCS_GUEST_DS_SELECTOR:
        case VMCS_GUEST_FS_SELECTOR:
        case VMCS_GUEST_GS_SELECTOR:
        case VMCS_GUEST_LDTR_SELECTOR:
        case VMCS_GUEST_TR_SELECTOR:
        case VMCS_HOST_ES_SELECTOR:
        case VMCS_HOST_CS_SELECTOR:
        case VMCS_HOST_SS_SELECTOR:
        case VMCS_HOST_DS_SELECTOR:
        case VMCS_HOST_FS_SELECTOR:
        case VMCS_HOST_GS_SELECTOR:
        case VMCS_HOST_TR_SELECTOR:
            return 2;

	/* 32 bit Control Fields */
        case VMCS_PIN_CTRLS:
        case VMCS_PROC_CTRLS:
        case VMCS_EXCP_BITMAP:
        case VMCS_PG_FAULT_ERR_MASK:
        case VMCS_PG_FAULT_ERR_MATCH:
        case VMCS_CR3_TGT_CNT:
        case VMCS_EXIT_CTRLS:
        case VMCS_EXIT_MSR_STORE_CNT:
        case VMCS_EXIT_MSR_LOAD_CNT:
        case VMCS_ENTRY_CTRLS:
        case VMCS_ENTRY_MSR_LOAD_CNT:
        case VMCS_ENTRY_INT_INFO:
        case VMCS_ENTRY_EXCP_ERR:
        case VMCS_ENTRY_INSTR_LEN:
        case VMCS_TPR_THRESHOLD:
        case VMCS_INSTR_ERR:
        case VMCS_EXIT_REASON:
        case VMCS_EXIT_INT_INFO:
        case VMCS_EXIT_INT_ERR:
        case VMCS_IDT_VECTOR_INFO:
        case VMCS_IDT_VECTOR_ERR:
        case VMCS_EXIT_INSTR_LEN:
        case VMCS_VMX_INSTR_INFO:
        case VMCS_GUEST_ES_LIMIT:
        case VMCS_GUEST_CS_LIMIT:
        case VMCS_GUEST_SS_LIMIT:
        case VMCS_GUEST_DS_LIMIT:
        case VMCS_GUEST_FS_LIMIT:
        case VMCS_GUEST_GS_LIMIT:
        case VMCS_GUEST_LDTR_LIMIT:
        case VMCS_GUEST_TR_LIMIT:
        case VMCS_GUEST_GDTR_LIMIT:
        case VMCS_GUEST_IDTR_LIMIT:
        case VMCS_GUEST_ES_ACCESS:
        case VMCS_GUEST_CS_ACCESS:
        case VMCS_GUEST_SS_ACCESS:
        case VMCS_GUEST_DS_ACCESS:
        case VMCS_GUEST_FS_ACCESS:
        case VMCS_GUEST_GS_ACCESS:
        case VMCS_GUEST_LDTR_ACCESS:
        case VMCS_GUEST_TR_ACCESS:
        case VMCS_GUEST_INT_STATE:
        case VMCS_GUEST_ACTIVITY_STATE:
        case VMCS_GUEST_SMBASE:
        case VMCS_GUEST_SYSENTER_CS:
        case VMCS_HOST_SYSENTER_CS:
            return 4;


	/* high bits of variable width fields
	 * We can probably just delete most of these....
	 */
        case VMCS_IO_BITMAP_A_ADDR_HIGH:
        case VMCS_IO_BITMAP_B_ADDR_HIGH:
        case VMCS_MSR_BITMAP_HIGH:
        case VMCS_EXIT_MSR_STORE_ADDR_HIGH:
        case VMCS_EXIT_MSR_LOAD_ADDR_HIGH:
        case VMCS_ENTRY_MSR_LOAD_ADDR_HIGH:
        case VMCS_EXEC_PTR_HIGH:
        case VMCS_TSC_OFFSET_HIGH:
        case VMCS_VAPIC_ADDR_HIGH:
        case VMCS_LINK_PTR_HIGH:
        case VMCS_GUEST_DBG_CTL_HIGH:
        case VMCS_GUEST_PERF_GLOBAL_CTRL_HIGH:
            return 4;

            /* Natural Width Control Fields */
        case VMCS_IO_BITMAP_A_ADDR:
        case VMCS_IO_BITMAP_B_ADDR:
        case VMCS_MSR_BITMAP:
        case VMCS_EXIT_MSR_STORE_ADDR:
        case VMCS_EXIT_MSR_LOAD_ADDR:
        case VMCS_ENTRY_MSR_LOAD_ADDR:
        case VMCS_EXEC_PTR:
        case VMCS_TSC_OFFSET:
        case VMCS_VAPIC_ADDR:
        case VMCS_LINK_PTR:
        case VMCS_GUEST_DBG_CTL:
        case VMCS_GUEST_PERF_GLOBAL_CTRL:
        case VMCS_CR0_MASK:
        case VMCS_CR4_MASK:
        case VMCS_CR0_READ_SHDW:
        case VMCS_CR4_READ_SHDW:
        case VMCS_CR3_TGT_VAL_0:
        case VMCS_CR3_TGT_VAL_1:
        case VMCS_CR3_TGT_VAL_2:
        case VMCS_CR3_TGT_VAL_3:
        case VMCS_EXIT_QUAL:
        case VMCS_IO_RCX:
        case VMCS_IO_RSI:
        case VMCS_IO_RDI:
        case VMCS_IO_RIP:
        case VMCS_GUEST_LINEAR_ADDR:
        case VMCS_GUEST_CR0:
        case VMCS_GUEST_CR3:
        case VMCS_GUEST_CR4:
        case VMCS_GUEST_ES_BASE:
        case VMCS_GUEST_CS_BASE:
        case VMCS_GUEST_SS_BASE:
        case VMCS_GUEST_DS_BASE:
        case VMCS_GUEST_FS_BASE:
        case VMCS_GUEST_GS_BASE:
        case VMCS_GUEST_LDTR_BASE:
        case VMCS_GUEST_TR_BASE:
        case VMCS_GUEST_GDTR_BASE:
        case VMCS_GUEST_IDTR_BASE:
        case VMCS_GUEST_DR7:
        case VMCS_GUEST_RSP:
        case VMCS_GUEST_RIP:
        case VMCS_GUEST_RFLAGS:
        case VMCS_GUEST_PENDING_DBG_EXCP:
        case VMCS_GUEST_SYSENTER_ESP:
        case VMCS_GUEST_SYSENTER_EIP:
        case VMCS_HOST_CR0:
        case VMCS_HOST_CR3:
        case VMCS_HOST_CR4:
        case VMCS_HOST_FS_BASE:
        case VMCS_HOST_GS_BASE:
        case VMCS_HOST_TR_BASE:
        case VMCS_HOST_GDTR_BASE:
        case VMCS_HOST_IDTR_BASE:
        case VMCS_HOST_SYSENTER_ESP:
        case VMCS_HOST_SYSENTER_EIP:
        case VMCS_HOST_RSP:
        case VMCS_HOST_RIP:
            return sizeof(addr_t);

        default:
	    PrintError("Invalid VMCS field\n");
            return -1;
    }
}












static const char VMCS_GUEST_ES_SELECTOR_STR[] = "GUEST_ES_SELECTOR";
static const char VMCS_GUEST_CS_SELECTOR_STR[] = "GUEST_CS_SELECTOR";
static const char VMCS_GUEST_SS_SELECTOR_STR[] = "GUEST_SS_SELECTOR";
static const char VMCS_GUEST_DS_SELECTOR_STR[] = "GUEST_DS_SELECTOR";
static const char VMCS_GUEST_FS_SELECTOR_STR[] = "GUEST_FS_SELECTOR";
static const char VMCS_GUEST_GS_SELECTOR_STR[] = "GUEST_GS_SELECTOR";
static const char VMCS_GUEST_LDTR_SELECTOR_STR[] = "GUEST_LDTR_SELECTOR";
static const char VMCS_GUEST_TR_SELECTOR_STR[] = "GUEST_TR_SELECTOR";
static const char VMCS_HOST_ES_SELECTOR_STR[] = "HOST_ES_SELECTOR";
static const char VMCS_HOST_CS_SELECTOR_STR[] = "HOST_CS_SELECTOR";
static const char VMCS_HOST_SS_SELECTOR_STR[] = "HOST_SS_SELECTOR";
static const char VMCS_HOST_DS_SELECTOR_STR[] = "HOST_DS_SELECTOR";
static const char VMCS_HOST_FS_SELECTOR_STR[] = "HOST_FS_SELECTOR";
static const char VMCS_HOST_GS_SELECTOR_STR[] = "HOST_GS_SELECTOR";
static const char VMCS_HOST_TR_SELECTOR_STR[] = "HOST_TR_SELECTOR";
static const char VMCS_IO_BITMAP_A_ADDR_STR[] = "IO_BITMAP_A_ADDR";
static const char VMCS_IO_BITMAP_A_ADDR_HIGH_STR[] = "IO_BITMAP_A_ADDR_HIGH";
static const char VMCS_IO_BITMAP_B_ADDR_STR[] = "IO_BITMAP_B_ADDR";
static const char VMCS_IO_BITMAP_B_ADDR_HIGH_STR[] = "IO_BITMAP_B_ADDR_HIGH";
static const char VMCS_MSR_BITMAP_STR[] = "MSR_BITMAPS";
static const char VMCS_MSR_BITMAP_HIGH_STR[] = "MSR_BITMAPS_HIGH";
static const char VMCS_EXIT_MSR_STORE_ADDR_STR[] = "EXIT_MSR_STORE_ADDR";
static const char VMCS_EXIT_MSR_STORE_ADDR_HIGH_STR[] = "EXIT_MSR_STORE_ADDR_HIGH";
static const char VMCS_EXIT_MSR_LOAD_ADDR_STR[] = "EXIT_MSR_LOAD_ADDR";
static const char VMCS_EXIT_MSR_LOAD_ADDR_HIGH_STR[] = "EXIT_MSR_LOAD_ADDR_HIGH";
static const char VMCS_ENTRY_MSR_LOAD_ADDR_STR[] = "ENTRY_MSR_LOAD_ADDR";
static const char VMCS_ENTRY_MSR_LOAD_ADDR_HIGH_STR[] = "ENTRY_MSR_LOAD_ADDR_HIGH";
static const char VMCS_EXEC_PTR_STR[] = "VMCS_EXEC_PTR";
static const char VMCS_EXEC_PTR_HIGH_STR[] = "VMCS_EXEC_PTR_HIGH";
static const char VMCS_TSC_OFFSET_STR[] = "TSC_OFFSET";
static const char VMCS_TSC_OFFSET_HIGH_STR[] = "TSC_OFFSET_HIGH";
static const char VMCS_VAPIC_ADDR_STR[] = "VAPIC_PAGE_ADDR";
static const char VMCS_VAPIC_ADDR_HIGH_STR[] = "VAPIC_PAGE_ADDR_HIGH";
static const char VMCS_LINK_PTR_STR[] = "VMCS_LINK_PTR";
static const char VMCS_LINK_PTR_HIGH_STR[] = "VMCS_LINK_PTR_HIGH";
static const char VMCS_GUEST_DBG_CTL_STR[] = "GUEST_DEBUG_CTL";
static const char VMCS_GUEST_DBG_CTL_HIGH_STR[] = "GUEST_DEBUG_CTL_HIGH";
static const char VMCS_GUEST_PERF_GLOBAL_CTRL_STR[] = "GUEST_PERF_GLOBAL_CTRL";
static const char VMCS_GUEST_PERF_GLOBAL_CTRL_HIGH_STR[] = "GUEST_PERF_GLOBAL_CTRL_HIGH";
static const char VMCS_PIN_CTRLS_STR[] = "PIN_VM_EXEC_CTRLS";
static const char VMCS_PROC_CTRLS_STR[] = "PROC_VM_EXEC_CTRLS";
static const char VMCS_EXCP_BITMAP_STR[] = "EXCEPTION_BITMAP";
static const char VMCS_PG_FAULT_ERR_MASK_STR[] = "PAGE_FAULT_ERROR_MASK";
static const char VMCS_PG_FAULT_ERR_MATCH_STR[] = "PAGE_FAULT_ERROR_MATCH";
static const char VMCS_CR3_TGT_CNT_STR[] = "CR3_TARGET_COUNT";
static const char VMCS_EXIT_CTRLS_STR[] = "VM_EXIT_CTRLS";
static const char VMCS_EXIT_MSR_STORE_CNT_STR[] = "VM_EXIT_MSR_STORE_COUNT";
static const char VMCS_EXIT_MSR_LOAD_CNT_STR[] = "VM_EXIT_MSR_LOAD_COUNT";
static const char VMCS_ENTRY_CTRLS_STR[] = "VM_ENTRY_CTRLS";
static const char VMCS_ENTRY_MSR_LOAD_CNT_STR[] = "VM_ENTRY_MSR_LOAD_COUNT";
static const char VMCS_ENTRY_INT_INFO_STR[] = "VM_ENTRY_INT_INFO_FIELD";
static const char VMCS_ENTRY_EXCP_ERR_STR[] = "VM_ENTRY_EXCEPTION_ERROR";
static const char VMCS_ENTRY_INSTR_LEN_STR[] = "VM_ENTRY_INSTR_LENGTH";
static const char VMCS_TPR_THRESHOLD_STR[] = "TPR_THRESHOLD";
static const char VMCS_INSTR_ERR_STR[] = "VM_INSTR_ERROR";
static const char VMCS_EXIT_REASON_STR[] = "EXIT_REASON";
static const char VMCS_EXIT_INT_INFO_STR[] = "VM_EXIT_INT_INFO";
static const char VMCS_EXIT_INT_ERR_STR[] = "VM_EXIT_INT_ERROR";
static const char VMCS_IDT_VECTOR_INFO_STR[] = "IDT_VECTOR_INFO";
static const char VMCS_IDT_VECTOR_ERR_STR[] = "IDT_VECTOR_ERROR";
static const char VMCS_EXIT_INSTR_LEN_STR[] = "VM_EXIT_INSTR_LENGTH";
static const char VMCS_VMX_INSTR_INFO_STR[] = "VMX_INSTR_INFO";
static const char VMCS_GUEST_ES_LIMIT_STR[] = "GUEST_ES_LIMIT";
static const char VMCS_GUEST_CS_LIMIT_STR[] = "GUEST_CS_LIMIT";
static const char VMCS_GUEST_SS_LIMIT_STR[] = "GUEST_SS_LIMIT";
static const char VMCS_GUEST_DS_LIMIT_STR[] = "GUEST_DS_LIMIT";
static const char VMCS_GUEST_FS_LIMIT_STR[] = "GUEST_FS_LIMIT";
static const char VMCS_GUEST_GS_LIMIT_STR[] = "GUEST_GS_LIMIT";
static const char VMCS_GUEST_LDTR_LIMIT_STR[] = "GUEST_LDTR_LIMIT";
static const char VMCS_GUEST_TR_LIMIT_STR[] = "GUEST_TR_LIMIT";
static const char VMCS_GUEST_GDTR_LIMIT_STR[] = "GUEST_GDTR_LIMIT";
static const char VMCS_GUEST_IDTR_LIMIT_STR[] = "GUEST_IDTR_LIMIT";
static const char VMCS_GUEST_ES_ACCESS_STR[] = "GUEST_ES_ACCESS";
static const char VMCS_GUEST_CS_ACCESS_STR[] = "GUEST_CS_ACCESS";
static const char VMCS_GUEST_SS_ACCESS_STR[] = "GUEST_SS_ACCESS";
static const char VMCS_GUEST_DS_ACCESS_STR[] = "GUEST_DS_ACCESS";
static const char VMCS_GUEST_FS_ACCESS_STR[] = "GUEST_FS_ACCESS";
static const char VMCS_GUEST_GS_ACCESS_STR[] = "GUEST_GS_ACCESS";
static const char VMCS_GUEST_LDTR_ACCESS_STR[] = "GUEST_LDTR_ACCESS";
static const char VMCS_GUEST_TR_ACCESS_STR[] = "GUEST_TR_ACCESS";
static const char VMCS_GUEST_INT_STATE_STR[] = "GUEST_INT_STATE";
static const char VMCS_GUEST_ACTIVITY_STATE_STR[] = "GUEST_ACTIVITY_STATE";
static const char VMCS_GUEST_SMBASE_STR[] = "GUEST_SMBASE";
static const char VMCS_GUEST_SYSENTER_CS_STR[] = "GUEST_SYSENTER_CS";
static const char VMCS_HOST_SYSENTER_CS_STR[] = "HOST_SYSENTER_CS";
static const char VMCS_CR0_MASK_STR[] = "CR0_GUEST_HOST_MASK";
static const char VMCS_CR4_MASK_STR[] = "CR4_GUEST_HOST_MASK";
static const char VMCS_CR0_READ_SHDW_STR[] = "CR0_READ_SHADOW";
static const char VMCS_CR4_READ_SHDW_STR[] = "CR4_READ_SHADOW";
static const char VMCS_CR3_TGT_VAL_0_STR[] = "CR3_TARGET_VALUE_0";
static const char VMCS_CR3_TGT_VAL_1_STR[] = "CR3_TARGET_VALUE_1";
static const char VMCS_CR3_TGT_VAL_2_STR[] = "CR3_TARGET_VALUE_2";
static const char VMCS_CR3_TGT_VAL_3_STR[] = "CR3_TARGET_VALUE_3";
static const char VMCS_EXIT_QUAL_STR[] = "EXIT_QUALIFICATION";
static const char VMCS_IO_RCX_STR[] = "IO_RCX";
static const char VMCS_IO_RSI_STR[] = "IO_RSI";
static const char VMCS_IO_RDI_STR[] = "IO_RDI";
static const char VMCS_IO_RIP_STR[] = "IO_RIP";
static const char VMCS_GUEST_LINEAR_ADDR_STR[] = "GUEST_LINEAR_ADDR";
static const char VMCS_GUEST_CR0_STR[] = "GUEST_CR0";
static const char VMCS_GUEST_CR3_STR[] = "GUEST_CR3";
static const char VMCS_GUEST_CR4_STR[] = "GUEST_CR4";
static const char VMCS_GUEST_ES_BASE_STR[] = "GUEST_ES_BASE";
static const char VMCS_GUEST_CS_BASE_STR[] = "GUEST_CS_BASE";
static const char VMCS_GUEST_SS_BASE_STR[] = "GUEST_SS_BASE";
static const char VMCS_GUEST_DS_BASE_STR[] = "GUEST_DS_BASE";
static const char VMCS_GUEST_FS_BASE_STR[] = "GUEST_FS_BASE";
static const char VMCS_GUEST_GS_BASE_STR[] = "GUEST_GS_BASE";
static const char VMCS_GUEST_LDTR_BASE_STR[] = "GUEST_LDTR_BASE";
static const char VMCS_GUEST_TR_BASE_STR[] = "GUEST_TR_BASE";
static const char VMCS_GUEST_GDTR_BASE_STR[] = "GUEST_GDTR_BASE";
static const char VMCS_GUEST_IDTR_BASE_STR[] = "GUEST_IDTR_BASE";
static const char VMCS_GUEST_DR7_STR[] = "GUEST_DR7";
static const char VMCS_GUEST_RSP_STR[] = "GUEST_RSP";
static const char VMCS_GUEST_RIP_STR[] = "GUEST_RIP";
static const char VMCS_GUEST_RFLAGS_STR[] = "GUEST_RFLAGS";
static const char VMCS_GUEST_PENDING_DBG_EXCP_STR[] = "GUEST_PENDING_DEBUG_EXCS";
static const char VMCS_GUEST_SYSENTER_ESP_STR[] = "GUEST_SYSENTER_ESP";
static const char VMCS_GUEST_SYSENTER_EIP_STR[] = "GUEST_SYSENTER_EIP";
static const char VMCS_HOST_CR0_STR[] = "HOST_CR0";
static const char VMCS_HOST_CR3_STR[] = "HOST_CR3";
static const char VMCS_HOST_CR4_STR[] = "HOST_CR4";
static const char VMCS_HOST_FS_BASE_STR[] = "HOST_FS_BASE";
static const char VMCS_HOST_GS_BASE_STR[] = "HOST_GS_BASE";
static const char VMCS_HOST_TR_BASE_STR[] = "HOST_TR_BASE";
static const char VMCS_HOST_GDTR_BASE_STR[] = "HOST_GDTR_BASE";
static const char VMCS_HOST_IDTR_BASE_STR[] = "HOST_IDTR_BASE";
static const char VMCS_HOST_SYSENTER_ESP_STR[] = "HOST_SYSENTER_ESP";
static const char VMCS_HOST_SYSENTER_EIP_STR[] = "HOST_SYSENTER_EIP";
static const char VMCS_HOST_RSP_STR[] = "HOST_RSP";
static const char VMCS_HOST_RIP_STR[] = "HOST_RIP";



static const char * vmcs_field_to_str(vmcs_field_t field) {   
    switch (field) {
        case VMCS_GUEST_ES_SELECTOR:
            return VMCS_GUEST_ES_SELECTOR_STR;
        case VMCS_GUEST_CS_SELECTOR:
            return VMCS_GUEST_CS_SELECTOR_STR;
        case VMCS_GUEST_SS_SELECTOR:
            return VMCS_GUEST_SS_SELECTOR_STR;
        case VMCS_GUEST_DS_SELECTOR:
            return VMCS_GUEST_DS_SELECTOR_STR;
        case VMCS_GUEST_FS_SELECTOR:
            return VMCS_GUEST_FS_SELECTOR_STR;
        case VMCS_GUEST_GS_SELECTOR:
            return VMCS_GUEST_GS_SELECTOR_STR;
        case VMCS_GUEST_LDTR_SELECTOR:
            return VMCS_GUEST_LDTR_SELECTOR_STR;
        case VMCS_GUEST_TR_SELECTOR:
            return VMCS_GUEST_TR_SELECTOR_STR;
        case VMCS_HOST_ES_SELECTOR:
            return VMCS_HOST_ES_SELECTOR_STR;
        case VMCS_HOST_CS_SELECTOR:
            return VMCS_HOST_CS_SELECTOR_STR;
        case VMCS_HOST_SS_SELECTOR:
            return VMCS_HOST_SS_SELECTOR_STR;
        case VMCS_HOST_DS_SELECTOR:
            return VMCS_HOST_DS_SELECTOR_STR;
        case VMCS_HOST_FS_SELECTOR:
            return VMCS_HOST_FS_SELECTOR_STR;
        case VMCS_HOST_GS_SELECTOR:
            return VMCS_HOST_GS_SELECTOR_STR;
        case VMCS_HOST_TR_SELECTOR:
            return VMCS_HOST_TR_SELECTOR_STR;
        case VMCS_IO_BITMAP_A_ADDR:
            return VMCS_IO_BITMAP_A_ADDR_STR;
        case VMCS_IO_BITMAP_A_ADDR_HIGH:
            return VMCS_IO_BITMAP_A_ADDR_HIGH_STR;
        case VMCS_IO_BITMAP_B_ADDR:
            return VMCS_IO_BITMAP_B_ADDR_STR;
        case VMCS_IO_BITMAP_B_ADDR_HIGH:
            return VMCS_IO_BITMAP_B_ADDR_HIGH_STR;
        case VMCS_MSR_BITMAP:
            return VMCS_MSR_BITMAP_STR;
        case VMCS_MSR_BITMAP_HIGH:
            return VMCS_MSR_BITMAP_HIGH_STR;
        case VMCS_EXIT_MSR_STORE_ADDR:
            return VMCS_EXIT_MSR_STORE_ADDR_STR;
        case VMCS_EXIT_MSR_STORE_ADDR_HIGH:
            return VMCS_EXIT_MSR_STORE_ADDR_HIGH_STR;
        case VMCS_EXIT_MSR_LOAD_ADDR:
            return VMCS_EXIT_MSR_LOAD_ADDR_STR;
        case VMCS_EXIT_MSR_LOAD_ADDR_HIGH:
            return VMCS_EXIT_MSR_LOAD_ADDR_HIGH_STR;
        case VMCS_ENTRY_MSR_LOAD_ADDR:
            return VMCS_ENTRY_MSR_LOAD_ADDR_STR;
        case VMCS_ENTRY_MSR_LOAD_ADDR_HIGH:
            return VMCS_ENTRY_MSR_LOAD_ADDR_HIGH_STR;
        case VMCS_EXEC_PTR:
            return VMCS_EXEC_PTR_STR;
        case VMCS_EXEC_PTR_HIGH:
            return VMCS_EXEC_PTR_HIGH_STR;
        case VMCS_TSC_OFFSET:
            return VMCS_TSC_OFFSET_STR;
        case VMCS_TSC_OFFSET_HIGH:
            return VMCS_TSC_OFFSET_HIGH_STR;
        case VMCS_VAPIC_ADDR:
            return VMCS_VAPIC_ADDR_STR;
        case VMCS_VAPIC_ADDR_HIGH:
            return VMCS_VAPIC_ADDR_HIGH_STR;
        case VMCS_LINK_PTR:
            return VMCS_LINK_PTR_STR;
        case VMCS_LINK_PTR_HIGH:
            return VMCS_LINK_PTR_HIGH_STR;
        case VMCS_GUEST_DBG_CTL:
            return VMCS_GUEST_DBG_CTL_STR;
        case VMCS_GUEST_DBG_CTL_HIGH:
            return VMCS_GUEST_DBG_CTL_HIGH_STR;
        case VMCS_GUEST_PERF_GLOBAL_CTRL:
            return VMCS_GUEST_PERF_GLOBAL_CTRL_STR;
        case VMCS_GUEST_PERF_GLOBAL_CTRL_HIGH:
            return VMCS_GUEST_PERF_GLOBAL_CTRL_HIGH_STR;
        case VMCS_PIN_CTRLS:
            return VMCS_PIN_CTRLS_STR;
        case VMCS_PROC_CTRLS:
            return VMCS_PROC_CTRLS_STR;
        case VMCS_EXCP_BITMAP:
            return VMCS_EXCP_BITMAP_STR;
        case VMCS_PG_FAULT_ERR_MASK:
            return VMCS_PG_FAULT_ERR_MASK_STR;
        case VMCS_PG_FAULT_ERR_MATCH:
            return VMCS_PG_FAULT_ERR_MATCH_STR;
        case VMCS_CR3_TGT_CNT:
            return VMCS_CR3_TGT_CNT_STR;
        case VMCS_EXIT_CTRLS:
            return VMCS_EXIT_CTRLS_STR;
        case VMCS_EXIT_MSR_STORE_CNT:
            return VMCS_EXIT_MSR_STORE_CNT_STR;
        case VMCS_EXIT_MSR_LOAD_CNT:
            return VMCS_EXIT_MSR_LOAD_CNT_STR;
        case VMCS_ENTRY_CTRLS:
            return VMCS_ENTRY_CTRLS_STR;
        case VMCS_ENTRY_MSR_LOAD_CNT:
            return VMCS_ENTRY_MSR_LOAD_CNT_STR;
        case VMCS_ENTRY_INT_INFO:
            return VMCS_ENTRY_INT_INFO_STR;
        case VMCS_ENTRY_EXCP_ERR:
            return VMCS_ENTRY_EXCP_ERR_STR;
        case VMCS_ENTRY_INSTR_LEN:
            return VMCS_ENTRY_INSTR_LEN_STR;
        case VMCS_TPR_THRESHOLD:
            return VMCS_TPR_THRESHOLD_STR;
        case VMCS_INSTR_ERR:
            return VMCS_INSTR_ERR_STR;
        case VMCS_EXIT_REASON:
            return VMCS_EXIT_REASON_STR;
        case VMCS_EXIT_INT_INFO:
            return VMCS_EXIT_INT_INFO_STR;
        case VMCS_EXIT_INT_ERR:
            return VMCS_EXIT_INT_ERR_STR;
        case VMCS_IDT_VECTOR_INFO:
            return VMCS_IDT_VECTOR_INFO_STR;
        case VMCS_IDT_VECTOR_ERR:
            return VMCS_IDT_VECTOR_ERR_STR;
        case VMCS_EXIT_INSTR_LEN:
            return VMCS_EXIT_INSTR_LEN_STR;
        case VMCS_VMX_INSTR_INFO:
            return VMCS_VMX_INSTR_INFO_STR;
        case VMCS_GUEST_ES_LIMIT:
            return VMCS_GUEST_ES_LIMIT_STR;
        case VMCS_GUEST_CS_LIMIT:
            return VMCS_GUEST_CS_LIMIT_STR;
        case VMCS_GUEST_SS_LIMIT:
            return VMCS_GUEST_SS_LIMIT_STR;
        case VMCS_GUEST_DS_LIMIT:
            return VMCS_GUEST_DS_LIMIT_STR;
        case VMCS_GUEST_FS_LIMIT:
            return VMCS_GUEST_FS_LIMIT_STR;
        case VMCS_GUEST_GS_LIMIT:
            return VMCS_GUEST_GS_LIMIT_STR;
        case VMCS_GUEST_LDTR_LIMIT:
            return VMCS_GUEST_LDTR_LIMIT_STR;
        case VMCS_GUEST_TR_LIMIT:
            return VMCS_GUEST_TR_LIMIT_STR;
        case VMCS_GUEST_GDTR_LIMIT:
            return VMCS_GUEST_GDTR_LIMIT_STR;
        case VMCS_GUEST_IDTR_LIMIT:
            return VMCS_GUEST_IDTR_LIMIT_STR;
        case VMCS_GUEST_ES_ACCESS:
            return VMCS_GUEST_ES_ACCESS_STR;
        case VMCS_GUEST_CS_ACCESS:
            return VMCS_GUEST_CS_ACCESS_STR;
        case VMCS_GUEST_SS_ACCESS:
            return VMCS_GUEST_SS_ACCESS_STR;
        case VMCS_GUEST_DS_ACCESS:
            return VMCS_GUEST_DS_ACCESS_STR;
        case VMCS_GUEST_FS_ACCESS:
            return VMCS_GUEST_FS_ACCESS_STR;
        case VMCS_GUEST_GS_ACCESS:
            return VMCS_GUEST_GS_ACCESS_STR;
        case VMCS_GUEST_LDTR_ACCESS:
            return VMCS_GUEST_LDTR_ACCESS_STR;
        case VMCS_GUEST_TR_ACCESS:
            return VMCS_GUEST_TR_ACCESS_STR;
        case VMCS_GUEST_INT_STATE:
            return VMCS_GUEST_INT_STATE_STR;
        case VMCS_GUEST_ACTIVITY_STATE:
            return VMCS_GUEST_ACTIVITY_STATE_STR;
        case VMCS_GUEST_SMBASE:
            return VMCS_GUEST_SMBASE_STR;
        case VMCS_GUEST_SYSENTER_CS:
            return VMCS_GUEST_SYSENTER_CS_STR;
        case VMCS_HOST_SYSENTER_CS:
            return VMCS_HOST_SYSENTER_CS_STR;
        case VMCS_CR0_MASK:
            return VMCS_CR0_MASK_STR;
        case VMCS_CR4_MASK:
            return VMCS_CR4_MASK_STR;
        case VMCS_CR0_READ_SHDW:
            return VMCS_CR0_READ_SHDW_STR;
        case VMCS_CR4_READ_SHDW:
            return VMCS_CR4_READ_SHDW_STR;
        case VMCS_CR3_TGT_VAL_0:
            return VMCS_CR3_TGT_VAL_0_STR;
        case VMCS_CR3_TGT_VAL_1:
            return VMCS_CR3_TGT_VAL_1_STR;
        case VMCS_CR3_TGT_VAL_2:
            return VMCS_CR3_TGT_VAL_2_STR;
        case VMCS_CR3_TGT_VAL_3:
            return VMCS_CR3_TGT_VAL_3_STR;
        case VMCS_EXIT_QUAL:
            return VMCS_EXIT_QUAL_STR;
        case VMCS_IO_RCX:
            return VMCS_IO_RCX_STR;
        case VMCS_IO_RSI:
            return VMCS_IO_RSI_STR;
        case VMCS_IO_RDI:
            return VMCS_IO_RDI_STR;
        case VMCS_IO_RIP:
            return VMCS_IO_RIP_STR;
        case VMCS_GUEST_LINEAR_ADDR:
            return VMCS_GUEST_LINEAR_ADDR_STR;
        case VMCS_GUEST_CR0:
            return VMCS_GUEST_CR0_STR;
        case VMCS_GUEST_CR3:
            return VMCS_GUEST_CR3_STR;
        case VMCS_GUEST_CR4:
            return VMCS_GUEST_CR4_STR;
        case VMCS_GUEST_ES_BASE:
            return VMCS_GUEST_ES_BASE_STR;
        case VMCS_GUEST_CS_BASE:
            return VMCS_GUEST_CS_BASE_STR;
        case VMCS_GUEST_SS_BASE:
            return VMCS_GUEST_SS_BASE_STR;
        case VMCS_GUEST_DS_BASE:
            return VMCS_GUEST_DS_BASE_STR;
        case VMCS_GUEST_FS_BASE:
            return VMCS_GUEST_FS_BASE_STR;
        case VMCS_GUEST_GS_BASE:
            return VMCS_GUEST_GS_BASE_STR;
        case VMCS_GUEST_LDTR_BASE:
            return VMCS_GUEST_LDTR_BASE_STR;
        case VMCS_GUEST_TR_BASE:
            return VMCS_GUEST_TR_BASE_STR;
        case VMCS_GUEST_GDTR_BASE:
            return VMCS_GUEST_GDTR_BASE_STR;
        case VMCS_GUEST_IDTR_BASE:
            return VMCS_GUEST_IDTR_BASE_STR;
        case VMCS_GUEST_DR7:
            return VMCS_GUEST_DR7_STR;
        case VMCS_GUEST_RSP:
            return VMCS_GUEST_RSP_STR;
        case VMCS_GUEST_RIP:
            return VMCS_GUEST_RIP_STR;
        case VMCS_GUEST_RFLAGS:
            return VMCS_GUEST_RFLAGS_STR;
        case VMCS_GUEST_PENDING_DBG_EXCP:
            return VMCS_GUEST_PENDING_DBG_EXCP_STR;
        case VMCS_GUEST_SYSENTER_ESP:
            return VMCS_GUEST_SYSENTER_ESP_STR;
        case VMCS_GUEST_SYSENTER_EIP:
            return VMCS_GUEST_SYSENTER_EIP_STR;
        case VMCS_HOST_CR0:
            return VMCS_HOST_CR0_STR;
        case VMCS_HOST_CR3:
            return VMCS_HOST_CR3_STR;
        case VMCS_HOST_CR4:
            return VMCS_HOST_CR4_STR;
        case VMCS_HOST_FS_BASE:
            return VMCS_HOST_FS_BASE_STR;
        case VMCS_HOST_GS_BASE:
            return VMCS_HOST_GS_BASE_STR;
        case VMCS_HOST_TR_BASE:
            return VMCS_HOST_TR_BASE_STR;
        case VMCS_HOST_GDTR_BASE:
            return VMCS_HOST_GDTR_BASE_STR;
        case VMCS_HOST_IDTR_BASE:
            return VMCS_HOST_IDTR_BASE_STR;
        case VMCS_HOST_SYSENTER_ESP:
            return VMCS_HOST_SYSENTER_ESP_STR;
        case VMCS_HOST_SYSENTER_EIP:
            return VMCS_HOST_SYSENTER_EIP_STR;
        case VMCS_HOST_RSP:
            return VMCS_HOST_RSP_STR;
        case VMCS_HOST_RIP:
            return VMCS_HOST_RIP_STR;
        default:
            return NULL;
    }
}



