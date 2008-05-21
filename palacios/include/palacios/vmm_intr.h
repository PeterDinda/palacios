#ifndef __VMM_INTR_H_
#define __VMM_INTR_H_

#include <palacios/vmm_intr.h>
#include <palacios/vmm_types.h>

#define DE_EXCEPTION          0x00  
#define DB_EXCEPTION          0x01
#define NMI_EXCEPTION         0x02
#define BP_EXCEPTION          0x03
#define OF_EXCEPTION          0x04
#define BR_EXCEPTION          0x05
#define UD_EXCEPTION          0x06
#define NM_EXCEPTION          0x07
#define DF_EXCEPTION          0x08
#define TS_EXCEPTION          0x0a
#define NP_EXCEPTION          0x0b
#define SS_EXCEPTION          0x0c
#define GPF_EXCEPTION         0x0d
#define PF_EXCEPTION          0x0e
#define MF_EXCEPTION          0x10
#define AC_EXCEPTION          0x11
#define MC_EXCEPTION          0x12
#define XF_EXCEPTION          0x13
#define SX_EXCEPTION          0x1e


typedef enum {INVALID_INTR, EXTERNAL_IRQ, NMI, EXCEPTION, SOFTWARE, VIRTUAL} intr_type_t;

struct guest_info;



/* We need a way to allow the APIC/PIC to decide when they are supposed to receive interrupts...
 * Maybe a notification call when they have been turned on, to deliver irqs to them...
 * We can rehook the guest raise_irq op, to the appropriate controller
 */


struct vm_intr {

  /* We need to rework the exception state, to handle stacking */
  uint_t excp_pending;
  uint_t excp_num;
  uint_t excp_error_code_valid : 1;
  uint_t excp_error_code;
  
  struct intr_ctrl_ops * controller;
  void * controller_state;

  /* some way to get the [A]PIC intr */

};


int raise_irq(struct guest_info * info, int irq);
int hook_irq(struct guest_info * info, int irq);

struct intr_ctrl_ops {
  int (*intr_pending)(void * private_data);
  int (*get_intr_number)(void * private_data);
  int (*raise_intr)(void * private_data, int irq);
  int (*begin_irq)(void * private_data, int irq);
};



void init_interrupt_state(struct guest_info * info);
void set_intr_controller(struct guest_info * info, struct intr_ctrl_ops * ops, void * state);

int raise_exception(struct guest_info * info, uint_t excp);
int raise_exception_with_error(struct guest_info * info, uint_t excp, uint_t error_code);

int intr_pending(struct guest_info * info);
uint_t get_intr_number(struct guest_info * info);
intr_type_t get_intr_type(struct guest_info * info);

int injecting_intr(struct guest_info * info, uint_t intr_num, intr_type_t type);

/*
int start_irq(struct vm_intr * intr);
int end_irq(struct vm_intr * intr, int irq);
*/
#endif
