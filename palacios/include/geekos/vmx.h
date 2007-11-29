#ifndef __VMX_H
#define __VMX_H

#include <geekos/ktypes.h>
#include <geekos/vmcs.h>

#define IA32_FEATURE_CONTROL_MSR ((unsigned int)0x3a)
#define IA32_VMX_BASIC_MSR ((unsigned int)0x480)
#define IA32_VMX_PINBASED_CTLS_MSR ((unsigned int)0x481)
#define IA32_VMX_PROCBASED_CTLS_MSR ((unsigned int)0x482)
#define IA32_VMX_EXIT_CTLS_MSR ((unsigned int)0x483)
#define IA32_VMX_ENTRY_CTLS_MSR ((unsigned int)0x484)
#define IA32_VMX_MISC_MSR ((unsigned int)0x485)
#define IA32_VMX_CR0_FIXED0_MSR ((unsigned int)0x486)
#define IA32_VMX_CR0_FIXED1_MSR ((unsigned int)0x487)
#define IA32_VMX_CR4_FIXED0_MSR ((unsigned int)0x488)
#define IA32_VMX_CR4_FIXED1_MSR ((unsigned int)0x489)
#define IA32_VMX_VMCS_ENUM_MSR ((unsigned ing)0x48A)

#define VMX_SUCCESS         0
#define VMX_FAIL_INVALID   1
#define VMX_FAIL_VALID     2
#define VMM_ERROR          3

#define FEATURE_CONTROL_LOCK (1)
#define FEATURE_CONTROL_VMXON (1<<2)
#define FEATURE_CONTROL_VALID ( FEATURE_CONTROL_LOCK | FEATURE_CONTROL_VMXON)


#define CPUID_1_ECX_VTXFLAG (1<<5)





typedef void VmxOnRegion;

#if __TINYC__
#define PACKED
#else
#define PACKED __attribute__((packed))
#endif


struct MSR_REGS {
  uint_t low PACKED;
  uint_t high  PACKED;
};

struct VMX_BASIC {
  uint_t revision          PACKED ;
  uint_t regionSize   : 13 PACKED ;
  uint_t rsvd1        : 4  PACKED ; // Always 0
  uint_t physWidth    : 1  PACKED ;
  uint_t smm          : 1  PACKED ; // Always 1
  uint_t memType      : 4  PACKED ;
  uint_t rsvd2        : 10 PACKED ; // Always 0
};

union VMX_MSR {
  struct MSR_REGS regs PACKED;
  struct VMX_BASIC vmxBasic PACKED;
};


struct VMDescriptor {
  uint_t   entry_ip;
  uint_t   exit_eip;
  uint_t   guest_esp;
} ;


enum VMState { VM_VMXASSIST_STARTUP, VM_VMXASSIST_V8086_BIOS, VM_VMXASSIST_V8086, VM_NORMAL };

struct VM {
  enum VMState        state;
  struct VMXRegs      registers;
  struct VMDescriptor descriptor;
  struct VMCSData     vmcs;
  struct VMCS         *vmcsregion;
  struct VmxOnRegion  *vmxonregion;
};


enum InstructionType { VM_UNKNOWN_INST, VM_MOV_TO_CR0 } ;

struct Instruction {
  enum InstructionType type;
  uint_t          address;
  uint_t          size;
  uint_t          input1;
  uint_t          input2;
  uint_t          output;
};


void DecodeCurrentInstruction(struct VM *vm, struct Instruction *out);


VmxOnRegion * InitVMX();
VmxOnRegion * CreateVmxOnRegion();

int VMLaunch(struct VMDescriptor *vm);


int Do_VMM(struct VMXRegs regs);




#endif 
