
static int activate_shadow_pt_64(struct guest_info * info) {
  //  struct cr3_64 * shadow_cr3 = (struct cr3_64 *)&(info->ctrl_regs.cr3);
 
  return -1;
}






/* 
 * *
 * * 
 * * 64 bit Page table fault handlers
 * *
 * *
 */

static int handle_shadow_pagefault_64(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  pt_access_status_t guest_access;
  pt_access_status_t shadow_access;
  int ret; 
  PrintDebug("64 bit shadow page fault\n");

  ret = v3_check_guest_pt_32(info, info->shdw_pg_state.guest_cr3, fault_addr, error_code, &guest_access);

  PrintDebug("Guest Access Check: %d (access=%d)\n", ret, guest_access);

  ret = v3_check_host_pt_32(info->ctrl_regs.cr3, fault_addr, error_code, &shadow_access);

  PrintDebug("Shadow Access Check: %d (access=%d)\n", ret, shadow_access);
  

  PrintError("64 bit shadow paging not implemented\n");
  return -1;
}
