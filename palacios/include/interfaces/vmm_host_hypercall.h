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

#ifndef __VMM_HOST_HYPERCALL_H__
#define __VMM_HOST_HYPERCALL_H__

#include <palacios/vmm.h>

/* palacios v3_vm_info struct is opaque to the host */
typedef void * host_vm_info_t;

typedef void * palacios_core_t;


// Notice that host implementation is itself
// palacios-specific at this point.  It must be
// include the palacios-headers needed to understand
// a guest_info, etc.
//
// The idea here is to make it possible to create something
// like a linux kernel module, that is compiled against
// palacios itself, but inserted after palacios. 
// The module then make full use of palacios functions
// to manipulate guest state, as if it were a part of
// palacios
//

#define GET_SET_REG_DECL(R) \
  uint64_t (*get_##R)(palacios_core_t core); \
  void (*set_##R)(palacios_core_t core, uint64_t val); 



struct guest_accessors {
  // You can read/write the GPRs
  GET_SET_REG_DECL(rax)
  GET_SET_REG_DECL(rbx)
  GET_SET_REG_DECL(rcx)
  GET_SET_REG_DECL(rdx)
  GET_SET_REG_DECL(rsi)
  GET_SET_REG_DECL(rdi)
  GET_SET_REG_DECL(rbp)
  GET_SET_REG_DECL(rsp)
  GET_SET_REG_DECL(r8)
  GET_SET_REG_DECL(r9)
  GET_SET_REG_DECL(r10)
  GET_SET_REG_DECL(r11)
  GET_SET_REG_DECL(r12)
  GET_SET_REG_DECL(r13)
  GET_SET_REG_DECL(r14)
  GET_SET_REG_DECL(r15)
  
  GET_SET_REG_DECL(rip);
  GET_SET_REG_DECL(rflags)
  GET_SET_REG_DECL(cr0)
  GET_SET_REG_DECL(cr2)
  GET_SET_REG_DECL(cr3)
  GET_SET_REG_DECL(cr4)
  GET_SET_REG_DECL(apic_tpr)
  GET_SET_REG_DECL(efer)

  int (*gva_to_hva)(palacios_core_t core, uint64_t gva, uint64_t *hva);
  int (*gva_to_gpa)(palacios_core_t core, uint64_t gva, uint64_t *gpa);
  int (*gpa_to_hva)(palacios_core_t core, uint64_t gpa, uint64_t *hva);

  int (*read_gva)(palacios_core_t core, uint64_t addr,
		  int n, void *dest);
  int (*read_gpa)(palacios_core_t core, uint64_t addr,
		  int n, void *dest);
  
  int (*write_gva)(palacios_core_t core, uint64_t addr,
		   int n, void *src);
  int  (*write_gpa)(palacios_core_t core, uint64_t addr,
		    int n, void *src);
};



int v3_register_host_hypercall(host_vm_info_t * vm, 
			       unsigned int hypercall_id, 
			       int (*hypercall)(palacios_core_t core, 
						unsigned int hcall_id,
						struct guest_accessors *accessors,
						void *priv_data),
			       void *priv_data);

int v3_unregister_host_hypercall(host_vm_info_t *vm,
				 unsigned int hypercall_id);

#ifdef __V3VEE__

#endif /* !__V3VEE__ */
#endif

