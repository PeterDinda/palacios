/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */
/* (c) 2008, The V3VEE Project <http://www.v3vee.org> */


#ifndef __SVM_H
#define __SVM_H




#ifdef __V3VEE__

#include <palacios/vmm.h>


#include <palacios/vmcb.h>
#include <palacios/vmm_util.h>

#define CPUID_FEATURE_IDS 0x80000001
#define CPUID_FEATURE_IDS_ecx_svm_avail 0x00000004

#define CPUID_SVM_REV_AND_FEATURE_IDS 0x8000000a
#define CPUID_SVM_REV_AND_FEATURE_IDS_edx_svml 0x00000004
#define CPUID_SVM_REV_AND_FEATURE_IDS_edx_np  0x00000001


#define EFER_MSR                 0xc0000080
#define EFER_MSR_svm_enable      0x00001000

/************/
/* SVM MSRs */
/************/
/* AMD Arch Vol 3, sec. 15.28, pg 420 */
/************/

// SVM VM_CR MSR 
#define SVM_VM_CR_MSR             0xc0010114
#define SVM_VM_CR_MSR_dpd         0x00000001
#define SVM_VM_CR_MSR_r_init      0x00000002
#define SVM_VM_CR_MSR_dis_a20m    0x00000004
#define SVM_VM_CR_MSR_lock        0x00000008
#define SVM_VM_CR_MSR_svmdis      0x00000010

#define SVM_IGNNE_MSR             0xc0010115

// SMM Signal Control Register 
#define SVM_SMM_CTL_MSR           0xc0010116
#define SVM_SMM_CTL_MSR_dismiss   0x00000001
#define SVM_SMM_CTL_MSR_enter     0x00000002
#define SVM_SMM_CTL_MSR_smi_cycle 0x00000004
#define SVM_SMM_CTL_MSR_exit      0x00000008
#define SVM_SMM_CTL_MSR_rsm_cycle 0x00000010

#define SVM_VM_HSAVE_PA_MSR      0xc0010117

#define SVM_KEY_MSR              0xc0010118

/******/




#define SVM_HANDLER_SUCCESS   0x0
#define SVM_HANDLER_ERROR     0x1
#define SVM_HANDLER_HALT      0x2



void Init_SVM(struct vmm_ctrl_ops * vmm_ops);
int is_svm_capable();

#endif



#endif
