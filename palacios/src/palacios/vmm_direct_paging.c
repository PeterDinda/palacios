#include <palacios/vmm_direct_paging.h>

// Inline handler functions for each cpu mode
#include "vmm_direct_paging_32.h"

#include <palacios/vmm_paging.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vm_guest.h>

pde32_t * v3_create_direct_passthrough_pts(struct guest_info * info) {
  v3_vm_cpu_mode_t mode = v3_get_cpu_mode(info);
  switch(mode) {
    case REAL:
    case PROTECTED:
      return v3_create_direct_passthrough_pts_32(info);
    case PROTECTED_PAE:
      break;
    case LONG:
      break;
    case LONG_32_COMPAT:
      break;
    default:
      PrintError("Unknown CPU Mode\n");
      break;
  }
  return NULL;
}

int v3_handle_shadow_pagefault_physical_mode(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  v3_vm_cpu_mode_t mode = v3_get_cpu_mode(info);
  switch(mode) {
    case REAL:
    case PROTECTED:
      return v3_handle_shadow_pagefault_physical_mode_32(info, fault_addr, error_code);
    case PROTECTED_PAE:
      break;
    case LONG:
      break;
    case LONG_32_COMPAT:
      break;
    default:
      PrintError("Unknown CPU Mode\n");
      break;
  }
  return -1;
}
