/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2012, Kyle C. Hale <kh@u.northwestern.edu> 
 * Copyright (c) 2012, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2012, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Authors: Kyle C. Hale <kh@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vm_guest.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_hypercall.h>
#include <palacios/vmm_types.h>

#include <interfaces/vmm_host_hypercall.h>


#define GET_SET_GPR_IMPL(R) \
  static uint64_t get_##R(palacios_core_t core) { return ((struct guest_info *)core)->vm_regs.R;} \
  static void set_##R(palacios_core_t core, uint64_t val) { ((struct guest_info *)core)->vm_regs.R = val; } 

#define GET_SET_CR_IMPL(R) \
  static uint64_t get_##R(palacios_core_t core) { return ((struct guest_info *)core)->ctrl_regs.R;} \
  static void set_##R(palacios_core_t core, uint64_t val) { ((struct guest_info *)core)->ctrl_regs.R = val; } 

#define DECL_IT(R) .get_##R = get_##R, .set_##R = set_##R, 

GET_SET_GPR_IMPL(rax)
GET_SET_GPR_IMPL(rbx)
GET_SET_GPR_IMPL(rcx)
GET_SET_GPR_IMPL(rdx)
GET_SET_GPR_IMPL(rsi)
GET_SET_GPR_IMPL(rdi)
GET_SET_GPR_IMPL(rbp)
GET_SET_GPR_IMPL(rsp)
GET_SET_GPR_IMPL(r8)
GET_SET_GPR_IMPL(r9)
GET_SET_GPR_IMPL(r10)
GET_SET_GPR_IMPL(r11)
GET_SET_GPR_IMPL(r12)
GET_SET_GPR_IMPL(r13)
GET_SET_GPR_IMPL(r14)
GET_SET_GPR_IMPL(r15)

static uint64_t get_rip(palacios_core_t core) { return ((struct guest_info *)core)->rip;} 

static void set_rip(palacios_core_t core, uint64_t val) { ((struct guest_info *)core)->rip = val; } 


GET_SET_CR_IMPL(cr0)
GET_SET_CR_IMPL(cr2)
GET_SET_CR_IMPL(cr3)
GET_SET_CR_IMPL(cr4)
GET_SET_CR_IMPL(apic_tpr)
GET_SET_CR_IMPL(efer)
GET_SET_CR_IMPL(rflags)



static struct guest_accessors guest_acc = {
DECL_IT(rax)
DECL_IT(rbx)
DECL_IT(rcx)
DECL_IT(rdx)
DECL_IT(rsi)
DECL_IT(rdi)
DECL_IT(rbp)
DECL_IT(rsp)
DECL_IT(r8)
DECL_IT(r9)
DECL_IT(r10)
DECL_IT(r11)
DECL_IT(r12)
DECL_IT(r13)
DECL_IT(r14)
DECL_IT(r15)

DECL_IT(rip)
DECL_IT(cr0)
DECL_IT(cr2)
DECL_IT(cr3)
DECL_IT(cr4)
DECL_IT(apic_tpr)
DECL_IT(efer)
DECL_IT(rflags)

.gva_to_hva = (int (*)(palacios_core_t, uint64_t, uint64_t *)) v3_gva_to_hva,
.gpa_to_hva = (int (*)(palacios_core_t, uint64_t, uint64_t *)) v3_gpa_to_hva,
.gva_to_gpa = (int (*)(palacios_core_t, uint64_t, uint64_t *)) v3_gva_to_gpa,
.read_gva = (int (*)(palacios_core_t, uint64_t, int, void *)) v3_read_gva_memory,
.read_gpa = (int (*)(palacios_core_t, uint64_t, int, void *)) v3_read_gpa_memory,
.write_gva = (int (*)(palacios_core_t, uint64_t, int, void *)) v3_write_gva_memory,
.write_gpa = (int (*)(palacios_core_t, uint64_t, int, void *)) v3_write_gpa_memory,

	      }  ;
	      





struct bounce_data {
  int (*hypercall)(palacios_core_t core,
		   unsigned int hcall_id,
		   struct guest_accessors *accessors,
		   void *priv_data);
  void *priv_data;
};

static int bounce(struct guest_info *core,
		  unsigned int hcall_id,
		  void *priv_data)
{
  struct bounce_data *b = (struct bounce_data *) priv_data;

  return b->hypercall(core,hcall_id,&guest_acc,b->priv_data);
}



int v3_register_host_hypercall(host_vm_info_t * vm, 
			       unsigned int hypercall_id,
			       int (*hypercall)(palacios_core_t core, 
						uint_t hcall_id, 
						struct guest_accessors *acc,
						void * priv_data),
			       void * priv_data) {

    struct bounce_data *b = V3_Malloc(sizeof(struct bounce_data));

    if (!b) { 
	PrintError("Unable to allocate in registering host hypercall\n");
	return -1;
    }
    
    b->hypercall=hypercall;
    b->priv_data=priv_data;
    
    if (v3_register_hypercall((struct v3_vm_info*) vm, 
			      hypercall_id, 
			      bounce,
			      b) < 0) {
	PrintError("Cannot register host hypercall\n");
	V3_Free(b);
	return -1;
    }
    
    return 0;
}

int v3_unregister_host_hypercall(host_vm_info_t * vm, 
				 unsigned int hypercall_id)
{
  return v3_remove_hypercall((struct v3_vm_info*)vm, hypercall_id);
}

