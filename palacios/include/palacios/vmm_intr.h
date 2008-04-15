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


typedef enum {INVALID_INTR, EXTERNAL_IRQ, NMI, EXCEPTION, SOFTWARE, VIRTUAL} intr_types_t;

struct guest_info;

struct vm_intr {
  uint_t excp_pending;
  uint_t excp_num;
  uint_t excp_error_code;
  
  /* some way to get the [A]PIC intr */

};


void init_interrupt_state(struct vm_intr * state);

int raise_exception(struct guest_info * info, uint_t excp);

int intr_pending(struct vm_intr * intr);
uint_t get_intr_number(struct vm_intr * intr);
intr_types_t get_intr_type(struct vm_intr * intr);

#endif
