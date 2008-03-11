#include <geekos/svm.h>
#include <geekos/vmm.h>

#include <geekos/vmcb.h>
#include <geekos/vmm_mem.h>
#include <geekos/vmm_paging.h>


extern struct vmm_os_hooks * os_hooks;

extern uint_t cpuid_ecx(uint_t op);
extern uint_t cpuid_edx(uint_t op);
extern void Get_MSR(uint_t MSR, uint_t * high_byte, uint_t * low_byte); 
extern void Set_MSR(uint_t MSR, uint_t high_byte, uint_t low_byte);
extern uint_t launch_svm(vmcb_t * vmcb_addr);
extern uint_t Get_CR3();

extern void GetGDTR(void * gdt);
extern void GetIDTR(void * idt);


/* Checks machine SVM capability */
/* Implemented from: AMD Arch Manual 3, sect 15.4 */ 
int is_svm_capable() {
  uint_t ret =  cpuid_ecx(CPUID_FEATURE_IDS);
  uint_t vm_cr_low = 0, vm_cr_high = 0;


  if ((ret & CPUID_FEATURE_IDS_ecx_svm_avail) == 0) {
    PrintDebug("SVM Not Available\n");
    return 0;
  } 

  Get_MSR(SVM_VM_CR_MSR, &vm_cr_high, &vm_cr_low);

  if ((vm_cr_low & SVM_VM_CR_MSR_svmdis) == 0) {
    return 1;
  }

  ret = cpuid_edx(CPUID_SVM_REV_AND_FEATURE_IDS);
  

  if ((ret & CPUID_SVM_REV_AND_FEATURE_IDS_edx_np) == 0) {
    PrintDebug("Nested Paging not supported\n");
  }

  if ((ret & CPUID_SVM_REV_AND_FEATURE_IDS_edx_svml) == 0) {
    PrintDebug("SVM BIOS Disabled, not unlockable\n");
  } else {
    PrintDebug("SVM is locked with a key\n");
  }

  return 0;
}



void Init_SVM(struct vmm_ctrl_ops * vmm_ops) {
  reg_ex_t msr;
  void * host_state;


  // Enable SVM on the CPU
  Get_MSR(EFER_MSR, &(msr.e_reg.high), &(msr.e_reg.low));
  msr.e_reg.low |= EFER_MSR_svm_enable;
  Set_MSR(EFER_MSR, 0, msr.e_reg.low);
  
  PrintDebug("SVM Enabled\n");


  // Setup the host state save area
  host_state = os_hooks->allocate_pages(1);
  
  msr.e_reg.high = 0;
  msr.e_reg.low = (uint_t)host_state;


  PrintDebug("Host State being saved at %x\n", (uint_t)host_state);
  Set_MSR(SVM_VM_HSAVE_PA_MSR, msr.e_reg.high, msr.e_reg.low);



  // Setup the SVM specific vmm operations
  vmm_ops->init_guest = &init_svm_guest;
  vmm_ops->start_guest = &start_svm_guest;


  return;
}


int init_svm_guest(struct guest_info *info) {
 
  PrintDebug("Allocating VMCB\n");
  info->vmm_data = (void*)Allocate_VMCB();


  PrintDebug("Generating Guest nested page tables\n");
  print_mem_list(&(info->mem_list));
  print_mem_layout(&(info->mem_layout));
  info->page_tables = generate_guest_page_tables_64(&(info->mem_layout), &(info->mem_list));
  //PrintDebugPageTables(info->page_tables);

  

  PrintDebug("Initializing VMCB (addr=%x)\n", info->vmm_data);
  Init_VMCB((vmcb_t*)(info->vmm_data), *info);


  

  return 0;
}


// can we start a kernel thread here...
int start_svm_guest(struct guest_info *info) {
  vmcb_ctrl_t * guest_ctrl = 0;

  ulong_t exit_code = 0;

  PrintDebug("Launching SVM VM (vmcb=%x)\n", info->vmm_data);
  // PrintDebugVMCB((vmcb_t*)(info->vmm_data));


  launch_svm((vmcb_t*)(info->vmm_data));

  guest_ctrl = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));


  PrintDebug("SVM Returned: (Exit Code=%x) (VMCB=%x)\n",&(guest_ctrl->exit_code), info->vmm_data); 


  exit_code = guest_ctrl->exit_code;

  PrintDebug("SVM Returned: Exit Code: %x\n",exit_code); 

  return 0;
}



/** 
 *  We handle the svm exits here
 *  This function should probably be moved to another file to keep things managable....
 */
int handle_svm_exit(struct VMM_GPRs guest_gprs) {

  return 0;
}


vmcb_t * Allocate_VMCB() {
  vmcb_t * vmcb_page = (vmcb_t*)os_hooks->allocate_pages(1);


  memset(vmcb_page, 0, 4096);

  return vmcb_page;
}



void Init_VMCB(vmcb_t *vmcb, guest_info_t vm_info) {
  vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA(vmcb);
  vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA(vmcb);
  uint_t i = 0;


  guest_state->rsp = vm_info.rsp;
  guest_state->rip = vm_info.rip;


  /* I pretty much just gutted this from TVMM */
  /* Note: That means its probably wrong */

  // set the segment registers to mirror ours
  guest_state->cs.selector = 0;
  guest_state->cs.attrib.fields.type = 0xa; // Code segment+read
  guest_state->cs.attrib.fields.S = 1;
  guest_state->cs.attrib.fields.P = 1;
  guest_state->cs.attrib.fields.db = 1;
  guest_state->cs.limit = 0xffffffff;
  guest_state->cs.base = 0;
  
  struct vmcb_selector *segregs [] = {&(guest_state->ss), &(guest_state->ds), &(guest_state->es), &(guest_state->fs), &(guest_state->gs), NULL};
  for ( i = 0; segregs[i] != NULL; i++) {
    struct vmcb_selector * seg = segregs[i];
    
    seg->selector = 0;
    seg->attrib.fields.type = 0x2; // Data Segment+read/write
    seg->attrib.fields.S = 1;
    seg->attrib.fields.P = 1;
    seg->attrib.fields.db = 1;
    seg->limit = 0xffffffff;
    seg->base = 0;
  }

  /* ** */


  guest_state->efer |= EFER_MSR_svm_enable;
  guest_state->rflags = 0x00000002; // The reserved bit is always 1
  ctrl_area->svm_instrs.instrs.VMRUN = 1;
  guest_state->cr0 = 0x00000001;    // PE 
  ctrl_area->guest_ASID = 1;


  //  guest_state->cpl = 3;





  // Setup exits

  
  ctrl_area->exceptions.ex_names.de = 1;
  ctrl_area->exceptions.ex_names.df = 1;
  ctrl_area->exceptions.ex_names.pf = 1;
  ctrl_area->exceptions.ex_names.ts = 1;
  ctrl_area->exceptions.ex_names.ss = 1;
  ctrl_area->exceptions.ex_names.ac = 1;
  ctrl_area->exceptions.ex_names.mc = 1;
  ctrl_area->exceptions.ex_names.gp = 1;
  ctrl_area->exceptions.ex_names.ud = 1;
  ctrl_area->exceptions.ex_names.np = 1;
  ctrl_area->exceptions.ex_names.of = 1;
  ctrl_area->exceptions.ex_names.nmi = 1;

  

  // ctrl_area->instrs.instrs.IOIO_PROT = 1;
  ctrl_area->IOPM_BASE_PA = (uint_t)os_hooks->allocate_pages(3);
  
  {
    reg_ex_t tmp_reg;
    tmp_reg.r_reg = ctrl_area->IOPM_BASE_PA;
    memset((void*)(tmp_reg.e_reg.low), 0xffffffff, PAGE_SIZE * 2);
  }

  ctrl_area->instrs.instrs.INTR = 1;

  /*
  {
    reg_ex_t gdt;
    reg_ex_t idt;
    
    GetGDTR(&(gdt.r_reg));
    PrintDebug("GDT: hi: %x, lo: %x\n", gdt.e_reg.high, gdt.e_reg.low);

    GetIDTR(&(idt.r_reg));

  }
  */

  // also determine if CPU supports nested paging
    if (vm_info.page_tables) {
  //  if (0) {
    // Flush the TLB on entries/exits
    ctrl_area->TLB_CONTROL = 1;

    // Enable Nested Paging
    ctrl_area->NP_ENABLE = 1;

    PrintDebug("NP_Enable at 0x%x\n", &(ctrl_area->NP_ENABLE));

        // Set the Nested Page Table pointer
    ctrl_area->N_CR3 |= ((addr_t)vm_info.page_tables & 0xfffff000);


    //   ctrl_area->N_CR3 = Get_CR3();
    // guest_state->cr3 |= (Get_CR3() & 0xfffff000);

    guest_state->g_pat = 0x7040600070406ULL;

    PrintDebug("Set Nested CR3: lo: 0x%x  hi: 0x%x\n", (uint_t)*(&(ctrl_area->N_CR3)), (uint_t)*((unsigned char *)&(ctrl_area->N_CR3) + 4));
    PrintDebug("Set Guest CR3: lo: 0x%x  hi: 0x%x\n", (uint_t)*(&(guest_state->cr3)), (uint_t)*((unsigned char *)&(guest_state->cr3) + 4));
    // Enable Paging
    //    guest_state->cr0 |= 0x80000000;
  }


}


