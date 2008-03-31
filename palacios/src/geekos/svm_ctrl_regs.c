#include <geekos/svm_ctrl_regs.h>
#include <geekos/vmm_mem.h>
#include <geekos/vmm.h>
#include <geekos/vmcb.h>
#include <geekos/vmm_emulate.h>


int handle_cr0_write(guest_info_t * info, ullong_t * new_cr0) {
  // vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t *)(info->vmm_data));
  //vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));
  

  
  /*

  if (info->cpu_mode == REAL) {
    addr_t host_addr;
    shadow_region_t * region = get_shadow_region_by_addr(&(info->mem_map), (addr_t)(info->rip));
    if (!region || (region->host_type != HOST_REGION_PHYSICAL_MEMORY)) {
      //PANIC
      return -1;
    }

    guest_paddr_to_host_paddr(region, (addr_t)(info->rip), &host_addr);
    // pa to va


    PrintDebug("Instr: %.4x\n", *(ushort_t*)host_addr);
    
    if ((*(ushort_t*)host_addr) == LMSW_EAX) {
      PrintDebug("lmsw from eax (0x%x)\n", guest_state->rax);
    }
    }*/
  return 0;
}


