#include <palacios/vmm_intr.h>
#include <palacios/vmm.h>

#include <palacios/vm_guest.h>


void init_interrupt_state(struct guest_info * info) {
  info->intr_state.excp_pending = 0;
  info->intr_state.excp_num = 0;
  info->intr_state.excp_error_code = 0;

  info->vm_ops.raise_irq = &raise_irq;
}

void set_intr_controller(struct guest_info * info, struct intr_ctrl_ops * ops, void * state) {
  info->intr_state.controller = ops;
  info->intr_state.controller_state = state;
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


int raise_irq(struct guest_info * info, int irq, int error_code) {
  // Look up PIC and resend
  info->intr_state.controller->raise_intr(info->intr_state.controller_state, irq, error_code);

  return 0;
}


int intr_pending(struct vm_intr * intr) {
  if (intr->excp_pending) {
    return 1;
  } else if (intr->controller->intr_pending(intr->controller_state)) {
    return 1;
  }

  /* Check [A]PIC */

  return 0;
}


uint_t get_intr_number(struct vm_intr * intr) {
  if (intr->excp_pending) {
    return intr->excp_num;
  } else if (intr->controller->intr_pending(intr->controller_state)) {
    return intr->controller->get_intr_number(intr->controller_state) + 32;
  }

  /* someway to get the [A]PIC intr */

  return 0;
}


uint_t get_intr_type(struct vm_intr * intr) {
  if (intr->excp_pending) {
    return EXCEPTION;
  } else if (intr->controller->intr_pending(intr->controller_state)) {
    return EXTERNAL_IRQ;
  }

  return INVALID_INTR;
}

int hook_irq(struct guest_info * info, int irq) {
  extern struct vmm_os_hooks * os_hooks;

  return os_hooks->hook_interrupt(info, irq);
}
