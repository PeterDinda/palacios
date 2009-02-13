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


#include <devices/8259a.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm.h>

#ifndef DEBUG_PIC
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


typedef enum {RESET, ICW1, ICW2, ICW3, ICW4,  READY} pic_state_t;

static const uint_t MASTER_PORT1 = 0x20;
static const uint_t MASTER_PORT2 = 0x21;
static const uint_t SLAVE_PORT1 = 0xA0;
static const uint_t SLAVE_PORT2 = 0xA1;

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


  uchar_t master_irr;
  uchar_t slave_irr;
  
  uchar_t master_isr;
  uchar_t slave_isr;

  uchar_t master_icw1;
  uchar_t master_icw2;
  uchar_t master_icw3;
  uchar_t master_icw4;


  uchar_t slave_icw1;
  uchar_t slave_icw2;
  uchar_t slave_icw3;
  uchar_t slave_icw4;


  uchar_t master_imr;
  uchar_t slave_imr;
  uchar_t master_ocw2;
  uchar_t master_ocw3;
  uchar_t slave_ocw2;
  uchar_t slave_ocw3;

  pic_state_t master_state;
  pic_state_t slave_state;
};


static void DumpPICState(struct pic_internal *p)
{

  PrintDebug("8259 PIC: master_state=0x%x\n",p->master_state);
  PrintDebug("8259 PIC: master_irr=0x%x\n",p->master_irr);
  PrintDebug("8259 PIC: master_isr=0x%x\n",p->master_isr);
  PrintDebug("8259 PIC: master_imr=0x%x\n",p->master_imr);

  PrintDebug("8259 PIC: master_ocw2=0x%x\n",p->master_ocw2);
  PrintDebug("8259 PIC: master_ocw3=0x%x\n",p->master_ocw3);

  PrintDebug("8259 PIC: master_icw1=0x%x\n",p->master_icw1);
  PrintDebug("8259 PIC: master_icw2=0x%x\n",p->master_icw2);
  PrintDebug("8259 PIC: master_icw3=0x%x\n",p->master_icw3);
  PrintDebug("8259 PIC: master_icw4=0x%x\n",p->master_icw4);

  PrintDebug("8259 PIC: slave_state=0x%x\n",p->slave_state);
  PrintDebug("8259 PIC: slave_irr=0x%x\n",p->slave_irr);
  PrintDebug("8259 PIC: slave_isr=0x%x\n",p->slave_isr);
  PrintDebug("8259 PIC: slave_imr=0x%x\n",p->slave_imr);

  PrintDebug("8259 PIC: slave_ocw2=0x%x\n",p->slave_ocw2);
  PrintDebug("8259 PIC: slave_ocw3=0x%x\n",p->slave_ocw3);

  PrintDebug("8259 PIC: slave_icw1=0x%x\n",p->slave_icw1);
  PrintDebug("8259 PIC: slave_icw2=0x%x\n",p->slave_icw2);
  PrintDebug("8259 PIC: slave_icw3=0x%x\n",p->slave_icw3);
  PrintDebug("8259 PIC: slave_icw4=0x%x\n",p->slave_icw4);

}


static int pic_raise_intr(void * private_data, int irq) {
  struct pic_internal * state = (struct pic_internal*)private_data;

  if (irq == 2) {
    irq = 9;
    state->master_irr |= 0x04;  // PAD
  }

  PrintDebug("8259 PIC: Raising irq %d in the PIC\n", irq);

  if (irq <= 7) {
    state->master_irr |= 0x01 << irq;
  } else if ((irq > 7) && (irq < 16)) {
    state->slave_irr |= 0x01 << (irq - 8);  // PAD if -7 then irq 15=no irq
  } else {
    PrintError("8259 PIC: Invalid IRQ raised (%d)\n", irq);
    return -1;
  }

  return 0;
}


static int pic_lower_intr(void *private_data, int irq) {

  struct pic_internal *state = (struct pic_internal*)private_data;

  PrintDebug("[pic_lower_intr] IRQ line %d now low\n", irq);
  if (irq <= 7) {

    state->master_irr &= ~(1 << irq);
    if ((state->master_irr & ~(state->master_imr)) == 0) {
      PrintDebug("\t\tFIXME: Master maybe should do sth\n");
    }
  } else if ((irq > 7) && (irq < 16)) {

    state->slave_irr &= ~(1 << (irq - 8));
    if ((state->slave_irr & (~(state->slave_imr))) == 0) {
      PrintDebug("\t\tFIXME: Slave maybe should do sth\n");
    }
  }
  return 0;
}



static int pic_intr_pending(void * private_data) {
  struct pic_internal * state = (struct pic_internal*)private_data;

  if ((state->master_irr & ~(state->master_imr)) || 
      (state->slave_irr & ~(state->slave_imr))) {
    return 1;
  }

  return 0;
}

static int pic_get_intr_number(void * private_data) {
  struct pic_internal * state = (struct pic_internal *)private_data;
  int i = 0;
  int irq = -1;

  PrintDebug("8259 PIC: getnum: master_irr: 0x%x master_imr: 0x%x\n", state->master_irr, state->master_imr);
  PrintDebug("8259 PIC: getnum: slave_irr: 0x%x slave_imr: 0x%x\n", state->slave_irr, state->slave_imr);

  for (i = 0; i < 16; i++) {
    if (i <= 7) {
      if (((state->master_irr & ~(state->master_imr)) >> i) == 0x01) {
	//state->master_isr |= (0x1 << i);
	// reset the irr
	//state->master_irr &= ~(0x1 << i);
	PrintDebug("8259 PIC: IRQ: %d, master_icw2: %x\n", i, state->master_icw2);
	irq= i + state->master_icw2;
	break;
      }
    } else {
      if (((state->slave_irr & ~(state->slave_imr)) >> (i - 8)) == 0x01) {
	//state->slave_isr |= (0x1 << (i - 8));
	//state->slave_irr &= ~(0x1 << (i - 8));
	PrintDebug("8259 PIC: IRQ: %d, slave_icw2: %x\n", i, state->slave_icw2);
	irq= (i - 8) + state->slave_icw2;
	break;
      }
    }
  }

  if ((i == 15) || (i == 6)) { 
    DumpPICState(state);
  }
  
  if (i == 16) { 
    return -1;
  } else {
    return irq;
  }
}



/* The IRQ number is the number returned by pic_get_intr_number(), not the pin number */
static int pic_begin_irq(void * private_data, int irq) {
  struct pic_internal * state = (struct pic_internal*)private_data;

  if ((irq >= state->master_icw2) && (irq <= state->master_icw2 + 7)) {
    irq &= 0x7;
  } else if ((irq >= state->slave_icw2) && (irq <= state->slave_icw2 + 7)) {
    irq &= 0x7;
    irq += 8;
  } else {
    PrintError("8259 PIC: Could not find IRQ (0x%x) to Begin\n",irq);
    return -1;
  }

  if (irq <= 7) {
    if (((state->master_irr & ~(state->master_imr)) >> irq) == 0x01) {
      state->master_isr |= (0x1 << irq);
      state->master_irr &= ~(0x1 << irq);
    }
  } else {
    state->slave_isr |= (0x1 << (irq - 8));
    state->slave_irr &= ~(0x1 << (irq - 8));
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
  .raise_intr = pic_raise_intr,
  .begin_irq = pic_begin_irq,
  .lower_intr = pic_lower_intr, 

};




static int read_master_port1(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
  struct pic_internal * state = (struct pic_internal*)dev->private_data;

  if (length != 1) {
    PrintError("8259 PIC: Invalid Read length (rd_Master1)\n");
    return -1;
  }
  
  if ((state->master_ocw3 & 0x03) == 0x02) {
    *(uchar_t *)dst = state->master_irr;
  } else if ((state->master_ocw3 & 0x03) == 0x03) {
    *(uchar_t *)dst = state->master_isr;
  } else {
    *(uchar_t *)dst = 0;
  }
  
  return 1;
}

static int read_master_port2(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
  struct pic_internal * state = (struct pic_internal*)dev->private_data;

  if (length != 1) {
    PrintError("8259 PIC: Invalid Read length (rd_Master2)\n");
    return -1;
  }

  *(uchar_t *)dst = state->master_imr;

  return 1;
  
}

static int read_slave_port1(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
  struct pic_internal * state = (struct pic_internal*)dev->private_data;

  if (length != 1) {
    PrintError("8259 PIC: Invalid Read length (rd_Slave1)\n");
    return -1;
  }
  
  if ((state->slave_ocw3 & 0x03) == 0x02) {
    *(uchar_t*)dst = state->slave_irr;
  } else if ((state->slave_ocw3 & 0x03) == 0x03) {
    *(uchar_t *)dst = state->slave_isr;
  } else {
    *(uchar_t *)dst = 0;
  }

  return 1;
}

static int read_slave_port2(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
  struct pic_internal * state = (struct pic_internal*)dev->private_data;

  if (length != 1) {
    PrintError("8259 PIC: Invalid Read length  (rd_Slave2)\n");
    return -1;
  }

  *(uchar_t *)dst = state->slave_imr;

  return 1;
}


static int write_master_port1(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
  struct pic_internal * state = (struct pic_internal*)dev->private_data;
  uchar_t cw = *(uchar_t *)src;

  PrintDebug("8259 PIC: Write master port 1 with 0x%x\n",cw);

  if (length != 1) {
    PrintError("8259 PIC: Invalid Write length (wr_Master1)\n");
    return -1;
  }
  
  if (IS_ICW1(cw)) {

    PrintDebug("8259 PIC: Setting ICW1 = %x (wr_Master1)\n", cw);
    
    state->master_icw1 = cw;
    state->master_state = ICW2;

  } else if (state->master_state == READY) {
    if (IS_OCW2(cw)) {
      // handle the EOI here
      struct ocw2 * cw2 =  (struct ocw2*)&cw;

      PrintDebug("8259 PIC: Handling OCW2 = %x (wr_Master1)\n", cw);
      
      if ((cw2->EOI) && (!cw2->R) && (cw2->SL)) {
	// specific EOI;
	state->master_isr &= ~(0x01 << cw2->level);
      } else if ((cw2->EOI) & (!cw2->R) && (!cw2->SL)) {
	int i;
	// Non-specific EOI
	PrintDebug("8259 PIC: Pre ISR = %x (wr_Master1)\n", state->master_isr);
	for (i = 0; i < 8; i++) {
	  if (state->master_isr & (0x01 << i)) {
	    state->master_isr &= ~(0x01 << i);
	    break;
	  }
	}	
       	PrintDebug("8259 PIC: Post ISR = %x (wr_Master1)\n", state->master_isr);
      } else {
	PrintError("8259 PIC: Command not handled, or in error (wr_Master1)\n");
	return -1;
      }

      state->master_ocw2 = cw;
    } else if (IS_OCW3(cw)) {
      PrintDebug("8259 PIC: Handling OCW3 = %x (wr_Master1)\n", cw);
      state->master_ocw3 = cw;
    } else {
      PrintError("8259 PIC: Invalid OCW to PIC (wr_Master1)\n");
      PrintError("8259 PIC: CW=%x\n", cw);
      return -1;
    }
  } else {
    PrintError("8259 PIC: Invalid PIC State (wr_Master1)\n");
    PrintError("8259 PIC: CW=%x\n", cw);
    return -1;
  }

  return 1;
}

static int write_master_port2(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    struct pic_internal * state = (struct pic_internal*)dev->private_data;
    uchar_t cw = *(uchar_t *)src;    

    PrintDebug("8259 PIC: Write master port 2 with 0x%x\n",cw);
  
    if (length != 1) {
      PrintError("8259 PIC: Invalid Write length (wr_Master2)\n");
      return -1;
    }
    
    if (state->master_state == ICW2) {
      struct icw1 * cw1 = (struct icw1 *)&(state->master_icw1);

      PrintDebug("8259 PIC: Setting ICW2 = %x (wr_Master2)\n", cw);
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

      PrintDebug("8259 PIC: Setting ICW3 = %x (wr_Master2)\n", cw);

      state->master_icw3 = cw;

      if (cw1->ic4 == 1) {
	state->master_state = ICW4;
      } else {
	state->master_state = READY;
      }

    } else if (state->master_state == ICW4) {
      PrintDebug("8259 PIC: Setting ICW4 = %x (wr_Master2)\n", cw);
      state->master_icw4 = cw;
      state->master_state = READY;
    } else if (state->master_state == READY) {
      PrintDebug("8259 PIC: Setting IMR = %x (wr_Master2)\n", cw);
      state->master_imr = cw;
    } else {
      // error
      PrintError("8259 PIC: Invalid master PIC State (wr_Master2)\n");
      return -1;
    }

    return 1;
}

static int write_slave_port1(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
  struct pic_internal * state = (struct pic_internal*)dev->private_data;
  uchar_t cw = *(uchar_t *)src;

  PrintDebug("8259 PIC: Write slave port 1 with 0x%x\n",cw);

  if (length != 1) {
    // error
    PrintError("8259 PIC: Invalid Write length (wr_Slave1)\n");
    return -1;
  }

  if (IS_ICW1(cw)) {
    PrintDebug("8259 PIC: Setting ICW1 = %x (wr_Slave1)\n", cw);
    state->slave_icw1 = cw;
    state->slave_state = ICW2;
  } else if (state->slave_state == READY) {
    if (IS_OCW2(cw)) {
      // handle the EOI here
      struct ocw2 * cw2 =  (struct ocw2 *)&cw;

      PrintDebug("8259 PIC: Setting OCW2 = %x (wr_Slave1)\n", cw);
      
      if ((cw2->EOI) && (!cw2->R) && (cw2->SL)) {
	// specific EOI;
	state->slave_isr &= ~(0x01 << cw2->level);
      } else if ((cw2->EOI) & (!cw2->R) && (!cw2->SL)) {
	int i;
	// Non-specific EOI
	PrintDebug("8259 PIC: Pre ISR = %x (wr_Slave1)\n", state->slave_isr);
	for (i = 0; i < 8; i++) {
	  if (state->slave_isr & (0x01 << i)) {
	    state->slave_isr &= ~(0x01 << i);
	    break;
	  }
	}	
       	PrintDebug("8259 PIC: Post ISR = %x (wr_Slave1)\n", state->slave_isr);
      } else {
	PrintError("8259 PIC: Command not handled or invalid  (wr_Slave1)\n");
	return -1;
      }

      state->slave_ocw2 = cw;
    } else if (IS_OCW3(cw)) {
      // Basically sets the IRR/ISR read flag
      PrintDebug("8259 PIC: Setting OCW3 = %x (wr_Slave1)\n", cw);
      state->slave_ocw3 = cw;
    } else {
      PrintError("8259 PIC: Invalid command work (wr_Slave1)\n");
      return -1;
    }
  } else {
    PrintError("8259 PIC: Invalid State writing (wr_Slave1)\n");
    return -1;
  }

  return 1;
}

static int write_slave_port2(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    struct pic_internal * state = (struct pic_internal*)dev->private_data;
    uchar_t cw = *(uchar_t *)src;    

    PrintDebug("8259 PIC: Write slave port 2 with 0x%x\n",cw);

    if (length != 1) {
      PrintError("8259 PIC: Invalid write length (wr_Slave2)\n");
      return -1;
    }

    if (state->slave_state == ICW2) {
      struct icw1 * cw1 =  (struct icw1 *)&(state->master_icw1);

      PrintDebug("8259 PIC: Setting ICW2 = %x (wr_Slave2)\n", cw);

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

      PrintDebug("8259 PIC: Setting ICW3 = %x (wr_Slave2)\n", cw);

      state->slave_icw3 = cw;

      if (cw1->ic4 == 1) {
	state->slave_state = ICW4;
      } else {
	state->slave_state = READY;
      }

    } else if (state->slave_state == ICW4) {
      PrintDebug("8259 PIC: Setting ICW4 = %x (wr_Slave2)\n", cw);
      state->slave_icw4 = cw;
      state->slave_state = READY;
    } else if (state->slave_state == READY) {
      PrintDebug("8259 PIC: Setting IMR = %x (wr_Slave2)\n", cw);
      state->slave_imr = cw;
    } else {
      PrintError("8259 PIC: Invalid State at write (wr_Slave2)\n");
      return -1;
    }

    return 1;
}








static int pic_init(struct vm_device * dev) {
  struct pic_internal * state = (struct pic_internal*)dev->private_data;

  v3_register_intr_controller(dev->vm, &intr_ops, state);

  state->master_irr = 0;
  state->master_isr = 0;
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
  state->slave_icw1 = 0;
  state->slave_icw2 = 0;
  state->slave_icw3 = 0;
  state->slave_icw4 = 0;
  state->slave_imr = 0;
  state->slave_ocw2 = 0;
  state->slave_ocw3 = 0x02;
  state->slave_state = ICW1;


  v3_dev_hook_io(dev, MASTER_PORT1, &read_master_port1, &write_master_port1);
  v3_dev_hook_io(dev, MASTER_PORT2, &read_master_port2, &write_master_port2);
  v3_dev_hook_io(dev, SLAVE_PORT1, &read_slave_port1, &write_slave_port1);
  v3_dev_hook_io(dev, SLAVE_PORT2, &read_slave_port2, &write_slave_port2);

  return 0;
}


static int pic_deinit(struct vm_device * dev) {
  v3_dev_unhook_io(dev, MASTER_PORT1);
  v3_dev_unhook_io(dev, MASTER_PORT2);
  v3_dev_unhook_io(dev, SLAVE_PORT1);
  v3_dev_unhook_io(dev, SLAVE_PORT2);

  return 0;
}







static struct vm_device_ops dev_ops = {
  .init = pic_init,
  .deinit = pic_deinit,
  .reset = NULL,
  .start = NULL,
  .stop = NULL,
};


struct vm_device * v3_create_pic() {
  struct pic_internal * state = NULL;
  state = (struct pic_internal *)V3_Malloc(sizeof(struct pic_internal));
  V3_ASSERT(state != NULL);

  struct vm_device *device = v3_create_device("8259A", &dev_ops, state);

  return device;
}




