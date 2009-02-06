#ifndef __VMM_DIRECT_PAGING_H__
#define __VMM_DIRECT_PAGING_H__

#ifdef __V3VEE__

#include <palacios/vmm_mem.h>
#include <palacios/vmm_paging.h>

pde32_t * v3_create_direct_passthrough_pts(struct guest_info * guest_info);

int v3_handle_shadow_pagefault_physical_mode(struct guest_info * info, addr_t fault_addr, pf_error_t error_code);

#endif // ! __V3VEE__

#endif
