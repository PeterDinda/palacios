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
#include <palacios/vmm_lowlevel.h>
#include <palacios/vmm_config.h>
#include <palacios/vmm_ctrl_regs.h>


// 
// 
// CRUFT
//
//

#if 0

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

#endif

static int update_vmcs_host_state(struct guest_info * info) {
    addr_t tmp;
    struct vmx_data * arch_data = (struct vmx_data *)(info->vmm_data);
    struct v3_msr tmp_msr;

    __asm__ __volatile__ ( "movq    %%cr0, %0; "		
			   : "=q"(tmp)
			   :
    );
    vmcs_write(VMCS_HOST_CR0, tmp);


    __asm__ __volatile__ ( "movq %%cr3, %0; "		
			   : "=q"(tmp)
			   :
    );
    vmcs_write(VMCS_HOST_CR3, tmp);


    __asm__ __volatile__ ( "movq %%cr4, %0; "		
			   : "=q"(tmp)
			   :
    );
    vmcs_write(VMCS_HOST_CR4, tmp);



    vmcs_write(VMCS_HOST_GDTR_BASE, arch_data->host_state.gdtr.base);
    vmcs_write(VMCS_HOST_IDTR_BASE, arch_data->host_state.idtr.base);
    vmcs_write(VMCS_HOST_TR_BASE, arch_data->host_state.tr.base);

#define FS_BASE_MSR 0xc0000100
#define GS_BASE_MSR 0xc0000101

    // FS.BASE MSR
    v3_get_msr(FS_BASE_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmcs_write(VMCS_HOST_FS_BASE, tmp_msr.value);    

    // GS.BASE MSR
    v3_get_msr(GS_BASE_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmcs_write(VMCS_HOST_GS_BASE, tmp_msr.value);    



    __asm__ __volatile__ ( "movq %%cs, %0; "		
			   : "=q"(tmp)
			   :
    );
    vmcs_write(VMCS_HOST_CS_SELECTOR, tmp);

    __asm__ __volatile__ ( "movq %%ss, %0; "		
			   : "=q"(tmp)
			   :
    );
    vmcs_write(VMCS_HOST_SS_SELECTOR, tmp);

    __asm__ __volatile__ ( "movq %%ds, %0; "		
			   : "=q"(tmp)
			   :
    );
    vmcs_write(VMCS_HOST_DS_SELECTOR, tmp);

    __asm__ __volatile__ ( "movq %%es, %0; "		
			   : "=q"(tmp)
			   :
    );
    vmcs_write(VMCS_HOST_ES_SELECTOR, tmp);

    __asm__ __volatile__ ( "movq %%fs, %0; "		
			   : "=q"(tmp)
			   :
    );
    vmcs_write(VMCS_HOST_FS_SELECTOR, tmp);

    __asm__ __volatile__ ( "movq %%gs, %0; "		
			   : "=q"(tmp)
			   :
    );
    vmcs_write(VMCS_HOST_GS_SELECTOR, tmp);

    vmcs_write(VMCS_HOST_TR_SELECTOR, arch_data->host_state.tr.selector);


#define SYSENTER_CS_MSR 0x00000174
#define SYSENTER_ESP_MSR 0x00000175
#define SYSENTER_EIP_MSR 0x00000176

   // SYSENTER CS MSR
    v3_get_msr(SYSENTER_CS_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmcs_write(VMCS_HOST_SYSENTER_CS, tmp_msr.value);

    // SYSENTER_ESP MSR
    v3_get_msr(SYSENTER_ESP_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmcs_write(VMCS_HOST_SYSENTER_ESP, tmp_msr.value);

    // SYSENTER_EIP MSR
    v3_get_msr(SYSENTER_EIP_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmcs_write(VMCS_HOST_SYSENTER_EIP, tmp_msr.value);



    return 0;
}




static addr_t vmxon_ptr_phys;
extern int v3_vmx_exit_handler();
extern int v3_vmx_vmlaunch();


#if 0
// For the 32 bit reserved bit fields 
// MB1s are in the low 32 bits, MBZs are in the high 32 bits of the MSR
static uint32_t sanitize_bits1(uint32_t msr_num, uint32_t val) {
    v3_msr_t mask_msr;

    PrintDebug("sanitize_bits1 (MSR:%x)\n", msr_num);

    v3_get_msr(msr_num, &mask_msr.hi, &mask_msr.lo);

    PrintDebug("MSR %x = %x : %x \n", msr_num, mask_msr.hi, mask_msr.lo);

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

    PrintDebug("MSR %x = %p, %x = %p \n", msr_num0, (void*)msr0_val, msr_num1, (void*)msr1_val);

    val &= msr0_val;
    val &= msr1_val;

    return val;
}

static int setup_base_host_state() {
    


    //   vmwrite(HOST_IDTR_BASE, 


}


#endif

static int inline check_vmcs_write(vmcs_field_t field, addr_t val)
{
    int ret = 0;
    ret = vmcs_write(field,val);

    if (ret != VMX_SUCCESS) {
        PrintError("VMWRITE error on %s!: %d\n", v3_vmcs_field_to_str(field), ret);
        return 1;
    }

    return 0;
}


static void inline translate_segment_access(struct v3_segment * v3_seg,  
					    struct vmcs_segment_access * access)
{
    access->type = v3_seg->type;
    access->desc_type = v3_seg->system;
    access->dpl = v3_seg->dpl;
    access->present = v3_seg->present;
    access->avail = v3_seg->avail;
    access->long_mode = v3_seg->long_mode;
    access->db = v3_seg->db;
    access->granularity = v3_seg->granularity;
}

static int inline vmcs_write_guest_segments(struct guest_info* info)
{
    int ret = 0;
    struct vmcs_segment_access access;

    memset(&access, 0, sizeof(access));

    /* CS Segment */
    translate_segment_access(&(info->segments.cs), &access);

    ret &= check_vmcs_write(VMCS_GUEST_CS_BASE, info->segments.cs.base);
    ret &= check_vmcs_write(VMCS_GUEST_CS_SELECTOR, info->segments.cs.selector);
    ret &= check_vmcs_write(VMCS_GUEST_CS_LIMIT, info->segments.cs.limit);
    ret &= check_vmcs_write(VMCS_GUEST_CS_ACCESS, access.value);

    /* SS Segment */
    translate_segment_access(&(info->segments.ss), &access);

    ret &= check_vmcs_write(VMCS_GUEST_SS_BASE, info->segments.ss.base);
    ret &= check_vmcs_write(VMCS_GUEST_SS_SELECTOR, info->segments.ss.selector);
    ret &= check_vmcs_write(VMCS_GUEST_SS_LIMIT, info->segments.ss.limit);
    ret &= check_vmcs_write(VMCS_GUEST_SS_ACCESS, access.value);

    /* DS Segment */
    translate_segment_access(&(info->segments.ds), &access);

    ret &= check_vmcs_write(VMCS_GUEST_DS_BASE, info->segments.ds.base);
    ret &= check_vmcs_write(VMCS_GUEST_DS_SELECTOR, info->segments.ds.selector);
    ret &= check_vmcs_write(VMCS_GUEST_DS_LIMIT, info->segments.ds.limit);
    ret &= check_vmcs_write(VMCS_GUEST_DS_ACCESS, access.value);


    /* ES Segment */
    translate_segment_access(&(info->segments.es), &access);

    ret &= check_vmcs_write(VMCS_GUEST_ES_BASE, info->segments.es.base);
    ret &= check_vmcs_write(VMCS_GUEST_ES_SELECTOR, info->segments.es.selector);
    ret &= check_vmcs_write(VMCS_GUEST_ES_LIMIT, info->segments.es.limit);
    ret &= check_vmcs_write(VMCS_GUEST_ES_ACCESS, access.value);

    /* FS Segment */
    translate_segment_access(&(info->segments.fs), &access);

    ret &= check_vmcs_write(VMCS_GUEST_FS_BASE, info->segments.fs.base);
    ret &= check_vmcs_write(VMCS_GUEST_FS_SELECTOR, info->segments.fs.selector);
    ret &= check_vmcs_write(VMCS_GUEST_FS_LIMIT, info->segments.fs.limit);
    ret &= check_vmcs_write(VMCS_GUEST_FS_ACCESS, access.value);

    /* GS Segment */
    translate_segment_access(&(info->segments.gs), &access);

    ret &= check_vmcs_write(VMCS_GUEST_GS_BASE, info->segments.gs.base);
    ret &= check_vmcs_write(VMCS_GUEST_GS_SELECTOR, info->segments.gs.selector);
    ret &= check_vmcs_write(VMCS_GUEST_GS_LIMIT, info->segments.gs.limit);
    ret &= check_vmcs_write(VMCS_GUEST_GS_ACCESS, access.value);

    /* LDTR segment */
    translate_segment_access(&(info->segments.ldtr), &access);

    ret &= check_vmcs_write(VMCS_GUEST_LDTR_BASE, info->segments.ldtr.base);
    ret &= check_vmcs_write(VMCS_GUEST_LDTR_SELECTOR, info->segments.ldtr.selector);
    ret &= check_vmcs_write(VMCS_GUEST_LDTR_LIMIT, info->segments.ldtr.limit);
    ret &= check_vmcs_write(VMCS_GUEST_LDTR_ACCESS, access.value);

    /* TR Segment */
    translate_segment_access(&(info->segments.tr), &access);

    ret &= check_vmcs_write(VMCS_GUEST_TR_BASE, info->segments.tr.base);
    ret &= check_vmcs_write(VMCS_GUEST_TR_SELECTOR, info->segments.ldtr.selector);
    ret &= check_vmcs_write(VMCS_GUEST_TR_LIMIT, info->segments.tr.limit);
    ret &= check_vmcs_write(VMCS_GUEST_TR_ACCESS, access.value);

    /* GDTR Segment */

    ret &= check_vmcs_write(VMCS_GUEST_GDTR_BASE, info->segments.gdtr.base);
    ret &= check_vmcs_write(VMCS_GUEST_GDTR_LIMIT, info->segments.gdtr.limit);

    /* IDTR Segment*/
    ret &= check_vmcs_write(VMCS_GUEST_IDTR_BASE, info->segments.idtr.base);
    ret &= check_vmcs_write(VMCS_GUEST_IDTR_LIMIT, info->segments.idtr.limit);

    return ret;
}

static void setup_v8086_mode_for_boot(struct guest_info * vm_info)
{

    ((struct vmx_data *)vm_info->vmm_data)->state = VMXASSIST_V8086_BIOS;
    ((struct rflags *)&(vm_info->ctrl_regs.rflags))->vm = 1;
    ((struct rflags *)&(vm_info->ctrl_regs.rflags))->iopl = 3;

   
    vm_info->rip = 0xfff0;

    vm_info->segments.cs.selector = 0xf000;
    vm_info->segments.cs.base = 0xf000 << 4;
    vm_info->segments.cs.limit = 0xffff;
    vm_info->segments.cs.type = 3;
    vm_info->segments.cs.system = 1;
    vm_info->segments.cs.dpl = 3;
    vm_info->segments.cs.present = 1;
    vm_info->segments.cs.granularity = 0;

    int i = 0;
    struct v3_segment * seg_ptr = (struct v3_segment *)&(vm_info->segments);

    /* Set values for selectors ds through ss */
    for(i = 1; i < 6 ; i++) {
        seg_ptr[i].selector = 0x0000;
        seg_ptr[i].base = 0x00000;
        seg_ptr[i].type = 3;
        seg_ptr[i].system = 1;
        seg_ptr[i].dpl = 3;
        seg_ptr[i].present = 1;
        seg_ptr[i].granularity = 0;
    }

    for(i = 6; i < 10; i++) {
        seg_ptr[i].base = 0x0;
        seg_ptr[i].limit = 0xffff;
    }

    vm_info->segments.ldtr.selector = 0x0;
    vm_info->segments.ldtr.type = 2;
    vm_info->segments.ldtr.system = 0;
    vm_info->segments.ldtr.present = 1;
    vm_info->segments.ldtr.granularity = 0;

    vm_info->segments.tr.selector = 0x0;
    vm_info->segments.tr.type = 3;
    vm_info->segments.tr.system = 0;
    vm_info->segments.tr.present = 1;
    vm_info->segments.tr.granularity = 0;
}


static addr_t allocate_vmcs() 
{
    reg_ex_t msr;
    PrintDebug("Allocating page\n");
    struct vmcs_data * vmcs_page = (struct vmcs_data *)V3_VAddr(V3_AllocPages(1));


    memset(vmcs_page, 0, 4096);

    v3_get_msr(VMX_BASIC_MSR, &(msr.e_reg.high), &(msr.e_reg.low));
    
    vmcs_page->revision = ((struct vmx_basic_msr*)&msr)->revision;
    PrintDebug("VMX Revision: 0x%x\n",vmcs_page->revision);

    return (addr_t)V3_PAddr((void *)vmcs_page);
}



static int init_vmcs_bios(struct guest_info * vm_info) 
{

    setup_v8086_mode_for_boot(vm_info);

    // TODO: Fix vmcs fields so they're 32-bit
    struct vmx_data * vmx_data = (struct vmx_data *)vm_info->vmm_data;
    int vmx_ret;

    // Have to do a whole lot of flag setting here
    PrintDebug("Clearing VMCS\n");
    vmx_ret = vmcs_clear(vmx_data->vmcs_ptr_phys);

    if (vmx_ret != VMX_SUCCESS) {
        PrintError("VMCLEAR failed\n");
        return -1;
    }

    PrintDebug("Loading VMCS\n");
    vmx_ret = vmcs_load(vmx_data->vmcs_ptr_phys);

    if (vmx_ret != VMX_SUCCESS) {
        PrintError("VMPTRLD failed\n");
        return -1;
    }

    struct v3_msr tmp_msr;

    /* Write VMX Control Fields */
    v3_get_msr(VMX_PINBASED_CTLS_MSR,&(tmp_msr.hi),&(tmp_msr.lo));
    vmcs_write(VMCS_PIN_CTRLS, tmp_msr.lo);

    v3_get_msr(VMX_PROCBASED_CTLS_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmcs_write(VMCS_PROC_CTRLS, tmp_msr.lo);

    v3_get_msr(VMX_EXIT_CTLS_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmcs_write(VMCS_EXIT_CTRLS, tmp_msr.lo);

    v3_get_msr(VMX_ENTRY_CTLS_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmcs_write(VMCS_ENTRY_CTRLS, tmp_msr.lo);

    /* Cache GDTR, IDTR, and TR in host struct */
    struct {
        uint16_t selector;
        addr_t   base;
    } __attribute__((packed)) tmp_seg;
    
    addr_t gdtr_base;

    __asm__ __volatile__(
			 "sgdt (%0);"
			 :
			 : "q"(&tmp_seg)
			 : "memory"
			 );
    vmx_data->host_state.gdtr.base = gdtr_base = tmp_seg.base;

    __asm__ __volatile__(
			 "sidt (%0);"
			 :
			 : "q"(&tmp_seg)
			 : "memory"
			 );
    vmx_data->host_state.idtr.base = tmp_seg.base;

    __asm__ __volatile__(
			 "str (%0);"
			 :
			 : "q"(&tmp_seg)
			 : "memory"
			 );
    vmx_data->host_state.tr.selector = tmp_seg.selector;

    struct tss_descriptor desc = ((struct tss_descriptor *)gdtr_base)[tmp_seg.selector];
    
    tmp_seg.base = (
		    (desc.base1) |
		    (desc.base2 << 16) |
		    (desc.base3 << 24) |
#ifdef __V3_64BIT__
		    ((uint64_t)desc.base4 << 32)
#else 
		    (0)
#endif
                );

    vmx_data->host_state.tr.base = tmp_seg.base;

    update_vmcs_host_state(vm_info);
    vmcs_write(VMCS_HOST_RIP, (addr_t)&v3_vmx_exit_handler);

    // Setup guest state 
    // TODO: This is not 32-bit safe!
    vmx_ret &= check_vmcs_write(VMCS_GUEST_RIP, vm_info->rip);
    vmx_ret &= check_vmcs_write(VMCS_GUEST_CR0, 0x60000010);

    vmx_ret &= vmcs_write_guest_segments(vm_info);

    vmx_ret &= check_vmcs_write(VMCS_GUEST_RFLAGS, vm_info->ctrl_regs.rflags);
    vmx_ret &= check_vmcs_write(VMCS_LINK_PTR, 0xffffffffffffffff);

    if (vmx_ret != 0) {
	PrintError("Could not initialize VMCS segments\n");
        return -1;
    }

    v3_print_vmcs_guest_state();
    return 0;
}

int v3_vmx_handle_exit()
{
    PrintDebug("Exit taken!\n");
    return 0;
}

static int init_vmx_guest(struct guest_info * info, struct v3_vm_config * config_ptr) {
    PrintDebug("Entering init_vmx_guest\n");
    v3_pre_config_guest(info, config_ptr);

    struct vmx_data * data = NULL;

    data = (struct vmx_data *)V3_Malloc(sizeof(struct vmx_data));

    PrintDebug("vmx_data pointer: %p\n", (void *)data);

    PrintDebug("Allocating VMCS\n");
    data->vmcs_ptr_phys = allocate_vmcs();

    PrintDebug("VMCS pointer: %p\n", (void *)(data->vmcs_ptr_phys));

    info->vmm_data = data;

    PrintDebug("Initializing VMCS (addr=%p)\n", info->vmm_data);

    if (init_vmcs_bios(info) != 0) {
	PrintError("Could not initialize VMCS BIOS\n");
        return -1;
    }

    //v3_post_config_guest(info, config_ptr);

    return 0;
}


static int start_vmx_guest(struct guest_info* info) {
    uint32_t error = 0;
    int ret = 0;

    PrintDebug("Attempting VMLAUNCH\n");

    ret = v3_vmx_vmlaunch();

    PrintDebug("Returned from VMLAUNCH\n");

    vmcs_read(VMCS_INSTR_ERR, &error, 4);

    if (ret != VMX_SUCCESS) {
        PrintError("VMLAUNCH failed: %d\n", error);
    }

    return -1;
}






int v3_is_vmx_capable() {
    v3_msr_t feature_msr;
    addr_t eax = 0, ebx = 0, ecx = 0, edx = 0;

    v3_cpuid(0x1, &eax, &ebx, &ecx, &edx);

    PrintDebug("ECX: %p\n", (void*)ecx);

    if (ecx & CPUID_1_ECX_VTXFLAG) {
        v3_get_msr(VMX_FEATURE_CONTROL_MSR, &(feature_msr.hi), &(feature_msr.lo));
	
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



void v3_init_vmx(struct v3_ctrl_ops * vm_ops) {
    extern v3_cpu_arch_t v3_cpu_type;

    struct v3_msr tmp_msr;
    uint64_t ret=0;

    v3_get_msr(VMX_CR4_FIXED0_MSR,&(tmp_msr.hi),&(tmp_msr.lo));
    
    __asm__ __volatile__ (
			  "movq %%cr4, %%rbx;"
			  "orq  $0x00002000, %%rbx;"
			  "movq %%rbx, %0;"
			  : "=m"(ret) 
			  :
			  : "%rbx"
			  );

    if((~ret & tmp_msr.value) == 0) {
        __asm__ __volatile__ (
			      "movq %0, %%cr4;"
			      :
			      : "q"(ret)
			      );
    } else {
        PrintError("Invalid CR4 Settings!\n");
        return;
    }
      __asm__ __volatile__ (
			    "movq %%cr0, %%rbx; "
			    "orq  $0x00000020,%%rbx; "
			    "movq %%rbx, %%cr0;"
			    :
			    :
			    : "%rbx"
			    );
      //
    // Should check and return Error here.... 

    // Setup VMXON Region
    vmxon_ptr_phys = allocate_vmcs();
    PrintDebug("VMXON pointer: 0x%p\n", (void*)vmxon_ptr_phys);

    if (v3_enable_vmx(vmxon_ptr_phys) == VMX_SUCCESS) {
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
    vm_ops->init_guest = &init_vmx_guest;
    vm_ops->start_guest = &start_vmx_guest;
    vm_ops->has_nested_paging = &has_vmx_nested_paging;

}
