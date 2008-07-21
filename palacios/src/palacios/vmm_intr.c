#include <palacios/vmm_intr.h>
#include <palacios/vmm.h>

#include <palacios/vm_guest.h>

#ifndef DEBUG_INTERRUPTS
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

void init_interrupt_state(struct guest_info * info) {
  info->intr_state.excp_pending = 0;
  info->intr_state.excp_num = 0;
  info->intr_state.excp_error_code = 0;

  info->vm_ops.raise_irq = &v3_raise_irq;
}

void set_intr_controller(struct guest_info * info, struct intr_ctrl_ops * ops, void * state) {
  info->intr_state.controller = ops;
  info->intr_state.controller_state = state;
}



// This structure is used to dispatch
// interrupts delivered to vmm via deliver interrupt to vmm 
// it is what we put into the opaque field given to 
// the host os when we install the handler
struct vmm_intr_decode { 
  void              (*handler)(struct vmm_intr_state *state);
  // This opaque is user supplied by the caller
  // of hook_irq_new
  void              *opaque;
};

int hook_irq_new(uint_t irq,
		 void (*handler)(struct vmm_intr_state *state),
		 void  *opaque)
{
  extern struct vmm_os_hooks * os_hooks;

  struct vmm_intr_decode *d = V3_Malloc(sizeof(struct vmm_intr_decode));

  if (!d) { return -1; }

  d->handler = handler;
  d->opaque = opaque;
  
  if (os_hooks->hook_interrupt_new(irq,d)) { 
    PrintError("hook_irq_new: failed to hook irq 0x%x to decode 0x%x\n", irq,d);
    return -1;
  } else {
    PrintDebug("hook_irq_new: hooked irq 0x%x to decode 0x%x\n", irq,d);
    return 0;
  }
}


void deliver_interrupt_to_vmm(struct vmm_intr_state *state)
{

  PrintDebug("deliver_interrupt_to_vmm: state=0x%x\n",state);

  struct vmm_intr_decode *d = (struct vmm_intr_decode *)(state->opaque);
  
  void *temp = state->opaque;
  state->opaque = d->opaque;

  d->handler(state);
  
  state->opaque=temp;
}


void guest_injection_irq_handler(struct vmm_intr_state *state)
{
  PrintDebug("guest_irq_injection: state=0x%x\n",state);

  struct guest_info *guest = (struct guest_info *)(state->opaque);

  PrintDebug("guest_irq_injection: injecting irq 0x%x into guest 0x%x\n",state->irq,guest);
  guest->vm_ops.raise_irq(guest,state->irq);
}


int hook_irq_for_guest_injection(struct guest_info *info, int irq)
{

  int rc = hook_irq_new(irq,
			guest_injection_irq_handler,
			info);

  if (rc) { 
    PrintError("guest_irq_injection: failed to hook irq 0x%x for guest 0x%x\n", irq,info);
    return -1;
  } else {
    PrintDebug("guest_irq_injection: hooked irq 0x%x for guest 0x%x\n", irq,info);
    return 0;
  }
}




int hook_irq(struct guest_info * info, int irq) {
  extern struct vmm_os_hooks * os_hooks;

  return os_hooks->hook_interrupt(info, irq);
}

int raise_exception_with_error(struct guest_info * info, uint_t excp, uint_t error_code) {
  struct vm_intr * intr_state = &(info->intr_state);

  if (intr_state->excp_pending == 0) {
    intr_state->excp_pending = 1;
    intr_state->excp_num = excp;
    intr_state->excp_error_code = error_code;
    intr_state->excp_error_code_valid = 1;
    PrintDebug("Raising exception with error code: %x\n", error_code);
  } else {
    PrintError("exception already pending, currently not implemented\n");
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
    PrintError("exception already pending, currently not implemented\n");
    return -1;
  }

  return 0;
}


int v3_raise_irq(struct guest_info * info, int irq) {
  // Look up PIC and resend
  V3_ASSERT(info);
  V3_ASSERT(info->intr_state.controller);
  V3_ASSERT(info->intr_state.controller->raise_intr);

  //  if ((info->intr_state.controller) && 
  //  (info->intr_state.controller->raise_intr)) {
    info->intr_state.controller->raise_intr(info->intr_state.controller_state, irq);
    //} else {
    // PrintDebug("There is no registered Interrupt Controller... (NULL POINTER)\n");
    // return -1;
    //}
  return 0;
}



int intr_pending(struct guest_info * info) {
  struct vm_intr * intr_state = &(info->intr_state);

  if (intr_state->excp_pending == 1) {
    return 1;
  } else if (intr_state->controller->intr_pending(intr_state->controller_state) == 1) {
    return 1;
  }

  /* Check [A]PIC */

  return 0;
}


uint_t get_intr_number(struct guest_info * info) {
  struct vm_intr * intr_state = &(info->intr_state);

  if (intr_state->excp_pending == 1) {
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
