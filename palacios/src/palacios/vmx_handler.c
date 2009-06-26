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

static int PanicUnhandledVMExit(struct VM *vm)
{
  PrintInfo("Panicking due to VMExit with reason %u\n", vm->vmcs.exitInfoFields.reason);
  PrintTrace("Panicking due to VMExit with reason %u\n", vm->vmcs.exitInfoFields.reason);
  PrintTrace_VMCS_ALL();
  PrintTrace_VMX_Regs(&(vm->registers));
  VMXPanic();
  return 0;
}





static int HandleVMPrintsAndPanics(struct VM *vm, uint_t port, uint_t data)
{
  if (port==VMXASSIST_INFO_PORT &&
      (vm->state == VM_VMXASSIST_STARTUP || 
       vm->state == VM_VMXASSIST_V8086_BIOS ||
       vm->state == VM_VMXASSIST_V8086)) { 
    // Communication channel from VMXAssist
    PrintTrace("VMXASSIST Output Port\n");
    PrintDebug("%c",data&0xff);
    return 1;
  } 

  if ((port==ROMBIOS_PANIC_PORT || 
       port==ROMBIOS_PANIC_PORT2 || 
       port==ROMBIOS_DEBUG_PORT ||
       port==ROMBIOS_INFO_PORT) &&
      (vm->state==VM_VMXASSIST_V8086_BIOS)) {
    // rombios is communicating
    PrintTrace("ROMBIOS Output Port\n");
    //    PrintDebug("%c",data&0xff);
    return 1;
  }

  if (port==BOOT_STATE_CARD_PORT && vm->state==VM_VMXASSIST_V8086_BIOS) { 
    // rombios is sending something to the display card
    PrintTrace("Hex Display: 0x%x\n",data&0xff);
    return 1;
  }
  return 0;
}

static int HandleInOutExit(struct VM *vm)
{
  uint_t address;

  struct VMCSExitInfoFields *exitinfo = &(vm->vmcs.exitInfoFields);
  struct VMExitIOQual * qual = (struct VMExitIOQual *)&(vm->vmcs.exitInfoFields.qualification);
  struct VMXRegs *regs = &(vm->registers);

  address=GetLinearIP(vm);

  PrintTrace("Handling Input/Output Instruction Exit\n");

  PrintTrace_VMX_Regs(regs);

  PrintTrace("Qualifications=0x%x\n", exitinfo->qualification);
  PrintTrace("Reason=0x%x\n", exitinfo->reason);
  PrintTrace("IO Port: 0x%x (%d)\n", qual->port, qual->port);
  PrintTrace("Instruction Info=%x\n", exitinfo->instrInfo);
  PrintTrace("%x : %s %s %s instruction of length %d for %d bytes from/to port 0x%x\n",
		   address,
		   qual->dir == 0 ? "output" : "input",
		   qual->string ==0 ? "nonstring" : "STRING",
		   qual->REP == 0 ? "with no rep" : "WITH REP",
		   exitinfo->instrLength, 
		   qual->accessSize==0 ? 1 : qual->accessSize==1 ? 2 : 4,
		   qual->port);

  if ((qual->port == PIC_MASTER_CMD_ISR_PORT) ||
      (qual->port == PIC_MASTER_IMR_PORT)     ||
      (qual->port == PIC_SLAVE_CMD_ISR_PORT)  ||
      (qual->port == PIC_SLAVE_IMR_PORT)) {
    PrintTrace( "PIC Access\n");
  }
                  

  if ((qual->dir == 1) && (qual->REP == 0) && (qual->string == 0)) { 
    char byte = In_Byte(qual->port);

    vm->vmcs.guestStateArea.rip += exitinfo->instrLength;
    regs->eax = (regs->eax & 0xffffff00) | byte;
    PrintTrace("Returning 0x%x in eax\n", (regs->eax));
  }

  if (qual->dir==0 && qual->REP==0 && qual->string==0) { 
    // See if we need to handle the outb as a signal or
    // print from the VM
    if (HandleVMPrintsAndPanics(vm,qual->port,regs->eax)) {
    } else {
      // If not, just go ahead and do the outb
      Out_Byte(qual->port,regs->eax);
      PrintTrace("Wrote 0x%x to port\n",(regs->eax));
    }
    vm->vmcs.guestStateArea.rip += exitinfo->instrLength;
  }

  return 0;
}  


static int HandleExternalIRQExit(struct VM *vm)
{
  struct VMCSExitInfoFields * exitinfo = &(vm->vmcs.exitInfoFields);
  struct VMExitIntInfo * intInfo  = (struct VMExitIntInfo *)&(vm->vmcs.exitInfoFields.intInfo);

  PrintTrace("External Interrupt captured\n");
  PrintTrace("IntInfo: %x\n", exitinfo->intInfo);


  if (!intInfo->valid) {
     // interrupts are off, but this interrupt is not acknoledged (still pending)
     // so we turn on interrupts to deliver appropriately in the
     // host
    PrintTrace("External Interrupt is invald.  Turning Interrupts back on\n");
    asm("sti");
    return 0;
  } 

  // At this point, interrupts are off and the interrupt has been 
  // acknowledged.  We will now handle the interrupt ourselves 
  // and turn interrupts  back on in the host

  PrintTrace("type: %d\n", intInfo->type);
  PrintTrace("number: %d\n", intInfo->nr);

  PrintTrace("Interrupt %d occuring now and handled by HandleExternalIRQExit\n",intInfo->nr);

  switch (intInfo->type) {
  case 0:  {  // ext. IRQ
    // In the following, we construct an "int x" instruction
    // where x is the specific interrupt number that is raised
    // then we execute that instruciton
    // because we are in host context, that means it is delivered as normal
    // through the host IDT
     
     ((char*)(&&ext_int_seq_start))[1] = intInfo->nr;
 
     PrintTrace("Interrupt instruction setup done %x\n", *((ushort_t *)(&&ext_int_seq_start)));
     
ext_int_seq_start:
     asm("int $0");
  }

    break;
  case 2: // NMI
    PrintTrace("Type: NMI\n");
    break;
  case 3: // hw exception
    PrintTrace("Type: HW Exception\n");
    break;
  case 4: // sw exception
    PrintTrace("Type: SW Exception\n");
    break;
  default:
    PrintTrace("Invalid Interrupt Type\n");
    return -1;
  }
  
  if (intInfo->valid && intInfo->errorCode) {
    PrintTrace("IntError: %x\n", exitinfo->intErrorCode);
  }


  return 0;

}







static int HandleExceptionOrNMI(struct VM *vm)
{
  struct Instruction inst;
  uint_t num;
  uint_t type;
  uint_t errorvalid;
  uint_t error;
  uint_t ext=0;
  uint_t idt=0;
  uint_t ti=0;
  uint_t selectorindex=0;

  PrintTrace("Exception or NMI occurred\n");
  
  num=vm->vmcs.exitInfoFields.intInfo & 0xff;
  type=(vm->vmcs.exitInfoFields.intInfo & 0x700)>>8;
  errorvalid=(vm->vmcs.exitInfoFields.intInfo & 0x800)>>11;
  if (errorvalid) { 
    error=vm->vmcs.exitInfoFields.intErrorCode;
    ext=error&0x1;
    idt=(error&0x2)>>1;
    ti=(error&0x4)>>2;
    selectorindex=(error>>3)&0xffff;
  }
  
  PrintTrace("Exception %d now - handled by HandleExceptionOrNMI\n",num);

  PrintTrace("Exception Number %u : %s\n", num, exception_names[num]);
  PrintTrace("Exception Type %u : %s\n", type, exception_type_names[type]);
  if (errorvalid) { 
    if (ext) { 
      PrintTrace("External\n");
    } else {
      PrintTrace("%s - Selector Index is %u\n", idt ? "IDT" : ti ? "LDT" : "GDT", selectorindex);
    }
  }

  DecodeCurrentInstruction(vm,&inst);

  if (inst.type==VM_MOV_TO_CR0) {
    PrintTrace("MOV TO CR0, oldvalue=0x%x, newvalue=0x%x\n",inst.input2, inst.input1);
    if ((inst.input2 & CR0_PE) && !(inst.input1 & CR0_PE) && vm->state==VM_VMXASSIST_STARTUP) {
      // This is VMXAssist signalling for us to turn on V8086 mode and
      // jump into the bios
      PrintTrace("VMXAssist is signaling us for switch to V8086 mode and jump to 0xf000:fff0\n");
      SetupV8086ModeForBoot(vm);
      goto leave;
    } else {
      PrintTrace("Instruction is a write to CR0, but we don't understand it so we'll just exec it\n");
    } 
  } 


  PrintTrace("Trying to execute the faulting instruction in VMM context now\n");
  ExecFaultingInstructionInVMM(vm);

    leave:
  //
  //PanicUnhandledVMExit(vmcs,regs);
  //VMXPanic();
  return 0;
}





int Do_VMM(struct VMXRegs regs) 
{

  ullong_t vmcs_ptr = 0;
  uint_t vmcs_ptr_low = 0;
  int ret = 0;
  uint_t vmx_abort = 0;


  
  PrintTrace("Vm Exit\n");
  ret = VMCS_STORE(&vmcs_ptr);
  vmcs_ptr &= 0xffffffff;
  vmcs_ptr_low +=  vmcs_ptr;




  PrintTrace("ret=%d\n", ret);
  PrintTrace("Revision: %x\n", *(uint_t *)(vmcs_ptr_low));
  vmx_abort = *(uint_t*)(((char *)vmcs_ptr_low)+4);
    
  struct VM *vm = FindVM();

  if (vmx_abort != 0) {
    PrintTrace("VM ABORTED w/ code: %x\n", vmx_abort);
    return -1;
  }

  vm->registers = regs;

  if (CopyOutVMCSData(&(vm->vmcs)) != 0) {
    PrintTrace("Could not copy out VMCS\n");
    return -1;
  }


  PrintTrace("Guest esp: 0x%x (%u)\n", vm->vmcs.guestStateArea.rsp, vm->vmcs.guestStateArea.rsp);

  PrintTrace("VM Exit for reason: %d (%x)\n", 
	      vm->vmcs.exitInfoFields.reason & 0x00000fff,
	      vm->vmcs.exitInfoFields.reason);  

  if (vm->vmcs.exitInfoFields.reason & (0x1<<29) ) { 
    PrintTrace("VM Exit is from VMX root operation.  Panicking\n");
    VMXPanic();
  }

  if (vm->vmcs.exitInfoFields.reason & (0x1<<31) ) { 
    PrintTrace("VM Exit is due to a VM entry failure.  Shouldn't happen here. Panicking\n");
    PrintTrace_VMCSData(&(vm->vmcs));
    VMXPanic();
  }

  switch (vm->vmcs.exitInfoFields.reason) {
  case VM_EXIT_REASON_INFO_EXCEPTION_OR_NMI:
    ret = HandleExceptionOrNMI(vm);
    break;
  case VM_EXIT_REASON_EXTERNAL_INTR:
    ret = HandleExternalIRQExit(vm);
    break;
  case VM_EXIT_REASON_TRIPLE_FAULT:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_INIT_SIGNAL:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_STARTUP_IPI:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_IO_SMI:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_OTHER_SMI:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_INTR_WINDOW:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_NMI_WINDOW:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_TASK_SWITCH:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_CPUID:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_INVD:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_INVLPG:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_RDPMC:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_RDTSC:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_RSM:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_VMCALL:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_VMCLEAR:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_VMLAUNCH:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_VMPTRLD:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_VMPTRST:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_VMREAD:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_VMRESUME:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_VMWRITE:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_VMXOFF:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_VMXON:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_CR_REG_ACCESSES:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_MOV_DR:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_IO_INSTR:
    ret = HandleInOutExit(vm);
    break;
  case VM_EXIT_REASON_RDMSR:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_WRMSR:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_ENTRY_FAIL_INVALID_GUEST_STATE:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_ENTRY_FAIL_MSR_LOAD:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_MWAIT:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_MONITOR:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_PAUSE:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_ENTRY_FAILURE_MACHINE_CHECK:
    ret = PanicUnhandledVMExit(vm);
    break;
  case VM_EXIT_REASON_TPR_BELOW_THRESHOLD:
    ret = PanicUnhandledVMExit(vm);
    break;
  default:
    ret = PanicUnhandledVMExit(vm);
    break;
  }
  
  
  regs = vm->registers;
  CopyInVMCSData(&(vm->vmcs));

  /*
    {
    VMCS_CLEAR(vmcs_ptr);
    }
  */

  PrintTrace("Returning from Do_VMM: %d\n", ret);
 
  return ret;
}






// simply execute the instruction that is faulting and return
static int ExecFaultingInstructionInVMM(struct VM *vm)
{
  uint_t address = GetLinearIP(vm);
  myregs = (uint_t)&(vm->registers);
  

  PrintTrace("About the execute faulting instruction!\n");
  PrintTrace("Instruction is:\n");
  PrintTraceMemDump((void*)(address),vm->vmcs.exitInfoFields.instrLength);
  

  PrintTrace("The template code is:\n");
  PrintTraceMemDump(&&template_code,TEMPLATE_CODE_LEN);

  // clone the template code
  //memcpy(&&template_code,code,MAX_CODE);
  
  // clean up the nop field
  memset(&&template_code+INSTR_OFFSET_START,*((uchar_t *)(&&template_code+0)),NOP_SEQ_LEN);
  // overwrite the nops with the faulting instruction
  memcpy(&&template_code+INSTR_OFFSET_START, (void*)(address),vm->vmcs.exitInfoFields.instrLength);
  
  PrintTrace("Finished modifying the template code, which now is:\n");
  PrintTraceMemDump(&&template_code,TEMPLATE_CODE_LEN);

  PrintTrace("Now entering modified template code\n");


 template_code:
  // Template code stores current registers,
  // restores registers, has a landing pad of noops 
  // that will be modified, restores current regs, and then returns
  //
  // Note that this currently ignores cr0, cr3, cr4, dr7, rsp, rip, and rflags
  // it also blythly assumes it can exec the instruction in protected mode
  //
  __asm__ __volatile__ ("nop\n"               // for cloning purposes                          (1 byte)
			"pusha\n"             // push our current regs onto the current stack  (1 byte)
			"movl %0, %%eax\n"    // Get oldesp location                           (5 bytes)
			"movl %%esp, (%%eax)\n"  // store the current stack pointer in oldesp       (2 bytes)
                        "movl %1, %%eax\n"    // Get regs location                             (5 bytes)
			"movl (%%eax), %%esp\n"  // point esp at regs                               (2 bytes)
			"popa\n"              // now we have the VM registers restored            (1 byte)
			"nop\n"               // now we execute the actual instruction         (1 byte x 10)
			"nop\n"               // now we execute the actual instruction
			"nop\n"               // now we execute the actual instruction
			"nop\n"               // now we execute the actual instruction
			"nop\n"               // now we execute the actual instruction
			"nop\n"               // now we execute the actual instruction
			"nop\n"               // now we execute the actual instruction
			"nop\n"               // now we execute the actual instruction
			"nop\n"               // now we execute the actual instruction
			"nop\n"               // now we execute the actual instruction
			// need to copy back to the VM registers!
                        "movl %0, %%eax\n"     // recapture oldesp location                     (5 bytes)
			"movl (%%eax), %%esp\n"   // now we'll get our esp back from oldesp       (2 bytes)
			"popa\n"              // and restore our GP regs and we're done       (1 byte)
			: "=m"(oldesp)
			: "m"(myregs)
			);
  
  PrintTrace("Survived executing the faulting instruction and returning.\n");

  vm->vmcs.guestStateArea.rip += vm->vmcs.exitInfoFields.instrLength;

  return 0;

}

