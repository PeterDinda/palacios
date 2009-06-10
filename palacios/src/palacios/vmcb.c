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

#include <palacios/vmcb.h>
#include <palacios/vmm.h>
#include <palacios/vmm_util.h>



void v3_set_vmcb_segment(struct vmcb_selector * vmcb_seg, struct v3_segment * seg) {
    vmcb_seg->selector = seg->selector;
    vmcb_seg->limit = seg->limit;
    vmcb_seg->base = seg->base;
    vmcb_seg->attrib.fields.type = seg->type;
    vmcb_seg->attrib.fields.S = seg->system;
    vmcb_seg->attrib.fields.dpl = seg->dpl;
    vmcb_seg->attrib.fields.P = seg->present;
    vmcb_seg->attrib.fields.avl = seg->avail;
    vmcb_seg->attrib.fields.L = seg->long_mode;
    vmcb_seg->attrib.fields.db = seg->db;
    vmcb_seg->attrib.fields.G = seg->granularity;
}


void v3_get_vmcb_segment(struct vmcb_selector * vmcb_seg, struct v3_segment * seg) {
    seg->selector = vmcb_seg->selector;
    seg->limit = vmcb_seg->limit;
    seg->base = vmcb_seg->base;
    seg->type = vmcb_seg->attrib.fields.type;
    seg->system = vmcb_seg->attrib.fields.S;
    seg->dpl = vmcb_seg->attrib.fields.dpl;
    seg->present = vmcb_seg->attrib.fields.P;
    seg->avail = vmcb_seg->attrib.fields.avl;
    seg->long_mode = vmcb_seg->attrib.fields.L;
    seg->db = vmcb_seg->attrib.fields.db;
    seg->granularity = vmcb_seg->attrib.fields.G;
}


void v3_set_vmcb_segments(vmcb_t * vmcb, struct v3_segments * segs) {
    vmcb_saved_state_t * guest_area = GET_VMCB_SAVE_STATE_AREA(vmcb);

    v3_set_vmcb_segment(&(guest_area->cs), &(segs->cs));
    v3_set_vmcb_segment(&(guest_area->ds), &(segs->ds));
    v3_set_vmcb_segment(&(guest_area->es), &(segs->es));
    v3_set_vmcb_segment(&(guest_area->fs), &(segs->fs));
    v3_set_vmcb_segment(&(guest_area->gs), &(segs->gs));
    v3_set_vmcb_segment(&(guest_area->ss), &(segs->ss));
    v3_set_vmcb_segment(&(guest_area->ldtr), &(segs->ldtr));
    v3_set_vmcb_segment(&(guest_area->gdtr), &(segs->gdtr));
    v3_set_vmcb_segment(&(guest_area->idtr), &(segs->idtr));
    v3_set_vmcb_segment(&(guest_area->tr), &(segs->tr));
}


void v3_get_vmcb_segments(vmcb_t * vmcb, struct v3_segments * segs) {
    vmcb_saved_state_t * guest_area = GET_VMCB_SAVE_STATE_AREA(vmcb);

    v3_get_vmcb_segment(&(guest_area->cs), &(segs->cs));
    v3_get_vmcb_segment(&(guest_area->ds), &(segs->ds));
    v3_get_vmcb_segment(&(guest_area->es), &(segs->es));
    v3_get_vmcb_segment(&(guest_area->fs), &(segs->fs));
    v3_get_vmcb_segment(&(guest_area->gs), &(segs->gs));
    v3_get_vmcb_segment(&(guest_area->ss), &(segs->ss));
    v3_get_vmcb_segment(&(guest_area->ldtr), &(segs->ldtr));
    v3_get_vmcb_segment(&(guest_area->gdtr), &(segs->gdtr));
    v3_get_vmcb_segment(&(guest_area->idtr), &(segs->idtr));
    v3_get_vmcb_segment(&(guest_area->tr), &(segs->tr));
}


void PrintDebugVMCB(vmcb_t * vmcb) {
    reg_ex_t tmp_reg;

    vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA(vmcb);
    vmcb_saved_state_t * guest_area = GET_VMCB_SAVE_STATE_AREA(vmcb);

    PrintDebug("VMCB (0x%p)\n", (void *)vmcb);

    PrintDebug("--Control Area--\n");
    PrintDebug("CR Reads: %x\n", *(ushort_t*)&(ctrl_area->cr_reads));
    PrintDebug("CR Writes: %x\n", *(ushort_t*)&(ctrl_area->cr_writes));
    PrintDebug("DR Reads: %x\n", *(ushort_t*)&(ctrl_area->dr_reads));
    PrintDebug("DR Writes: %x\n", *(ushort_t*)&(ctrl_area->dr_writes));
  
    PrintDebug("Exception Bitmap: %x (at 0x%p)\n", *(uint_t*)&(ctrl_area->exceptions), (void *)&(ctrl_area->exceptions));
    PrintDebug("\tDivide-by-Zero: %d\n", ctrl_area->exceptions.de);
    PrintDebug("\tDebug: %d\n", ctrl_area->exceptions.db);
    PrintDebug("\tNon-maskable interrupts: %d\n", ctrl_area->exceptions.nmi);
    PrintDebug("\tBreakpoint: %d\n", ctrl_area->exceptions.bp);
    PrintDebug("\tOverflow: %d\n", ctrl_area->exceptions.of);
    PrintDebug("\tBound-Range: %d\n", ctrl_area->exceptions.br);
    PrintDebug("\tInvalid Opcode: %d\n", ctrl_area->exceptions.ud);
    PrintDebug("\tDevice not available: %d\n", ctrl_area->exceptions.nm);
    PrintDebug("\tDouble Fault: %d\n", ctrl_area->exceptions.df);
    PrintDebug("\tInvalid TSS: %d\n", ctrl_area->exceptions.ts);
    PrintDebug("\tSegment not present: %d\n", ctrl_area->exceptions.np);
    PrintDebug("\tStack: %d\n", ctrl_area->exceptions.ss);
    PrintDebug("\tGPF: %d\n", ctrl_area->exceptions.gp);
    PrintDebug("\tPage Fault: %d\n", ctrl_area->exceptions.pf);
    PrintDebug("\tFloating Point: %d\n", ctrl_area->exceptions.mf);
    PrintDebug("\tAlignment Check: %d\n", ctrl_area->exceptions.ac);
    PrintDebug("\tMachine Check: %d\n", ctrl_area->exceptions.mc);
    PrintDebug("\tSIMD floating point: %d\n", ctrl_area->exceptions.xf);
    PrintDebug("\tSecurity: %d\n", ctrl_area->exceptions.sx);

    PrintDebug("Instructions bitmap: %.8x (at 0x%p)\n", *(uint_t*)&(ctrl_area->instrs), &(ctrl_area->instrs));
    PrintDebug("\tINTR: %d\n", ctrl_area->instrs.INTR);
    PrintDebug("\tNMI: %d\n", ctrl_area->instrs.NMI);
    PrintDebug("\tSMI: %d\n", ctrl_area->instrs.SMI);
    PrintDebug("\tINIT: %d\n", ctrl_area->instrs.INIT);
    PrintDebug("\tVINTR: %d\n", ctrl_area->instrs.VINTR);
    PrintDebug("\tCR0: %d\n", ctrl_area->instrs.CR0);
    PrintDebug("\tRD_IDTR: %d\n", ctrl_area->instrs.RD_IDTR);
    PrintDebug("\tRD_GDTR: %d\n", ctrl_area->instrs.RD_GDTR);
    PrintDebug("\tRD_LDTR: %d\n", ctrl_area->instrs.RD_LDTR);
    PrintDebug("\tRD_TR: %d\n", ctrl_area->instrs.RD_TR);
    PrintDebug("\tWR_IDTR: %d\n", ctrl_area->instrs.WR_IDTR);
    PrintDebug("\tWR_GDTR: %d\n", ctrl_area->instrs.WR_GDTR);
    PrintDebug("\tWR_LDTR: %d\n", ctrl_area->instrs.WR_LDTR);
    PrintDebug("\tWR_TR: %d\n", ctrl_area->instrs.WR_TR);
    PrintDebug("\tRDTSC: %d\n", ctrl_area->instrs.RDTSC);
    PrintDebug("\tRDPMC: %d\n", ctrl_area->instrs.RDPMC);
    PrintDebug("\tPUSHF: %d\n", ctrl_area->instrs.PUSHF);
    PrintDebug("\tPOPF: %d\n", ctrl_area->instrs.POPF);
    PrintDebug("\tCPUID: %d\n", ctrl_area->instrs.CPUID);
    PrintDebug("\tRSM: %d\n", ctrl_area->instrs.RSM);
    PrintDebug("\tIRET: %d\n", ctrl_area->instrs.IRET);
    PrintDebug("\tINTn: %d\n", ctrl_area->instrs.INTn);
    PrintDebug("\tINVD: %d\n", ctrl_area->instrs.INVD);
    PrintDebug("\tPAUSE: %d\n", ctrl_area->instrs.PAUSE);
    PrintDebug("\tHLT: %d\n", ctrl_area->instrs.HLT);
    PrintDebug("\tINVLPG: %d\n", ctrl_area->instrs.INVLPG);
    PrintDebug("\tINVLPGA: %d\n", ctrl_area->instrs.INVLPGA);
    PrintDebug("\tIOIO_PROT: %d\n", ctrl_area->instrs.IOIO_PROT);
    PrintDebug("\tMSR_PROT: %d\n", ctrl_area->instrs.MSR_PROT);
    PrintDebug("\ttask_switch: %d\n", ctrl_area->instrs.task_switch);
    PrintDebug("\tFERR_FREEZE: %d\n", ctrl_area->instrs.FERR_FREEZE);
    PrintDebug("\tshutdown_evts: %d\n", ctrl_area->instrs.shutdown_evts);

    PrintDebug("SVM Instruction Bitmap: %x (at 0x%p)\n", *(uint_t*)&(ctrl_area->svm_instrs), &(ctrl_area->svm_instrs));
    PrintDebug("\tVMRUN: %d\n", ctrl_area->svm_instrs.VMRUN);
    PrintDebug("\tVMMCALL: %d\n", ctrl_area->svm_instrs.VMMCALL);
    PrintDebug("\tVMLOAD: %d\n", ctrl_area->svm_instrs.VMLOAD);
    PrintDebug("\tVMSAVE: %d\n", ctrl_area->svm_instrs.VMSAVE);
    PrintDebug("\tSTGI: %d\n", ctrl_area->svm_instrs.STGI);
    PrintDebug("\tCLGI: %d\n", ctrl_area->svm_instrs.CLGI);
    PrintDebug("\tSKINIT: %d\n", ctrl_area->svm_instrs.SKINIT);
    PrintDebug("\tRDTSCP: %d\n", ctrl_area->svm_instrs.RDTSCP);
    PrintDebug("\tICEBP: %d\n", ctrl_area->svm_instrs.ICEBP);
    PrintDebug("\tWBINVD: %d\n", ctrl_area->svm_instrs.WBINVD);
    PrintDebug("\tMONITOR: %d\n", ctrl_area->svm_instrs.MONITOR);
    PrintDebug("\tMWAIT_always: %d\n", ctrl_area->svm_instrs.MWAIT_always);
    PrintDebug("\tMWAIT_if_armed: %d\n", ctrl_area->svm_instrs.MWAIT_if_armed);



    tmp_reg.r_reg = ctrl_area->IOPM_BASE_PA;
    PrintDebug("IOPM_BASE_PA: lo: 0x%x, hi: 0x%x\n", tmp_reg.e_reg.low, tmp_reg.e_reg.high);
    tmp_reg.r_reg = ctrl_area->MSRPM_BASE_PA;
    PrintDebug("MSRPM_BASE_PA: lo: 0x%x, hi: 0x%x\n", tmp_reg.e_reg.low, tmp_reg.e_reg.high);
    tmp_reg.r_reg = ctrl_area->TSC_OFFSET;
    PrintDebug("TSC_OFFSET: lo: 0x%x, hi: 0x%x\n", tmp_reg.e_reg.low, tmp_reg.e_reg.high);

    PrintDebug("guest_ASID: %d\n", ctrl_area->guest_ASID);
    PrintDebug("TLB_CONTROL: %d\n", ctrl_area->TLB_CONTROL);


    PrintDebug("Guest Control Bitmap: %x (at 0x%p)\n", *(uint_t*)&(ctrl_area->guest_ctrl), &(ctrl_area->guest_ctrl));
    PrintDebug("\tV_TPR: %d\n", ctrl_area->guest_ctrl.V_TPR);
    PrintDebug("\tV_IRQ: %d\n", ctrl_area->guest_ctrl.V_IRQ);
    PrintDebug("\tV_INTR_PRIO: %d\n", ctrl_area->guest_ctrl.V_INTR_PRIO);
    PrintDebug("\tV_IGN_TPR: %d\n", ctrl_area->guest_ctrl.V_IGN_TPR);
    PrintDebug("\tV_INTR_MASKING: %d\n", ctrl_area->guest_ctrl.V_INTR_MASKING);
    PrintDebug("\tV_INTR_VECTOR: %d\n", ctrl_area->guest_ctrl.V_INTR_VECTOR);

    PrintDebug("Interrupt_shadow: %d\n", ctrl_area->interrupt_shadow);


    tmp_reg.r_reg = ctrl_area->exit_code;
    PrintDebug("exit_code: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = ctrl_area->exit_info1;
    PrintDebug("exit_info1: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = ctrl_area->exit_info2;
    PrintDebug("exit_info2: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug("Exit Int Info: (at 0x%p)\n", &(ctrl_area->exit_int_info));
    PrintDebug("Vector: %d\n", ctrl_area->exit_int_info.vector);
    PrintDebug("(type=%d) (ev=%d) (valid=%d)\n", ctrl_area->exit_int_info.type, 
	       ctrl_area->exit_int_info.ev, ctrl_area->exit_int_info.valid);
    PrintDebug("Error Code: %d\n", ctrl_area->exit_int_info.error_code);


    tmp_reg.r_reg = ctrl_area->NP_ENABLE;
    PrintDebug("NP_ENABLE: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);

    PrintDebug("Event Injection: (at 0x%p)\n", &(ctrl_area->EVENTINJ));
    PrintDebug("Vector: %d\n", ctrl_area->EVENTINJ.vector);
    PrintDebug("(type=%d) (ev=%d) (valid=%d)\n", ctrl_area->EVENTINJ.type, 
	       ctrl_area->EVENTINJ.ev, ctrl_area->EVENTINJ.valid);
    PrintDebug("Error Code: %d\n", ctrl_area->EVENTINJ.error_code);


    tmp_reg.r_reg = ctrl_area->N_CR3;
    PrintDebug("N_CR3: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);

    PrintDebug("LBR_VIRTUALIZATION_ENABLE: %d\n", ctrl_area->LBR_VIRTUALIZATION_ENABLE);


    PrintDebug("\n--Guest Saved State--\n");

    PrintDebug("es Selector (at 0x%p): \n", &(guest_area->es));
    PrintDebug("\tSelector: %d\n", guest_area->es.selector); 
    PrintDebug("\t(type=%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->es.attrib.fields.type, guest_area->es.attrib.fields.S, 
	       guest_area->es.attrib.fields.dpl, guest_area->es.attrib.fields.P,
	       guest_area->es.attrib.fields.avl, guest_area->es.attrib.fields.L,
	       guest_area->es.attrib.fields.db, guest_area->es.attrib.fields.G);
    PrintDebug("\tlimit: %u\n", guest_area->es.limit);
    tmp_reg.r_reg = guest_area->es.base;
    PrintDebug("\tBase: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug("cs Selector (at 0x%p): \n", &(guest_area->cs));
    PrintDebug("\tSelector: %d\n", guest_area->cs.selector); 
    PrintDebug("\t(type=%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->cs.attrib.fields.type, guest_area->cs.attrib.fields.S, 
	       guest_area->cs.attrib.fields.dpl, guest_area->cs.attrib.fields.P,
	       guest_area->cs.attrib.fields.avl, guest_area->cs.attrib.fields.L,
	       guest_area->cs.attrib.fields.db, guest_area->cs.attrib.fields.G);
    PrintDebug("\tlimit: %u\n", guest_area->cs.limit);
    tmp_reg.r_reg = guest_area->cs.base;
    PrintDebug("\tBase: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug("ss Selector (at 0x%p): \n", &(guest_area->ss));
    PrintDebug("\tSelector: %d\n", guest_area->ss.selector); 
    PrintDebug("\t(type=%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->ss.attrib.fields.type, guest_area->ss.attrib.fields.S, 
	       guest_area->ss.attrib.fields.dpl, guest_area->ss.attrib.fields.P,
	       guest_area->ss.attrib.fields.avl, guest_area->ss.attrib.fields.L,
	       guest_area->ss.attrib.fields.db, guest_area->ss.attrib.fields.G);
    PrintDebug("\tlimit: %u\n", guest_area->ss.limit);
    tmp_reg.r_reg = guest_area->ss.base;
    PrintDebug("\tBase: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug("ds Selector (at 0x%p): \n", &(guest_area->ds));
    PrintDebug("\tSelector: %d\n", guest_area->ds.selector); 
    PrintDebug("\t(type=%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->ds.attrib.fields.type, guest_area->ds.attrib.fields.S, 
	       guest_area->ds.attrib.fields.dpl, guest_area->ds.attrib.fields.P,
	       guest_area->ds.attrib.fields.avl, guest_area->ds.attrib.fields.L,
	       guest_area->ds.attrib.fields.db, guest_area->ds.attrib.fields.G);
    PrintDebug("\tlimit: %u\n", guest_area->ds.limit);
    tmp_reg.r_reg = guest_area->ds.base;
    PrintDebug("\tBase: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug("fs Selector (at 0x%p): \n", &(guest_area->fs));
    PrintDebug("\tSelector: %d\n", guest_area->fs.selector); 
    PrintDebug("\t(type=%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->fs.attrib.fields.type, guest_area->fs.attrib.fields.S, 
	       guest_area->fs.attrib.fields.dpl, guest_area->fs.attrib.fields.P,
	       guest_area->fs.attrib.fields.avl, guest_area->fs.attrib.fields.L,
	       guest_area->fs.attrib.fields.db, guest_area->fs.attrib.fields.G);
    PrintDebug("\tlimit: %u\n", guest_area->fs.limit);
    tmp_reg.r_reg = guest_area->fs.base;
    PrintDebug("\tBase: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug("gs Selector (at 0x%p): \n", &(guest_area->gs));
    PrintDebug("\tSelector: %d\n", guest_area->gs.selector); 
    PrintDebug("\t(type=%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->gs.attrib.fields.type, guest_area->gs.attrib.fields.S, 
	       guest_area->gs.attrib.fields.dpl, guest_area->gs.attrib.fields.P,
	       guest_area->gs.attrib.fields.avl, guest_area->gs.attrib.fields.L,
	       guest_area->gs.attrib.fields.db, guest_area->gs.attrib.fields.G);
    PrintDebug("\tlimit: %u\n", guest_area->gs.limit);
    tmp_reg.r_reg = guest_area->gs.base;
    PrintDebug("\tBase: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug("gdtr Selector (at 0x%p): \n", &(guest_area->gdtr));
    PrintDebug("\tSelector: %d\n", guest_area->gdtr.selector); 
    PrintDebug("\t(type=%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->gdtr.attrib.fields.type, guest_area->gdtr.attrib.fields.S, 
	       guest_area->gdtr.attrib.fields.dpl, guest_area->gdtr.attrib.fields.P,
	       guest_area->gdtr.attrib.fields.avl, guest_area->gdtr.attrib.fields.L,
	       guest_area->gdtr.attrib.fields.db, guest_area->gdtr.attrib.fields.G);
    PrintDebug("\tlimit: %u\n", guest_area->gdtr.limit);
    tmp_reg.r_reg = guest_area->gdtr.base;
    PrintDebug("\tBase: hi: 0x%.8x, lo: 0x%.8x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug("ldtr Selector (at 0x%p): \n", &(guest_area->ldtr));
    PrintDebug("\tSelector: %d\n", guest_area->ldtr.selector); 
    PrintDebug("\t(type=%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->ldtr.attrib.fields.type, guest_area->ldtr.attrib.fields.S, 
	       guest_area->ldtr.attrib.fields.dpl, guest_area->ldtr.attrib.fields.P,
	       guest_area->ldtr.attrib.fields.avl, guest_area->ldtr.attrib.fields.L,
	       guest_area->ldtr.attrib.fields.db, guest_area->ldtr.attrib.fields.G);
    PrintDebug("\tlimit: %u\n", guest_area->ldtr.limit);
    tmp_reg.r_reg = guest_area->ldtr.base;
    PrintDebug("\tBase: hi: 0x%.8x, lo: 0x%.8x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug("idtr Selector (at 0x%p): \n", &(guest_area->idtr));
    PrintDebug("\tSelector: %d\n", guest_area->idtr.selector); 
    PrintDebug("\t(type=%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->idtr.attrib.fields.type, guest_area->idtr.attrib.fields.S, 
	       guest_area->idtr.attrib.fields.dpl, guest_area->idtr.attrib.fields.P,
	       guest_area->idtr.attrib.fields.avl, guest_area->idtr.attrib.fields.L,
	       guest_area->idtr.attrib.fields.db, guest_area->idtr.attrib.fields.G);
    PrintDebug("\tlimit: %u\n", guest_area->idtr.limit);
    tmp_reg.r_reg = guest_area->idtr.base;
    PrintDebug("\tBase: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug("tr Selector (at 0x%p): \n", &(guest_area->tr));
    PrintDebug("\tSelector: %d\n", guest_area->tr.selector); 
    PrintDebug("\t(type=%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->tr.attrib.fields.type, guest_area->tr.attrib.fields.S, 
	       guest_area->tr.attrib.fields.dpl, guest_area->tr.attrib.fields.P,
	       guest_area->tr.attrib.fields.avl, guest_area->tr.attrib.fields.L,
	       guest_area->tr.attrib.fields.db, guest_area->tr.attrib.fields.G);
    PrintDebug("\tlimit: %u\n", guest_area->tr.limit);
    tmp_reg.r_reg = guest_area->tr.base;
    PrintDebug("\tBase: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug("cpl: %d\n", guest_area->cpl);

  
    tmp_reg.r_reg = guest_area->efer;
    PrintDebug("EFER: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);

    tmp_reg.r_reg = guest_area->cr4;
    PrintDebug("CR4: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->cr3;
    PrintDebug("CR3: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->cr0;
    PrintDebug("CR0: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->dr7;
    PrintDebug("DR7: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->dr6;
    PrintDebug("DR6: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->rflags;
    PrintDebug("RFLAGS: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->rip;
    PrintDebug("RIP: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    tmp_reg.r_reg = guest_area->rsp;
    PrintDebug("RSP: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);

  
    tmp_reg.r_reg = guest_area->rax;
    PrintDebug("RAX: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->star;
    PrintDebug("STAR: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->lstar;
    PrintDebug("LSTAR: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->cstar;
    PrintDebug("CSTAR: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->sfmask;
    PrintDebug("SFMASK: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->KernelGsBase;
    PrintDebug("KernelGsBase: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->sysenter_cs;
    PrintDebug("sysenter_cs: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->sysenter_esp;
    PrintDebug("sysenter_esp: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->sysenter_eip;
    PrintDebug("sysenter_eip: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->cr2;
    PrintDebug("CR2: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);

    tmp_reg.r_reg = guest_area->g_pat;
    PrintDebug("g_pat: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->dbgctl;
    PrintDebug("dbgctl: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->br_from;
    PrintDebug("br_from: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->br_to;
    PrintDebug("br_to: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->lastexcpfrom;
    PrintDebug("lastexcpfrom: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->lastexcpto;
    PrintDebug("lastexcpto: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
}
