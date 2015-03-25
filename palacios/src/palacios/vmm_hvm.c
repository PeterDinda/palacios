/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2015, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author:  Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm_mem.h>
#include <palacios/vmm.h>
#include <palacios/vmm_util.h>
#include <palacios/vmm_emulator.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_hypercall.h>

#include <palacios/vmm_xml.h>

#include <palacios/vm_guest_mem.h>

#include <stdio.h>
#include <stdlib.h>

/*

  MEM     = Total size of memory in the GPA (in MB)
  ROS_MEM = Total size of memory for the ROS (in MB) (<RAM)

  GPAs [0,ROS_MEM) are what the ROS sees
  GPAs [ROS_MEM, MEM) are HRT only
  GPAS [0,MEM) are accessible by the HRT

  CORES   = Total number of cores in VM
  ROS_CORES = Total numbber of cores for the ROS

  Cores [0,ROS_CORES) are what the ROS sees
  Cores [ROS_CORES,CORES) are HRT only
  Cores [0,CORES) are accessible by the HRT

  In a Pal file:

  <files> 
    <file id="hrtelf" filename="hrtelf.o" />
  </files>

  <mem ... >RAM</mem>   (MB)  Note these are  
  <cores count="CORES" ...>   backward compatible

  <hvm enable="y">
    <ros cores="ROS_CORES" mem="ROS_MEM" /> (MB)
    <hrt file_id="hrtelf" /hrt>
  </hvm>

*/

#ifndef V3_CONFIG_DEBUG_HVM
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

int v3_init_hvm()
{
    PrintDebug(VM_NONE,VCORE_NONE, "hvm: init\n");
    return 0;
}

int v3_deinit_hvm()
{
    PrintDebug(VM_NONE,VCORE_NONE, "hvm: deinit\n");
    return 0;
}


static int hvm_hcall_handler(struct guest_info * core , hcall_id_t hcall_id, void * priv_data)
{
    V3_Print(core->vm_info,core, "hvm: received hypercall %x  rax=%llx rbx=%llx rcx=%llx\n",
	     hcall_id, core->vm_regs.rax, core->vm_regs.rbx, core->vm_regs.rcx);
    return 0;
}

#define CEIL_DIV(x,y) (((x)/(y)) + !!((x)%(y)))

int v3_init_hvm_vm(struct v3_vm_info *vm, struct v3_xml *config)
{
    v3_cfg_tree_t *hvm_config;
    v3_cfg_tree_t *ros_config;
    v3_cfg_tree_t *hrt_config;
    char *enable;
    char *ros_cores;
    char *ros_mem;
    char *hrt_file_id;

    PrintDebug(vm, VCORE_NONE, "hvm: vm init\n");

    /* 
       Defaults - all ROS
    */
    memset(&vm->hvm_state,0,sizeof(struct v3_vm_hvm));
    vm->hvm_state.is_hvm=0;
    vm->hvm_state.first_hrt_core=vm->num_cores;
    vm->hvm_state.first_hrt_gpa=vm->mem_size;

    if (!config || !(hvm_config=v3_cfg_subtree(config,"hvm"))) {
	PrintDebug(vm,VCORE_NONE,"hvm: no HVM configuration found (all HW is ROS)\n");
	goto out_ok;
    }
    
    if (!(enable=v3_cfg_val(hvm_config,"enable")) || strcasecmp(enable,"y")) {
	PrintDebug(vm,VCORE_NONE,"hvm: HVM configuration disabled (all HW is ROS)\n");
	goto out_ok;
    }

    if (!(ros_config=v3_cfg_subtree(hvm_config,"ros"))) { 
	PrintError(vm,VCORE_NONE,"hvm: HVM configuration without ROS block...\n");
	return -1;
    }
 
    if (!(ros_cores=v3_cfg_val(ros_config,"cores"))) { 
	PrintError(vm,VCORE_NONE,"hvm: ROS block without cores...\n");
	return -1;
    }
   
    vm->hvm_state.first_hrt_core = ((uint32_t)atoi(ros_cores));
    
    if (!(ros_mem=v3_cfg_val(ros_config,"mem"))) { 
	PrintError(vm,VCORE_NONE,"hvm: ROS block without mem...\n");
	return -1;
    }

    vm->hvm_state.first_hrt_gpa = ((uint64_t)atoi(ros_mem))*1024*1024;
	
    if (!(hrt_config=v3_cfg_subtree(hvm_config,"hrt"))) { 
	PrintError(vm,VCORE_NONE,"hvm: HVM configuration without HRT block...\n");
	return -1;
    }
 
    if (!(hrt_file_id=v3_cfg_val(hrt_config,"file_id"))) { 
	PrintError(vm,VCORE_NONE,"hvm: HRT block without file_id...\n");
	return -1;
    }

    vm->hvm_state.hrt_file = v3_cfg_get_file(vm,hrt_file_id);
    
    if (!vm->hvm_state.hrt_file) { 
	PrintError(vm,VCORE_NONE,"hvm: HRT block contains bad file_id (%s)\n",hrt_file_id);
	return -1;
    }

    if (v3_register_hypercall(vm, HVM_HCALL, 
			      hvm_hcall_handler, 0)) { 
	PrintError(vm,VCORE_NONE, "hvm: cannot register hypercall....\n");
	return -1;
    }

    // XXX sanity check config here

    vm->hvm_state.is_hvm=1;

 out_ok:
    if (vm->hvm_state.is_hvm) {
	V3_Print(vm,VCORE_NONE,"hvm: [ROS: cores 0..%u, mem 0..%p] [HRT: cores %u..%u, mem %p..%p, file_id=%s (tag %s)]\n",
		 vm->hvm_state.first_hrt_core-1,
		 (void*) vm->hvm_state.first_hrt_gpa-1,
		 vm->hvm_state.first_hrt_core,
		 vm->num_cores-1,
		 (void*) vm->hvm_state.first_hrt_gpa,
		 (void*)vm->mem_size-1,
		 hrt_file_id,
		 vm->hvm_state.hrt_file->tag);
    } else {
	V3_Print(vm,VCORE_NONE,"hvm: This is a pure ROS VM\n");
    }
    return 0;
    
}


int v3_deinit_hvm_vm(struct v3_vm_info *vm)
{
    PrintDebug(vm, VCORE_NONE, "hvm: HVM VM deinit\n");

    v3_remove_hypercall(vm,HVM_HCALL);

    return 0;
}

int v3_init_hvm_core(struct guest_info *core)
{
    memset(&core->hvm_state,0,sizeof(core->hvm_state));
    if (core->vm_info->hvm_state.is_hvm) { 
	if (core->vcpu_id >= core->vm_info->hvm_state.first_hrt_core) { 
	    core->hvm_state.is_hrt=1;
	}
    }
    return 0;
}

int v3_deinit_hvm_core(struct guest_info *core)
{
    PrintDebug(core->vm_info, VCORE_NONE, "hvm: HVM core deinit\n");

    return 0;
}


uint64_t v3_get_hvm_ros_memsize(struct v3_vm_info *vm)
{
    if (vm->hvm_state.is_hvm) { 
	return vm->hvm_state.first_hrt_gpa;
    } else {
	return vm->mem_size;
    }
}
uint64_t v3_get_hvm_hrt_memsize(struct v3_vm_info *vm)
{
    if (vm->hvm_state.is_hvm) { 
	return vm->mem_size - vm->hvm_state.first_hrt_gpa;
    } else {
	return 0;
    }
}

uint32_t v3_get_hvm_ros_cores(struct v3_vm_info *vm)
{
    if (vm->hvm_state.is_hvm) { 
	return vm->hvm_state.first_hrt_core;
    } else {
	return vm->num_cores;
    }
}

uint32_t v3_get_hvm_hrt_cores(struct v3_vm_info *vm)
{
    if (vm->hvm_state.is_hvm) { 
	return vm->num_cores - vm->hvm_state.first_hrt_core;
    } else {
	return 0;
    }
}


int v3_is_hvm_ros_mem_gpa(struct v3_vm_info *vm, addr_t gpa)
{
    if (vm->hvm_state.is_hvm) { 
	return gpa>=0 && gpa<vm->hvm_state.first_hrt_gpa;
    } else {
	return 1;
    }
}

int v3_is_hvm_hrt_mem_gpa(struct v3_vm_info *vm, addr_t gpa)
{
    if (vm->hvm_state.is_hvm) { 
	return gpa>=vm->hvm_state.first_hrt_gpa && gpa<vm->mem_size;
    } else {
	return 0;
    }
}

int v3_is_hvm_hrt_core(struct guest_info *core)
{
    return core->hvm_state.is_hrt;
}

int v3_is_hvm_ros_core(struct guest_info *core)
{
    return !core->hvm_state.is_hrt;
}

int      v3_hvm_should_deliver_ipi(struct guest_info *src, struct guest_info *dest)
{
    if (!src) {
	// ioapic or msi to apic
	return !dest->hvm_state.is_hrt;
    } else {
	// apic to apic
	return src->hvm_state.is_hrt || (!src->hvm_state.is_hrt && !dest->hvm_state.is_hrt) ;
    }
}

void     v3_hvm_find_apics_seen_by_core(struct guest_info *core, struct v3_vm_info *vm, 
					uint32_t *start_apic, uint32_t *num_apics)
{
    if (!core) { 
	// Seen from ioapic, msi, etc: 
	if (vm->hvm_state.is_hvm) {
	    // HVM VM shows only the ROS cores/apics to ioapic, msi, etc
	    *start_apic = 0;
	    *num_apics = vm->hvm_state.first_hrt_core;
	} else {
	    // Non-HVM shows all cores/APICs to apic, msi, etc.
	    *start_apic = 0;
	    *num_apics = vm->num_cores;
	}
    } else {
	// Seen from apic
	if (core->hvm_state.is_hrt) { 
	    // HRT core/apic sees all apics
	    // (this policy may change...)
	    *start_apic = 0;
	    *num_apics = vm->num_cores;
	} else {
	    // non-HRT core/apic sees only non-HRT cores/apics
	    *start_apic = 0 ;
	    *num_apics = vm->hvm_state.first_hrt_core;
	}
    }
}


static void get_null_int_handler_loc(struct v3_vm_info *vm, void **base, uint64_t *limit)
{
    *base = (void*) PAGE_ADDR(vm->mem_size - PAGE_SIZE);
    *limit = PAGE_SIZE;
}

extern v3_cpu_arch_t v3_mach_type;

extern void *v3_hvm_svm_null_int_handler_start;
extern void *v3_hvm_svm_null_int_handler_end;
extern void *v3_hvm_vmx_null_int_handler_start;
extern void *v3_hvm_vmx_null_int_handler_end;

static void write_null_int_handler(struct v3_vm_info *vm)
{
    void *base;
    uint64_t limit;
    void *data;
    uint64_t len;

    get_null_int_handler_loc(vm,&base,&limit);

    switch (v3_mach_type) {
#ifdef V3_CONFIG_SVM
	case V3_SVM_CPU:
	case V3_SVM_REV3_CPU:
	    data = (void*) &v3_hvm_svm_null_int_handler_start;
	    len = (void*) &v3_hvm_svm_null_int_handler_end - data;
	    break;
#endif
#if V3_CONFIG_VMX
	case V3_VMX_CPU:
	case V3_VMX_EPT_CPU:
	case V3_VMX_EPT_UG_CPU:
	    data = (void*) &v3_hvm_vmx_null_int_handler_start;
	    len = (void*) &v3_hvm_vmx_null_int_handler_end - data;
	    break;
#endif
	default:
	    PrintError(vm,VCORE_NONE,"hvm: cannot determine CPU type to select null interrupt handler...\n");
	    data = 0;
	    len = 0;
    }

    if (data) {
	v3_write_gpa_memory(&vm->cores[0],(addr_t)(base),len,(uint8_t*)data);
    }

    PrintDebug(vm,VCORE_NONE,"hvm: wrote null interrupt handler at %p (%llu bytes)\n",base,len);
}


static void get_idt_loc(struct v3_vm_info *vm, void **base, uint64_t *limit)
{
    *base = (void*) PAGE_ADDR(vm->mem_size - 2 * PAGE_SIZE);
    *limit = 16*256;
}

// default IDT entries (int and trap gates)
//
// Format is 16 bytes long:
//   16 offsetlo   => 0
//   16 selector   => (target code selector) => 0x8 // entry 1 of GDT
//    3 ist        => (stack) = 0 => current stack
//    5 reserved   => 0
//    4 type       => 0xe=>INT, 0xf=>TRAP 
//    1 reserved   => 0
//    2 dpl        => 0
//    1 present    => 1
//   16 offsetmid  => 0
//   32 offsethigh => 0   (total is a 64 bit offset)
//   32 reserved   => 0
//
// 00 00 | 08 00 | 00 | 8[typenybble] | offsetmid | offsethigh | reserved
// 
// Note little endian
//
static uint64_t idt64_trap_gate_entry_mask[2] = {  0x00008f0000080000, 0x0 } ;
static uint64_t idt64_int_gate_entry_mask[2] =  { 0x00008e0000080000, 0x0 };

static void write_idt(struct v3_vm_info *vm)
{
    void *base;
    uint64_t limit;
    void *handler;
    uint64_t handler_len;
    int i;
    uint64_t trap_gate[2];
    uint64_t int_gate[2];

    get_idt_loc(vm,&base,&limit);

    get_null_int_handler_loc(vm,&handler,&handler_len);

    memcpy(trap_gate,idt64_trap_gate_entry_mask,16);
    memcpy(int_gate,idt64_int_gate_entry_mask,16);

    if (handler) {
	// update the entries for the handler location
	uint8_t *mask;
	uint8_t *hand;
	
	hand = (uint8_t*) &handler;

	mask = (uint8_t *)trap_gate;
	memcpy(&(mask[0]),&(hand[0]),2); // offset low
	memcpy(&(mask[6]),&(hand[2]),2); // offset med
	memcpy(&(mask[8]),&(hand[4]),4); // offset high

	mask = (uint8_t *)int_gate;
	memcpy(&(mask[0]),&(hand[0]),2); // offset low
	memcpy(&(mask[6]),&(hand[2]),2); // offset med
	memcpy(&(mask[8]),&(hand[4]),4); // offset high

	PrintDebug(vm,VCORE_NONE,"hvm: Adding default null trap and int gates\n");
    }

    for (i=0;i<32;i++) { 
	v3_write_gpa_memory(&vm->cores[0],(addr_t)(base+i*16),16,(uint8_t*)trap_gate);
    }

    for (i=32;i<256;i++) { 
	v3_write_gpa_memory(&vm->cores[0],(addr_t)(base+i*16),16,(uint8_t*)int_gate);
    }

    PrintDebug(vm,VCORE_NONE,"hvm: wrote IDT at %p\n",base);
}



static void get_gdt_loc(struct v3_vm_info *vm, void **base, uint64_t *limit)
{
    *base = (void*)PAGE_ADDR(vm->mem_size - 3 * PAGE_SIZE);
    *limit = 8*3;
}

static uint64_t gdt64[3] = {
    0x0000000000000000, /* null */
    0x00a09a0000000000, /* code (note lme bit) */
    0x00a0920000000000, /* data (most entries don't matter) */
};

static void write_gdt(struct v3_vm_info *vm)
{
    void *base;
    uint64_t limit;

    get_gdt_loc(vm,&base,&limit);
    v3_write_gpa_memory(&vm->cores[0],(addr_t)base,limit,(uint8_t*) gdt64);

    PrintDebug(vm,VCORE_NONE,"hvm: wrote GDT at %p\n",base);
}



static void get_tss_loc(struct v3_vm_info *vm, void **base, uint64_t *limit)
{
    *base = (void*)PAGE_ADDR(vm->mem_size - 4 * PAGE_SIZE);
    *limit = PAGE_SIZE;
}

static uint64_t tss_data=0x0;

static void write_tss(struct v3_vm_info *vm)
{
    void *base;
    uint64_t limit;
    int i;

    get_tss_loc(vm,&base,&limit);
    for (i=0;i<limit/8;i++) {
	v3_write_gpa_memory(&vm->cores[0],(addr_t)(base+8*i),8,(uint8_t*) &tss_data);
    }

    PrintDebug(vm,VCORE_NONE,"hvm: wrote TSS at %p\n",base);
}

/*
  PTS MAP FIRST 512 GB identity mapped: 
  1 second level
     512 entries
  1 top level
     1 entries
*/

static void get_pt_loc(struct v3_vm_info *vm, void **base, uint64_t *limit)
{
    *base = (void*)PAGE_ADDR(vm->mem_size-(5+1)*PAGE_SIZE);
    *limit =  2*PAGE_SIZE;
}

static void write_pt(struct v3_vm_info *vm)
{
    void *base;
    uint64_t size;
    struct pml4e64 pml4e;
    struct pdpe64 pdpe;
    uint64_t i;

    get_pt_loc(vm,&base, &size);
    if (size!=2*PAGE_SIZE) { 
	PrintError(vm,VCORE_NONE,"Cannot support pt request, defaulting\n");
    }

    memset(&pdpe,0,sizeof(pdpe));
    pdpe.present=1;
    pdpe.writable=1;
    pdpe.large_page=1;
    
    for (i=0;i<512;i++) {
	pdpe.pd_base_addr = i*0x40000;  // 0x4000 = 256K pages = 1 GB
	v3_write_gpa_memory(&vm->cores[0],(addr_t)(base+PAGE_SIZE+i*sizeof(pdpe)),sizeof(pdpe),(uint8_t*)&pdpe);
    }

    memset(&pml4e,0,sizeof(pml4e));
    pml4e.present=1;
    pml4e.writable=1;
    pml4e.pdp_base_addr = PAGE_BASE_ADDR((addr_t)(base+PAGE_SIZE));

    v3_write_gpa_memory(&vm->cores[0],(addr_t)base,sizeof(pml4e),(uint8_t*)&pml4e);    

    for (i=1;i<512;i++) {
	pml4e.present=0;
	v3_write_gpa_memory(&vm->cores[0],(addr_t)(base+i*sizeof(pml4e)),sizeof(pml4e),(uint8_t*)&pml4e);
    }

    PrintDebug(vm,VCORE_NONE,"hvm: Wrote page tables (1 PML4, 1 PDPE) at %p\n",base);
}

static void get_bp_loc(struct v3_vm_info *vm, void **base, uint64_t *limit)
{
    *base = (void*) PAGE_ADDR(vm->mem_size-(6+1)*PAGE_SIZE);
    *limit =  PAGE_SIZE;
}

static void write_bp(struct v3_vm_info *vm)
{
    void *base;
    uint64_t limit;
    uint64_t data=-1;
    int i;

    get_bp_loc(vm,&base,&limit);
    
    for (i=0;i<limit/8;i++) { 
	v3_write_gpa_memory(&vm->cores[0],(addr_t)(base+i*8),8,(uint8_t*)&data);
    }

    PrintDebug(vm,VCORE_NONE,"hvm: wrote boundary page at %p\n", base);
    
}

#define MIN_STACK (4096*4)


static void get_hrt_loc(struct v3_vm_info *vm, void **base, uint64_t *limit)
{
    void *bp_base;
    uint64_t bp_limit;
    
    get_bp_loc(vm,&bp_base,&bp_limit);
    
    // assume at least a minimal stack

    bp_base-=MIN_STACK;

    *base = (void*)PAGE_ADDR(vm->hvm_state.first_hrt_gpa);

    if (bp_base < *base+PAGE_SIZE) { 
	PrintError(vm,VCORE_NONE,"hvm: HRT stack colides with HRT\n");
    }

    *limit = bp_base - *base;
}

static void write_hrt(struct v3_vm_info *vm)
{
    void *base;
    uint64_t limit;

    get_hrt_loc(vm,&base,&limit);
    
    if (vm->hvm_state.hrt_file->size > limit) { 
	PrintError(vm,VCORE_NONE,"hvm: Cannot map HRT because it is too big (%llu bytes, but only have %llu space\n", vm->hvm_state.hrt_file->size, (uint64_t)limit);
	return;
    }

    v3_write_gpa_memory(&vm->cores[0],(addr_t)base,vm->hvm_state.hrt_file->size,vm->hvm_state.hrt_file->data);

    PrintDebug(vm,VCORE_NONE,"hvm: wrote HRT %s at %p\n", vm->hvm_state.hrt_file->tag,base);
    
}


	

/*
  GPA layout:

  HRT
  ---
  ROS

  We do not touch the ROS portion of the address space.
  The HRT portion looks like:

  INT_HANDLER (1 page - page aligned)
  IDT (1 page - page aligned)
  GDT (1 page - page aligned)
  TSS (1 page - page asligned)
  PAGETABLES  (identy map of first N GB)
     ROOT PT first, followed by 2nd level, etc.
     Currently PML4 followed by 1 PDPE for 512 GB of mapping
  BOUNDARY PAGE (all 0xff - avoid smashing page tables in case we keep going...)
  (stack - we will push machine description)
  ...
  HRT (as many pages as needed, page-aligned, starting at first HRT address)
  ---
  ROS
      
*/


int v3_setup_hvm_vm_for_boot(struct v3_vm_info *vm)
{
    if (!vm->hvm_state.is_hvm) { 
	PrintDebug(vm,VCORE_NONE,"hvm: skipping HVM setup for boot as this is not an HVM\n");
	return 0;
    }

    PrintDebug(vm,VCORE_NONE,"hvm: setup of HVM memory begins\n");

    write_null_int_handler(vm);
    write_idt(vm);
    write_gdt(vm);
    write_tss(vm);

    write_pt(vm);

    write_bp(vm);
    
    write_hrt(vm);


    PrintDebug(vm,VCORE_NONE,"hvm: setup of HVM memory done\n");

    return 0;
}

/*
  On entry:

   IDTR points to stub IDT
   GDTR points to stub GDT
   TS   points to stub TSS
   CR3 points to root page table
   CR0 has PE and PG
   EFER has LME AND LMA
   RSP is TOS (looks like a call)
       INFO                     <= RDI
       0 (fake return address)  <= RSP
       
   RIP is entry point to HRT
   RDI points to machine info on stack

   Other regs are zeroed

   shadow/nested paging state reset for long mode

*/
int v3_setup_hvm_hrt_core_for_boot(struct guest_info *core)
{
    void *base;
    uint64_t limit;

    if (!core->hvm_state.is_hrt) { 
	PrintDebug(core->vm_info,core,"hvm: skipping HRT setup for core %u as it is not an HRT core\n", core->vcpu_id);
	return 0;
    }

    PrintDebug(core->vm_info, core, "hvm: setting up HRT core (%u) for boot\n", core->vcpu_id);

    
    memset(&core->vm_regs,0,sizeof(core->vm_regs));
    memset(&core->ctrl_regs,0,sizeof(core->ctrl_regs));
    memset(&core->dbg_regs,0,sizeof(core->dbg_regs));
    memset(&core->segments,0,sizeof(core->segments));    
    memset(&core->msrs,0,sizeof(core->msrs));    
    memset(&core->fp_state,0,sizeof(core->fp_state));    

    // We are in long mode with virtual memory and we want
    // to start immediatley
    core->cpl = 0; // we are going right into the kernel
    core->cpu_mode = LONG;
    core->mem_mode = VIRTUAL_MEM; 
    core->core_run_state = CORE_RUNNING ;

    // We are going to enter right into the HRT
    // HRT stack and argument passing
    get_bp_loc(core->vm_info, &base,&limit);
    // TODO: push description here
    core->vm_regs.rsp = (v3_reg_t) base;  // so if we ret, we will blow up
    core->vm_regs.rbp = (v3_reg_t) base; 
    // TODO: RDI should really get pointer to description
    core->vm_regs.rdi = (v3_reg_t) base;
    // HRT entry point
    get_hrt_loc(core->vm_info, &base,&limit);
    core->rip = (uint64_t) base + 0x40; // hack for test.o

    // Setup CRs for long mode and our stub page table
    // CR0: PG, PE
    core->ctrl_regs.cr0 = 0x80000001;
    // CR2: don't care (output from #PF)
    // CE3: set to our PML4E, without setting PCD or PWT
    get_pt_loc(core->vm_info, &base,&limit);
    core->ctrl_regs.cr3 = PAGE_ADDR((addr_t)base);
    // CR4: PGE, PAE, PSE (last byte: 1 0 1 1 0 0 0 0)
    core->ctrl_regs.cr4 = 0xb0;
    // CR8 as usual
    // RFLAGS zeroed is fine: come in with interrupts off
    // EFER needs SVME LMA LME (last 16 bites: 0 0 0 1 0 1 0 1 0 0 0 0 0 0 0 0
    core->ctrl_regs.efer = 0x1500;


    /* 
       Notes on selectors:

       selector is 13 bits of index, 1 bit table indicator 
       (0=>GDT), 2 bit RPL
       
       index is scaled by 8, even in long mode, where some entries 
       are 16 bytes long.... 
          -> code, data descriptors have 8 byte format
             because base, limit, etc, are ignored (no segmentation)
          -> interrupt/trap gates have 16 byte format 
             because offset needs to be 64 bits
    */
    
    // Install our stub IDT
    get_idt_loc(core->vm_info, &base,&limit);
    core->segments.idtr.selector = 0;  // entry 0 (NULL) of the GDT
    core->segments.idtr.base = (addr_t) base;
    core->segments.idtr.limit = limit-1;
    core->segments.idtr.type = 0xe;
    core->segments.idtr.system = 1; 
    core->segments.idtr.dpl = 0;
    core->segments.idtr.present = 1;
    core->segments.idtr.long_mode = 1;

    // Install our stub GDT
    get_gdt_loc(core->vm_info, &base,&limit);
    core->segments.gdtr.selector = 0;
    core->segments.gdtr.base = (addr_t) base;
    core->segments.gdtr.limit = limit-1;
    core->segments.gdtr.type = 0x6;
    core->segments.gdtr.system = 1; 
    core->segments.gdtr.dpl = 0;
    core->segments.gdtr.present = 1;
    core->segments.gdtr.long_mode = 1;
    
    // And our TSS
    get_tss_loc(core->vm_info, &base,&limit);
    core->segments.tr.selector = 0;
    core->segments.tr.base = (addr_t) base;
    core->segments.tr.limit = limit-1;
    core->segments.tr.type = 0x6;
    core->segments.tr.system = 1; 
    core->segments.tr.dpl = 0;
    core->segments.tr.present = 1;
    core->segments.tr.long_mode = 1;
    
    base = 0x0;
    limit = -1;

    // And CS
    core->segments.cs.selector = 0x8 ; // entry 1 of GDT (RPL=0)
    core->segments.cs.base = (addr_t) base;
    core->segments.cs.limit = limit;
    core->segments.cs.type = 0xe;
    core->segments.cs.system = 0; 
    core->segments.cs.dpl = 0;
    core->segments.cs.present = 1;
    core->segments.cs.long_mode = 1;

    // DS, SS, etc are identical
    core->segments.ds.selector = 0x10; // entry 2 of GDT (RPL=0)
    core->segments.ds.base = (addr_t) base;
    core->segments.ds.limit = limit;
    core->segments.ds.type = 0x6;
    core->segments.ds.system = 0; 
    core->segments.ds.dpl = 0;
    core->segments.ds.present = 1;
    core->segments.ds.long_mode = 1;
    
    memcpy(&core->segments.ss,&core->segments.ds,sizeof(core->segments.ds));
    memcpy(&core->segments.es,&core->segments.ds,sizeof(core->segments.ds));
    memcpy(&core->segments.fs,&core->segments.ds,sizeof(core->segments.ds));
    memcpy(&core->segments.gs,&core->segments.ds,sizeof(core->segments.ds));
    
    // reset paging here for shadow... 

    if (core->shdw_pg_mode != NESTED_PAGING) { 
	PrintError(core->vm_info, core, "hvm: shadow paging guest... this will end badly\n");
    }


    return 0;
}
