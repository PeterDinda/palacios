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


#define CHK_VMCS_READ(tag, val) {if (VMCS_READ(tag, val) != 0) return -1;}
#define CHK_VMCS_WRITE(tag, val) {if (VMCS_WRITE(tag, val) != 0) return -1;}



int CopyOutVMCSGuestStateArea(struct VMCSGuestStateArea *p) {
  CHK_VMCS_READ(GUEST_CR0, &(p->cr0));
  CHK_VMCS_READ(GUEST_CR3, &(p->cr3));
  CHK_VMCS_READ(GUEST_CR4, &(p->cr4));
  CHK_VMCS_READ(GUEST_DR7, &(p->dr7));
  CHK_VMCS_READ(GUEST_RSP, &(p->rsp));
  CHK_VMCS_READ(GUEST_RIP, &(p->rip));
  CHK_VMCS_READ(GUEST_RFLAGS, &(p->rflags));
  CHK_VMCS_READ(VMCS_GUEST_CS_SELECTOR, &(p->cs.selector));
  CHK_VMCS_READ(VMCS_GUEST_SS_SELECTOR, &(p->ss.selector));
  CHK_VMCS_READ(VMCS_GUEST_DS_SELECTOR, &(p->ds.selector));
  CHK_VMCS_READ(VMCS_GUEST_ES_SELECTOR, &(p->es.selector));
  CHK_VMCS_READ(VMCS_GUEST_FS_SELECTOR, &(p->fs.selector));
  CHK_VMCS_READ(VMCS_GUEST_GS_SELECTOR, &(p->gs.selector));
  CHK_VMCS_READ(VMCS_GUEST_LDTR_SELECTOR, &(p->ldtr.selector));
  CHK_VMCS_READ(VMCS_GUEST_TR_SELECTOR, &(p->tr.selector));
  CHK_VMCS_READ(GUEST_CS_BASE, &(p->cs.baseAddr));
  CHK_VMCS_READ(GUEST_SS_BASE, &(p->ss.baseAddr));
  CHK_VMCS_READ(GUEST_DS_BASE, &(p->ds.baseAddr));
  CHK_VMCS_READ(GUEST_ES_BASE, &(p->es.baseAddr));
  CHK_VMCS_READ(GUEST_FS_BASE, &(p->fs.baseAddr));
  CHK_VMCS_READ(GUEST_GS_BASE, &(p->gs.baseAddr));
  CHK_VMCS_READ(GUEST_LDTR_BASE, &(p->ldtr.baseAddr));
  CHK_VMCS_READ(GUEST_TR_BASE, &(p->tr.baseAddr));
  CHK_VMCS_READ(GUEST_CS_LIMIT, &(p->cs.limit));
  CHK_VMCS_READ(GUEST_SS_LIMIT, &(p->ss.limit));
  CHK_VMCS_READ(GUEST_DS_LIMIT, &(p->ds.limit));
  CHK_VMCS_READ(GUEST_ES_LIMIT, &(p->es.limit));
  CHK_VMCS_READ(GUEST_FS_LIMIT, &(p->fs.limit));
  CHK_VMCS_READ(GUEST_GS_LIMIT, &(p->gs.limit));
  CHK_VMCS_READ(GUEST_LDTR_LIMIT, &(p->ldtr.limit));
  CHK_VMCS_READ(GUEST_TR_LIMIT, &(p->tr.limit));
  CHK_VMCS_READ(GUEST_CS_ACCESS, &(p->cs.access));
  CHK_VMCS_READ(GUEST_SS_ACCESS, &(p->ss.access));
  CHK_VMCS_READ(GUEST_DS_ACCESS, &(p->ds.access));
  CHK_VMCS_READ(GUEST_ES_ACCESS, &(p->es.access));
  CHK_VMCS_READ(GUEST_FS_ACCESS, &(p->fs.access));
  CHK_VMCS_READ(GUEST_GS_ACCESS, &(p->gs.access));
  CHK_VMCS_READ(GUEST_LDTR_ACCESS, &(p->ldtr.access));
  CHK_VMCS_READ(GUEST_TR_ACCESS, &(p->tr.access));
  CHK_VMCS_READ(GUEST_GDTR_BASE, &(p->gdtr.baseAddr));
  CHK_VMCS_READ(GUEST_IDTR_BASE, &(p->idtr.baseAddr));
  CHK_VMCS_READ(GUEST_GDTR_LIMIT, &(p->gdtr.limit));
  CHK_VMCS_READ(GUEST_IDTR_LIMIT, &(p->idtr.limit));
  CHK_VMCS_READ(GUEST_IA32_DEBUGCTL, &(p->dbg_ctrl));
  CHK_VMCS_READ(GUEST_IA32_DEBUGCTL_HIGH, ((char *)&(p->dbg_ctrl)) + 4);
  CHK_VMCS_READ(GUEST_IA32_SYSENTER_CS, &(p->sysenter_cs));
  CHK_VMCS_READ(GUEST_IA32_SYSENTER_ESP, &(p->sysenter_esp));
  CHK_VMCS_READ(GUEST_IA32_SYSENTER_EIP, &(p->sysenter_eip));
  CHK_VMCS_READ(GUEST_SMBASE, &(p->smbase));

  CHK_VMCS_READ(GUEST_ACTIVITY_STATE, &(p->activity));
  CHK_VMCS_READ(GUEST_INT_STATE, &(p->interrupt_state));
  CHK_VMCS_READ(GUEST_PENDING_DEBUG_EXCS, &(p->pending_dbg_exceptions));
  CHK_VMCS_READ(VMCS_LINK_PTR, &(p->vmcs_link));
  CHK_VMCS_READ(VMCS_LINK_PTR_HIGH, ((char *)&(p->vmcs_link)) + 4);
  return 0;
}


int CopyInVMCSGuestStateArea(struct VMCSGuestStateArea *p) {
  CHK_VMCS_WRITE(GUEST_CR0, &(p->cr0));
  CHK_VMCS_WRITE(GUEST_CR3, &(p->cr3));
  CHK_VMCS_WRITE(GUEST_CR4, &(p->cr4));
  CHK_VMCS_WRITE(GUEST_DR7, &(p->dr7));
  CHK_VMCS_WRITE(GUEST_RSP, &(p->rsp));
  CHK_VMCS_WRITE(GUEST_RIP, &(p->rip));
  CHK_VMCS_WRITE(GUEST_RFLAGS, &(p->rflags));
  CHK_VMCS_WRITE(VMCS_GUEST_CS_SELECTOR, &(p->cs.selector));
  CHK_VMCS_WRITE(VMCS_GUEST_SS_SELECTOR, &(p->ss.selector));
  CHK_VMCS_WRITE(VMCS_GUEST_DS_SELECTOR, &(p->ds.selector));
  CHK_VMCS_WRITE(VMCS_GUEST_ES_SELECTOR, &(p->es.selector));
  CHK_VMCS_WRITE(VMCS_GUEST_FS_SELECTOR, &(p->fs.selector));
  CHK_VMCS_WRITE(VMCS_GUEST_GS_SELECTOR, &(p->gs.selector));
  CHK_VMCS_WRITE(VMCS_GUEST_LDTR_SELECTOR, &(p->ldtr.selector));
  CHK_VMCS_WRITE(VMCS_GUEST_TR_SELECTOR, &(p->tr.selector));
  CHK_VMCS_WRITE(GUEST_CS_BASE, &(p->cs.baseAddr));
  CHK_VMCS_WRITE(GUEST_SS_BASE, &(p->ss.baseAddr));
  CHK_VMCS_WRITE(GUEST_DS_BASE, &(p->ds.baseAddr));
  CHK_VMCS_WRITE(GUEST_ES_BASE, &(p->es.baseAddr));
  CHK_VMCS_WRITE(GUEST_FS_BASE, &(p->fs.baseAddr));
  CHK_VMCS_WRITE(GUEST_GS_BASE, &(p->gs.baseAddr));
  CHK_VMCS_WRITE(GUEST_LDTR_BASE, &(p->ldtr.baseAddr));
  CHK_VMCS_WRITE(GUEST_TR_BASE, &(p->tr.baseAddr));
  CHK_VMCS_WRITE(GUEST_CS_LIMIT, &(p->cs.limit));
  CHK_VMCS_WRITE(GUEST_SS_LIMIT, &(p->ss.limit));
  CHK_VMCS_WRITE(GUEST_DS_LIMIT, &(p->ds.limit));
  CHK_VMCS_WRITE(GUEST_ES_LIMIT, &(p->es.limit));
  CHK_VMCS_WRITE(GUEST_FS_LIMIT, &(p->fs.limit));
  CHK_VMCS_WRITE(GUEST_GS_LIMIT, &(p->gs.limit));
  CHK_VMCS_WRITE(GUEST_LDTR_LIMIT, &(p->ldtr.limit));
  CHK_VMCS_WRITE(GUEST_TR_LIMIT, &(p->tr.limit));
  CHK_VMCS_WRITE(GUEST_CS_ACCESS, &(p->cs.access));
  CHK_VMCS_WRITE(GUEST_SS_ACCESS, &(p->ss.access));
  CHK_VMCS_WRITE(GUEST_DS_ACCESS, &(p->ds.access));
  CHK_VMCS_WRITE(GUEST_ES_ACCESS, &(p->es.access));
  CHK_VMCS_WRITE(GUEST_FS_ACCESS, &(p->fs.access));
  CHK_VMCS_WRITE(GUEST_GS_ACCESS, &(p->gs.access));
  CHK_VMCS_WRITE(GUEST_LDTR_ACCESS, &(p->ldtr.access));
  CHK_VMCS_WRITE(GUEST_TR_ACCESS, &(p->tr.access));
  CHK_VMCS_WRITE(GUEST_GDTR_BASE, &(p->gdtr.baseAddr));
  CHK_VMCS_WRITE(GUEST_IDTR_BASE, &(p->idtr.baseAddr));
  CHK_VMCS_WRITE(GUEST_GDTR_LIMIT, &(p->gdtr.limit));
  CHK_VMCS_WRITE(GUEST_IDTR_LIMIT, &(p->idtr.limit));
  CHK_VMCS_WRITE(GUEST_IA32_DEBUGCTL, &(p->dbg_ctrl));
  CHK_VMCS_WRITE(GUEST_IA32_DEBUGCTL_HIGH, ((char *)&(p->dbg_ctrl)) + 4);
  CHK_VMCS_WRITE(GUEST_IA32_SYSENTER_CS, &(p->sysenter_cs));
  CHK_VMCS_WRITE(GUEST_IA32_SYSENTER_ESP, &(p->sysenter_esp));
  CHK_VMCS_WRITE(GUEST_IA32_SYSENTER_EIP, &(p->sysenter_eip));
  CHK_VMCS_WRITE(GUEST_SMBASE, &(p->smbase));

  CHK_VMCS_WRITE(GUEST_ACTIVITY_STATE, &(p->activity));
  CHK_VMCS_WRITE(GUEST_INT_STATE, &(p->interrupt_state));
  CHK_VMCS_WRITE(GUEST_PENDING_DEBUG_EXCS, &(p->pending_dbg_exceptions));
  CHK_VMCS_WRITE(VMCS_LINK_PTR, &(p->vmcs_link));
  CHK_VMCS_WRITE(VMCS_LINK_PTR_HIGH, ((char *)&(p->vmcs_link)) + 4);
  return 0;
}



int CopyOutVMCSHostStateArea(struct VMCSHostStateArea *p) {
  CHK_VMCS_READ(HOST_CR0, &(p->cr0));
  CHK_VMCS_READ(HOST_CR3, &(p->cr3));
  CHK_VMCS_READ(HOST_CR4, &(p->cr4));
  CHK_VMCS_READ(HOST_RSP, &(p->rsp));
  CHK_VMCS_READ(HOST_RIP, &(p->rip));
  CHK_VMCS_READ(VMCS_HOST_CS_SELECTOR, &(p->csSelector));
  CHK_VMCS_READ(VMCS_HOST_SS_SELECTOR, &(p->ssSelector));
  CHK_VMCS_READ(VMCS_HOST_DS_SELECTOR, &(p->dsSelector));
  CHK_VMCS_READ(VMCS_HOST_ES_SELECTOR, &(p->esSelector));
  CHK_VMCS_READ(VMCS_HOST_FS_SELECTOR, &(p->fsSelector));
  CHK_VMCS_READ(VMCS_HOST_GS_SELECTOR, &(p->gsSelector));
  CHK_VMCS_READ(VMCS_HOST_TR_SELECTOR, &(p->trSelector));
  CHK_VMCS_READ(HOST_FS_BASE, &(p->fsBaseAddr));
  CHK_VMCS_READ(HOST_GS_BASE, &(p->gsBaseAddr));
  CHK_VMCS_READ(HOST_TR_BASE, &(p->trBaseAddr));
  CHK_VMCS_READ(HOST_GDTR_BASE, &(p->gdtrBaseAddr));
  CHK_VMCS_READ(HOST_IDTR_BASE, &(p->idtrBaseAddr));
  CHK_VMCS_READ(HOST_IA32_SYSENTER_CS, &(p->sysenter_cs));
  CHK_VMCS_READ(HOST_IA32_SYSENTER_ESP, &(p->sysenter_esp));
  CHK_VMCS_READ(HOST_IA32_SYSENTER_EIP, &(p->sysenter_eip));
  return 0;
}



int CopyInVMCSHostStateArea(struct VMCSHostStateArea *p) {
  CHK_VMCS_WRITE(HOST_CR0, &(p->cr0));
  CHK_VMCS_WRITE(HOST_CR3, &(p->cr3));
  CHK_VMCS_WRITE(HOST_CR4, &(p->cr4));
  CHK_VMCS_WRITE(HOST_RSP, &(p->rsp));
  CHK_VMCS_WRITE(HOST_RIP, &(p->rip));
  CHK_VMCS_WRITE(VMCS_HOST_CS_SELECTOR, &(p->csSelector));
  CHK_VMCS_WRITE(VMCS_HOST_SS_SELECTOR, &(p->ssSelector));
  CHK_VMCS_WRITE(VMCS_HOST_DS_SELECTOR, &(p->dsSelector));
  CHK_VMCS_WRITE(VMCS_HOST_ES_SELECTOR, &(p->esSelector));
  CHK_VMCS_WRITE(VMCS_HOST_FS_SELECTOR, &(p->fsSelector));
  CHK_VMCS_WRITE(VMCS_HOST_GS_SELECTOR, &(p->gsSelector));
  CHK_VMCS_WRITE(VMCS_HOST_TR_SELECTOR, &(p->trSelector));
  CHK_VMCS_WRITE(HOST_FS_BASE, &(p->fsBaseAddr));
  CHK_VMCS_WRITE(HOST_GS_BASE, &(p->gsBaseAddr));
  CHK_VMCS_WRITE(HOST_TR_BASE, &(p->trBaseAddr));
  CHK_VMCS_WRITE(HOST_GDTR_BASE, &(p->gdtrBaseAddr));
  CHK_VMCS_WRITE(HOST_IDTR_BASE, &(p->idtrBaseAddr));
  CHK_VMCS_WRITE(HOST_IA32_SYSENTER_CS, &(p->sysenter_cs));
  CHK_VMCS_WRITE(HOST_IA32_SYSENTER_ESP, &(p->sysenter_esp));
  CHK_VMCS_WRITE(HOST_IA32_SYSENTER_EIP, &(p->sysenter_eip));
  return 0;
}


int CopyOutVMCSExitCtrlFields(struct VMCSExitCtrlFields *p)
{
  CHK_VMCS_READ(VM_EXIT_CTRLS,&(p->exitCtrls));
  CHK_VMCS_READ(VM_EXIT_MSR_STORE_COUNT,&(p->msrStoreCount));
  CHK_VMCS_READ(VM_EXIT_MSR_STORE_ADDR,&(p->msrStoreAddr));
  CHK_VMCS_READ(VM_EXIT_MSR_LOAD_COUNT,&(p->msrLoadCount));
  CHK_VMCS_READ(VM_EXIT_MSR_LOAD_ADDR,&(p->msrLoadAddr));
  return 0;
}

int CopyInVMCSExitCtrlFields(struct VMCSExitCtrlFields *p)
{
  CHK_VMCS_WRITE(VM_EXIT_CTRLS,&(p->exitCtrls));
  CHK_VMCS_WRITE(VM_EXIT_MSR_STORE_COUNT,&(p->msrStoreCount));
  CHK_VMCS_WRITE(VM_EXIT_MSR_STORE_ADDR,&(p->msrStoreAddr));
  CHK_VMCS_WRITE(VM_EXIT_MSR_LOAD_COUNT,&(p->msrLoadCount));
  CHK_VMCS_WRITE(VM_EXIT_MSR_LOAD_ADDR,&(p->msrLoadAddr));
  return 0;
}


int CopyOutVMCSEntryCtrlFields(struct VMCSEntryCtrlFields *p)
{
  CHK_VMCS_READ(VM_ENTRY_CTRLS,&(p->entryCtrls));
  CHK_VMCS_READ(VM_ENTRY_MSR_LOAD_COUNT,&(p->msrLoadCount));
  CHK_VMCS_READ(VM_ENTRY_MSR_LOAD_ADDR,&(p->msrLoadAddr));
  CHK_VMCS_READ(VM_ENTRY_INT_INFO_FIELD,&(p->intInfo));
  CHK_VMCS_READ(VM_ENTRY_EXCEPTION_ERROR,&(p->exceptionErrorCode));
  CHK_VMCS_READ(VM_ENTRY_INSTR_LENGTH,&(p->instrLength));
  return 0;
}

int CopyInVMCSEntryCtrlFields(struct VMCSEntryCtrlFields *p)
{
  CHK_VMCS_WRITE(VM_ENTRY_CTRLS,&(p->entryCtrls));
  CHK_VMCS_WRITE(VM_ENTRY_MSR_LOAD_COUNT,&(p->msrLoadCount));
  CHK_VMCS_WRITE(VM_ENTRY_MSR_LOAD_ADDR,&(p->msrLoadAddr));
  CHK_VMCS_WRITE(VM_ENTRY_INT_INFO_FIELD,&(p->intInfo));
  CHK_VMCS_WRITE(VM_ENTRY_EXCEPTION_ERROR,&(p->exceptionErrorCode));
  CHK_VMCS_WRITE(VM_ENTRY_INSTR_LENGTH,&(p->instrLength));
  return 0;
}

int CopyOutVMCSExitInfoFields(struct VMCSExitInfoFields *p) {
  CHK_VMCS_READ(EXIT_REASON,&(p->reason));
  CHK_VMCS_READ(EXIT_QUALIFICATION,&(p->qualification));
  CHK_VMCS_READ(VM_EXIT_INT_INFO,&(p->intInfo));
  CHK_VMCS_READ(VM_EXIT_INT_ERROR,&(p->intErrorCode));
  CHK_VMCS_READ(IDT_VECTOR_INFO,&(p->idtVectorInfo));
  CHK_VMCS_READ(IDT_VECTOR_ERROR,&(p->idtVectorErrorCode));
  CHK_VMCS_READ(VM_EXIT_INSTR_LENGTH,&(p->instrLength));
  CHK_VMCS_READ(GUEST_LINEAR_ADDR,&(p->guestLinearAddr));
  CHK_VMCS_READ(VMX_INSTR_INFO,&(p->instrInfo));
  CHK_VMCS_READ(IO_RCX,&(p->ioRCX));
  CHK_VMCS_READ(IO_RSI,&(p->ioRSI));
  CHK_VMCS_READ(IO_RDI,&(p->ioRDI));
  CHK_VMCS_READ(IO_RIP,&(p->ioRIP));
  CHK_VMCS_READ(VM_INSTR_ERROR,&(p->instrErrorField));
  return 0;
}


int CopyOutVMCSExecCtrlFields(struct VMCSExecCtrlFields *p)
{
  CHK_VMCS_READ(PIN_VM_EXEC_CTRLS,&(p->pinCtrls));
  CHK_VMCS_READ(PROC_VM_EXEC_CTRLS,&(p->procCtrls));
  CHK_VMCS_READ(EXCEPTION_BITMAP,&(p->execBitmap));
  CHK_VMCS_READ(PAGE_FAULT_ERROR_MASK,&(p->pageFaultErrorMask));
  CHK_VMCS_READ(PAGE_FAULT_ERROR_MATCH,&(p->pageFaultErrorMatch));
  CHK_VMCS_READ(IO_BITMAP_A_ADDR,&(p->ioBitmapA));
  CHK_VMCS_READ(IO_BITMAP_B_ADDR,&(p->ioBitmapB));
  CHK_VMCS_READ(TSC_OFFSET,&(p->tscOffset));
  CHK_VMCS_READ(CR0_GUEST_HOST_MASK,&(p->cr0GuestHostMask));
  CHK_VMCS_READ(CR0_READ_SHADOW,&(p->cr0ReadShadow));
  CHK_VMCS_READ(CR4_GUEST_HOST_MASK,&(p->cr4GuestHostMask));
  CHK_VMCS_READ(CR4_READ_SHADOW,&(p->cr4ReadShadow));
  CHK_VMCS_READ(CR3_TARGET_COUNT, &(p->cr3TargetCount));
  CHK_VMCS_READ(CR3_TARGET_VALUE_0, &(p->cr3TargetValue0));
  CHK_VMCS_READ(CR3_TARGET_VALUE_1, &(p->cr3TargetValue1));
  CHK_VMCS_READ(CR3_TARGET_VALUE_2, &(p->cr3TargetValue2));
  CHK_VMCS_READ(CR3_TARGET_VALUE_3, &(p->cr3TargetValue3));
  CHK_VMCS_READ(VIRT_APIC_PAGE_ADDR, &(p->virtApicPageAddr));
  CHK_VMCS_READ(TPR_THRESHOLD, &(p->tprThreshold));
  CHK_VMCS_READ(MSR_BITMAPS, &(p->MSRBitmapsBaseAddr));
  CHK_VMCS_READ(VMCS_EXEC_PTR,&(p->vmcsExecPtr));
  return 0;
}


int CopyInVMCSExecCtrlFields(struct VMCSExecCtrlFields *p)
{
  CHK_VMCS_WRITE(PIN_VM_EXEC_CTRLS,&(p->pinCtrls));
  CHK_VMCS_WRITE(PROC_VM_EXEC_CTRLS,&(p->procCtrls));
  CHK_VMCS_WRITE(EXCEPTION_BITMAP,&(p->execBitmap));
  CHK_VMCS_WRITE(PAGE_FAULT_ERROR_MASK,&(p->pageFaultErrorMask));
  CHK_VMCS_WRITE(PAGE_FAULT_ERROR_MATCH,&(p->pageFaultErrorMatch));
  CHK_VMCS_WRITE(IO_BITMAP_A_ADDR,&(p->ioBitmapA));
  CHK_VMCS_WRITE(IO_BITMAP_B_ADDR,&(p->ioBitmapB));
  CHK_VMCS_WRITE(TSC_OFFSET,&(p->tscOffset));
  CHK_VMCS_WRITE(CR0_GUEST_HOST_MASK,&(p->cr0GuestHostMask));
  CHK_VMCS_WRITE(CR0_READ_SHADOW,&(p->cr0ReadShadow));
  CHK_VMCS_WRITE(CR4_GUEST_HOST_MASK,&(p->cr4GuestHostMask));
  CHK_VMCS_WRITE(CR4_READ_SHADOW,&(p->cr4ReadShadow));
  CHK_VMCS_WRITE(CR3_TARGET_COUNT, &(p->cr3TargetCount));
  CHK_VMCS_WRITE(CR3_TARGET_VALUE_0, &(p->cr3TargetValue0));
  CHK_VMCS_WRITE(CR3_TARGET_VALUE_1, &(p->cr3TargetValue1));
  CHK_VMCS_WRITE(CR3_TARGET_VALUE_2, &(p->cr3TargetValue2));
  CHK_VMCS_WRITE(CR3_TARGET_VALUE_3, &(p->cr3TargetValue3));
  CHK_VMCS_WRITE(VIRT_APIC_PAGE_ADDR, &(p->virtApicPageAddr));
  CHK_VMCS_WRITE(TPR_THRESHOLD, &(p->tprThreshold));
  CHK_VMCS_WRITE(MSR_BITMAPS, &(p->MSRBitmapsBaseAddr));
  CHK_VMCS_WRITE(VMCS_EXEC_PTR,&(p->vmcsExecPtr));
  return 0;
}


int CopyOutVMCSData(struct VMCSData *p) {
  if (CopyOutVMCSGuestStateArea(&(p->guestStateArea)) != 0) {
    return -1;
  }
  if (CopyOutVMCSHostStateArea(&(p->hostStateArea)) != 0) {
    return -1;
  }
  if (CopyOutVMCSExecCtrlFields(&(p->execCtrlFields)) != 0) {
    return -1;
  }
  if (CopyOutVMCSExitCtrlFields(&(p->exitCtrlFields)) != 0) {
    return -1;
  }
  if (CopyOutVMCSEntryCtrlFields(&(p->entryCtrlFields)) != 0) {
    return -1;
  }
  if (CopyOutVMCSExitInfoFields(&(p->exitInfoFields)) != 0) {
    return -1;
  }
  return 0;
}


int CopyInVMCSData(struct VMCSData *p) {
  if (CopyInVMCSGuestStateArea(&(p->guestStateArea)) != 0) {
    return -1;
  }
  if (CopyInVMCSHostStateArea(&(p->hostStateArea)) != 0) {
    return -1;
  }
  if (CopyInVMCSExecCtrlFields(&(p->execCtrlFields)) != 0) {
    return -1;
  }
  if (CopyInVMCSExitCtrlFields(&(p->exitCtrlFields)) != 0) {
    return -1;
  }
  if (CopyInVMCSEntryCtrlFields(&(p->entryCtrlFields)) != 0) {
    return -1;
  }
  return 0;
}


void PrintTrace_VMX_Regs(struct VMXRegs * regs) {
  PrintTrace("==>VMX Register values:\n");
  PrintTrace("EAX: %x\n", regs->eax);
  PrintTrace("ECX: %x\n", regs->ecx);
  PrintTrace("EDX: %x\n", regs->edx);
  PrintTrace("EBX: %x\n", regs->ebx);
  PrintTrace("ESP: %x\n", regs->esp);
  PrintTrace("EBP: %x\n", regs->ebp);
  PrintTrace("ESI: %x\n", regs->esi);
  PrintTrace("EDI: %x\n", regs->edi);
  PrintTrace("\n");
}


void PrintTrace_VMCSSegment(char * segname, struct VMCSSegment * seg, int abbr) {
  PrintTrace("Segment: %s\n", segname);
  if (abbr == 0) {
    PrintTrace("\tSelector: %x\n", (uint_t)seg->selector);
    PrintTrace("\tAccess: %x\n", *(uint_t*)&(seg->access));
  }
  PrintTrace("\tBase Addr: %x\n", (uint_t)seg->baseAddr);
  PrintTrace("\tLimit: %x\n", (uint_t)seg->limit);

}


void PrintTrace_VMCSGuestStateArea(struct VMCSGuestStateArea * guestState) {
  PrintTrace("==>Guest State Area\n");
  PrintTrace("==>==> Guest Register State\n");
  PrintTrace("GUEST_CR0: %x\n",(uint_t) guestState->cr0);
  PrintTrace("GUEST_CR3: %x\n",(uint_t)guestState->cr3);
  PrintTrace("GUEST_CR4: %x\n",(uint_t)guestState->cr4);
  PrintTrace("GUEST_DR7: %x\n",(uint_t)guestState->dr7);
  PrintTrace("GUEST_RSP: %x\n",(uint_t)guestState->rsp);
  PrintTrace("GUEST_RIP: %x\n",(uint_t)guestState->rip);
  PrintTrace("GUEST_RFLAGS: %x\n",(uint_t)guestState->rflags);

  PrintTrace_VMCSSegment("Guest CS", &(guestState->cs), 0);
  PrintTrace_VMCSSegment("Guest SS", &(guestState->ss), 0);
  PrintTrace_VMCSSegment("Guest DS",&(guestState->ds), 0);
  PrintTrace_VMCSSegment("Guest ES", &(guestState->es), 0);
  PrintTrace_VMCSSegment("Guest FS", &(guestState->fs), 0);
  PrintTrace_VMCSSegment("Guest GS", &(guestState->gs), 0);
  PrintTrace_VMCSSegment("Guest LDTR", &(guestState->ldtr), 0);
  PrintTrace_VMCSSegment("Guest TR", &(guestState->tr), 0);
  PrintTrace_VMCSSegment("Guest GDTR", &(guestState->gdtr), 1);  
  PrintTrace_VMCSSegment("Guest IDTR", &(guestState->idtr), 1);  


  PrintTrace("GUEST_IA32_DEBUGCTL: %x\n",(uint_t)(guestState->dbg_ctrl & 0xffffffff));
  PrintTrace("GUEST_IA32_DEBUGCTL_HIGH: %x\n",(uint_t)(guestState->dbg_ctrl >> 32) & 0xffffffff);
  PrintTrace("GUEST_IA32_SYSENTER_CS: %x\n",guestState->sysenter_cs);
  PrintTrace("GUEST_IA32_SYSENTER_ESP: %x\n",(uint_t)guestState->sysenter_esp);
  PrintTrace("GUEST_IA32_SYSENTER_EIP: %x\n",(uint_t)guestState->sysenter_eip);
  PrintTrace("GUEST_SMBASE: %x\n", (uint_t)guestState->smbase);

  PrintTrace("==>==> Guest Non-Register State\n");
  PrintTrace("GUEST_ACTIVITY_STATE: %x\n", (uint_t)guestState->activity);
  PrintTrace("GUEST_INT_STATE: %x\n", (uint_t)guestState->interrupt_state);
  PrintTrace("GUEST_PENDING_DEBUG_EXCS: %x\n", (uint_t)guestState->pending_dbg_exceptions);
  PrintTrace("VMCS_LINK_PTR: %x\n", (uint_t)guestState->vmcs_link & 0xffffffff);
  PrintTrace("VMCS_LINK_PTR_HIGH: %x\n", (uint_t)(guestState->vmcs_link >> 32) & 0xffffffff);
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
