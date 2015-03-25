/* HVM environment code and data */

/* This must match the number in vmm_hypercall.h */
#define HVM_HCALL $0xf000
	
/*
	The default installed interrupt handlers simply hcall
	and then halt.  They should never be called - this is
	a panic
*/
	
.global v3_hvm_svm_null_int_handler_start
v3_hvm_svm_null_int_handler_start:
	pushq %rax
	pushq %rbx
	pushq %rcx
	movq HVM_HCALL, %rax
	movq 24(%rsp), %rbx  /* last two words of interrupt stack */
	movq 32(%rsp), %rcx  
	vmmcall
	popq %rcx
	popq %rbx
	popq %rax
	hlt
	iretq
.global v3_hvm_svm_null_int_handler_end
v3_hvm_svm_null_int_handler_end:

.global v3_hvm_vmx_null_int_handler_start
v3_hvm_vmx_null_int_handler_start:
	pushq %rax
	pushq %rbx
	pushq %rcx
	movq HVM_HCALL, %rax
	movq 24(%rsp), %rbx /* last two words of interrupt stack */
	movq 32(%rsp), %rcx
	vmcall              /* That's right - one letter different */
	popq %rcx
	popq %rbx
	popq %rax
	hlt
	iretq
.global v3_hvm_vmx_null_int_handler_end
v3_hvm_vmx_null_int_handler_end:

	
	
	