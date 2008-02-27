#include <geekos/vmx.h>
#include <geekos/vmcs.h>
#include <geekos/mem.h>
#include <geekos/serial.h>
#include <geekos/segment.h>
#include <geekos/gdt.h>
#include <geekos/idt.h>


#include <geekos/cpu.h>
#include <geekos/io_devs.h>


extern void Get_MSR(unsigned int msr, uint_t * high, uint_t * low);
extern void Set_MSR(unsigned int msr, uint_t high, uint_t low);
extern int Enable_VMX(ullong_t regionPtr);
extern int cpuid_ecx(unsigned int op);
extern int Launch_VM(ullong_t vmcsPtr, uint_t eip);

#define NUMPORTS 65536


#define VMXASSIST_INFO_PORT   0x0e9
#define ROMBIOS_PANIC_PORT    0x400
#define ROMBIOS_PANIC_PORT2   0x401
#define ROMBIOS_INFO_PORT     0x402
#define ROMBIOS_DEBUG_PORT    0x403



static struct VM theVM;

static uint_t GetLinearIP(struct VM *vm)
{
  if (vm->state==VM_VMXASSIST_V8086_BIOS || vm->state==VM_VMXASSIST_V8086) { 
    return vm->vmcs.guestStateArea.cs.baseAddr + vm->vmcs.guestStateArea.rip;
  } else {
    return vm->vmcs.guestStateArea.rip;
  }
}


static void VMXPanic()
{
  while (1) {}
}


#define MAX_CODE 512
#define INSTR_OFFSET_START 17
#define NOP_SEQ_LEN        10
#define INSTR_OFFSET_END   (INSTR_OFFSET_START+NOP_SEQ_LEN-1)
#define TEMPLATE_CODE_LEN  35

uint_t oldesp=0;
uint_t myregs=0;

// simply execute the instruction that is faulting and return
static int ExecFaultingInstructionInVMM(struct VM *vm)
{
  uint_t address = GetLinearIP(vm);
  myregs = (uint_t)&(vm->registers);
  

  SerialPrintLevel(1000,"About the execute faulting instruction!\n");
  SerialPrintLevel(1000,"Instruction is:\n");
  SerialMemDump((void*)(address),vm->vmcs.exitInfoFields.instrLength);
  

  SerialPrintLevel(1000,"The template code is:\n");
  SerialMemDump(&&template_code,TEMPLATE_CODE_LEN);

  // clone the template code
  //memcpy(&&template_code,code,MAX_CODE);
  
  // clean up the nop field
  memset(&&template_code+INSTR_OFFSET_START,*((uchar_t *)(&&template_code+0)),NOP_SEQ_LEN);
  // overwrite the nops with the faulting instruction
  memcpy(&&template_code+INSTR_OFFSET_START, (void*)(address),vm->vmcs.exitInfoFields.instrLength);
  
  SerialPrintLevel(1000,"Finished modifying the template code, which now is:\n");
  SerialMemDump(&&template_code,TEMPLATE_CODE_LEN);

  SerialPrintLevel(1000,"Now entering modified template code\n");


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
  
  SerialPrintLevel(1000,"Survived executing the faulting instruction and returning.\n");

  vm->vmcs.guestStateArea.rip += vm->vmcs.exitInfoFields.instrLength;

  return 0;

}


int is_vmx_capable() {
  uint_t ret;
  union VMX_MSR featureMSR;
  
  ret = cpuid_ecx(1);
  if (ret & CPUID_1_ECX_VTXFLAG) {
    Get_MSR(IA32_FEATURE_CONTROL_MSR, &featureMSR.regs.high, &featureMSR.regs.low);

    SerialPrintLevel(100,"MSRREGlow: 0x%.8x\n", featureMSR.regs.low);

    if ((featureMSR.regs.low & FEATURE_CONTROL_VALID) != FEATURE_CONTROL_VALID) {
      PrintBoth("VMX is locked -- enable in the BIOS\n");
      return 0;
    }
  } else {
    PrintBoth("VMX not supported on this cpu\n");
    return 0;
  }

  return 1;

}


VmxOnRegion * Init_VMX() {
  uint_t ret;
  VmxOnRegion * region = NULL;


  region = CreateVmxOnRegion();


  ret = Enable_VMX((ullong_t)((uint_t)region));
  if (ret == 0) {
    PrintBoth("VMX Enabled\n");
  } else {
    PrintBoth("VMX failure (ret = %d)\n", ret);
  }

  theVM.vmxonregion = region;

  return region;
}

extern uint_t VMCS_CLEAR();
extern uint_t VMCS_LOAD();
extern uint_t VMCS_STORE();
extern uint_t VMCS_LAUNCH();
extern uint_t VMCS_RESUME();
extern uint_t Init_VMCS_HostState();
extern uint_t Init_VMCS_GuestState();

void SetCtrlBitsCorrectly(int msrno, int vmcsno)
{
  uint_t reserved =0;
  union VMX_MSR msr;

  SerialPrintLevel(100,"SetCtrlBitsCorrectly(%x,%x)\n",msrno,vmcsno);
  Get_MSR(msrno, &msr.regs.high, &msr.regs.low);
  SerialPrintLevel(100,"MSR %x = %x : %x \n", msrno, msr.regs.high, msr.regs.low);
  reserved = msr.regs.low;
  reserved &= msr.regs.high;
  VMCS_WRITE(vmcsno, &reserved);
}


void SetCRBitsCorrectly(int msr0no, int msr1no, int vmcsno)
{
  uint_t reserved =0;
  union VMX_MSR msr0, msr1;

  SerialPrintLevel(100,"SetCRBitsCorrectly(%x,%x,%x)\n",msr0no,msr1no,vmcsno);
  Get_MSR(msr0no, &msr0.regs.high, &msr0.regs.low);
  Get_MSR(msr1no, &msr1.regs.high, &msr1.regs.low);
  SerialPrintLevel(100,"MSR %x = %x, %x =  %x \n", msr0no, msr0.regs.low, msr1no, msr1.regs.low);
  reserved = msr0.regs.low;
  reserved &= msr1.regs.low;
  VMCS_WRITE(vmcsno, &reserved);
}


extern int Get_CR2();
extern int vmRunning;


static int PanicUnhandledVMExit(struct VM *vm)
{
  Print("Panicking due to VMExit with reason %u\n",vm->vmcs.exitInfoFields.reason);
  SerialPrint("Panicking due to VMExit with reason %u\n",vm->vmcs.exitInfoFields.reason);
  SerialPrint_VMCS_ALL();
  SerialPrint_VMX_Regs(&(vm->registers));
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
    SerialPrintLevel(1000,"VMXASSIST Output Port\n");
    PrintBoth("%c",data&0xff);
    return 1;
  } 

  if ((port==ROMBIOS_PANIC_PORT || 
       port==ROMBIOS_PANIC_PORT2 || 
       port==ROMBIOS_DEBUG_PORT ||
       port==ROMBIOS_INFO_PORT) &&
      (vm->state==VM_VMXASSIST_V8086_BIOS)) {
    // rombios is communicating
    SerialPrintLevel(1000,"ROMBIOS Output Port\n");
    //    PrintBoth("%c",data&0xff);
    return 1;
  }

  if (port==BOOT_STATE_CARD_PORT && vm->state==VM_VMXASSIST_V8086_BIOS) { 
    // rombios is sending something to the display card
    SerialPrintLevel(1000,"Hex Display: 0x%x\n",data&0xff);
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

  SerialPrintLevel(1000,"Handling Input/Output Instruction Exit\n");
  if (SERIAL_PRINT_DEBUG && 1000>=SERIAL_PRINT_DEBUG_LEVEL) {
      SerialPrint_VMX_Regs(regs);
  }
  SerialPrintLevel(1000,"Qualifications=0x%x\n",exitinfo->qualification);
  SerialPrintLevel(1000,"Reason=0x%x\n",exitinfo->reason);
  SerialPrintLevel(1000,"IO Port: 0x%x (%d)\n", qual->port, qual->port);
  SerialPrintLevel(1000,"Instruction Info=%x\n",exitinfo->instrInfo);
  SerialPrintLevel(1000,"%x : %s %s %s instruction of length %d for %d bytes from/to port 0x%x\n",
		   address,
		   qual->dir == 0 ? "output" : "input",
		   qual->string ==0 ? "nonstring" : "STRING",
		   qual->REP == 0 ? "with no rep" : "WITH REP",
		   exitinfo->instrLength, 
		   qual->accessSize==0 ? 1 : qual->accessSize==1 ? 2 : 4,
		   qual->port);

  if (qual->port==PIC_MASTER_CMD_ISR_PORT ||
      qual->port==PIC_MASTER_IMR_PORT ||
      qual->port==PIC_SLAVE_CMD_ISR_PORT ||
      qual->port==PIC_SLAVE_IMR_PORT) {
    SerialPrintLevel(1000, "PIC Access\n");
  }
                  

  if (qual->dir==1 && qual->REP==0 && qual->string==0) { 
    char byte = In_Byte(qual->port);

    vm->vmcs.guestStateArea.rip += exitinfo->instrLength;
    regs->eax = (regs->eax & 0xffffff00) | byte;
    SerialPrintLevel(1000,"Returning 0x%x in eax\n",(regs->eax));
  }

  if (qual->dir==0 && qual->REP==0 && qual->string==0) { 
    // See if we need to handle the outb as a signal or
    // print from the VM
    if (HandleVMPrintsAndPanics(vm,qual->port,regs->eax)) {
    } else {
      // If not, just go ahead and do the outb
      Out_Byte(qual->port,regs->eax);
      SerialPrintLevel(1000,"Wrote 0x%x to port\n",(regs->eax));
    }
    vm->vmcs.guestStateArea.rip += exitinfo->instrLength;
  }

  return 0;
}  


static int HandleExternalIRQExit(struct VM *vm)
{
  struct VMCSExitInfoFields * exitinfo = &(vm->vmcs.exitInfoFields);
  struct VMExitIntInfo * intInfo  = (struct VMExitIntInfo *)&(vm->vmcs.exitInfoFields.intInfo);

  SerialPrintLevel(1000,"External Interrupt captured\n");
  SerialPrintLevel(100,"IntInfo: %x\n", exitinfo->intInfo);


  if (!intInfo->valid) {
     // interrupts are off, but this interrupt is not acknoledged (still pending)
     // so we turn on interrupts to deliver appropriately in the
     // host
    SerialPrintLevel(100,"External Interrupt is invald.  Turning Interrupts back on\n");
    asm("sti");
    return 0;
  } 

  // At this point, interrupts are off and the interrupt has been 
  // acknowledged.  We will now handle the interrupt ourselves 
  // and turn interrupts  back on in the host

  SerialPrintLevel(100,"type: %d\n", intInfo->type);
  SerialPrintLevel(100,"number: %d\n", intInfo->nr);

  SerialPrint("Interrupt %d occuring now and handled by HandleExternalIRQExit\n",intInfo->nr);

  switch (intInfo->type) {
  case 0:  {  // ext. IRQ
    // In the following, we construct an "int x" instruction
    // where x is the specific interrupt number that is raised
    // then we execute that instruciton
    // because we are in host context, that means it is delivered as normal
    // through the host IDT
     
     ((char*)(&&ext_int_seq_start))[1] = intInfo->nr;
 
     SerialPrintLevel(100,"Interrupt instruction setup done %x\n", *((ushort_t *)(&&ext_int_seq_start)));
     
ext_int_seq_start:
     asm("int $0");
  }

    break;
  case 2: // NMI
    SerialPrintLevel(100,"Type: NMI\n");
    break;
  case 3: // hw exception
    SerialPrintLevel(100,"Type: HW Exception\n");
    break;
  case 4: // sw exception
    SerialPrintLevel(100,"Type: SW Exception\n");
    break;
  default:
    SerialPrintLevel(100,"Invalid Interrupt Type\n");
    return -1;
  }
  
  if (intInfo->valid && intInfo->errorCode) {
    SerialPrintLevel(100,"IntError: %x\n", exitinfo->intErrorCode);
  }


  return 0;

}



void DecodeCurrentInstruction(struct VM *vm, struct Instruction *inst)
{
  // this is a gruesome hack
  uint_t address = GetLinearIP(vm);
  uint_t length = vm->vmcs.exitInfoFields.instrLength;
  unsigned char *t = (unsigned char *) address;


  
  SerialPrintLevel(100,"DecodeCurrentInstruction: instruction is\n");
  SerialMemDump(t,length);
  
  if (length==3 && t[0]==0x0f && t[1]==0x22 && t[2]==0xc0) { 
    // mov from eax to cr0
    // usually used to signal
    inst->type=VM_MOV_TO_CR0;
    inst->address=address;
    inst->size=length;
    inst->input1=vm->registers.eax;
    inst->input2=vm->vmcs.guestStateArea.cr0;
    inst->output=vm->registers.eax;
    SerialPrintLevel(100,"MOV FROM EAX TO CR0\n");
  } else {
    inst->type=VM_UNKNOWN_INST;
  }
}


static void V8086ModeSegmentRegisterFixup(struct VM *vm)
{
  vm->vmcs.guestStateArea.cs.baseAddr=vm->vmcs.guestStateArea.cs.selector<<4;
  vm->vmcs.guestStateArea.es.baseAddr=vm->vmcs.guestStateArea.es.selector<<4;
  vm->vmcs.guestStateArea.ss.baseAddr=vm->vmcs.guestStateArea.ss.selector<<4;
  vm->vmcs.guestStateArea.ds.baseAddr=vm->vmcs.guestStateArea.ds.selector<<4;
  vm->vmcs.guestStateArea.fs.baseAddr=vm->vmcs.guestStateArea.fs.selector<<4;
  vm->vmcs.guestStateArea.gs.baseAddr=vm->vmcs.guestStateArea.gs.selector<<4;
}

static void SetupV8086ModeForBoot(struct VM *vm)
{
  vm->state = VM_VMXASSIST_V8086_BIOS;

  // Put guest into V8086 mode on return
  vm->vmcs.guestStateArea.rflags |= EFLAGS_VM | EFLAGS_IOPL_HI | EFLAGS_IOPL_LO ;
  
  // We will start at f000:fff0 on return
  //
  // We want this to look as much as possible as a processor
  // reset
  vm->vmcs.guestStateArea.rip = 0xfff0;  // note, 16 bit rip
  vm->vmcs.guestStateArea.cs.selector = 0xf000;
  vm->vmcs.guestStateArea.cs.limit=0xffff;
  vm->vmcs.guestStateArea.cs.access.as_dword = 0xf3;

  vm->vmcs.guestStateArea.ss.selector = 0x0000;
  vm->vmcs.guestStateArea.ss.limit=0xffff;
  vm->vmcs.guestStateArea.ss.access.as_dword = 0xf3;

  vm->vmcs.guestStateArea.ds.selector = 0x0000;
  vm->vmcs.guestStateArea.ds.limit=0xffff;
  vm->vmcs.guestStateArea.ds.access.as_dword = 0xf3;

  vm->vmcs.guestStateArea.es.selector = 0x0000;
  vm->vmcs.guestStateArea.es.limit=0xffff;
  vm->vmcs.guestStateArea.es.access.as_dword = 0xf3;

  vm->vmcs.guestStateArea.fs.selector = 0x0000;
  vm->vmcs.guestStateArea.fs.limit=0xffff;
  vm->vmcs.guestStateArea.fs.access.as_dword = 0xf3;

  vm->vmcs.guestStateArea.gs.selector = 0x0000;
  vm->vmcs.guestStateArea.gs.limit=0xffff;
  vm->vmcs.guestStateArea.gs.access.as_dword = 0xf3;
  
  V8086ModeSegmentRegisterFixup(vm);

  SerialPrint_VMCSData(&(vm->vmcs));

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

  SerialPrintLevel(1000,"Exception or NMI occurred\n");
  
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
  
  SerialPrint("Exception %d now - handled by HandleExceptionOrNMI\n",num);

  SerialPrintLevel(1000,"Exception Number %u : %s\n", num, exception_names[num]);
  SerialPrintLevel(1000,"Exception Type %u : %s\n", type, exception_type_names[type]);
  if (errorvalid) { 
    if (ext) { 
      SerialPrintLevel(1000,"External\n");
    } else {
      SerialPrintLevel(1000,"%s - Selector Index is %u\n", idt ? "IDT" : ti ? "LDT" : "GDT", selectorindex);
    }
  }

  DecodeCurrentInstruction(vm,&inst);

  if (inst.type==VM_MOV_TO_CR0) {
    SerialPrintLevel(1000,"MOV TO CR0, oldvalue=0x%x, newvalue=0x%x\n",inst.input2, inst.input1);
    if ((inst.input2 & CR0_PE) && !(inst.input1 & CR0_PE) && vm->state==VM_VMXASSIST_STARTUP) {
      // This is VMXAssist signalling for us to turn on V8086 mode and
      // jump into the bios
      SerialPrintLevel(1000,"VMXAssist is signaling us for switch to V8086 mode and jump to 0xf000:fff0\n");
      SetupV8086ModeForBoot(vm);
      goto leave;
    } else {
      SerialPrintLevel(1000,"Instruction is a write to CR0, but we don't understand it so we'll just exec it\n");
    } 
  } 


  SerialPrintLevel(1000,"Trying to execute the faulting instruction in VMM context now\n");
  ExecFaultingInstructionInVMM(vm);

    leave:
  //
  //PanicUnhandledVMExit(vmcs,regs);
  //VMXPanic();
  return 0;
}


static struct VM *FindVM()
{
  return &theVM;
}


int Do_VMM(struct VMXRegs regs) 
{

  ullong_t vmcs_ptr = 0;
  uint_t vmcs_ptr_low = 0;
  int ret = 0;
  uint_t vmx_abort = 0;


  
  SerialPrintLevel(100,"Vm Exit\n");
  ret = VMCS_STORE(&vmcs_ptr);
  vmcs_ptr &= 0xffffffff;
  vmcs_ptr_low +=  vmcs_ptr;




  SerialPrintLevel(100,"ret=%d\n", ret);
  SerialPrintLevel(100,"Revision: %x\n", *(uint_t *)(vmcs_ptr_low));
  vmx_abort = *(uint_t*)(((char *)vmcs_ptr_low)+4);
    
  struct VM *vm = FindVM();

  if (vmx_abort != 0) {
    SerialPrintLevel(1000,"VM ABORTED w/ code: %x\n", vmx_abort);
    return -1;
  }

  vm->registers = regs;

  if (CopyOutVMCSData(&(vm->vmcs)) != 0) {
    SerialPrintLevel(1000,"Could not copy out VMCS\n");
    return -1;
  }


  SerialPrint("Guest esp: 0x%x (%u)\n", vm->vmcs.guestStateArea.rsp, vm->vmcs.guestStateArea.rsp);

  SerialPrintLevel(100,"VM Exit for reason: %d (%x)\n", 
	      vm->vmcs.exitInfoFields.reason & 0x00000fff,
	      vm->vmcs.exitInfoFields.reason);  

  if (vm->vmcs.exitInfoFields.reason & (0x1<<29) ) { 
    SerialPrintLevel(1000,"VM Exit is from VMX root operation.  Panicking\n");
    VMXPanic();
  }

  if (vm->vmcs.exitInfoFields.reason & (0x1<<31) ) { 
    SerialPrintLevel(1000,"VM Exit is due to a VM entry failure.  Shouldn't happen here. Panicking\n");
    SerialPrint_VMCSData(&(vm->vmcs));
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

  SerialPrintLevel(100,"Returning from Do_VMM: %d\n", ret);
 
  return ret;
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
    |INVLPG_EXITING           
    |MWAIT_EXITING            
    |RDPMC_EXITING           
    |RDTSC_EXITING         
    |MOVDR_EXITING         
    |UNCONDITION_IO_EXITING
    |MONITOR_EXITING       
    |PAUSE_EXITING         ;

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

  SerialPrint("Guest ESP: 0x%x (%u)\n", guest_esp, guest_esp);

  exit_eip=(uint_t)RunVMM;

  SerialPrintLevel(100,"Clear\n");
  VMCS_CLEAR(vmcs);
  SerialPrintLevel(100,"Load\n");
  VMCS_LOAD(vmcs);


  SerialPrintLevel(100,"VMCS_LINK_PTR\n");
  VMCS_WRITE(VMCS_LINK_PTR, &f);
  SerialPrintLevel(100,"VMCS_LINK_PTR_HIGH\n");
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
  SerialPrintLevel(100,"Setting up host state\n");
  SetCRBitsCorrectly(IA32_VMX_CR0_FIXED0_MSR, IA32_VMX_CR0_FIXED1_MSR, HOST_CR0);
  SetCRBitsCorrectly(IA32_VMX_CR4_FIXED0_MSR, IA32_VMX_CR4_FIXED1_MSR, HOST_CR4);
  ret = Init_VMCS_HostState();

  if (ret != VMX_SUCCESS) {
    if (ret == VMX_FAIL_VALID) {
      SerialPrintLevel(100,"Init Host state: VMCS FAILED WITH ERROR\n");
    } else {
      SerialPrintLevel(100,"Init Host state: Invalid VMCS\n");
    }
    return ret;
  }

  //  SerialPrintLevel(100,"HOST_RIP: %x (%u)\n", exit_eip, exit_eip);
  VMCS_WRITE(HOST_RIP, &exit_eip);

  /* Guest state */
  SerialPrintLevel(100,"Setting up guest state\n");
  SerialPrintLevel(100,"GUEST_RIP: %x (%u)\n", entry_eip, entry_eip);
  VMCS_WRITE(GUEST_RIP,&entry_eip);

  SetCRBitsCorrectly(IA32_VMX_CR0_FIXED0_MSR, IA32_VMX_CR0_FIXED1_MSR, GUEST_CR0);
  SetCRBitsCorrectly(IA32_VMX_CR4_FIXED0_MSR, IA32_VMX_CR4_FIXED1_MSR, GUEST_CR4);
  ret = Init_VMCS_GuestState();

  SerialPrintLevel(100,"InitGuestState returned\n");
  if (ret != VMX_SUCCESS) {
    if (ret == VMX_FAIL_VALID) {
      SerialPrintLevel(100,"Init Guest state: VMCS FAILED WITH ERROR\n");
    } else {
      SerialPrintLevel(100,"Init Guest state: Invalid VMCS\n");
    }
    return ret;
  }
  SerialPrintLevel(100,"GUEST_RSP: %x (%u)\n", guest_esp, (uint_t)guest_esp);
  VMCS_WRITE(GUEST_RSP,&guest_esp);

  //  tmpReg = 0x4100;
  tmpReg = 0xffffffff;
  if (VMCS_WRITE(EXCEPTION_BITMAP,&tmpReg ) != VMX_SUCCESS) {
    Print("Bitmap error\n");
  }

  ConfigureExits(vm);

  SerialPrintLevel(100,"VMCS_LAUNCH\n");

  vm->state=VM_VMXASSIST_STARTUP;

  vmm_ret = SAFE_VM_LAUNCH();

  SerialPrintLevel(100,"VMM error %d\n", vmm_ret);

  return vmm_ret;
}



  
int VMLaunch(struct VMDescriptor *vm) 
{
  VMCS * vmcs = CreateVMCS();
  int rc;

  ullong_t vmcs_ptr = (ullong_t)((uint_t)vmcs);
  uint_t top = (vmcs_ptr>>32)&0xffffffff;
  uint_t bottom = (vmcs_ptr)&0xffffffff;

  theVM.vmcsregion = vmcs;
  theVM.descriptor = *vm;

  SerialPrintLevel(100,"vmcs_ptr_top=%x vmcs_ptr_bottom=%x, eip=%x\n", top, bottom, vm->entry_ip);
  rc=MyLaunch(&theVM); // vmcs_ptr, vm->entry_ip, vm->exit_eip, vm->guest_esp);
  SerialPrintLevel(100,"Returned from MyLaunch();\n");
  return rc;
}


VmxOnRegion * CreateVmxOnRegion() {
  union VMX_MSR basicMSR;
  VmxOnRegion * region = (VmxOnRegion *)Alloc_Page();

  Get_MSR(IA32_VMX_BASIC_MSR, &basicMSR.regs.high, &basicMSR.regs.low);
  //  memcpy(region, &basicMSR.vmxBasic.revision, sizeof(uint_t));

  *(ulong_t*)region = basicMSR.vmxBasic.revision;

  Print("VMX revision: 0x%lu\n", *(ulong_t *)region);

  return region;
}

VMCS * CreateVMCS() {
  union VMX_MSR basicMSR;
  VMCS * vmcs = (VMCS *)Alloc_Page();

  Get_MSR(IA32_VMX_BASIC_MSR, &basicMSR.regs.high, &basicMSR.regs.low);
  *(ulong_t *)vmcs = basicMSR.vmxBasic.revision;
  *(ulong_t *)((char*)vmcs + 4) = 0;

  SerialPrintLevel(100,"VMCS Region size: %u\n", basicMSR.vmxBasic.regionSize);
  SerialPrintLevel(100,"VMCS Abort: %x\n",*(uint_t *)(((char*)vmcs)+4));

  return vmcs;
}
