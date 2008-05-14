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









int hook_irq(struct guest_info * info, int irq) {
  extern struct vmm_os_hooks * os_hooks;

  return os_hooks->hook_interrupt(info, irq);
}

int raise_exception_with_error(struct guest_info * info, uint_t excp, uint_t error_code) {
  struct vm_intr * intr_state = &(info->intr_state);

  if (intr_state->excp_pending) {
    intr_state->excp_pending = 1;
    intr_state->excp_num = excp;
    intr_state->excp_error_code = error_code;
    intr_state->excp_error_code_valid = 1;
  } else {
    return -1;
  }

  return 0;
}

int raise_exception(struct guest_info * info, uint_t excp) {
  struct vm_intr * intr_state = &(info->intr_state);

  if (intr_state->excp_pending == 0) {
    intr_state->excp_pending = 1;
    intr_state->excp_num = excp;
    intr_state->excp_error_code = 0;
    intr_state->excp_error_code_valid = 0;
  } else {
    return -1;
  }

  return 0;
}


int raise_irq(struct guest_info * info, int irq, int error_code) {
  // Look up PIC and resend
  V3_ASSERT(info);
  V3_ASSERT(info->intr_state.controller);
  V3_ASSERT(info->intr_state.controller->raise_intr);

  //  if ((info->intr_state.controller) && 
  //  (info->intr_state.controller->raise_intr)) {
    info->intr_state.controller->raise_intr(info->intr_state.controller_state, irq, error_code);
    //} else {
    // PrintDebug("There is no registered Interrupt Controller... (NULL POINTER)\n");
    // return -1;
    //}
  return 0;
}











int intr_pending(struct guest_info * info) {
  struct vm_intr * intr_state = &(info->intr_state);

  if (intr_state->excp_pending) {
    return 1;
  } else if (intr_state->controller->intr_pending(intr_state->controller_state)) {
    return 1;
  }

  /* Check [A]PIC */

  return 0;
}


uint_t get_intr_number(struct guest_info * info) {
  struct vm_intr * intr_state = &(info->intr_state);

  if (intr_state->excp_pending) {
    return intr_state->excp_num;
  } else if (intr_state->controller->intr_pending(intr_state->controller_state)) {
    return intr_state->controller->get_intr_number(intr_state->controller_state);
  }

  /* someway to get the [A]PIC intr */

  return 0;
}


intr_type_t get_intr_type(struct guest_info * info) {
 struct vm_intr * intr_state = &(info->intr_state);

  if (intr_state->excp_pending) {
    return EXCEPTION;
  } else if (intr_state->controller->intr_pending(intr_state->controller_state)) {
    return EXTERNAL_IRQ;
  }

  return INVALID_INTR;
}






int injecting_intr(struct guest_info * info, uint_t intr_num, intr_type_t type) {
  struct vm_intr * intr_state = &(info->intr_state);

  if (type == EXCEPTION) {

    intr_state->excp_pending = 0;
    intr_state->excp_num = 0;
    intr_state->excp_error_code = 0;
    intr_state->excp_error_code_valid = 0;
    
  } else if (type == EXTERNAL_IRQ) {
    return intr_state->controller->begin_irq(intr_state->controller_state, intr_num);
  }

  return 0;
}
