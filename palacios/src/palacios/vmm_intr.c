#include <palacios/vmm_intr.h>
#include <palacios/vm_guest.h>


void init_interrupt_state(struct vm_intr * state) {
  state->excp_pending = 0;
  state->excp_num = 0;
  state->excp_error_code = 0;
}

int raise_exception(struct guest_info * info, uint_t excp) {

  /* We can't stack exceptions, 
     but do we need to have some sort of priority?
  */
  if (info->intr_state.excp_pending) {
    info->intr_state.excp_pending = 1;
    info->intr_state.excp_num = excp;
  } else {
    return -1;
  }

  return 0;
}


int intr_pending(struct vm_intr * intr) {
  if (intr->excp_pending) {
    return 1;
  }

  /* Check [A]PIC */

  return 0;
}


uint_t get_intr_number(struct vm_intr * intr) {
  if (intr->excp_pending) {
    return intr->excp_num;
  } 

  /* someway to get the [A]PIC intr */

  return 0;
}


uint_t get_intr_type(struct vm_intr * intr) {
  if (intr->excp_pending) {
    return EXCEPTION;
  }

  return INVALID_INTR;
}
