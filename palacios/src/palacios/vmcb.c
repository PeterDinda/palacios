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
	int i;
    reg_ex_t tmp_reg;

    vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA(vmcb);
    vmcb_saved_state_t * guest_area = GET_VMCB_SAVE_STATE_AREA(vmcb);

    PrintDebug(VM_NONE, VCORE_NONE, "VMCB (0x%p)\n", (void *)vmcb);

    PrintDebug(VM_NONE, VCORE_NONE, "--Control Area--\n");
    PrintDebug(VM_NONE, VCORE_NONE, "CR Reads: 0x%x\n", *(ushort_t*)&(ctrl_area->cr_reads));
    PrintDebug(VM_NONE, VCORE_NONE, "CR Writes: 0x%x\n", *(ushort_t*)&(ctrl_area->cr_writes));
    PrintDebug(VM_NONE, VCORE_NONE, "DR Reads: 0x%x\n", *(ushort_t*)&(ctrl_area->dr_reads));
    PrintDebug(VM_NONE, VCORE_NONE, "DR Writes: 0x%x\n", *(ushort_t*)&(ctrl_area->dr_writes));
  
    PrintDebug(VM_NONE, VCORE_NONE, "Exception Bitmap: 0x%x (at 0x%p)\n", *(uint_t*)&(ctrl_area->exceptions), (void *)&(ctrl_area->exceptions));
    PrintDebug(VM_NONE, VCORE_NONE, "   Divide-by-Zero: %d\n", ctrl_area->exceptions.de);
    PrintDebug(VM_NONE, VCORE_NONE, "   Debug: %d\n", ctrl_area->exceptions.db);
    PrintDebug(VM_NONE, VCORE_NONE, "   Non-maskable interrupts: %d\n", ctrl_area->exceptions.nmi);
    PrintDebug(VM_NONE, VCORE_NONE, "   Breakpoint: %d\n", ctrl_area->exceptions.bp);
    PrintDebug(VM_NONE, VCORE_NONE, "   Overflow: %d\n", ctrl_area->exceptions.of);
    PrintDebug(VM_NONE, VCORE_NONE, "   Bound-Range: %d\n", ctrl_area->exceptions.br);
    PrintDebug(VM_NONE, VCORE_NONE, "   Invalid Opcode: %d\n", ctrl_area->exceptions.ud);
    PrintDebug(VM_NONE, VCORE_NONE, "   Device not available: %d\n", ctrl_area->exceptions.nm);
    PrintDebug(VM_NONE, VCORE_NONE, "   Double Fault: %d\n", ctrl_area->exceptions.df);
    PrintDebug(VM_NONE, VCORE_NONE, "   Invalid TSS: %d\n", ctrl_area->exceptions.ts);
    PrintDebug(VM_NONE, VCORE_NONE, "   Segment not present: %d\n", ctrl_area->exceptions.np);
    PrintDebug(VM_NONE, VCORE_NONE, "   Stack: %d\n", ctrl_area->exceptions.ss);
    PrintDebug(VM_NONE, VCORE_NONE, "   GPF: %d\n", ctrl_area->exceptions.gp);
    PrintDebug(VM_NONE, VCORE_NONE, "   Page Fault: %d\n", ctrl_area->exceptions.pf);
    PrintDebug(VM_NONE, VCORE_NONE, "   Floating Point: %d\n", ctrl_area->exceptions.mf);
    PrintDebug(VM_NONE, VCORE_NONE, "   Alignment Check: %d\n", ctrl_area->exceptions.ac);
    PrintDebug(VM_NONE, VCORE_NONE, "   Machine Check: %d\n", ctrl_area->exceptions.mc);
    PrintDebug(VM_NONE, VCORE_NONE, "   SIMD floating point: %d\n", ctrl_area->exceptions.xf);
    PrintDebug(VM_NONE, VCORE_NONE, "   Security: %d\n", ctrl_area->exceptions.sx);

    PrintDebug(VM_NONE, VCORE_NONE, "Instructions bitmap: 0x%.8x (at 0x%p)\n", *(uint_t*)&(ctrl_area->instrs), &(ctrl_area->instrs));
    PrintDebug(VM_NONE, VCORE_NONE, "   INTR: %d\n", ctrl_area->instrs.INTR);
    PrintDebug(VM_NONE, VCORE_NONE, "   NMI: %d\n", ctrl_area->instrs.NMI);
    PrintDebug(VM_NONE, VCORE_NONE, "   SMI: %d\n", ctrl_area->instrs.SMI);
    PrintDebug(VM_NONE, VCORE_NONE, "   INIT: %d\n", ctrl_area->instrs.INIT);
    PrintDebug(VM_NONE, VCORE_NONE, "   VINTR: %d\n", ctrl_area->instrs.VINTR);
    PrintDebug(VM_NONE, VCORE_NONE, "   CR0: %d\n", ctrl_area->instrs.CR0);
    PrintDebug(VM_NONE, VCORE_NONE, "   RD_IDTR: %d\n", ctrl_area->instrs.RD_IDTR);
    PrintDebug(VM_NONE, VCORE_NONE, "   RD_GDTR: %d\n", ctrl_area->instrs.RD_GDTR);
    PrintDebug(VM_NONE, VCORE_NONE, "   RD_LDTR: %d\n", ctrl_area->instrs.RD_LDTR);
    PrintDebug(VM_NONE, VCORE_NONE, "   RD_TR: %d\n", ctrl_area->instrs.RD_TR);
    PrintDebug(VM_NONE, VCORE_NONE, "   WR_IDTR: %d\n", ctrl_area->instrs.WR_IDTR);
    PrintDebug(VM_NONE, VCORE_NONE, "   WR_GDTR: %d\n", ctrl_area->instrs.WR_GDTR);
    PrintDebug(VM_NONE, VCORE_NONE, "   WR_LDTR: %d\n", ctrl_area->instrs.WR_LDTR);
    PrintDebug(VM_NONE, VCORE_NONE, "   WR_TR: %d\n", ctrl_area->instrs.WR_TR);
    PrintDebug(VM_NONE, VCORE_NONE, "   RDTSC: %d\n", ctrl_area->instrs.RDTSC);
    PrintDebug(VM_NONE, VCORE_NONE, "   RDPMC: %d\n", ctrl_area->instrs.RDPMC);
    PrintDebug(VM_NONE, VCORE_NONE, "   PUSHF: %d\n", ctrl_area->instrs.PUSHF);
    PrintDebug(VM_NONE, VCORE_NONE, "   POPF: %d\n", ctrl_area->instrs.POPF);
    PrintDebug(VM_NONE, VCORE_NONE, "   CPUID: %d\n", ctrl_area->instrs.CPUID);
    PrintDebug(VM_NONE, VCORE_NONE, "   RSM: %d\n", ctrl_area->instrs.RSM);
    PrintDebug(VM_NONE, VCORE_NONE, "   IRET: %d\n", ctrl_area->instrs.IRET);
    PrintDebug(VM_NONE, VCORE_NONE, "   INTn: %d\n", ctrl_area->instrs.INTn);
    PrintDebug(VM_NONE, VCORE_NONE, "   INVD: %d\n", ctrl_area->instrs.INVD);
    PrintDebug(VM_NONE, VCORE_NONE, "   PAUSE: %d\n", ctrl_area->instrs.PAUSE);
    PrintDebug(VM_NONE, VCORE_NONE, "   HLT: %d\n", ctrl_area->instrs.HLT);
    PrintDebug(VM_NONE, VCORE_NONE, "   INVLPG: %d\n", ctrl_area->instrs.INVLPG);
    PrintDebug(VM_NONE, VCORE_NONE, "   INVLPGA: %d\n", ctrl_area->instrs.INVLPGA);
    PrintDebug(VM_NONE, VCORE_NONE, "   IOIO_PROT: %d\n", ctrl_area->instrs.IOIO_PROT);
    PrintDebug(VM_NONE, VCORE_NONE, "   MSR_PROT: %d\n", ctrl_area->instrs.MSR_PROT);
    PrintDebug(VM_NONE, VCORE_NONE, "   task_switch: %d\n", ctrl_area->instrs.task_switch);
    PrintDebug(VM_NONE, VCORE_NONE, "   FERR_FREEZE: %d\n", ctrl_area->instrs.FERR_FREEZE);
    PrintDebug(VM_NONE, VCORE_NONE, "   shutdown_evts: %d\n", ctrl_area->instrs.shutdown_evts);

    PrintDebug(VM_NONE, VCORE_NONE, "SVM Instruction Bitmap: %x (at 0x%p)\n", *(uint_t*)&(ctrl_area->svm_instrs), &(ctrl_area->svm_instrs));
    PrintDebug(VM_NONE, VCORE_NONE, "   VMRUN: %d\n", ctrl_area->svm_instrs.VMRUN);
    PrintDebug(VM_NONE, VCORE_NONE, "   VMMCALL: %d\n", ctrl_area->svm_instrs.VMMCALL);
    PrintDebug(VM_NONE, VCORE_NONE, "   VMLOAD: %d\n", ctrl_area->svm_instrs.VMLOAD);
    PrintDebug(VM_NONE, VCORE_NONE, "   VMSAVE: %d\n", ctrl_area->svm_instrs.VMSAVE);
    PrintDebug(VM_NONE, VCORE_NONE, "   STGI: %d\n", ctrl_area->svm_instrs.STGI);
    PrintDebug(VM_NONE, VCORE_NONE, "   CLGI: %d\n", ctrl_area->svm_instrs.CLGI);
    PrintDebug(VM_NONE, VCORE_NONE, "   SKINIT: %d\n", ctrl_area->svm_instrs.SKINIT);
    PrintDebug(VM_NONE, VCORE_NONE, "   RDTSCP: %d\n", ctrl_area->svm_instrs.RDTSCP);
    PrintDebug(VM_NONE, VCORE_NONE, "   ICEBP: %d\n", ctrl_area->svm_instrs.ICEBP);
    PrintDebug(VM_NONE, VCORE_NONE, "   WBINVD: %d\n", ctrl_area->svm_instrs.WBINVD);
    PrintDebug(VM_NONE, VCORE_NONE, "   MONITOR: %d\n", ctrl_area->svm_instrs.MONITOR);
    PrintDebug(VM_NONE, VCORE_NONE, "   MWAIT_always: %d\n", ctrl_area->svm_instrs.MWAIT_always);
    PrintDebug(VM_NONE, VCORE_NONE, "   MWAIT_if_armed: %d\n", ctrl_area->svm_instrs.MWAIT_if_armed);
    PrintDebug(VM_NONE, VCORE_NONE, "   XSETBV: %d\n", ctrl_area->svm_instrs.XSETBV);
    PrintDebug(VM_NONE, VCORE_NONE, "   Pause Filter Threshold: 0x%x\n", ctrl_area->pause_filter_threshold);
    PrintDebug(VM_NONE, VCORE_NONE, "   Pause Filter Count: 0x%x\n", ctrl_area->pause_filter_count);


    tmp_reg.r_reg = ctrl_area->IOPM_BASE_PA;
    PrintDebug(VM_NONE, VCORE_NONE, "IOPM_BASE_PA: lo: 0x%x, hi: 0x%x\n", tmp_reg.e_reg.low, tmp_reg.e_reg.high);
    tmp_reg.r_reg = ctrl_area->MSRPM_BASE_PA;
    PrintDebug(VM_NONE, VCORE_NONE, "MSRPM_BASE_PA: lo: 0x%x, hi: 0x%x\n", tmp_reg.e_reg.low, tmp_reg.e_reg.high);
    tmp_reg.r_reg = ctrl_area->TSC_OFFSET;
    PrintDebug(VM_NONE, VCORE_NONE, "TSC_OFFSET: lo: 0x%x, hi: 0x%x\n", tmp_reg.e_reg.low, tmp_reg.e_reg.high);

    PrintDebug(VM_NONE, VCORE_NONE, "guest_ASID: 0x%x\n", ctrl_area->guest_ASID);
    PrintDebug(VM_NONE, VCORE_NONE, "TLB_CONTROL: 0x%x\n", ctrl_area->TLB_CONTROL);

    PrintDebug(VM_NONE, VCORE_NONE, "Guest Control Bitmap: 0x%x (at %p)\n", *(uint_t*)&(ctrl_area->guest_ctrl), &(ctrl_area->guest_ctrl));
    PrintDebug(VM_NONE, VCORE_NONE, "   V_TPR: 0x%x\n", ctrl_area->guest_ctrl.V_TPR);
    PrintDebug(VM_NONE, VCORE_NONE, "   V_IRQ: %d\n", ctrl_area->guest_ctrl.V_IRQ);
    PrintDebug(VM_NONE, VCORE_NONE, "   V_INTR_PRIO: 0x%x\n", ctrl_area->guest_ctrl.V_INTR_PRIO);
    PrintDebug(VM_NONE, VCORE_NONE, "   V_IGN_TPR: %d\n", ctrl_area->guest_ctrl.V_IGN_TPR);
    PrintDebug(VM_NONE, VCORE_NONE, "   V_INTR_MASKING: %d\n", ctrl_area->guest_ctrl.V_INTR_MASKING);
    PrintDebug(VM_NONE, VCORE_NONE, "   V_INTR_VECTOR: 0x%x\n", ctrl_area->guest_ctrl.V_INTR_VECTOR);

    PrintDebug(VM_NONE, VCORE_NONE, "Interrupt_shadow: %d\n", ctrl_area->interrupt_shadow);


    tmp_reg.r_reg = ctrl_area->exit_code;
    PrintDebug(VM_NONE, VCORE_NONE, "exit_code: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = ctrl_area->exit_info1;
    PrintDebug(VM_NONE, VCORE_NONE, "exit_info1: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = ctrl_area->exit_info2;
    PrintDebug(VM_NONE, VCORE_NONE, "exit_info2: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug(VM_NONE, VCORE_NONE, "Exit Int Info: (at %p)\n", &(ctrl_area->exit_int_info));
    PrintDebug(VM_NONE, VCORE_NONE, "   Vector: 0x%x\n", ctrl_area->exit_int_info.vector);
    PrintDebug(VM_NONE, VCORE_NONE, "   (type=0x%x) (ev=0x%x) (valid=0x%x)\n", ctrl_area->exit_int_info.type, 
	       ctrl_area->exit_int_info.ev, ctrl_area->exit_int_info.valid);
    PrintDebug(VM_NONE, VCORE_NONE, "   Error Code: 0x%x\n", ctrl_area->exit_int_info.error_code);


    PrintDebug(VM_NONE, VCORE_NONE, "NP_ENABLE: %d\n",ctrl_area->NP_ENABLE);

    PrintDebug(VM_NONE, VCORE_NONE, "AVIC_APIC_BAR: 0x%llx\n", (uint64_t) ctrl_area->AVIC_APIC_BAR);

    PrintDebug(VM_NONE, VCORE_NONE, "Event Injection: (at %p)\n", &(ctrl_area->EVENTINJ));
    PrintDebug(VM_NONE, VCORE_NONE, "   Vector: 0x%x\n", ctrl_area->EVENTINJ.vector);
    PrintDebug(VM_NONE, VCORE_NONE, "   (type=0x%x) (ev=0x%x) (valid=0x%x)\n", ctrl_area->EVENTINJ.type, 
	       ctrl_area->EVENTINJ.ev, ctrl_area->EVENTINJ.valid);
    PrintDebug(VM_NONE, VCORE_NONE, "   Error Code: 0x%x\n", ctrl_area->EVENTINJ.error_code);


    tmp_reg.r_reg = ctrl_area->N_CR3;
    PrintDebug(VM_NONE, VCORE_NONE, "N_CR3: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);

    PrintDebug(VM_NONE, VCORE_NONE, "LBR_VIRTUALIZATION_ENABLE: %d\n", ctrl_area->LBR_VIRTUALIZATION_ENABLE);

    PrintDebug(VM_NONE, VCORE_NONE, "VMCB CLEAN BITS: 0x%x\n", ctrl_area->clean_bits);

    PrintDebug(VM_NONE, VCORE_NONE, "NRIP: 0x%llx\n", ctrl_area->nrip);
    
    PrintDebug(VM_NONE, VCORE_NONE, "Instruction (at %p)\n", &ctrl_area->num_ifetch_bytes);

    PrintDebug(VM_NONE, VCORE_NONE, "   num_ifetch_bytes=0x%x\n",ctrl_area->num_ifetch_bytes);
    PrintDebug(VM_NONE,VCORE_NONE, "   bytes=0x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
	       ctrl_area->ifetch_bytes[0],
	       ctrl_area->ifetch_bytes[1],
	       ctrl_area->ifetch_bytes[2],
	       ctrl_area->ifetch_bytes[3],
	       ctrl_area->ifetch_bytes[4],
	       ctrl_area->ifetch_bytes[5],
	       ctrl_area->ifetch_bytes[6],
	       ctrl_area->ifetch_bytes[7],
	       ctrl_area->ifetch_bytes[8],
	       ctrl_area->ifetch_bytes[9],
	       ctrl_area->ifetch_bytes[10],
	       ctrl_area->ifetch_bytes[11],
	       ctrl_area->ifetch_bytes[12],
	       ctrl_area->ifetch_bytes[13],
	       ctrl_area->ifetch_bytes[14]);

    PrintDebug(VM_NONE, VCORE_NONE, "AVIC_APIC_backing_page: 0x%llx\n", (uint64_t) ctrl_area->AVIC_APIC_backing_page);
    PrintDebug(VM_NONE, VCORE_NONE, "AVIC_logical_table: 0x%llx\n",  (uint64_t)ctrl_area->AVIC_logical_table);
    PrintDebug(VM_NONE, VCORE_NONE, "AVIC_PHYSICAL_MAX_INDEX: 0x%x\n", ctrl_area->AVIC_PHYSICAL_MAX_INDEX);
    PrintDebug(VM_NONE, VCORE_NONE, "AVIC_PHYSICAL_TABLE_PTR: 0x%llx\n",  (uint64_t)ctrl_area->AVIC_PHYSICAL_TABLE_PTR);


	for (i=0;i<40;i++) { 
		if (ctrl_area->rsvd1[i]) {
			PrintDebug(VM_NONE, VCORE_NONE, "control rsvd1[%d] has value 0x%x\n", i, ctrl_area->rsvd1[i]);
		}
	}

	for (i=0;i<3;i++) { 
		if (ctrl_area->rsvd2[i]) {
			PrintDebug(VM_NONE, VCORE_NONE, "control rsvd2[%d] has value 0x%x\n", i, ctrl_area->rsvd2[i]);
		}
	}

	if (ctrl_area->rsvd3) {
		PrintDebug(VM_NONE, VCORE_NONE, "control rsvd3 has value 0x%llx\n", (uint64_t) ctrl_area->rsvd3);
	}

	if (ctrl_area->rsvd4) {
		PrintDebug(VM_NONE, VCORE_NONE, "control rsvd4 has value 0x%llx\n", (uint64_t) ctrl_area->rsvd4);
	}

	if (ctrl_area->rsvd5) {
		PrintDebug(VM_NONE, VCORE_NONE, "control rsvd5 has value 0x%llx\n", (uint64_t) ctrl_area->rsvd5);
	}

	for (i=0;i<8;i++) { 
		if (ctrl_area->rsvd6[i]) {
			PrintDebug(VM_NONE, VCORE_NONE, "control rsvd6[%d] has value 0x%x\n", i, ctrl_area->rsvd6[i]);
		}
	}


	if (ctrl_area->rsvd7) {
		PrintDebug(VM_NONE, VCORE_NONE, "control rsvd7 has value 0x%llx\n", (uint64_t) ctrl_area->rsvd7);
	}

	if (ctrl_area->rsvd8) {
		PrintDebug(VM_NONE, VCORE_NONE, "control rsvd8 has value 0x%llx\n", (uint64_t) ctrl_area->rsvd8);
	}

	if (ctrl_area->rsvd9) {
		PrintDebug(VM_NONE, VCORE_NONE, "control rsvd9 has value 0x%llx\n", (uint64_t) ctrl_area->rsvd9);
	}

	if (ctrl_area->rsvd10) {
		PrintDebug(VM_NONE, VCORE_NONE, "control rsvd10 has value 0x%llx\n", (uint64_t) ctrl_area->rsvd10);
	}

	if (ctrl_area->rsvd11) {
		PrintDebug(VM_NONE, VCORE_NONE, "control rsvd11 has value 0x%llx\n", (uint64_t) ctrl_area->rsvd11);
	}

	if (ctrl_area->rsvd12) {
		PrintDebug(VM_NONE, VCORE_NONE, "control rsvd12 has value 0x%llx\n", (uint64_t) ctrl_area->rsvd12);
	}

	if (ctrl_area->rsvd13) {
		PrintDebug(VM_NONE, VCORE_NONE, "control rsvd13 has value 0x%llx\n", (uint64_t) ctrl_area->rsvd13);
	}


	for (i=0;i<VMCB_CTRL_AREA_SIZE-0x100;i++) { 
		if (ctrl_area->rsvd_tail[i]) {
			PrintDebug(VM_NONE, VCORE_NONE, "control reserved tail %d has value 0x%x\n", i, ctrl_area->rsvd_tail[i]);
		}
	}


    PrintDebug(VM_NONE, VCORE_NONE, "\n--Guest Saved State--\n");

    PrintDebug(VM_NONE, VCORE_NONE, "es Selector (at %p): \n", &(guest_area->es));
    PrintDebug(VM_NONE, VCORE_NONE, "   Selector: 0x%x\n", guest_area->es.selector); 
    PrintDebug(VM_NONE, VCORE_NONE, "   (type=0x%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->es.attrib.fields.type, guest_area->es.attrib.fields.S, 
	       guest_area->es.attrib.fields.dpl, guest_area->es.attrib.fields.P,
	       guest_area->es.attrib.fields.avl, guest_area->es.attrib.fields.L,
	       guest_area->es.attrib.fields.db, guest_area->es.attrib.fields.G);
    PrintDebug(VM_NONE, VCORE_NONE, "   limit: 0x%x\n", guest_area->es.limit);
    tmp_reg.r_reg = guest_area->es.base;
    PrintDebug(VM_NONE, VCORE_NONE, "   Base: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug(VM_NONE, VCORE_NONE, "cs Selector (at %p): \n", &(guest_area->cs));
    PrintDebug(VM_NONE, VCORE_NONE, "   Selector: 0x%x\n", guest_area->cs.selector); 
    PrintDebug(VM_NONE, VCORE_NONE, "   (type=0x%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->cs.attrib.fields.type, guest_area->cs.attrib.fields.S, 
	       guest_area->cs.attrib.fields.dpl, guest_area->cs.attrib.fields.P,
	       guest_area->cs.attrib.fields.avl, guest_area->cs.attrib.fields.L,
	       guest_area->cs.attrib.fields.db, guest_area->cs.attrib.fields.G);
    PrintDebug(VM_NONE, VCORE_NONE, "   limit: 0x%x\n", guest_area->cs.limit);
    tmp_reg.r_reg = guest_area->cs.base;
    PrintDebug(VM_NONE, VCORE_NONE, "   Base: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug(VM_NONE, VCORE_NONE, "ss Selector (at %p): \n", &(guest_area->ss));
    PrintDebug(VM_NONE, VCORE_NONE, "   Selector: 0x%x\n", guest_area->ss.selector); 
    PrintDebug(VM_NONE, VCORE_NONE, "   (type=0x%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->ss.attrib.fields.type, guest_area->ss.attrib.fields.S, 
	       guest_area->ss.attrib.fields.dpl, guest_area->ss.attrib.fields.P,
	       guest_area->ss.attrib.fields.avl, guest_area->ss.attrib.fields.L,
	       guest_area->ss.attrib.fields.db, guest_area->ss.attrib.fields.G);
    PrintDebug(VM_NONE, VCORE_NONE, "   limit: 0x%x\n", guest_area->ss.limit);
    tmp_reg.r_reg = guest_area->ss.base;
    PrintDebug(VM_NONE, VCORE_NONE, "   Base: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug(VM_NONE, VCORE_NONE, "ds Selector (at %p): \n", &(guest_area->ds));
    PrintDebug(VM_NONE, VCORE_NONE, "   Selector: 0x%x\n", guest_area->ds.selector); 
    PrintDebug(VM_NONE, VCORE_NONE, "   (type=0x%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->ds.attrib.fields.type, guest_area->ds.attrib.fields.S, 
	       guest_area->ds.attrib.fields.dpl, guest_area->ds.attrib.fields.P,
	       guest_area->ds.attrib.fields.avl, guest_area->ds.attrib.fields.L,
	       guest_area->ds.attrib.fields.db, guest_area->ds.attrib.fields.G);
    PrintDebug(VM_NONE, VCORE_NONE, "   limit: 0x%x\n", guest_area->ds.limit);
    tmp_reg.r_reg = guest_area->ds.base;
    PrintDebug(VM_NONE, VCORE_NONE, "   Base: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug(VM_NONE, VCORE_NONE, "fs Selector (at %p): \n", &(guest_area->fs));
    PrintDebug(VM_NONE, VCORE_NONE, "   Selector: 0x%x\n", guest_area->fs.selector); 
    PrintDebug(VM_NONE, VCORE_NONE, "   (type=0x%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->fs.attrib.fields.type, guest_area->fs.attrib.fields.S, 
	       guest_area->fs.attrib.fields.dpl, guest_area->fs.attrib.fields.P,
	       guest_area->fs.attrib.fields.avl, guest_area->fs.attrib.fields.L,
	       guest_area->fs.attrib.fields.db, guest_area->fs.attrib.fields.G);
    PrintDebug(VM_NONE, VCORE_NONE, "   limit: 0x%x\n", guest_area->fs.limit);
    tmp_reg.r_reg = guest_area->fs.base;
    PrintDebug(VM_NONE, VCORE_NONE, "   Base: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug(VM_NONE, VCORE_NONE, "gs Selector (at %p): \n", &(guest_area->gs));
    PrintDebug(VM_NONE, VCORE_NONE, "   Selector: 0x%x\n", guest_area->gs.selector); 
    PrintDebug(VM_NONE, VCORE_NONE, "   (type=0x%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->gs.attrib.fields.type, guest_area->gs.attrib.fields.S, 
	       guest_area->gs.attrib.fields.dpl, guest_area->gs.attrib.fields.P,
	       guest_area->gs.attrib.fields.avl, guest_area->gs.attrib.fields.L,
	       guest_area->gs.attrib.fields.db, guest_area->gs.attrib.fields.G);
    PrintDebug(VM_NONE, VCORE_NONE, "   limit: 0x%x\n", guest_area->gs.limit);
    tmp_reg.r_reg = guest_area->gs.base;
    PrintDebug(VM_NONE, VCORE_NONE, "   Base: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug(VM_NONE, VCORE_NONE, "gdtr Selector (at %p): \n", &(guest_area->gdtr));
    PrintDebug(VM_NONE, VCORE_NONE, "   Selector: 0x%x\n", guest_area->gdtr.selector); 
    PrintDebug(VM_NONE, VCORE_NONE, "   (type=0x%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->gdtr.attrib.fields.type, guest_area->gdtr.attrib.fields.S, 
	       guest_area->gdtr.attrib.fields.dpl, guest_area->gdtr.attrib.fields.P,
	       guest_area->gdtr.attrib.fields.avl, guest_area->gdtr.attrib.fields.L,
	       guest_area->gdtr.attrib.fields.db, guest_area->gdtr.attrib.fields.G);
    PrintDebug(VM_NONE, VCORE_NONE, "   limit: 0x%x\n", guest_area->gdtr.limit);
    tmp_reg.r_reg = guest_area->gdtr.base;
    PrintDebug(VM_NONE, VCORE_NONE, "   Base: hi: 0x%.8x, lo: 0x%.8x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug(VM_NONE, VCORE_NONE, "ldtr Selector (at %p): \n", &(guest_area->ldtr));
    PrintDebug(VM_NONE, VCORE_NONE, "   Selector: 0x%x\n", guest_area->ldtr.selector); 
    PrintDebug(VM_NONE, VCORE_NONE, "   (type=0x%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->ldtr.attrib.fields.type, guest_area->ldtr.attrib.fields.S, 
	       guest_area->ldtr.attrib.fields.dpl, guest_area->ldtr.attrib.fields.P,
	       guest_area->ldtr.attrib.fields.avl, guest_area->ldtr.attrib.fields.L,
	       guest_area->ldtr.attrib.fields.db, guest_area->ldtr.attrib.fields.G);
    PrintDebug(VM_NONE, VCORE_NONE, "   limit: 0x%x\n", guest_area->ldtr.limit);
    tmp_reg.r_reg = guest_area->ldtr.base;
    PrintDebug(VM_NONE, VCORE_NONE, "   Base: hi: 0x%.8x, lo: 0x%.8x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug(VM_NONE, VCORE_NONE, "idtr Selector (at %p): \n", &(guest_area->idtr));
    PrintDebug(VM_NONE, VCORE_NONE, "   Selector: 0x%x\n", guest_area->idtr.selector); 
    PrintDebug(VM_NONE, VCORE_NONE, "   (type=0x%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->idtr.attrib.fields.type, guest_area->idtr.attrib.fields.S, 
	       guest_area->idtr.attrib.fields.dpl, guest_area->idtr.attrib.fields.P,
	       guest_area->idtr.attrib.fields.avl, guest_area->idtr.attrib.fields.L,
	       guest_area->idtr.attrib.fields.db, guest_area->idtr.attrib.fields.G);
    PrintDebug(VM_NONE, VCORE_NONE, "   limit: 0x%x\n", guest_area->idtr.limit);
    tmp_reg.r_reg = guest_area->idtr.base;
    PrintDebug(VM_NONE, VCORE_NONE, "   Base: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug(VM_NONE, VCORE_NONE, "tr Selector (at %p): \n", &(guest_area->tr));
    PrintDebug(VM_NONE, VCORE_NONE, "   Selector: 0x%x\n", guest_area->tr.selector); 
    PrintDebug(VM_NONE, VCORE_NONE, "   (type=0x%x), (S=%d), (dpl=%d), (P=%d), (avl=%d), (L=%d), (db=%d), (G=%d)\n", 
	       guest_area->tr.attrib.fields.type, guest_area->tr.attrib.fields.S, 
	       guest_area->tr.attrib.fields.dpl, guest_area->tr.attrib.fields.P,
	       guest_area->tr.attrib.fields.avl, guest_area->tr.attrib.fields.L,
	       guest_area->tr.attrib.fields.db, guest_area->tr.attrib.fields.G);
    PrintDebug(VM_NONE, VCORE_NONE, "   limit: 0x%x\n", guest_area->tr.limit);
    tmp_reg.r_reg = guest_area->tr.base;
    PrintDebug(VM_NONE, VCORE_NONE, "   Base: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    PrintDebug(VM_NONE, VCORE_NONE, "cpl: %d\n", guest_area->cpl);

  
    tmp_reg.r_reg = guest_area->efer;
    PrintDebug(VM_NONE, VCORE_NONE, "EFER: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);

    tmp_reg.r_reg = guest_area->cr4;
    PrintDebug(VM_NONE, VCORE_NONE, "CR4: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->cr3;
    PrintDebug(VM_NONE, VCORE_NONE, "CR3: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->cr0;
    PrintDebug(VM_NONE, VCORE_NONE, "CR0: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->dr7;
    PrintDebug(VM_NONE, VCORE_NONE, "DR7: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->dr6;
    PrintDebug(VM_NONE, VCORE_NONE, "DR6: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->rflags;
    PrintDebug(VM_NONE, VCORE_NONE, "RFLAGS: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->rip;
    PrintDebug(VM_NONE, VCORE_NONE, "RIP: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);


    tmp_reg.r_reg = guest_area->rsp;
    PrintDebug(VM_NONE, VCORE_NONE, "RSP: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);

  
    tmp_reg.r_reg = guest_area->rax;
    PrintDebug(VM_NONE, VCORE_NONE, "RAX: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->star;
    PrintDebug(VM_NONE, VCORE_NONE, "STAR: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->lstar;
    PrintDebug(VM_NONE, VCORE_NONE, "LSTAR: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->cstar;
    PrintDebug(VM_NONE, VCORE_NONE, "CSTAR: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->sfmask;
    PrintDebug(VM_NONE, VCORE_NONE, "SFMASK: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->KernelGsBase;
    PrintDebug(VM_NONE, VCORE_NONE, "KernelGsBase: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->sysenter_cs;
    PrintDebug(VM_NONE, VCORE_NONE, "sysenter_cs: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->sysenter_esp;
    PrintDebug(VM_NONE, VCORE_NONE, "sysenter_esp: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->sysenter_eip;
    PrintDebug(VM_NONE, VCORE_NONE, "sysenter_eip: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->cr2;
    PrintDebug(VM_NONE, VCORE_NONE, "CR2: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);

    tmp_reg.r_reg = guest_area->g_pat;
    PrintDebug(VM_NONE, VCORE_NONE, "g_pat: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->dbgctl;
    PrintDebug(VM_NONE, VCORE_NONE, "dbgctl: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->br_from;
    PrintDebug(VM_NONE, VCORE_NONE, "br_from: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->br_to;
    PrintDebug(VM_NONE, VCORE_NONE, "br_to: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->lastexcpfrom;
    PrintDebug(VM_NONE, VCORE_NONE, "lastexcpfrom: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);
    tmp_reg.r_reg = guest_area->lastexcpto;
    PrintDebug(VM_NONE, VCORE_NONE, "lastexcpto: hi: 0x%x, lo: 0x%x\n", tmp_reg.e_reg.high, tmp_reg.e_reg.low);

	for (i=0;i<43;i++) { 
		if (guest_area->rsvd1[i]) {
			PrintDebug(VM_NONE, VCORE_NONE, "guest rsvd1[%d] has value 0x%x\n", i, guest_area->rsvd1[i]);
		}
	}

	if (guest_area->rsvd2) {
		PrintDebug(VM_NONE, VCORE_NONE, "guest rsvd2 has value 0x%llx\n", (uint64_t) guest_area->rsvd2);
	}

	for (i=0;i<112;i++) { 
		if (guest_area->rsvd3[i]) {
			PrintDebug(VM_NONE, VCORE_NONE, "guest rsvd3[%d] has value 0x%x\n", i, guest_area->rsvd3[i]);
		}
	}

	for (i=0;i<88;i++) { 
		if (guest_area->rsvd4[i]) {
			PrintDebug(VM_NONE, VCORE_NONE, "guest rsvd4[%d] has value 0x%x\n", i, guest_area->rsvd4[i]);
		}
	}

	for (i=0;i<24;i++) { 
		if (guest_area->rsvd5[i]) {
			PrintDebug(VM_NONE, VCORE_NONE, "guest rsvd5[%d] has value 0x%x\n", i, guest_area->rsvd5[i]);
		}
	}

	for (i=0;i<32;i++) { 
		if (guest_area->rsvd6[i]) {
			PrintDebug(VM_NONE, VCORE_NONE, "guest rsvd6[%d] has value 0x%x\n", i, guest_area->rsvd6[i]);
		}
	}


	for (i=VMCB_END_OFFSET; i<VMCB_TOTAL_SIZE; i++) {
		if (((uint8_t*)vmcb)[i]) {
			PrintDebug(VM_NONE, VCORE_NONE, "VMCB reserved offset %d has value 0x%x\n", i,((uint8_t*)vmcb)[i]);
		}
	}

}
