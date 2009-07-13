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



//extern char * exception_names;
//
// Ignores "HIGH" addresses - 32 bit only for now
//


static inline void print_vmcs_field(uint_t vmcs_index) {
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





void print_debug_vmcs_load_guest() {
    const int wordsize = sizeof(addr_t);
    uint64_t temp;
    vmcs_segment tmp_seg;

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
}

void PrintTrace_VMCSHostStateArea(struct VMCSHostStateArea * hostState) {
  PrintTrace("\n==> Host State Area\n");
  PrintTrace("HOST_CR0: %x\n", (uint_t)hostState->cr0);
  PrintTrace("HOST_CR3: %x\n", (uint_t)hostState->cr3);
  PrintTrace("HOST_CR4: %x\n", (uint_t)hostState->cr4);
  PrintTrace("HOST_RSP: %x\n", (uint_t)hostState->rsp);
  PrintTrace("HOST_RIP: %x\n", (uint_t)hostState->rip);
  PrintTrace("VMCS_HOST_CS_SELECTOR: %x\n", (uint_t)hostState->csSelector);
  PrintTrace("VMCS_HOST_SS_SELECTOR: %x\n", (uint_t)hostState->ssSelector);
  PrintTrace("VMCS_HOST_DS_SELECTOR: %x\n", (uint_t)hostState->dsSelector);
  PrintTrace("VMCS_HOST_ES_SELECTOR: %x\n", (uint_t)hostState->esSelector);
  PrintTrace("VMCS_HOST_FS_SELECTOR: %x\n", (uint_t)hostState->fsSelector);
  PrintTrace("VMCS_HOST_GS_SELECTOR: %x\n", (uint_t)hostState->gsSelector);
  PrintTrace("VMCS_HOST_TR_SELECTOR: %x\n", (uint_t)hostState->trSelector);
  PrintTrace("HOST_FS_BASE: %x\n", (uint_t)hostState->fsBaseAddr);
  PrintTrace("HOST_GS_BASE: %x\n", (uint_t)hostState->gsBaseAddr);
  PrintTrace("HOST_TR_BASE: %x\n", (uint_t)hostState->trBaseAddr);
  PrintTrace("HOST_GDTR_BASE: %x\n", (uint_t)hostState->gdtrBaseAddr);
  PrintTrace("HOST_IDTR_BASE: %x\n", (uint_t)hostState->idtrBaseAddr);
  PrintTrace("HOST_IA32_SYSENTER_CS: %x\n", (uint_t)hostState->sysenter_cs);
  PrintTrace("HOST_IA32_SYSENTER_ESP: %x\n", (uint_t)hostState->sysenter_esp);
  PrintTrace("HOST_IA32_SYSENTER_EIP: %x\n", (uint_t)hostState->sysenter_eip);
}

void PrintTrace_VMCSExecCtrlFields(struct VMCSExecCtrlFields * execCtrls) {
  PrintTrace("\n==> VM-Execution Controls:\n");
  PrintTrace("PIN_VM_EXEC_CTRLS: %x\n", (uint_t) execCtrls->pinCtrls);
  PrintTrace("PROC_VM_EXEC_CTRLS: %x\n", (uint_t) execCtrls->procCtrls);
  PrintTrace("EXCEPTION_BITMAP: %x\n", (uint_t) execCtrls->execBitmap);
  PrintTrace("PAGE_FAULT_ERROR_MASK: %x\n", (uint_t) execCtrls->pageFaultErrorMask);
  PrintTrace("PAGE_FAULT_ERROR_MATCH: %x\n", (uint_t) execCtrls->pageFaultErrorMatch);
  PrintTrace("IO_BITMAP_A_ADDR: %x\n", (uint_t) execCtrls->ioBitmapA);
  //  PrintTrace("IO_BITMAP_A_ADDR_HIGH: %x\n", (uint_t) execCtrls->);
  PrintTrace("IO_BITMAP_B_ADDR: %x\n", (uint_t) execCtrls->ioBitmapB);
  // PrintTrace("IO_BITMAP_B_ADDR_HIGH: %x\n", (uint_t) execCtrls->);
  PrintTrace("TSC_OFFSET: %x\n", (uint_t) execCtrls->tscOffset & 0xffffffff);
  PrintTrace("TSC_OFFSET_HIGH: %x\n", (uint_t) (execCtrls->tscOffset >> 32) & 0xffffffff);
  PrintTrace("CR0_GUEST_HOST_MASK: %x\n", (uint_t) execCtrls->cr0GuestHostMask);
  PrintTrace("CR0_READ_SHADOW: %x\n", (uint_t) execCtrls->cr0ReadShadow);
  PrintTrace("CR4_GUEST_HOST_MASK: %x\n", (uint_t) execCtrls->cr4GuestHostMask);
  PrintTrace("CR4_READ_SHADOW: %x\n", (uint_t) execCtrls->cr4ReadShadow);
  PrintTrace("CR3_TARGET_COUNT: %x\n", (uint_t) execCtrls->cr3TargetCount);
  PrintTrace("CR3_TARGET_VALUE_0: %x\n", (uint_t) execCtrls->cr3TargetValue0);
  PrintTrace("CR3_TARGET_VALUE_1: %x\n", (uint_t) execCtrls->cr3TargetValue1);
  PrintTrace("CR3_TARGET_VALUE_2: %x\n", (uint_t) execCtrls->cr3TargetValue2);
  PrintTrace("CR3_TARGET_VALUE_3: %x\n", (uint_t) execCtrls->cr3TargetValue3);
  PrintTrace("VIRT_APIC_PAGE_ADDR: %x\n", (uint_t) execCtrls->virtApicPageAddr & 0xffffffff);
  PrintTrace("VIRT_APIC_PAGE_ADDR_HIGH: %x\n", (uint_t) (execCtrls->virtApicPageAddr >> 32) & 0xffffffff);
  PrintTrace("TPR_THRESHOLD: %x\n", (uint_t) execCtrls->tprThreshold);
  PrintTrace("MSR_BITMAPS: %x\n", (uint_t) execCtrls->MSRBitmapsBaseAddr & 0xffffffff);
  PrintTrace("MSR_BITMAPS_HIGH: %x\n", (uint_t) (execCtrls->MSRBitmapsBaseAddr >> 32) & 0xffffffff);
  PrintTrace("VMCS_EXEC_PTR: %x\n", (uint_t) execCtrls->vmcsExecPtr & 0xffffffff);
  PrintTrace("VMCS_EXEC_PTR_HIGH: %x\n", (uint_t) (execCtrls->vmcsExecPtr >> 32) & 0xffffffff);
}

void PrintTrace_VMCSExitCtrlFields(struct VMCSExitCtrlFields * exitCtrls) {
  PrintTrace("\n==> VM Exit Controls\n");
  PrintTrace("VM_EXIT_CTRLS: %x\n", (uint_t) exitCtrls->exitCtrls);
  PrintTrace("VM_EXIT_MSR_STORE_COUNT: %x\n", (uint_t) exitCtrls->msrStoreCount);
  PrintTrace("VM_EXIT_MSR_STORE_ADDR: %x\n", (uint_t) exitCtrls->msrStoreAddr & 0xffffffff);
  PrintTrace("VM_EXIT_MSR_STORE_ADDR_HIGH: %x\n", (uint_t) (exitCtrls->msrStoreAddr >> 32) & 0xffffffff);
  PrintTrace("VM_EXIT_MSR_LOAD_COUNT: %x\n", (uint_t) exitCtrls->msrLoadCount);
  PrintTrace("VM_EXIT_MSR_LOAD_ADDR: %x\n", (uint_t) exitCtrls->msrLoadAddr & 0xffffffff);
  PrintTrace("VM_EXIT_MSR_LOAD_ADDR_HIGH: %x\n", (uint_t) (exitCtrls->msrLoadAddr >> 32) & 0xffffffff);
}

void PrintTrace_VMCSEntryCtrlFields(struct VMCSEntryCtrlFields * entryCtrls) {
  PrintTrace("\n==> VM Entry Controls\n");
  PrintTrace("VM_ENTRY_CTRLS: %x\n", (uint_t) entryCtrls->entryCtrls);
  PrintTrace("VM_ENTRY_MSR_LOAD_COUNT: %x\n", (uint_t) entryCtrls->msrLoadCount);
  PrintTrace("VM_ENTRY_MSR_LOAD_ADDR: %x\n", (uint_t) entryCtrls->msrLoadAddr & 0xffffffff);
  PrintTrace("VM_ENTRY_MSR_LOAD_ADDR_HIGH: %x\n", (uint_t) (entryCtrls->msrLoadAddr >> 32) & 0xffffffff);
  PrintTrace("VM_ENTRY_INT_INFO_FIELD: %x\n", (uint_t) entryCtrls->intInfo);
  PrintTrace("VM_ENTRY_EXCEPTION_ERROR: %x\n", (uint_t) entryCtrls->exceptionErrorCode);
  PrintTrace("VM_ENTRY_INSTR_LENGTH: %x\n", (uint_t) entryCtrls->instrLength);
}

void PrintTrace_VMCSExitInfoFields(struct VMCSExitInfoFields * exitInfo) {
  PrintTrace("\n==> VM Exit Info\n");
  PrintTrace("EXIT_REASON: %x\n", (uint_t) exitInfo->reason);
  PrintTrace("EXIT_QUALIFICATION: %x\n", (uint_t) exitInfo->qualification);
  PrintTrace("VM_EXIT_INT_INFO: %x\n", (uint_t) exitInfo->intInfo);
  PrintTrace("VM_EXIT_INT_ERROR: %x\n", (uint_t) exitInfo->intErrorCode);
  PrintTrace("IDT_VECTOR_INFO: %x\n", (uint_t) exitInfo->idtVectorInfo);
  PrintTrace("IDT_VECTOR_ERROR: %x\n", (uint_t) exitInfo->idtVectorErrorCode);
  PrintTrace("VM_EXIT_INSTR_LENGTH: %x\n", (uint_t) exitInfo->instrLength);
  PrintTrace("GUEST_LINEAR_ADDR: %x\n", (uint_t) exitInfo->guestLinearAddr);
  PrintTrace("VMX_INSTR_INFO: %x\n", (uint_t) exitInfo->instrInfo);
  PrintTrace("IO_RCX: %x\n", (uint_t) exitInfo->ioRCX);
  PrintTrace("IO_RSI: %x\n", (uint_t) exitInfo->ioRSI);
  PrintTrace("IO_RDI: %x\n", (uint_t) exitInfo->ioRDI);
  PrintTrace("IO_RIP: %x\n", (uint_t) exitInfo->ioRIP);
  PrintTrace("VM_INSTR_ERROR: %x\n", (uint_t) exitInfo->instrErrorField);
}


void PrintTrace_VMCSData(struct VMCSData * vmcs) {
  PrintTrace("VMCSData Structure\n");

  PrintTrace_VMCSGuestStateArea(&(vmcs->guestStateArea));
  PrintTrace_VMCSHostStateArea(&(vmcs->hostStateArea));
  PrintTrace_VMCSExecCtrlFields(&(vmcs->execCtrlFields));
  PrintTrace_VMCSExitCtrlFields(&(vmcs->exitCtrlFields));
  PrintTrace_VMCSEntryCtrlFields(&(vmcs->entryCtrlFields));
  PrintTrace_VMCSExitInfoFields(&(vmcs->exitInfoFields));
  PrintTrace("\n");
}
