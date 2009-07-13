/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Peter Dinda <pdinda@northwestern.edu> 
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu>
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *         Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <palacios/vmx.h>
#include <palacios/vmcs.h>
#include <palacios/vmm.h>
#include <palacios/vmx_lowlevel.h>


// 
// 
// CRUFT
//
//



#include <palacios/vmm_util.h>
#include <palacios/vmm_string.h>
#include <palacios/vmm_ctrl_regs.h>



extern int Launch_VM(ullong_t vmcsPtr, uint_t eip);

#define NUMPORTS 65536


#define VMXASSIST_INFO_PORT   0x0e9
#define ROMBIOS_PANIC_PORT    0x400
#define ROMBIOS_PANIC_PORT2   0x401
#define ROMBIOS_INFO_PORT     0x402
#define ROMBIOS_DEBUG_PORT    0x403



static uint_t GetLinearIP(struct VM * vm) {
  if (vm->state == VM_VMXASSIST_V8086_BIOS || vm->state == VM_VMXASSIST_V8086) { 
    return vm->vmcs.guestStateArea.cs.baseAddr + vm->vmcs.guestStateArea.rip;
  } else {
    return vm->vmcs.guestStateArea.rip;
  }
}




#define MAX_CODE 512
#define INSTR_OFFSET_START 17
#define NOP_SEQ_LEN        10
#define INSTR_OFFSET_END   (INSTR_OFFSET_START + NOP_SEQ_LEN - 1)
#define TEMPLATE_CODE_LEN  35

uint_t oldesp = 0;
uint_t myregs = 0;






extern uint_t VMCS_LAUNCH();
extern uint_t Init_VMCS_HostState();
extern uint_t Init_VMCS_GuestState();




extern int Get_CR2();
extern int vmRunning;





void DecodeCurrentInstruction(struct VM *vm, struct Instruction *inst)
{
  // this is a gruesome hack
  uint_t address = GetLinearIP(vm);
  uint_t length = vm->vmcs.exitInfoFields.instrLength;
  unsigned char *t = (unsigned char *) address;


  
  PrintTrace("DecodeCurrentInstruction: instruction is\n");
  PrintTraceMemDump(t,length);
  
  if (length==3 && t[0]==0x0f && t[1]==0x22 && t[2]==0xc0) { 
    // mov from eax to cr0
    // usually used to signal
    inst->type=VM_MOV_TO_CR0;
    inst->address=address;
    inst->size=length;
    inst->input1=vm->registers.eax;
    inst->input2=vm->vmcs.guestStateArea.cr0;
    inst->output=vm->registers.eax;
    PrintTrace("MOV FROM EAX TO CR0\n");
  } else {
    inst->type=VM_UNKNOWN_INST;
  }
}


static void setup_v8086_mode_for_boot(struct guest_info* vm_info)
{

    ((struct vmx_data*)vm_info->vmm_data)->state = VMXASSIST_V8086_BIOS;
    ((struct rflags)info->ctrl_regs.rflags).vm = 1;
    ((struct rflags)info->ctrl_regs.rflags).iopl = 3;


    vm_info->rip = 0xfff0;

    vm_info->segments.cs.selector = 0xf000;
    vm_info->segments.cs.base = 0xf000<<4;
    vm_info->segments.cs.limit = 0xffff;
    vm_info->segments.cs.type = 3;
    vm_info->segments.cs.system = 1;
    vm_info->segments.cs.dpl = 3;
    vm_info->segments.cs.present = 1;
    vm_info->segments.cs.granularity = 0;

    vm_info->segments.ss.selector = 0x0000;
    vm_info->segments.ss.base = 0x0000<<4;
    vm_info->segments.ss.limit = 0xffff;
    vm_info->segments.ss.type = 3;
    vm_info->segments.ss.system = 1;
    vm_info->segments.ss.dpl = 3;
    vm_info->segments.ss.present = 1;
    vm_info->segments.ss.granularity = 0;

    vm_info->segments.es.selector = 0x0000;
    vm_info->segments.es.base = 0x0000<<4;
    vm_info->segments.es.limit = 0xffff;
    vm_info->segments.es.type = 3;
    vm_info->segments.es.system = 1;
    vm_info->segments.es.dpl = 3;
    vm_info->segments.es.present = 1;
    vm_info->segments.es.granularity = 0;

    vm_info->segments.fs.selector = 0x0000;
    vm_info->segments.fs.base = 0x0000<<4;
    vm_info->segments.fs.limit = 0xffff;
    vm_info->segments.fs.type = 3;
    vm_info->segments.fs.system = 1;
    vm_info->segments.fs.dpl = 3;
    vm_info->segments.fs.present = 1;
    vm_info->segments.fs.granularity = 0;

    vm_info->segments.gs.selector = 0x0000;
    vm_info->segments.gs.base = 0x0000<<4;
    vm_info->segments.gs.limit = 0xffff;
    vm_info->segments.gs.type = 3;
    vm_info->segments.gs.system = 1;
    vm_info->segments.gs.dpl = 3;
    vm_info->segments.gs.present = 1;
    vm_info->segments.gs.granularity = 0;
}

static void ConfigureExits(struct VM *vm)
{
  CopyOutVMCSExecCtrlFields(&(vm->vmcs.execCtrlFields));

  vm->vmcs.execCtrlFields.pinCtrls |= 0 
    // EXTERNAL_INTERRUPT_EXITING 
    | NMI_EXITING;
  vm->vmcs.execCtrlFields.procCtrls |= 0
      // INTERRUPT_WINDOWS_EXIT 
      | USE_TSC_OFFSETTING
      | HLT_EXITING  
      | INVLPG_EXITING           
      | MWAIT_EXITING            
      | RDPMC_EXITING           
      | RDTSC_EXITING         
      | MOVDR_EXITING         
      | UNCONDITION_IO_EXITING
      | MONITOR_EXITING       
      | PAUSE_EXITING         ;

  CopyInVMCSExecCtrlFields(&(vm->vmcs.execCtrlFields));
  
  CopyOutVMCSExitCtrlFields(&(vm->vmcs.exitCtrlFields));

  vm->vmcs.exitCtrlFields.exitCtrls |= ACK_IRQ_ON_EXIT;
  
  CopyInVMCSExitCtrlFields(&(vm->vmcs.exitCtrlFields));


/*   VMCS_READ(VM_EXIT_CTRLS, &flags); */
/*   flags |= ACK_IRQ_ON_EXIT; */
/*   VMCS_WRITE(VM_EXIT_CTRLS, &flags); */
}


extern int RunVMM();
extern int SAFE_VM_LAUNCH();

int MyLaunch(struct VM *vm)
{
  ullong_t vmcs = (ullong_t)((uint_t) (vm->vmcsregion));
  uint_t entry_eip = vm->descriptor.entry_ip;
  uint_t exit_eip = vm->descriptor.exit_eip;
  uint_t guest_esp = vm->descriptor.guest_esp;
  uint_t f = 0xffffffff;
  uint_t tmpReg = 0;
  int ret;
  int vmm_ret = 0;

  PrintTrace("Guest ESP: 0x%x (%u)\n", guest_esp, guest_esp);

  exit_eip = (uint_t)RunVMM;

  PrintTrace("Clear\n");
  VMCS_CLEAR(vmcs);
  PrintTrace("Load\n");
  VMCS_LOAD(vmcs);


  PrintTrace("VMCS_LINK_PTR\n");
  VMCS_WRITE(VMCS_LINK_PTR, &f);
  PrintTrace("VMCS_LINK_PTR_HIGH\n");
  VMCS_WRITE(VMCS_LINK_PTR_HIGH, &f);

 
  SetCtrlBitsCorrectly(IA32_VMX_PINBASED_CTLS_MSR, PIN_VM_EXEC_CTRLS);
  SetCtrlBitsCorrectly(IA32_VMX_PROCBASED_CTLS_MSR, PROC_VM_EXEC_CTRLS);
  SetCtrlBitsCorrectly(IA32_VMX_EXIT_CTLS_MSR, VM_EXIT_CTRLS);
  SetCtrlBitsCorrectly(IA32_VMX_ENTRY_CTLS_MSR, VM_ENTRY_CTRLS);

  //
  //
  //SetCtrlBitsCorrectly(IA32_something,GUEST_IA32_DEBUGCTL);
  //SetCtrlBitsCorrectly(IA32_something,GUEST_IA32_DEBUGCTL_HIGH);


  /* Host state */
  PrintTrace("Setting up host state\n");
  SetCRBitsCorrectly(IA32_VMX_CR0_FIXED0_MSR, IA32_VMX_CR0_FIXED1_MSR, HOST_CR0);
  SetCRBitsCorrectly(IA32_VMX_CR4_FIXED0_MSR, IA32_VMX_CR4_FIXED1_MSR, HOST_CR4);
  ret = Init_VMCS_HostState();

  if (ret != VMX_SUCCESS) {
    if (ret == VMX_FAIL_VALID) {
      PrintTrace("Init Host state: VMCS FAILED WITH ERROR\n");
    } else {
      PrintTrace("Init Host state: Invalid VMCS\n");
    }
    return ret;
  }

  //  PrintTrace("HOST_RIP: %x (%u)\n", exit_eip, exit_eip);
  VMCS_WRITE(HOST_RIP, &exit_eip);

  /* Guest state */
  PrintTrace("Setting up guest state\n");
  PrintTrace("GUEST_RIP: %x (%u)\n", entry_eip, entry_eip);
  VMCS_WRITE(GUEST_RIP, &entry_eip);

  SetCRBitsCorrectly(IA32_VMX_CR0_FIXED0_MSR, IA32_VMX_CR0_FIXED1_MSR, GUEST_CR0);
  SetCRBitsCorrectly(IA32_VMX_CR4_FIXED0_MSR, IA32_VMX_CR4_FIXED1_MSR, GUEST_CR4);
  ret = Init_VMCS_GuestState();

  PrintTrace("InitGuestState returned\n");

  if (ret != VMX_SUCCESS) {
    if (ret == VMX_FAIL_VALID) {
      PrintTrace("Init Guest state: VMCS FAILED WITH ERROR\n");
    } else {
      PrintTrace("Init Guest state: Invalid VMCS\n");
    }
    return ret;
  }
  PrintTrace("GUEST_RSP: %x (%u)\n", guest_esp, (uint_t)guest_esp);
  VMCS_WRITE(GUEST_RSP, &guest_esp);

  //  tmpReg = 0x4100;
  tmpReg = 0xffffffff;
  if (VMCS_WRITE(EXCEPTION_BITMAP, &tmpReg) != VMX_SUCCESS) {
    PrintInfo("Bitmap error\n");
  }

  ConfigureExits(vm);

  PrintTrace("VMCS_LAUNCH\n");

  vm->state=VM_VMXASSIST_STARTUP;

  vmm_ret = SAFE_VM_LAUNCH();

  PrintTrace("VMM error %d\n", vmm_ret);

  return vmm_ret;
}



  
int VMLaunch(struct VMDescriptor *vm) 
{
  VMCS * vmcs = CreateVMCS();
  int rc;

  ullong_t vmcs_ptr = (ullong_t)((uint_t)vmcs);
  uint_t top = (vmcs_ptr >> 32) & 0xffffffff;
  uint_t bottom = (vmcs_ptr) & 0xffffffff;

  theVM.vmcsregion = vmcs;
  theVM.descriptor = *vm;

  PrintTrace("vmcs_ptr_top=%x vmcs_ptr_bottom=%x, eip=%x\n", top, bottom, vm->entry_ip);
  rc = MyLaunch(&theVM); // vmcs_ptr, vm->entry_ip, vm->exit_eip, vm->guest_esp);
  PrintTrace("Returned from MyLaunch();\n");
  return rc;
}




//
//
//  END CRUFT
//
//


// For the 32 bit reserved bit fields 
// MB1s are in the low 32 bits, MBZs are in the high 32 bits of the MSR
static uint32_t sanitize_bits1(uint32_t msr_num, uint32_t val) {
    v3_msr_t mask_msr;

    PrintDebug("sanitize_bits1 (MSR:%x)\n", msr_num);

    v3_get_msr(msr_num, &mask_msr.hi, &mask_msr.lo);

    PrintDebug("MSR %x = %x : %x \n", msr_num, msr.hi, msr.lo);

    val &= mask_msr.lo;
    val &= mask_msr.hi;
  
    return val;
}


static addr_t sanitize_bits2(uint32_t msr_num0, uint32_t msr_num1, addr_t val) {
    v3_msr_t msr0, msr1;
    addr_t msr0_val, msr1_val;

    PrintDebug("sanitize_bits2 (MSR0=%x, MSR1=%x)\n", msr_num0, msr_num1);

    v3_get_msr(msr_num0, &msr0.hi, &msr0.lo);
    v3_get_msr(msr_num1, &msr1.hi, &msr1.lo);
  
    // This generates a mask that is the natural bit width of the CPU
    msr0_val = msr0.value;
    msr1_val = msr1.value;

    PrintDebug("MSR %x = %p, %x = %p \n", msr_num0, msr0_val, msr_num1, msr1_val);

    val &= msr0_val;
    val &= msr1_val;

    return val;
}



static vmcs_data* allocate_vmcs() {
    reg_ex_t msr;
    vmcs_data* vmcs_page = (vmcs_data*)V3_VAddr(V3_AllocPages(1));

    memset(vmcs_page, 0, 4096);

    v3_get_msr(VMX_BASIC_MSR, &(msr.e_reg.high), &(msr.e_reg.low));
    
    vmcs_page->revision = ((struct vmx_basic_msr)msr).revision;

    return vmcs_page;
}



static void init_vmcs_bios(vmcs_t * vmcs, struct guest_info * vm_info) {

}



static int init_vmx_guest(struct guest_info * info, struct v3_vm_config * config_ptr) {
    v3_pre_config_guest(info, config_ptr);

    struct vmx_data* data;

    PrintDebug("Allocating vmx_data\n");
    data = (struct vmx_data*)V3_Malloc(sizeof(vmx_data));
    PrintDebug("Allocating VMCS\n");
    data->vmcs = allocate_vmcs();

    info->vmm_data = (void*)data;

    PrintDebug("Initializing VMCS (addr=%p)\n", (void *)info->vmm_data);
    init_vmcs_bios((vmcs_t *)(info->vmm_data), info);

    v3_post_config_guest(info, config_ptr);

    return 0;
}




static int start_svm_guest(struct guest_info *info) {
    return -1;
}






int v3_is_vmx_capable() {
    uint_t ret;
    v3_msr_t feature_msr;
    addr_t eax = 0, ebx = 0, ecx = 0, edx = 0;

    v3_cpuid(CPUID_FEATURE_IDS, &eax, &ebx, &ecx, &edx);

    if (ecx & CPUID_1_ECX_VTXFLAG) {
	v3_get_msr(IA32_FEATURE_CONTROL_MSR, &(feature_msr.hi), &(feature_msr.lo));
	
	PrintTrace("MSRREGlow: 0x%.8x\n", feature_msr.lo);

	if ((feature_msr.lo & FEATURE_CONTROL_VALID) != FEATURE_CONTROL_VALID) {
	    PrintDebug("VMX is locked -- enable in the BIOS\n");
	    return 0;
	}

    } else {
	PrintDebug("VMX not supported on this cpu\n");
	return 0;
    }

    return 1;
}

static int has_vmx_nested_paging() {
    return 0;
}



// We set up the global host state that is unlikely to change across processes here
// Segment Descriptors mainly

struct seg_descriptor {

};


static int setup_base_host_state() {
    uint8_t gdt[10];
    


    //   vmwrite(HOST_IDTR_BASE, 


}



void v3_init_vmx(struct v3_ctrl_ops * vm_ops) {
    v3_msr_t basic_msr;

    // Setup the host state save area
    void * host_state = V3_AllocPages(1);

    v3_get_msr(VMX_BASIC_MSR, &(basic_msr.hi), &(basic_msr.lo));
    
    *(uint32_t *)host_state = ((struct vmx_basic_msr *)basic_msr.value)->revision;
    
    PrintDebug("VMX revision: 0x%p\n", host_state);

    __asm__ __volatile__ (
			  "movl %%cr4, %%ebx; "
			  "orl  %%ebx, 0x00002000; "
			  "movl %%ebx, %%cr4"
			  );



    // Should check and return Error here.... 
    __asm__ __volatile__ (
			  "movl %%cr0, %%ebx; "
			  "orl  %%ebx, 0x00000020; "
			  "movl %%ebx, %%cr0"
			  );


    if (v3_enable_vmx(host_state) == 0) {
	PrintDebug("VMX Enabled\n");
    } else {
	PrintError("VMX initialization failure\n");
	return;
    }
	

    if (has_vmx_nested_paging() == 1) {
	v3_cpu_type = V3_VMX_EPT_CPU;
    } else {
	v3_cpu_type = V3_VMX_CPU;
    }

    // Setup the VMX specific vmm operations
    vmm_ops->init_guest = &init_vmx_guest;
    vmm_ops->start_guest = &start_vmx_guest;
    vmm_ops->has_nested_paging = &has_vmx_nested_paging;

}
