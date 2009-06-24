/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Automatically Generated File
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmcs_gen.h>



void    PrintTrace_VMCS_GUEST_ES_SELECTOR() { PrintTrace("VMCS_GUEST_ES_SELECTOR = %x\n", Get_VMCS_GUEST_ES_SELECTOR()); }
void    PrintTrace_VMCS_GUEST_CS_SELECTOR() { PrintTrace("VMCS_GUEST_CS_SELECTOR = %x\n", Get_VMCS_GUEST_CS_SELECTOR()); }
void    PrintTrace_VMCS_GUEST_SS_SELECTOR() { PrintTrace("VMCS_GUEST_SS_SELECTOR = %x\n", Get_VMCS_GUEST_SS_SELECTOR()); }
void    PrintTrace_VMCS_GUEST_DS_SELECTOR() { PrintTrace("VMCS_GUEST_DS_SELECTOR = %x\n", Get_VMCS_GUEST_DS_SELECTOR()); }
void    PrintTrace_VMCS_GUEST_FS_SELECTOR() { PrintTrace("VMCS_GUEST_FS_SELECTOR = %x\n", Get_VMCS_GUEST_FS_SELECTOR()); }
void    PrintTrace_VMCS_GUEST_GS_SELECTOR() { PrintTrace("VMCS_GUEST_GS_SELECTOR = %x\n", Get_VMCS_GUEST_GS_SELECTOR()); }
void    PrintTrace_VMCS_GUEST_LDTR_SELECTOR() { PrintTrace("VMCS_GUEST_LDTR_SELECTOR = %x\n", Get_VMCS_GUEST_LDTR_SELECTOR()); }
void    PrintTrace_VMCS_GUEST_TR_SELECTOR() { PrintTrace("VMCS_GUEST_TR_SELECTOR = %x\n", Get_VMCS_GUEST_TR_SELECTOR()); }
void    PrintTrace_VMCS_HOST_ES_SELECTOR() { PrintTrace("VMCS_HOST_ES_SELECTOR = %x\n", Get_VMCS_HOST_ES_SELECTOR()); }
void    PrintTrace_VMCS_HOST_CS_SELECTOR() { PrintTrace("VMCS_HOST_CS_SELECTOR = %x\n", Get_VMCS_HOST_CS_SELECTOR()); }
void    PrintTrace_VMCS_HOST_SS_SELECTOR() { PrintTrace("VMCS_HOST_SS_SELECTOR = %x\n", Get_VMCS_HOST_SS_SELECTOR()); }
void    PrintTrace_VMCS_HOST_DS_SELECTOR() { PrintTrace("VMCS_HOST_DS_SELECTOR = %x\n", Get_VMCS_HOST_DS_SELECTOR()); }
void    PrintTrace_VMCS_HOST_FS_SELECTOR() { PrintTrace("VMCS_HOST_FS_SELECTOR = %x\n", Get_VMCS_HOST_FS_SELECTOR()); }
void    PrintTrace_VMCS_HOST_GS_SELECTOR() { PrintTrace("VMCS_HOST_GS_SELECTOR = %x\n", Get_VMCS_HOST_GS_SELECTOR()); }
void    PrintTrace_VMCS_HOST_TR_SELECTOR() { PrintTrace("VMCS_HOST_TR_SELECTOR = %x\n", Get_VMCS_HOST_TR_SELECTOR()); }
void    PrintTrace_IO_BITMAP_A_ADDR() { PrintTrace("IO_BITMAP_A_ADDR = %x\n", Get_IO_BITMAP_A_ADDR()); }
void    PrintTrace_IO_BITMAP_A_ADDR_HIGH() { PrintTrace("IO_BITMAP_A_ADDR_HIGH = %x\n", Get_IO_BITMAP_A_ADDR_HIGH()); }
void    PrintTrace_IO_BITMAP_B_ADDR() { PrintTrace("IO_BITMAP_B_ADDR = %x\n", Get_IO_BITMAP_B_ADDR()); }
void    PrintTrace_IO_BITMAP_B_ADDR_HIGH() { PrintTrace("IO_BITMAP_B_ADDR_HIGH = %x\n", Get_IO_BITMAP_B_ADDR_HIGH()); }
void    PrintTrace_MSR_BITMAPS() { PrintTrace("MSR_BITMAPS = %x\n", Get_MSR_BITMAPS()); }
void    PrintTrace_MSR_BITMAPS_HIGH() { PrintTrace("MSR_BITMAPS_HIGH = %x\n", Get_MSR_BITMAPS_HIGH()); }
void    PrintTrace_VM_EXIT_MSR_STORE_ADDR() { PrintTrace("VM_EXIT_MSR_STORE_ADDR = %x\n", Get_VM_EXIT_MSR_STORE_ADDR()); }
void    PrintTrace_VM_EXIT_MSR_STORE_ADDR_HIGH() { PrintTrace("VM_EXIT_MSR_STORE_ADDR_HIGH = %x\n", Get_VM_EXIT_MSR_STORE_ADDR_HIGH()); }
void    PrintTrace_VM_EXIT_MSR_LOAD_ADDR() { PrintTrace("VM_EXIT_MSR_LOAD_ADDR = %x\n", Get_VM_EXIT_MSR_LOAD_ADDR()); }
void    PrintTrace_VM_EXIT_MSR_LOAD_ADDR_HIGH() { PrintTrace("VM_EXIT_MSR_LOAD_ADDR_HIGH = %x\n", Get_VM_EXIT_MSR_LOAD_ADDR_HIGH()); }
void    PrintTrace_VM_ENTRY_MSR_LOAD_ADDR() { PrintTrace("VM_ENTRY_MSR_LOAD_ADDR = %x\n", Get_VM_ENTRY_MSR_LOAD_ADDR()); }
void    PrintTrace_VM_ENTRY_MSR_LOAD_ADDR_HIGH() { PrintTrace("VM_ENTRY_MSR_LOAD_ADDR_HIGH = %x\n", Get_VM_ENTRY_MSR_LOAD_ADDR_HIGH()); }
void    PrintTrace_VMCS_EXEC_PTR() { PrintTrace("VMCS_EXEC_PTR = %x\n", Get_VMCS_EXEC_PTR()); }
void    PrintTrace_VMCS_EXEC_PTR_HIGH() { PrintTrace("VMCS_EXEC_PTR_HIGH = %x\n", Get_VMCS_EXEC_PTR_HIGH()); }
void    PrintTrace_TSC_OFFSET() { PrintTrace("TSC_OFFSET = %x\n", Get_TSC_OFFSET()); }
void    PrintTrace_TSC_OFFSET_HIGH() { PrintTrace("TSC_OFFSET_HIGH = %x\n", Get_TSC_OFFSET_HIGH()); }
void    PrintTrace_VIRT_APIC_PAGE_ADDR() { PrintTrace("VIRT_APIC_PAGE_ADDR = %x\n", Get_VIRT_APIC_PAGE_ADDR()); }
void    PrintTrace_VIRT_APIC_PAGE_ADDR_HIGH() { PrintTrace("VIRT_APIC_PAGE_ADDR_HIGH = %x\n", Get_VIRT_APIC_PAGE_ADDR_HIGH()); }
void    PrintTrace_VMCS_LINK_PTR() { PrintTrace("VMCS_LINK_PTR = %x\n", Get_VMCS_LINK_PTR()); }
void    PrintTrace_VMCS_LINK_PTR_HIGH() { PrintTrace("VMCS_LINK_PTR_HIGH = %x\n", Get_VMCS_LINK_PTR_HIGH()); }
void    PrintTrace_GUEST_IA32_DEBUGCTL() { PrintTrace("GUEST_IA32_DEBUGCTL = %x\n", Get_GUEST_IA32_DEBUGCTL()); }
void    PrintTrace_GUEST_IA32_DEBUGCTL_HIGH() { PrintTrace("GUEST_IA32_DEBUGCTL_HIGH = %x\n", Get_GUEST_IA32_DEBUGCTL_HIGH()); }
void    PrintTrace_PIN_VM_EXEC_CTRLS() { PrintTrace("PIN_VM_EXEC_CTRLS = %x\n", Get_PIN_VM_EXEC_CTRLS()); }
void    PrintTrace_PROC_VM_EXEC_CTRLS() { PrintTrace("PROC_VM_EXEC_CTRLS = %x\n", Get_PROC_VM_EXEC_CTRLS()); }
void    PrintTrace_EXCEPTION_BITMAP() { PrintTrace("EXCEPTION_BITMAP = %x\n", Get_EXCEPTION_BITMAP()); }
void    PrintTrace_PAGE_FAULT_ERROR_MASK() { PrintTrace("PAGE_FAULT_ERROR_MASK = %x\n", Get_PAGE_FAULT_ERROR_MASK()); }
void    PrintTrace_PAGE_FAULT_ERROR_MATCH() { PrintTrace("PAGE_FAULT_ERROR_MATCH = %x\n", Get_PAGE_FAULT_ERROR_MATCH()); }
void    PrintTrace_CR3_TARGET_COUNT() { PrintTrace("CR3_TARGET_COUNT = %x\n", Get_CR3_TARGET_COUNT()); }
void    PrintTrace_VM_EXIT_CTRLS() { PrintTrace("VM_EXIT_CTRLS = %x\n", Get_VM_EXIT_CTRLS()); }
void    PrintTrace_VM_EXIT_MSR_STORE_COUNT() { PrintTrace("VM_EXIT_MSR_STORE_COUNT = %x\n", Get_VM_EXIT_MSR_STORE_COUNT()); }
void    PrintTrace_VM_EXIT_MSR_LOAD_COUNT() { PrintTrace("VM_EXIT_MSR_LOAD_COUNT = %x\n", Get_VM_EXIT_MSR_LOAD_COUNT()); }
void    PrintTrace_VM_ENTRY_CTRLS() { PrintTrace("VM_ENTRY_CTRLS = %x\n", Get_VM_ENTRY_CTRLS()); }
void    PrintTrace_VM_ENTRY_MSR_LOAD_COUNT() { PrintTrace("VM_ENTRY_MSR_LOAD_COUNT = %x\n", Get_VM_ENTRY_MSR_LOAD_COUNT()); }
void    PrintTrace_VM_ENTRY_INT_INFO_FIELD() { PrintTrace("VM_ENTRY_INT_INFO_FIELD = %x\n", Get_VM_ENTRY_INT_INFO_FIELD()); }
void    PrintTrace_VM_ENTRY_EXCEPTION_ERROR() { PrintTrace("VM_ENTRY_EXCEPTION_ERROR = %x\n", Get_VM_ENTRY_EXCEPTION_ERROR()); }
void    PrintTrace_VM_ENTRY_INSTR_LENGTH() { PrintTrace("VM_ENTRY_INSTR_LENGTH = %x\n", Get_VM_ENTRY_INSTR_LENGTH()); }
void    PrintTrace_TPR_THRESHOLD() { PrintTrace("TPR_THRESHOLD = %x\n", Get_TPR_THRESHOLD()); }
void    PrintTrace_VM_INSTR_ERROR() { PrintTrace("VM_INSTR_ERROR = %x\n", Get_VM_INSTR_ERROR()); }
void    PrintTrace_EXIT_REASON() { PrintTrace("EXIT_REASON = %x\n", Get_EXIT_REASON()); }
void    PrintTrace_VM_EXIT_INT_INFO() { PrintTrace("VM_EXIT_INT_INFO = %x\n", Get_VM_EXIT_INT_INFO()); }
void    PrintTrace_VM_EXIT_INT_ERROR() { PrintTrace("VM_EXIT_INT_ERROR = %x\n", Get_VM_EXIT_INT_ERROR()); }
void    PrintTrace_IDT_VECTOR_INFO() { PrintTrace("IDT_VECTOR_INFO = %x\n", Get_IDT_VECTOR_INFO()); }
void    PrintTrace_IDT_VECTOR_ERROR() { PrintTrace("IDT_VECTOR_ERROR = %x\n", Get_IDT_VECTOR_ERROR()); }
void    PrintTrace_VM_EXIT_INSTR_LENGTH() { PrintTrace("VM_EXIT_INSTR_LENGTH = %x\n", Get_VM_EXIT_INSTR_LENGTH()); }
void    PrintTrace_VMX_INSTR_INFO() { PrintTrace("VMX_INSTR_INFO = %x\n", Get_VMX_INSTR_INFO()); }
void    PrintTrace_GUEST_ES_LIMIT() { PrintTrace("GUEST_ES_LIMIT = %x\n", Get_GUEST_ES_LIMIT()); }
void    PrintTrace_GUEST_CS_LIMIT() { PrintTrace("GUEST_CS_LIMIT = %x\n", Get_GUEST_CS_LIMIT()); }
void    PrintTrace_GUEST_SS_LIMIT() { PrintTrace("GUEST_SS_LIMIT = %x\n", Get_GUEST_SS_LIMIT()); }
void    PrintTrace_GUEST_DS_LIMIT() { PrintTrace("GUEST_DS_LIMIT = %x\n", Get_GUEST_DS_LIMIT()); }
void    PrintTrace_GUEST_FS_LIMIT() { PrintTrace("GUEST_FS_LIMIT = %x\n", Get_GUEST_FS_LIMIT()); }
void    PrintTrace_GUEST_GS_LIMIT() { PrintTrace("GUEST_GS_LIMIT = %x\n", Get_GUEST_GS_LIMIT()); }
void    PrintTrace_GUEST_LDTR_LIMIT() { PrintTrace("GUEST_LDTR_LIMIT = %x\n", Get_GUEST_LDTR_LIMIT()); }
void    PrintTrace_GUEST_TR_LIMIT() { PrintTrace("GUEST_TR_LIMIT = %x\n", Get_GUEST_TR_LIMIT()); }
void    PrintTrace_GUEST_GDTR_LIMIT() { PrintTrace("GUEST_GDTR_LIMIT = %x\n", Get_GUEST_GDTR_LIMIT()); }
void    PrintTrace_GUEST_IDTR_LIMIT() { PrintTrace("GUEST_IDTR_LIMIT = %x\n", Get_GUEST_IDTR_LIMIT()); }
void    PrintTrace_GUEST_ES_ACCESS() { PrintTrace("GUEST_ES_ACCESS = %x\n", Get_GUEST_ES_ACCESS()); }
void    PrintTrace_GUEST_CS_ACCESS() { PrintTrace("GUEST_CS_ACCESS = %x\n", Get_GUEST_CS_ACCESS()); }
void    PrintTrace_GUEST_SS_ACCESS() { PrintTrace("GUEST_SS_ACCESS = %x\n", Get_GUEST_SS_ACCESS()); }
void    PrintTrace_GUEST_DS_ACCESS() { PrintTrace("GUEST_DS_ACCESS = %x\n", Get_GUEST_DS_ACCESS()); }
void    PrintTrace_GUEST_FS_ACCESS() { PrintTrace("GUEST_FS_ACCESS = %x\n", Get_GUEST_FS_ACCESS()); }
void    PrintTrace_GUEST_GS_ACCESS() { PrintTrace("GUEST_GS_ACCESS = %x\n", Get_GUEST_GS_ACCESS()); }
void    PrintTrace_GUEST_LDTR_ACCESS() { PrintTrace("GUEST_LDTR_ACCESS = %x\n", Get_GUEST_LDTR_ACCESS()); }
void    PrintTrace_GUEST_TR_ACCESS() { PrintTrace("GUEST_TR_ACCESS = %x\n", Get_GUEST_TR_ACCESS()); }
void    PrintTrace_GUEST_INT_STATE() { PrintTrace("GUEST_INT_STATE = %x\n", Get_GUEST_INT_STATE()); }
void    PrintTrace_GUEST_ACTIVITY_STATE() { PrintTrace("GUEST_ACTIVITY_STATE = %x\n", Get_GUEST_ACTIVITY_STATE()); }
void    PrintTrace_GUEST_SMBASE() { PrintTrace("GUEST_SMBASE = %x\n", Get_GUEST_SMBASE()); }
void    PrintTrace_GUEST_IA32_SYSENTER_CS() { PrintTrace("GUEST_IA32_SYSENTER_CS = %x\n", Get_GUEST_IA32_SYSENTER_CS()); }
void    PrintTrace_HOST_IA32_SYSENTER_CS() { PrintTrace("HOST_IA32_SYSENTER_CS = %x\n", Get_HOST_IA32_SYSENTER_CS()); }
void    PrintTrace_CR0_GUEST_HOST_MASK() { PrintTrace("CR0_GUEST_HOST_MASK = %x\n", Get_CR0_GUEST_HOST_MASK()); }
void    PrintTrace_CR4_GUEST_HOST_MASK() { PrintTrace("CR4_GUEST_HOST_MASK = %x\n", Get_CR4_GUEST_HOST_MASK()); }
void    PrintTrace_CR0_READ_SHADOW() { PrintTrace("CR0_READ_SHADOW = %x\n", Get_CR0_READ_SHADOW()); }
void    PrintTrace_CR4_READ_SHADOW() { PrintTrace("CR4_READ_SHADOW = %x\n", Get_CR4_READ_SHADOW()); }
void    PrintTrace_CR3_TARGET_VALUE_0() { PrintTrace("CR3_TARGET_VALUE_0 = %x\n", Get_CR3_TARGET_VALUE_0()); }
void    PrintTrace_CR3_TARGET_VALUE_1() { PrintTrace("CR3_TARGET_VALUE_1 = %x\n", Get_CR3_TARGET_VALUE_1()); }
void    PrintTrace_CR3_TARGET_VALUE_2() { PrintTrace("CR3_TARGET_VALUE_2 = %x\n", Get_CR3_TARGET_VALUE_2()); }
void    PrintTrace_CR3_TARGET_VALUE_3() { PrintTrace("CR3_TARGET_VALUE_3 = %x\n", Get_CR3_TARGET_VALUE_3()); }
void    PrintTrace_EXIT_QUALIFICATION() { PrintTrace("EXIT_QUALIFICATION = %x\n", Get_EXIT_QUALIFICATION()); }
void    PrintTrace_IO_RCX() { PrintTrace("IO_RCX = %x\n", Get_IO_RCX()); }
void    PrintTrace_IO_RSI() { PrintTrace("IO_RSI = %x\n", Get_IO_RSI()); }
void    PrintTrace_IO_RDI() { PrintTrace("IO_RDI = %x\n", Get_IO_RDI()); }
void    PrintTrace_IO_RIP() { PrintTrace("IO_RIP = %x\n", Get_IO_RIP()); }
void    PrintTrace_GUEST_LINEAR_ADDR() { PrintTrace("GUEST_LINEAR_ADDR = %x\n", Get_GUEST_LINEAR_ADDR()); }
void    PrintTrace_GUEST_CR0() { PrintTrace("GUEST_CR0 = %x\n", Get_GUEST_CR0()); }
void    PrintTrace_GUEST_CR3() { PrintTrace("GUEST_CR3 = %x\n", Get_GUEST_CR3()); }
void    PrintTrace_GUEST_CR4() { PrintTrace("GUEST_CR4 = %x\n", Get_GUEST_CR4()); }
void    PrintTrace_GUEST_ES_BASE() { PrintTrace("GUEST_ES_BASE = %x\n", Get_GUEST_ES_BASE()); }
void    PrintTrace_GUEST_CS_BASE() { PrintTrace("GUEST_CS_BASE = %x\n", Get_GUEST_CS_BASE()); }
void    PrintTrace_GUEST_SS_BASE() { PrintTrace("GUEST_SS_BASE = %x\n", Get_GUEST_SS_BASE()); }
void    PrintTrace_GUEST_DS_BASE() { PrintTrace("GUEST_DS_BASE = %x\n", Get_GUEST_DS_BASE()); }
void    PrintTrace_GUEST_FS_BASE() { PrintTrace("GUEST_FS_BASE = %x\n", Get_GUEST_FS_BASE()); }
void    PrintTrace_GUEST_GS_BASE() { PrintTrace("GUEST_GS_BASE = %x\n", Get_GUEST_GS_BASE()); }
void    PrintTrace_GUEST_LDTR_BASE() { PrintTrace("GUEST_LDTR_BASE = %x\n", Get_GUEST_LDTR_BASE()); }
void    PrintTrace_GUEST_TR_BASE() { PrintTrace("GUEST_TR_BASE = %x\n", Get_GUEST_TR_BASE()); }
void    PrintTrace_GUEST_GDTR_BASE() { PrintTrace("GUEST_GDTR_BASE = %x\n", Get_GUEST_GDTR_BASE()); }
void    PrintTrace_GUEST_IDTR_BASE() { PrintTrace("GUEST_IDTR_BASE = %x\n", Get_GUEST_IDTR_BASE()); }
void    PrintTrace_GUEST_DR7() { PrintTrace("GUEST_DR7 = %x\n", Get_GUEST_DR7()); }
void    PrintTrace_GUEST_RSP() { PrintTrace("GUEST_RSP = %x\n", Get_GUEST_RSP()); }
void    PrintTrace_GUEST_RIP() { PrintTrace("GUEST_RIP = %x\n", Get_GUEST_RIP()); }
void    PrintTrace_GUEST_RFLAGS() { PrintTrace("GUEST_RFLAGS = %x\n", Get_GUEST_RFLAGS()); }
void    PrintTrace_GUEST_PENDING_DEBUG_EXCS() { PrintTrace("GUEST_PENDING_DEBUG_EXCS = %x\n", Get_GUEST_PENDING_DEBUG_EXCS()); }
void    PrintTrace_GUEST_IA32_SYSENTER_ESP() { PrintTrace("GUEST_IA32_SYSENTER_ESP = %x\n", Get_GUEST_IA32_SYSENTER_ESP()); }
void    PrintTrace_GUEST_IA32_SYSENTER_EIP() { PrintTrace("GUEST_IA32_SYSENTER_EIP = %x\n", Get_GUEST_IA32_SYSENTER_EIP()); }
void    PrintTrace_HOST_CR0() { PrintTrace("HOST_CR0 = %x\n", Get_HOST_CR0()); }
void    PrintTrace_HOST_CR3() { PrintTrace("HOST_CR3 = %x\n", Get_HOST_CR3()); }
void    PrintTrace_HOST_CR4() { PrintTrace("HOST_CR4 = %x\n", Get_HOST_CR4()); }
void    PrintTrace_HOST_FS_BASE() { PrintTrace("HOST_FS_BASE = %x\n", Get_HOST_FS_BASE()); }
void    PrintTrace_HOST_GS_BASE() { PrintTrace("HOST_GS_BASE = %x\n", Get_HOST_GS_BASE()); }
void    PrintTrace_HOST_TR_BASE() { PrintTrace("HOST_TR_BASE = %x\n", Get_HOST_TR_BASE()); }
void    PrintTrace_HOST_GDTR_BASE() { PrintTrace("HOST_GDTR_BASE = %x\n", Get_HOST_GDTR_BASE()); }
void    PrintTrace_HOST_IDTR_BASE() { PrintTrace("HOST_IDTR_BASE = %x\n", Get_HOST_IDTR_BASE()); }
void    PrintTrace_HOST_IA32_SYSENTER_ESP() { PrintTrace("HOST_IA32_SYSENTER_ESP = %x\n", Get_HOST_IA32_SYSENTER_ESP()); }
void    PrintTrace_HOST_IA32_SYSENTER_EIP() { PrintTrace("HOST_IA32_SYSENTER_EIP = %x\n", Get_HOST_IA32_SYSENTER_EIP()); }
void    PrintTrace_HOST_RSP() { PrintTrace("HOST_RSP = %x\n", Get_HOST_RSP()); }
void    PrintTrace_HOST_RIP() { PrintTrace("HOST_RIP = %x\n", Get_HOST_RIP()); }



static void print_guest_segments() {
    struct vmcs_segment tmp_seg;
    int i = 0;

    char names[] = {"CS", "SS", "DS", "ES", "FS", "GS", "LDTR", "TR"};
    
    uint32_t sels[] = { VMCS_GUEST_CS_SELECTOR,
			VMCS_GUEST_SS_SELECTOR,
			VMCS_GUEST_DS_SELECTOR,
			VMCS_GUEST_ES_SELECTOR,
			VMCS_GUEST_FS_SELECTOR,
			VMCS_GUEST_GS_SELECTOR,
			VMCS_GUEST_LDTR_SELECTOR,
			VMCS_GUEST_TR_SELECTOR };
    
    uint32_t bases[] = { VMCS_GUEST_CS_BASE,
			 VMCS_GUEST_SS_BASE,
			 VMCS_GUEST_DS_BASE,
			 VMCS_GUEST_ES_BASE,
			 VMCS_GUEST_FS_BASE,
			 VMCS_GUEST_GS_BASE,
			 VMCS_GUEST_LDTR_BASE,
			 VMCS_GUEST_TR_BASE };

    uint32_t limits[] = { VMCS_GUEST_CS_LIMIT,
			  VMCS_GUEST_SS_LIMIT,
			  VMCS_GUEST_DS_LIMIT,
			  VMCS_GUEST_ES_LIMIT,
			  VMCS_GUEST_FS_LIMIT,
			  VMCS_GUEST_GS_LIMIT,
			  VMCS_GUEST_LDTR_LIMIT,
			  VMCS_GUEST_TR_LIMIT };

    uint32_t accesses[] = { VMCS_GUEST_CS_ACCESS,
			    VMCS_GUEST_SS_ACCESS,
			    VMCS_GUEST_DS_ACCESS,
			    VMCS_GUEST_ES_ACCESS,
			    VMCS_GUEST_FS_ACCESS,
			    VMCS_GUEST_GS_ACCESS,
			    VMCS_GUEST_LDTR_ACCESS,
			    VMCS_GUEST_TR_ACCESS };


    for (i = 0; i < 8; i++) {
	vmcs_read(sels[i], &(tmp_seg.selector), 2);
	vmcs_read(bases[i], &(tmp_seg.base), 4);
	vmcs_read(limits[i], &(tmp_seg.limit), 4);
	vmcs_read(accesses[i], &(tmp_seg.access.value), 4);
	
    }


    VMCS_GUEST_GDTR_BASE;
    VMCS_GUEST_GDTR_LIMIT;

    VMCS_GUEST_IDTR_BASE;
    VMCS_GUEST_IDTR_LIMIT;


}


static void print_guest_reg_state() {
    PrintDebug(" Guest Register State\n");
    PrintDebug("\tGuest Ctrl Regs\n");

    PrintDebug_GUEST_CR0();
    PrintDebug_GUEST_CR3();
    PrintDebug_GUEST_CR4();

    PrintDebug_GUEST_DR7();

    PrintDebug_GUEST_RSP();
    PrintDebug_GUEST_RIP();
  PrintDebug_GUEST_RFLAGS();
  PrintDebug_GUEST_IA32_DEBUGCTL();
  PrintDebug_GUEST_IA32_DEBUGCTL_HIGH();
  PrintDebug_GUEST_IA32_SYSENTER_CS();
  PrintDebug_GUEST_IA32_SYSENTER_ESP();
  PrintDebug_GUEST_IA32_SYSENTER_EIP();
  PrintDebug_GUEST_SMBASE();



}


void PrintVMCS() {

  PrintDebug("==>Guest State Area\n");

  PrintDebug("==>==> Guest Non-Register State\n");
  PrintDebug_GUEST_ACTIVITY_STATE();
  PrintDebug_GUEST_INT_STATE();
  PrintDebug_GUEST_PENDING_DEBUG_EXCS();
  PrintDebug_VMCS_LINK_PTR();
  PrintDebug_VMCS_LINK_PTR_HIGH();

  PrintDebug("\n==> Host State Area\n");
  PrintDebug_HOST_CR0();
  PrintDebug_HOST_CR3();
  PrintDebug_HOST_CR4();
  PrintDebug_HOST_RSP();
  PrintDebug_HOST_RIP();
  PrintDebug_VMCS_HOST_CS_SELECTOR();
  PrintDebug_VMCS_HOST_SS_SELECTOR();
  PrintDebug_VMCS_HOST_DS_SELECTOR();
  PrintDebug_VMCS_HOST_ES_SELECTOR();
  PrintDebug_VMCS_HOST_FS_SELECTOR();
  PrintDebug_VMCS_HOST_GS_SELECTOR();
  PrintDebug_VMCS_HOST_TR_SELECTOR();
  PrintDebug_HOST_FS_BASE();
  PrintDebug_HOST_GS_BASE();
  PrintDebug_HOST_TR_BASE();
  PrintDebug_HOST_GDTR_BASE();
  PrintDebug_HOST_IDTR_BASE();
  PrintDebug_HOST_IA32_SYSENTER_CS();
  PrintDebug_HOST_IA32_SYSENTER_ESP();
  PrintDebug_HOST_IA32_SYSENTER_EIP();


  PrintDebug("\n==> VM-Execution Controls:\n");
  PrintDebug_PIN_VM_EXEC_CTRLS();
  PrintDebug_PROC_VM_EXEC_CTRLS();
  PrintDebug_EXCEPTION_BITMAP();
  PrintDebug_PAGE_FAULT_ERROR_MASK();
  PrintDebug_PAGE_FAULT_ERROR_MATCH();
  PrintDebug_IO_BITMAP_A_ADDR();
  PrintDebug_IO_BITMAP_A_ADDR_HIGH();
  PrintDebug_IO_BITMAP_B_ADDR();
  PrintDebug_IO_BITMAP_B_ADDR_HIGH();
  PrintDebug_TSC_OFFSET();
  PrintDebug_TSC_OFFSET_HIGH();
  PrintDebug_CR0_GUEST_HOST_MASK();
  PrintDebug_CR0_READ_SHADOW();
  PrintDebug_CR4_GUEST_HOST_MASK();
  PrintDebug_CR4_READ_SHADOW();
  PrintDebug_CR3_TARGET_COUNT();
  PrintDebug_CR3_TARGET_VALUE_0();
  PrintDebug_CR3_TARGET_VALUE_1();
  PrintDebug_CR3_TARGET_VALUE_2();
  PrintDebug_CR3_TARGET_VALUE_3();
  PrintDebug_VIRT_APIC_PAGE_ADDR();
  PrintDebug_VIRT_APIC_PAGE_ADDR_HIGH();
  PrintDebug_TPR_THRESHOLD();
  PrintDebug_MSR_BITMAPS();
  PrintDebug_MSR_BITMAPS_HIGH();
  PrintDebug_VMCS_EXEC_PTR();
  PrintDebug_VMCS_EXEC_PTR_HIGH();

  PrintDebug("\n==> VM Exit Controls\n");
  PrintDebug_VM_EXIT_CTRLS();
  PrintDebug_VM_EXIT_MSR_STORE_COUNT();
  PrintDebug_VM_EXIT_MSR_STORE_ADDR();
  PrintDebug_VM_EXIT_MSR_STORE_ADDR_HIGH();
  PrintDebug_VM_EXIT_MSR_LOAD_COUNT();
  PrintDebug_VM_EXIT_MSR_LOAD_ADDR();
  PrintDebug_VM_EXIT_MSR_LOAD_ADDR_HIGH();

  PrintDebug("\n==> VM Entry Controls\n");
  PrintDebug_VM_ENTRY_CTRLS();
  PrintDebug_VM_ENTRY_MSR_LOAD_COUNT();
  PrintDebug_VM_ENTRY_MSR_LOAD_ADDR();
  PrintDebug_VM_ENTRY_MSR_LOAD_ADDR_HIGH();
  PrintDebug_VM_ENTRY_INT_INFO_FIELD();
  PrintDebug_VM_ENTRY_EXCEPTION_ERROR();
  PrintDebug_VM_ENTRY_INSTR_LENGTH();

  PrintDebug("\n==> VM Exit Info\n");
  PrintDebug_EXIT_REASON();
  PrintDebug_EXIT_QUALIFICATION();
  PrintDebug_VM_EXIT_INT_INFO();
  PrintDebug_VM_EXIT_INT_ERROR();
  PrintDebug_IDT_VECTOR_INFO();
  PrintDebug_IDT_VECTOR_ERROR();
  PrintDebug_VM_EXIT_INSTR_LENGTH();
  PrintDebug_GUEST_LINEAR_ADDR();
  PrintDebug_VMX_INSTR_INFO();
  PrintDebug_IO_RCX();
  PrintDebug_IO_RSI();
  PrintDebug_IO_RDI();
  PrintDebug_IO_RIP();
  PrintDebug_VM_INSTR_ERROR();
  PrintDebug("\n");
}
