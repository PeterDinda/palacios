/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */



#include <palacios/vmm_intr.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vm_guest.h>

#ifndef V3_CONFIG_DEBUG_PIC
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


typedef enum {RESET, ICW1, ICW2, ICW3, ICW4,  READY} pic_state_t;

static const uint_t MASTER_PORT1 = 0x20;
static const uint_t MASTER_PORT2 = 0x21;
static const uint_t SLAVE_PORT1 = 0xA0;
static const uint_t SLAVE_PORT2 = 0xA1;

static const uint_t ELCR1_PORT = 0x4d0;
static const uint_t ELCR2_PORT = 0x4d1;


#define IS_ICW1(x) (((x & 0x10) >> 4) == 0x1)
#define IS_OCW2(x) (((x & 0x18) >> 3) == 0x0)
#define IS_OCW3(x) (((x & 0x18) >> 3) == 0x1)


struct icw1 {
    uint_t ic4    : 1;  // ICW4 has to be read
    uint_t sngl   : 1;  // single (only one PIC)
    uint_t adi    : 1;  // call address interval
    uint_t ltim   : 1;  // level interrupt mode
    uint_t one    : 1;
    uint_t rsvd   : 3;
};


struct icw2 {
    uint_t rsvd   : 3;
    uint_t vector : 5;
};


// Each bit that is set indicates that the IR input has a slave
struct icw3_master {
    uint_t S0   : 1;
    uint_t S1   : 1;
    uint_t S2   : 1;
    uint_t S3   : 1;
    uint_t S4   : 1;
    uint_t S5   : 1;
    uint_t S6   : 1;
    uint_t S7   : 1;
};

// The ID is the Slave device ID
struct icw3_slave {
    uint_t id     : 3;
    uint_t zeroes : 5;
};

struct icw4 {
    uint_t uPM    : 1;  // 1=x86
    uint_t AEOI   : 1;  // Automatic End of Interrupt
    uint_t M_S    : 1;  // only if buffered 1=master,0=slave 
    uint_t BUF    : 1;  // buffered mode
    uint_t SFNM   : 1;  // special fully nexted mode
    uint_t zeroes : 3;
};


struct ocw1 {
    uint_t m0     : 1;
    uint_t m1     : 1;
    uint_t m2     : 1;
    uint_t m3     : 1;
    uint_t m4     : 1;
    uint_t m5     : 1;
    uint_t m6     : 1;
    uint_t m7     : 1;
};

struct ocw2 {
    uint_t level  : 3;
    uint_t cw_code : 2; // should be 00
    uint_t EOI    : 1;
    uint_t SL     : 1;
    uint_t R      : 1;
};

struct ocw3 {
    uint_t RIS    : 1;
    uint_t RR     : 1;
    uint_t P      : 1;
    uint_t cw_code : 2; // should be 01
    uint_t smm    : 1;
    uint_t esmm   : 1;
    uint_t zero2  : 1;
};


struct pic_internal {


    uint8_t master_irr;
    uint8_t slave_irr;
  
    uint8_t master_isr;
    uint8_t slave_isr;

    uint8_t master_elcr;
    uint8_t slave_elcr;
    uint8_t master_elcr_mask;
    uint8_t slave_elcr_mask;

    uint8_t master_icw1;
    uint8_t master_icw2;
    uint8_t master_icw3;
    uint8_t master_icw4;


    uint8_t slave_icw1;
    uint8_t slave_icw2;
    uint8_t slave_icw3;
    uint8_t slave_icw4;


    uint8_t master_imr;
    uint8_t slave_imr;
    uint8_t master_ocw2;
    uint8_t master_ocw3;
    uint8_t slave_ocw2;
    uint8_t slave_ocw3;

    pic_state_t master_state;
    pic_state_t slave_state;

    struct guest_info * core;

    struct {
	int (*ack)(struct guest_info * core, uint32_t irq, void * private_data);
	void * private_data;
    } irq_ack_cbs[15];


    void * router_handle;
    void * controller_handle;
};


static void DumpPICState(struct pic_internal *p)
{

    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: master_state=0x%x\n",p->master_state);
    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: master_irr=0x%x\n",p->master_irr);
    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: master_isr=0x%x\n",p->master_isr);
    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: master_imr=0x%x\n",p->master_imr);

    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: master_ocw2=0x%x\n",p->master_ocw2);
    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: master_ocw3=0x%x\n",p->master_ocw3);

    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: master_icw1=0x%x\n",p->master_icw1);
    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: master_icw2=0x%x\n",p->master_icw2);
    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: master_icw3=0x%x\n",p->master_icw3);
    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: master_icw4=0x%x\n",p->master_icw4);

    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: slave_state=0x%x\n",p->slave_state);
    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: slave_irr=0x%x\n",p->slave_irr);
    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: slave_isr=0x%x\n",p->slave_isr);
    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: slave_imr=0x%x\n",p->slave_imr);

    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: slave_ocw2=0x%x\n",p->slave_ocw2);
    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: slave_ocw3=0x%x\n",p->slave_ocw3);

    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: slave_icw1=0x%x\n",p->slave_icw1);
    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: slave_icw2=0x%x\n",p->slave_icw2);
    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: slave_icw3=0x%x\n",p->slave_icw3);
    V3_Print(VM_NONE, VCORE_NONE, "8259 PIC: slave_icw4=0x%x\n",p->slave_icw4);

}


static int pic_vec_to_irq(struct guest_info *info, struct pic_internal *state, int vec)
{
  if ((vec >= state->master_icw2) && (vec <= state->master_icw2 + 7)) {
    return vec & 0x7;
  } else if ((vec >= state->slave_icw2) && (vec <= state->slave_icw2 + 7)) {
    return (vec & 0x7) + 8;
  } else {
    // Note that this is not an error since there may also be IOAPICs 
    PrintDebug(info->vm_info, info, "8259 PIC: Cannot translate vector %d back to an IRQ I support\n",vec);
    return -1;
  }
}

static int pic_irq_to_vec(struct guest_info *info, struct pic_internal *state, int irq)
{
  if (irq<0) { 
    return -1;
  }

  // This will treat IRQ2 as occuring on the master, 
  // not on slave IRQ9 as expected for legacy behavior
  // We shouldn't see anything attempting to raise IRQ2...
  if (irq==2) { 
    PrintError(info->vm_info, info, "8259 PIC: Warning - IRQ 2 is being translated...\n");
  }

  if (irq<=7) { 
    return irq + state->master_icw2;
  } else if (irq<=15) { 
    return (irq-8) + state->slave_icw2;
  } else {
    PrintDebug(info->vm_info, info, "8259 PIC: Warning: IRQ %d is being translated, but only IRQs 0..15 are supported\n",irq);
    return -1;
  }
    
}


static int pic_raise_intr(struct v3_vm_info * vm, void * private_data, struct v3_irq * irq) {
    struct pic_internal * state = (struct pic_internal*)private_data;
    uint8_t irq_num = irq->irq;

    if (irq_num == 2) {
        PrintError(vm, VCORE_NONE, "8259 PIC: Warning - IRQ 2 is being raised...\n");
        // This is the legacy reroute of IRQ2 to IRQ9
	irq_num = 9;
    }

    PrintDebug(vm, VCORE_NONE, "8259 PIC: Raising irq %d in the PIC\n", irq_num);

    if (irq_num <= 7) {
      state->master_irr |= 0x01 << irq_num;
      PrintDebug(vm, VCORE_NONE, "8259 PIC: Master: Raising IRQ %d\n",irq_num);
    } else if ((irq_num > 7) && (irq_num < 16)) {
      state->slave_irr |= 0x01 << (irq_num - 8);
      state->master_irr |= 0x04;   // immediately signal to the master pin we're attached to
      PrintDebug(vm, VCORE_NONE, "8259 PIC: Master + Slave: Raising IRQ %d\n",irq_num);
    } else {
      // This is not an error as the system could have other interrupt controllers
      PrintDebug(vm, VCORE_NONE, "8259 PIC: Ignoring raise of IRQ %d as it is not supported by the PIC\n", irq_num);
      return 0;
    }

    state->irq_ack_cbs[irq_num].ack = irq->ack;
    state->irq_ack_cbs[irq_num].private_data = irq->private_data;

    if (V3_Get_CPU() != vm->cores[0].pcpu_id) {
	// guest is running on another core, interrupt it to deliver irq
	v3_interrupt_cpu(vm, 0, 0);
    }

    return 0;
}


static int pic_lower_intr(struct v3_vm_info * vm, void * private_data, struct v3_irq * irq) {
    struct pic_internal * state = (struct pic_internal*)private_data;
    uint8_t irq_num = irq->irq;

    if (irq_num == 2) {
      PrintError(vm, VCORE_NONE, "8259 PIC: Warning - IRQ 2 is being lowered...\n");
      // Legacy reroute of IRQ2 to IRQ9
      irq_num = 9;
    }
    
    PrintDebug(vm, VCORE_NONE, "8259 PIC: [pic_lower_intr] IRQ line %d now low\n", irq_num);

    if (irq_num <= 7) {
        // master
        PrintDebug(vm, VCORE_NONE, "8259 PIC: Master: IRQ line %d lowered\n", irq_num);
	state->master_irr &= ~(1 << irq_num);
	// Note that another interrupt may still be in the IRR, but that's OK
	// We'll recognize it on the next entry
    } else if ((irq_num > 7) && (irq_num < 16)) {
        // slave
        PrintDebug(vm, VCORE_NONE, "8259 PIC: Slave: IRQ line %d lowered\n", irq_num);
	state->slave_irr &= ~(1 << (irq_num - 8));
	if ((state->slave_irr & (~(state->slave_imr))) == 0) {
	  // If there is no other slave interrupt available, we can
	  // turn off IRQ2 on the master
          PrintDebug(vm, VCORE_NONE, "8259 PIC: Master: IRQ line 2 also lowered due to no other interrupts pending in slave\n");
	  state->master_irr &= ~(0x04);
	}
    } else {
      // This is not an error as the system could have other interrupt controllers
      PrintDebug(vm, VCORE_NONE, "8259 PIC: Ignoring lower of IRQ %d as it is not supported by the PIC\n",irq_num);
    }

    return 0;
}



static int pic_intr_pending_from_master(struct guest_info * info, void * private_data) {
    struct pic_internal * state = (struct pic_internal*)private_data;

    return state->master_irr        // interrupt pending in the master's irr
	& (~(state->master_imr))    // and is not masked in the master 
	& (~(state->master_icw3));  // and the pin is not hooked to slave
}

static int pic_intr_pending_from_slave(struct guest_info * info, void * private_data) {
    struct pic_internal * state = (struct pic_internal*)private_data;

    return (!(state->master_imr & 0x4)) &&                // master has slave unmasked and
              (state->slave_irr & (~(state->slave_imr))); // slave is pending
}

static int pic_intr_pending(struct guest_info * info, void * private_data) {

    return pic_intr_pending_from_master(info,private_data) || 
           pic_intr_pending_from_slave(info,private_data);
}

/*
  8259 prioritization is oddball since there are two chips.  The 
  slave chip signals an interrupt through pin 2 of the master chip.  
  This means that all the slave chip's pins are actually at a higher priority
  than pins 3..7 of the master.   The scheme is as follows, from highest
  to lowest priority, including legacy mappings:
 
  Master  Slave    Typical Legacy Use
  --------------------------------------------------------------
  IRQ0             Timer (8254)
  IRQ1             Keyboard (8042)
  IRQ2              ****NOT USED - Slave chip inputs here
          IRQ8     RTC
          IRQ9     VGA / previous IRQ2 (or PCI via PIRQ LINK B)
          IRQ10    unused (or PCI via PIRQ LINK C)
          IRQ11    unused (or PCI via PIRQ LINK D)
          IRQ12    PS/2 Mouse (8042)
          IRQ13    Coprocessor error
          IRQ14    First IDE controller
          IRQ15    Second IDE controller
  IRQ3             Second and Fourth Serial Port (COM2/4)
  IRQ4             First and Third serial port (COM1/3)
  IRQ5             Second Parallel Port (or PCI via PIRQ LINK A)
  IRQ6             Floppy controller
  IRQ7             First Parallel Port

*/

static int pic_get_intr_number(struct guest_info * info, void * private_data) {
    struct pic_internal * state = (struct pic_internal *)private_data;
    int i = 0;
    int vec = -1;

    PrintDebug(info->vm_info, info, "8259 PIC: getnum: master_irr: 0x%x master_imr: 0x%x\n", state->master_irr, state->master_imr);
    PrintDebug(info->vm_info, info, "8259 PIC: getnum: slave_irr: 0x%x slave_imr: 0x%x\n", state->slave_irr, state->slave_imr);

    // First, see if we have something upstream of the slave
    for (i=0;i<2;i++) {
      // Interrupt requested and not masked
      if (((state->master_irr & ~(state->master_imr)) >> i) & 0x01) {
	PrintDebug(info->vm_info, info, "8259 PIC: IRQ: %d, master_icw2: %x\n", i, state->master_icw2);
	vec = pic_irq_to_vec(info, state, i);
	if (vec<0) { 
	  PrintError(info->vm_info, info, "8259 PIC: Master Interrupt Ready, but vector=%d\n",vec); 
	}
	break;
      }
    }
    
    // Next, the slave
    if (vec<0 &&                      // Nothing upstream and
	!(state->master_imr & 0x4)) { // Master is not masking the slave 
      for (i = 8; i < 16; i++) {
	if (((state->slave_irr & ~(state->slave_imr)) >> (i - 8)) & 0x01) {
	  PrintDebug(info->vm_info, info, "8259 PIC: IRQ: %d, slave_icw2: %x\n", i, state->slave_icw2);
	  vec = pic_irq_to_vec(info, state, i);
	  if (vec<0) { 
	    PrintError(info->vm_info, info, "8259 PIC: Slave Interrupt Readby, but vector=%d\n",vec); 
	  }
	  break;
	}
      }
    }

    // And finally the master downstream of the slave
    if (vec<0) {
      for (i = 3; i < 8; i++) {
	if (((state->master_irr & ~(state->master_imr)) >> i) & 0x01) {
	  PrintDebug(info->vm_info, info, "8259 PIC: IRQ: %d, master_icw2: %x\n", i, state->master_icw2);
	  vec = pic_irq_to_vec(info, state, i);
	  if (vec<0) { 
	    PrintError(info->vm_info, info, "8259 PIC: Master Interrupt Ready in 2nd pass, but vector=%d\n",vec);
	  }
	  break;
	}
      }
    }
  
    if (vec>=0) {
      PrintDebug(info->vm_info, info, "8259 PIC: get num is returning vector %d\n",vec);
    } else {
      PrintDebug(info->vm_info, info, "8259 PIC: no vector available\n");
    }
    
    return vec;
}




/* The vec number is the number returned by pic_get_irq_number(), not the pin number. */
/* In other words, it's the INT vector the PIC is feeding the processor                */
static int pic_begin_irq(struct guest_info * info, void * private_data, int vec) {
  struct pic_internal * state = (struct pic_internal*)private_data;
  int irq;
  
  irq = pic_vec_to_irq(info,state,vec);
  
  if (irq<0) { 
    // Not an error - could be for other interrupt controller
    PrintDebug(info->vm_info,info,"8259 PIC: Ignoring begin_irq on vector %d since it's not ours\n", vec);
    return 0;
  }
  
  if (irq <= 7) {
    // Master
    PrintDebug(info->vm_info, info, "8259 PIC: Master: Beginning IRQ %d\n",irq);
    // This should always be true: See pic_get_irq_number
    if (((state->master_irr & (~(state->master_imr))) >> irq) & 0x01) {
      // unmasked - let's start it
      state->master_isr |= (0x1 << irq);
      // auto reset the request if the elcr has this as edge-triggered
      if (!(state->master_elcr & (0x1 << irq))) {
	state->master_irr &= ~(0x1 << irq);
      }
    } else {
      PrintDebug(info->vm_info, info, "8259 PIC: Master: Ignoring begin_irq vector %d since I either do not see it set or have it masked (mnaster_irr=0x%x, master_imr=0x%x\n", irq, state->master_irr, state->master_imr);
    }
  } else if (irq>=8 && irq<=15)  {
    // Slave
    PrintDebug(info->vm_info, info, "8259 PIC: Master + Slave: Beginning IRQ %d\n",irq);
    // This should always be true: See pic_get_irq_number
    if (((state->slave_irr & (~(state->slave_imr))) >> (irq - 8)) & 0x01) {
      // unmasked - so let's start it in the slave
      state->slave_isr |= (0x1 << (irq - 8));
      // We must have previously pushed it to the master's irr,
      // so all we need to do here is put it in service there too
      state->master_isr |= 0x4; // pin 2 is where the slave attaches
      
      // auto-reset the request in the slave if it's marked as edge-triggered
      if (!(state->slave_elcr & (0x1 << (irq - 8)))) {
	state->slave_irr &= ~(0x1 << (irq - 8));
      }
      
      // auto-reset the request in pin 2 of the master if it's marked as edge-trigged
      if (!(state->master_elcr & 0x04)) {
	state->master_irr &= ~0x04;
      }
    } else {
      PrintDebug(info->vm_info, info, "8259 PIC: Maser + Slave: Ignoring begin_irq for %d since I either don't see it set or I don't own it (master_irr=0x%x, master_imr=0x%x, slave_irr=0x%x, slave_imr=0x%x\n", irq,state->master_irr, state->master_imr, state->slave_irr, state->slave_imr);
    }
  } else {
    PrintDebug(info->vm_info, info, "8259 PIC: Ignoring begin_irq for %d since I don't own it\n", irq);
  }

  return 0;
}


/*
  static int pic_end_irq(void * private_data, int irq) {
  return 0;
  }
*/



static struct intr_ctrl_ops intr_ops = {
    .intr_pending = pic_intr_pending,
    .get_intr_number = pic_get_intr_number,
    .begin_irq = pic_begin_irq
};

static struct intr_router_ops router_ops = {
    .raise_intr = pic_raise_intr,
    .lower_intr = pic_lower_intr
};


static int read_master_port1(struct guest_info * core, ushort_t port, void * dst, uint_t length, void * priv_data) {
    struct pic_internal * state = (struct pic_internal *)priv_data;

    if (length != 1) {
	PrintError(core->vm_info, core, "8259 PIC: Master: Invalid Read length (rd_Master1)\n");
	return -1;
    }
  
    if ((state->master_ocw3 & 0x03) == 0x02) {
	*(uint8_t *)dst = state->master_irr;
    } else if ((state->master_ocw3 & 0x03) == 0x03) {
	*(uint8_t *)dst = state->master_isr;
    } else {
	*(uint8_t *)dst = 0;
    }
  
    return 1;
}

static int read_master_port2(struct guest_info * core, ushort_t port, void * dst, uint_t length, void * priv_data) {
    struct pic_internal * state = (struct pic_internal *)priv_data;

    if (length != 1) {
	PrintError(core->vm_info, core, "8259 PIC: Master: Invalid Read length (rd_Master2)\n");
	return -1;
    }

    *(uint8_t *)dst = state->master_imr;

    return 1;
  
}

static int read_slave_port1(struct guest_info * core, ushort_t port, void * dst, uint_t length, void * priv_data) {
    struct pic_internal * state = (struct pic_internal *)priv_data;

    if (length != 1) {
	PrintError(core->vm_info, core, "8259 PIC: Slave: Invalid Read length (rd_Slave1)\n");
	return -1;
    }
  
    if ((state->slave_ocw3 & 0x03) == 0x02) {
	*(uint8_t*)dst = state->slave_irr;
    } else if ((state->slave_ocw3 & 0x03) == 0x03) {
	*(uint8_t *)dst = state->slave_isr;
    } else {
	*(uint8_t *)dst = 0;
    }

    return 1;
}

static int read_slave_port2(struct guest_info * core, ushort_t port, void * dst, uint_t length, void * priv_data) {
    struct pic_internal * state = (struct pic_internal *)priv_data;

    if (length != 1) {
	PrintError(core->vm_info, core, "8259 PIC: Slave: Invalid Read length  (rd_Slave2)\n");
	return -1;
    }

    *(uint8_t *)dst = state->slave_imr;

    return 1;
}


static int write_master_port1(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    struct pic_internal * state = (struct pic_internal *)priv_data;
    uint8_t cw = *(uint8_t *)src;

    PrintDebug(core->vm_info, core, "8259 PIC: Master: Write port 1 with 0x%x\n",cw);

    if (length != 1) {
        PrintError(core->vm_info, core, "8259 PIC: Master: Invalid Write length (wr_Master1)\n");
        return -1;
    }

    v3_clear_pending_intr(core);

    if (IS_ICW1(cw)) {

        PrintDebug(core->vm_info, core, "8259 PIC: Master: Setting ICW1 = %x (wr_Master1)\n", cw);

        state->master_icw1 = cw;
        state->master_state = ICW2;

    } else if (state->master_state == READY) {
        if (IS_OCW2(cw)) {
            // handle the EOI here
            struct ocw2 * cw2 =  (struct ocw2*)&cw;
	    int eoi_irq;

            PrintDebug(core->vm_info, core, "8259 PIC: Master: Handling OCW2 = %x (wr_Master1)\n", cw);

            if ((cw2->EOI) && (!cw2->R) && (cw2->SL)) {
                // specific EOI;
                state->master_isr &= ~(0x01 << cw2->level);
		eoi_irq = cw2->level;

		/*
		// ack the irq if requested
		if (state->irq_ack_cbs[irq].ack) {
		    state->irq_ack_cbs[irq].ack(info, irq, state->irq_ack_cbs[irq].private_data);
		}
		*/

            } else if ((cw2->EOI) & (!cw2->R) && (!cw2->SL)) {
                int i;
                // Non-specific EOI
                PrintDebug(core->vm_info, core, "8259 PIC: Master: Pre ISR = %x (wr_Master1)\n", state->master_isr);
                for (i = 0; i < 8; i++) {
                    if (state->master_isr & (0x01 << i)) {
                        state->master_isr &= ~(0x01 << i);
			eoi_irq=i;
                        break;
                    }
                }
		if (i==8) { 
		  PrintDebug(core->vm_info, core, "8259 PIC: Master: Strange... non-specific EOI but no in-service interrupts\n");
		}
		
                PrintDebug(core->vm_info, core, "8259 PIC: Master: Post ISR = %x (wr_Master1)\n", state->master_isr);
            } else if (!(cw2->EOI) && (cw2->R) && (cw2->SL)) {
                PrintDebug(core->vm_info, core, "8259 PIC: Master: Ignoring set-priority, priorities not implemented (level=%d, wr_Master1)\n", cw2->level);
            } else if (!(cw2->EOI) && !(cw2->R) && (cw2->SL)) {
                PrintDebug(core->vm_info, core, "8259 PIC: Master: Ignoring no-op (level=%d, wr_Master1)\n", cw2->level);
	    } else {
                PrintError(core->vm_info, core, "8259 PIC: Master: Command not handled, or in error (wr_Master1)\n");
                return -1;
            }

	    if (cw2->EOI) {
	      if (pic_intr_pending_from_master(core,state)) {
		// this is perfectly fine as there may be other latched interrupts
		// but it would be strange if the one we just cleared is suddenly
		// alive again - well, depending on concurrent behavior external to 
		int irq = pic_vec_to_irq(core,state,pic_get_intr_number(core,state));

		if (irq == eoi_irq) { 
          // Not necessarily an error, since it could have been raised again in another thread...
		  PrintError(core->vm_info, core, "8259 PIC: Master: IRQ %d pending after EOI of IRQ %d\n", irq,eoi_irq);
		  DumpPICState(state);
		}
	      }
	    }
	    
            state->master_ocw2 = cw;
        } else if (IS_OCW3(cw)) {
	  PrintDebug(core->vm_info, core, "8259 PIC: Master: Handling OCW3 = %x (wr_Master1)\n", cw);
	  state->master_ocw3 = cw;
        } else {
	  PrintError(core->vm_info, core, "8259 PIC: Master: Invalid OCW to PIC (wr_Master1)\n");
	  PrintError(core->vm_info, core, "8259 PIC: Master: CW=%x\n", cw);
	  return -1;
        }
    } else {
      PrintError(core->vm_info, core, "8259 PIC: Master: Invalid PIC State (wr_Master1)\n");
      PrintError(core->vm_info, core, "8259 PIC: Master: CW=%x\n", cw);
      return -1;
    }
    
    return 1;
}

static int write_master_port2(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
  struct pic_internal * state = (struct pic_internal *)priv_data;
  uint8_t cw = *(uint8_t *)src;    
  
  PrintDebug(core->vm_info, core, "8259 PIC: Master: Write master port 2 with 0x%x\n",cw);

  if (length != 1) {
    PrintError(core->vm_info, core, "8259 PIC: Master: Invalid Write length (wr_Master2)\n");
        return -1;
    }

    v3_clear_pending_intr(core);

    if (state->master_state == ICW2) {
        struct icw1 * cw1 = (struct icw1 *)&(state->master_icw1);

        PrintDebug(core->vm_info, core, "8259 PIC: Master: Setting ICW2 = %x (wr_Master2)\n", cw);
        state->master_icw2 = cw;



        if (cw1->sngl == 0) {
            state->master_state = ICW3;
        } else if (cw1->ic4 == 1) {
            state->master_state = ICW4;
        } else {
            state->master_state = READY;
        }



    } else if (state->master_state == ICW3) {
        struct icw1 * cw1 = (struct icw1 *)&(state->master_icw1);

        PrintDebug(core->vm_info, core, "8259 PIC: Master: Setting ICW3 = %x (wr_Master2)\n", cw);

        state->master_icw3 = cw;

        if (cw1->ic4 == 1) {
            state->master_state = ICW4;
        } else {
            state->master_state = READY;
        }

    } else if (state->master_state == ICW4) {
        PrintDebug(core->vm_info, core, "8259 PIC: Master: Setting ICW4 = %x (wr_Master2)\n", cw);
        state->master_icw4 = cw;
        state->master_state = READY;
    } else if ((state->master_state == ICW1) || (state->master_state == READY)) {
        PrintDebug(core->vm_info, core, "8259 PIC: Master: Setting IMR = %x (wr_Master2)\n", cw);
        state->master_imr = cw;
    } else {
        // error
        PrintError(core->vm_info, core, "8259 PIC: Master: Invalid master PIC State (wr_Master2) (state=%d)\n", 
                state->master_state);
        return -1;
    }

    return 1;
}

static int write_slave_port1(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    struct pic_internal * state = (struct pic_internal *)priv_data;
    uint8_t cw = *(uint8_t *)src;

    PrintDebug(core->vm_info, core, "8259 PIC: Slave: Write slave port 1 with 0x%x\n",cw);

    if (length != 1) {
	// error
	PrintError(core->vm_info, core, "8259 PIC: Slave: Invalid Write length (wr_Slave1)\n");
	return -1;
    }

    v3_clear_pending_intr(core);

    if (IS_ICW1(cw)) {
	PrintDebug(core->vm_info, core, "8259 PIC: Slave: Setting ICW1 = %x (wr_Slave1)\n", cw);
	state->slave_icw1 = cw;
	state->slave_state = ICW2;
    } else if (state->slave_state == READY) {
	if (IS_OCW2(cw)) {
	    int eoi_irq;
	    // handle the EOI here
	    struct ocw2 * cw2 =  (struct ocw2 *)&cw;

	    PrintDebug(core->vm_info, core, "8259 PIC: Slave: Setting OCW2 = %x (wr_Slave1)\n", cw);

	    if ((cw2->EOI) && (!cw2->R) && (cw2->SL)) {
		// specific EOI;
		state->slave_isr &= ~(0x01 << cw2->level);
		eoi_irq = 8+cw2->level;
	    } else if ((cw2->EOI) & (!cw2->R) && (!cw2->SL)) {
		int i;
		// Non-specific EOI
		PrintDebug(core->vm_info, core, "8259 PIC: Slave: Pre ISR = %x (wr_Slave1)\n", state->slave_isr);
		for (i = 0; i < 8; i++) {
		    if (state->slave_isr & (0x01 << i)) {
			state->slave_isr &= ~(0x01 << i);
			eoi_irq=8+i;
			break;
		    }
		}
		if (i==8) { 
		  PrintDebug(core->vm_info, core, "8259 PIC: Slave: Strange... non-specific EOI but no in-service interrupts\n");
		}
		PrintDebug(core->vm_info, core, "8259 PIC: Slave: Post ISR = %x (wr_Slave1)\n", state->slave_isr);
	    } else {
		PrintError(core->vm_info, core, "8259 PIC: Slave: Command not handled or invalid  (wr_Slave1)\n");
		return -1;
	    }

	    // If we now have no further requested interrupts, 
	    // we are not requesting from the master either
	    if (!(state->slave_irr)) { 
	      state->master_irr &= ~0x04;
	    }

	    if (cw2->EOI) {
	        if (pic_intr_pending_from_slave(core,state)) {
		  // this is perfectly fine as there may be other latched interrupts
		  // but it would be strange if the one we just cleared is suddenly
		  // alive again - well, depending on concurrent behavior external to 
		  int irq = pic_vec_to_irq(core,state,pic_get_intr_number(core,state));
		  
		  if (irq == eoi_irq) { 
		    // Not necessarily an error, since it could have been raised again in another thread.
            PrintError(core->vm_info, core, "8259 PIC: Slave: IRQ %d pending after EOI of IRQ %d\n", irq,eoi_irq);
		    DumpPICState(state);
		  }
		}
	    }

	    state->slave_ocw2 = cw;
	} else if (IS_OCW3(cw)) {
	    // Basically sets the IRR/ISR read flag
	    PrintDebug(core->vm_info, core, "8259 PIC: Slave: Setting OCW3 = %x (wr_Slave1)\n", cw);
	    state->slave_ocw3 = cw;
	} else {
	    PrintError(core->vm_info, core, "8259 PIC: Slave: Invalid command work (wr_Slave1)\n");
	    return -1;
	}
    } else {
	PrintError(core->vm_info, core, "8259 PIC: Slave: Invalid State writing (wr_Slave1)\n");
	return -1;
    }

    return 1;
}

static int write_slave_port2(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    struct pic_internal * state = (struct pic_internal *)priv_data;
    uint8_t cw = *(uint8_t *)src;    

    PrintDebug(core->vm_info, core, "8259 PIC: Slave: Write slave port 2 with 0x%x\n",cw);

    if (length != 1) {
        PrintError(core->vm_info, core, "8259 PIC: Slave: Invalid write length (wr_Slave2)\n");
        return -1;
    }

    v3_clear_pending_intr(core);


    if (state->slave_state == ICW2) {
        struct icw1 * cw1 =  (struct icw1 *)&(state->master_icw1);

        PrintDebug(core->vm_info, core, "8259 PIC: Slave: Setting ICW2 = %x (wr_Slave2)\n", cw);

        state->slave_icw2 = cw;

        if (cw1->sngl == 0) {
            state->slave_state = ICW3;
        } else if (cw1->ic4 == 1) {
            state->slave_state = ICW4;
        } else {
            state->slave_state = READY;
        }

    } else if (state->slave_state == ICW3) {
        struct icw1 * cw1 =  (struct icw1 *)&(state->master_icw1);

        PrintDebug(core->vm_info, core, "8259 PIC: Slave: Setting ICW3 = %x (wr_Slave2)\n", cw);

        state->slave_icw3 = cw;

        if (cw1->ic4 == 1) {
            state->slave_state = ICW4;
        } else {
            state->slave_state = READY;
        }

    } else if (state->slave_state == ICW4) {
        PrintDebug(core->vm_info, core, "8259 PIC: Slave: Setting ICW4 = %x (wr_Slave2)\n", cw);
        state->slave_icw4 = cw;
        state->slave_state = READY;
    } else if ((state->slave_state == ICW1) || (state->slave_state == READY)) {
        PrintDebug(core->vm_info, core, "8259 PIC: Slave: Setting IMR = %x (wr_Slave2)\n", cw);
        state->slave_imr = cw;
    } else {
        PrintError(core->vm_info, core, "8259 PIC: Slave: Invalid State at write (wr_Slave2)\n");
        return -1;
    }

    return 1;
}




static int read_elcr_port(struct guest_info * core, ushort_t port, void * dst, uint_t length, void * priv_data) {
    struct pic_internal * state = (struct pic_internal *)priv_data;
    
    if (length != 1) {
	PrintError(core->vm_info, core, "8259 PIC: ELCR read of invalid length %d\n", length);
	return -1;
    }

    if (port == ELCR1_PORT) {
	// master
	*(uint8_t *)dst = state->master_elcr;
    } else if (port == ELCR2_PORT) {
	*(uint8_t *)dst = state->slave_elcr;
    } else {
	PrintError(core->vm_info, core, "8259 PIC: Invalid port %x\n", port);
	return -1;
    }

    return length;
}


static int write_elcr_port(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    struct pic_internal * state = (struct pic_internal *)priv_data;
    
    if (length != 1) {
	PrintError(core->vm_info, core, "8259 PIC: ELCR read of invalid length %d\n", length);
	return -1;
    }

    if (port == ELCR1_PORT) {
	// master
	state->master_elcr  = (*(uint8_t *)src) & state->master_elcr_mask;
    } else if (port == ELCR2_PORT) {
	state->slave_elcr  = (*(uint8_t *)src) & state->slave_elcr_mask;
    } else {
	PrintError(core->vm_info, core, "8259 PIC: Invalid port %x\n", port);
	return -1;
    }

    return length;
}



static int pic_free(struct pic_internal * state) {
    struct guest_info * core = state->core;

    v3_remove_intr_controller(core, state->controller_handle);
    v3_remove_intr_router(core->vm_info, state->router_handle);

    V3_Free(state);
    return 0;
}

#ifdef V3_CONFIG_CHECKPOINT
static int pic_save(struct v3_chkpt_ctx * ctx, void * private_data) {
    struct pic_internal * pic = (struct pic_internal *)private_data;

    V3_CHKPT_SAVE(ctx, "MASTER_IRR", pic->master_irr, savefailout);
    V3_CHKPT_SAVE(ctx, "SLAVE_IRR", pic->slave_irr, savefailout);
  
    V3_CHKPT_SAVE(ctx, "MASTER_ISR", pic->master_isr, savefailout);
    V3_CHKPT_SAVE(ctx, "SLAVE_ISR", pic->slave_isr, savefailout);

    V3_CHKPT_SAVE(ctx, "MASTER_ELCR", pic->master_elcr, savefailout);
    V3_CHKPT_SAVE(ctx, "SLAVE_ELCR", pic->slave_elcr, savefailout);
    V3_CHKPT_SAVE(ctx, "MASTER_ELCR_MASK", pic->master_elcr_mask, savefailout);
    V3_CHKPT_SAVE(ctx, "SLAVE_ELCR_MASK", pic->slave_elcr_mask, savefailout);

    V3_CHKPT_SAVE(ctx, "MASTER_ICW1", pic->master_icw1, savefailout);
    V3_CHKPT_SAVE(ctx, "MASTER_ICW2", pic->master_icw2, savefailout);
    V3_CHKPT_SAVE(ctx, "MASTER_ICW3", pic->master_icw3, savefailout);
    V3_CHKPT_SAVE(ctx, "MASTER_ICW4", pic->master_icw4, savefailout);


    V3_CHKPT_SAVE(ctx, "SLAVE_ICW1", pic->slave_icw1, savefailout);
    V3_CHKPT_SAVE(ctx, "SLAVE_ICW2", pic->slave_icw2, savefailout);
    V3_CHKPT_SAVE(ctx, "SLAVE_ICW3", pic->slave_icw3, savefailout);
    V3_CHKPT_SAVE(ctx, "SLAVE_ICW4", pic->slave_icw4, savefailout);


    V3_CHKPT_SAVE(ctx, "MASTER_IMR", pic->master_imr, savefailout);
    V3_CHKPT_SAVE(ctx, "SLAVE_IMR", pic->slave_imr, savefailout);
    V3_CHKPT_SAVE(ctx, "MASTER_OCW2", pic->master_ocw2, savefailout);
    V3_CHKPT_SAVE(ctx, "MASTER_OCW3", pic->master_ocw3, savefailout);
    V3_CHKPT_SAVE(ctx, "SLAVE_OCW2", pic->slave_ocw2, savefailout);
    V3_CHKPT_SAVE(ctx, "SLAVE_OCW3", pic->slave_ocw3, savefailout);

    V3_CHKPT_SAVE(ctx, "MASTER_STATE", pic->master_state, savefailout);
    V3_CHKPT_SAVE(ctx, "SLAVE_STATE", pic->slave_state, savefailout);

    
    return 0;

 savefailout:
    PrintError(VM_NONE, VCORE_NONE, "Failed to save PIC\n");
    return -1;

}

static int pic_load(struct v3_chkpt_ctx * ctx, void * private_data) {
    struct pic_internal * pic = (struct pic_internal *)private_data;

   
    V3_CHKPT_LOAD(ctx, "MASTER_IRR", pic->master_irr, loadfailout);
    V3_CHKPT_LOAD(ctx, "SLAVE_IRR", pic->slave_irr, loadfailout);
  
    V3_CHKPT_LOAD(ctx, "MASTER_ISR", pic->master_isr, loadfailout);
    V3_CHKPT_LOAD(ctx, "SLAVE_ISR", pic->slave_isr, loadfailout);

    V3_CHKPT_LOAD(ctx, "MASTER_ELCR", pic->master_elcr, loadfailout);
    V3_CHKPT_LOAD(ctx, "SLAVE_ELCR", pic->slave_elcr, loadfailout);
    V3_CHKPT_LOAD(ctx, "MASTER_ELCR_MASK", pic->master_elcr_mask, loadfailout);
    V3_CHKPT_LOAD(ctx, "SLAVE_ELCR_MASK", pic->slave_elcr_mask, loadfailout);

    V3_CHKPT_LOAD(ctx, "MASTER_ICW1", pic->master_icw1, loadfailout);
    V3_CHKPT_LOAD(ctx, "MASTER_ICW2", pic->master_icw2, loadfailout);
    V3_CHKPT_LOAD(ctx, "MASTER_ICW3", pic->master_icw3, loadfailout);
    V3_CHKPT_LOAD(ctx, "MASTER_ICW4", pic->master_icw4, loadfailout);


    V3_CHKPT_LOAD(ctx, "SLAVE_ICW1", pic->slave_icw1, loadfailout);
    V3_CHKPT_LOAD(ctx, "SLAVE_ICW2", pic->slave_icw2, loadfailout);
    V3_CHKPT_LOAD(ctx, "SLAVE_ICW3", pic->slave_icw3, loadfailout);
    V3_CHKPT_LOAD(ctx, "SLAVE_ICW4", pic->slave_icw4, loadfailout);


    V3_CHKPT_LOAD(ctx, "MASTER_IMR", pic->master_imr, loadfailout);
    V3_CHKPT_LOAD(ctx, "SLAVE_IMR", pic->slave_imr, loadfailout);
    V3_CHKPT_LOAD(ctx, "MASTER_OCW2", pic->master_ocw2, loadfailout);
    V3_CHKPT_LOAD(ctx, "MASTER_OCW3", pic->master_ocw3, loadfailout);
    V3_CHKPT_LOAD(ctx, "SLAVE_OCW2", pic->slave_ocw2, loadfailout);
    V3_CHKPT_LOAD(ctx, "SLAVE_OCW3", pic->slave_ocw3, loadfailout);

    V3_CHKPT_LOAD(ctx, "MASTER_STATE", pic->master_state, loadfailout);
    V3_CHKPT_LOAD(ctx, "SLAVE_STATE", pic->slave_state, loadfailout);

    return 0;

 loadfailout:
    PrintError(VM_NONE, VCORE_NONE, "Failed to load PIC\n");
    return -1;
}

#endif


static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))pic_free,
#ifdef V3_CONFIG_CHECKPOINT
    .save = pic_save,
    .load = pic_load
#endif
};





static int pic_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct pic_internal * state = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");
    int ret = 0;

    // PIC is only usable in non-multicore environments
    // just hardcode the core context
    struct guest_info * core = &(vm->cores[0]);
	
    state = (struct pic_internal *)V3_Malloc(sizeof(struct pic_internal));

    if (!state) {
        PrintError(vm, VCORE_NONE, "8259 PIC: Cannot allocate in init\n");
	return -1;
    }

    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, state);

    if (dev == NULL) {
	PrintError(vm, VCORE_NONE, "8259 PIC: Could not add device %s\n", dev_id);
	V3_Free(state);
	return -1;
    }

    state->core = core;

    state->controller_handle = v3_register_intr_controller(core, &intr_ops, state);
    state->router_handle = v3_register_intr_router(vm, &router_ops, state);

    state->master_irr = 0;
    state->master_isr = 0;
    state->master_elcr = 0;
    state->master_elcr_mask = 0xf8;
    state->master_icw1 = 0;
    state->master_icw2 = 0;
    state->master_icw3 = 0;
    state->master_icw4 = 0;
    state->master_imr = 0;
    state->master_ocw2 = 0;
    state->master_ocw3 = 0x02;
    state->master_state = ICW1;


    state->slave_irr = 0;
    state->slave_isr = 0;
    state->slave_elcr = 0;
    state->slave_elcr_mask = 0xde;
    state->slave_icw1 = 0;
    state->slave_icw2 = 0;
    state->slave_icw3 = 0;
    state->slave_icw4 = 0;
    state->slave_imr = 0;
    state->slave_ocw2 = 0;
    state->slave_ocw3 = 0x02;
    state->slave_state = ICW1;


    ret |= v3_dev_hook_io(dev, MASTER_PORT1, &read_master_port1, &write_master_port1);
    ret |= v3_dev_hook_io(dev, MASTER_PORT2, &read_master_port2, &write_master_port2);
    ret |= v3_dev_hook_io(dev, SLAVE_PORT1, &read_slave_port1, &write_slave_port1);
    ret |= v3_dev_hook_io(dev, SLAVE_PORT2, &read_slave_port2, &write_slave_port2);


    ret |= v3_dev_hook_io(dev, ELCR1_PORT, &read_elcr_port, &write_elcr_port);
    ret |= v3_dev_hook_io(dev, ELCR2_PORT, &read_elcr_port, &write_elcr_port);

    if (ret != 0) {
        PrintError(vm, VCORE_NONE, "8259 PIC: Error hooking io ports\n");
	v3_remove_device(dev);
	return -1;
    }

    return 0;
}



device_register("8259A", pic_init);
