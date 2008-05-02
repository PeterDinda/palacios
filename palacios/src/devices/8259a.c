#include <devices/8259a.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm.h>


typedef enum {RESET, ICW1, ICW2, ICW3, ICW4,  READY} pic_state_t;

static const uint_t MASTER_PORT1 = 0x20;
static const uint_t MASTER_PORT2 = 0x21;
static const uint_t SLAVE_PORT1 = 0xA0;
static const uint_t SLAVE_PORT2 = 0xA1;

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


  char master_irr;
  char slave_irr;
  
  char master_isr;
  char slave_isr;

  char master_icw1;
  char master_icw2;
  char master_icw3;
  char master_icw4;


  char slave_icw1;
  char slave_icw2;
  char slave_icw3;
  char slave_icw4;


  char master_imr;
  char slave_imr;
  char master_ocw2;
  char master_ocw3;
  char slave_ocw2;
  char slave_ocw3;

  pic_state_t master_state;
  pic_state_t slave_state;
};



static int pic_raise_intr(void * private_data, int irq, int error_code) {
  struct pic_internal * state = (struct pic_internal*)private_data;

  if (irq == 2) {
    irq = 9;
  }

  PrintDebug("Raising irq %d in the PIC\n", irq);

  if (irq <= 7) {
    state->master_irr |= 0x01 << irq;
  } else if ((irq > 7) && (irq < 16)) {
    state->slave_irr |= 0x01 << (irq - 7);
  } else {
    PrintDebug("Invalid IRQ raised (%d)\n", irq);
    return -1;
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
  struct pic_internal * state = (struct pic_internal*)private_data;
  int i;

  for (i = 0; i < 16; i++) {
    if (i <= 7) {
      if (((state->master_irr & ~(state->master_imr)) >> i) == 0x01) {
	state->master_isr |= (0x1 << i);
	// reset the irr
	state->master_irr &= ~(0x1 << i);
	PrintDebug("IRQ: %d, icw2: %x\n", i, state->master_icw2);
	return i + state->master_icw2;
      }
    } else {
      if (((state->slave_irr & ~(state->slave_imr)) >> (i - 8)) == 0x01) {
	state->slave_isr |= (0x1 << (i - 8));
	state->slave_irr &= ~(0x1 << (i - 8));
	return (i - 8) + state->slave_icw2;
      }
    }
  }

  return 0;
}


static int pic_begin_irq(void * private_data, int irq) {

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
};




int read_master_port1(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
  struct pic_internal * state = (struct pic_internal*)dev->private_data;
  if (length != 1) {
    //error
  }
  
  if ((state->master_ocw3 & 0x03) == 0x02) {
    *(char *)dst = state->master_irr;
  } else if ((state->master_ocw3 & 0x03) == 0x03) {
    *(char *)dst = state->master_isr;
  } else {
    *(char *)dst = 0;
  }
  
  return 1;
}

int read_master_port2(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
  struct pic_internal * state = (struct pic_internal*)dev->private_data;
  if (length != 1) {
    // error
  }

  *(char *)dst = state->master_imr;

  return 1;
  
}

int read_slave_port1(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
  struct pic_internal * state = (struct pic_internal*)dev->private_data;
  if (length != 1) {
    // error
  }
  
  if ((state->slave_ocw3 & 0x03) == 0x02) {
    *(char*)dst = state->slave_irr;
  } else if ((state->slave_ocw3 & 0x03) == 0x03) {
    *(char *)dst = state->slave_isr;
  } else {
    *(char *)dst = 0;
  }

  return 1;
}

int read_slave_port2(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
  struct pic_internal * state = (struct pic_internal*)dev->private_data;
  if (length != 1) {
    // error
  }

  *(char *)dst = state->slave_imr;

  return 1;
}


int write_master_port1(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
  struct pic_internal * state = (struct pic_internal*)dev->private_data;
  char cw = *(char *)src;

  if (length != 1) {
    // error
  }
  
  if (state->master_state == ICW1) {
    state->master_icw1 = cw;
    state->master_state = ICW2;

  } else if (state->master_state == READY) {
    if (IS_OCW2(cw)) {
      // handle the EOI here
      struct ocw2 * cw2 =  (struct ocw2*)&cw;

      
      if ((cw2->EOI) && (!cw2->R) && (cw2->SL)) {
	// specific EOI;
	state->master_isr &= ~(0x01 << cw2->level);
      } else if ((cw2->EOI) & (!cw2->R) && (!cw2->SL)) {
	int i;
	// Non-specific EOI
	PrintDebug("Pre ISR = %x\n", state->master_isr);
	for (i = 0; i < 8; i++) {
	  if (state->master_isr & (0x01 << i)) {
	    state->master_isr &= ~(0x01 << i);
	    break;
	  }
	}	
       	PrintDebug("Post ISR = %x\n", state->master_isr);
      } else {
	// error;
      }

      state->master_ocw2 = cw;
    } else if (IS_OCW3(cw)) {
      state->master_ocw3 = cw;
    } else {
      // error
    }
  } else {
    // error
  }

  return 1;
}

int write_master_port2(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    struct pic_internal * state = (struct pic_internal*)dev->private_data;
    char cw = *(char *)src;    

    if (length != 1) {
      //error
    }
    
    if (state->master_state == ICW2) {
      struct icw1 * cw1 = (struct icw1 *)&(state->master_icw1);

      PrintDebug("Setting ICW2 = %x\n", cw);
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

      state->master_icw3 = cw;

      if (cw1->ic4 == 1) {
	state->master_state = ICW4;
      } else {
	state->master_state = READY;
      }

    } else if (state->master_state == ICW4) {
      state->master_icw4 = cw;
      state->master_state = READY;
    } else if (state->master_state == READY) {
      state->master_imr = cw;
    } else {
      // error
    }

    return 1;
}

int write_slave_port1(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
  struct pic_internal * state = (struct pic_internal*)dev->private_data;
  char cw = *(char *)src;

  if (length != 1) {
    // error
  }

  if (state->slave_state == ICW1) {
    state->slave_icw1 = cw;
    state->slave_state = ICW2;
  } else if (state->slave_state == READY) {
    if (IS_OCW2(cw)) {
      // handle the EOI here
      struct ocw2 * cw2 =  (struct ocw2 *)&cw;
      
      if ((cw2->EOI) && (!cw2->R) && (cw2->SL)) {
	// specific EOI;
	state->slave_isr &= ~(0x01 << cw2->level);
      } else if ((cw2->EOI) & (!cw2->R) && (!cw2->SL)) {
	int i;
	// Non-specific EOI
	PrintDebug("Pre ISR = %x\n", state->slave_isr);
	for (i = 0; i < 8; i++) {
	  if (state->slave_isr & (0x01 << i)) {
	    state->slave_isr &= ~(0x01 << i);
	    break;
	  }
	}	
       	PrintDebug("Post ISR = %x\n", state->slave_isr);
      } else {
	// error;
      }

      state->slave_ocw2 = cw;
    } else if (IS_OCW3(cw)) {
      // Basically sets the IRR/ISR read flag
      state->slave_ocw3 = cw;
    } else {
      // error
    }
  } else {
    // error
  }

  return 1;
}

int write_slave_port2(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    struct pic_internal * state = (struct pic_internal*)dev->private_data;
    char cw = *(char *)src;    

    if (length != 1) {
      //error
    }

    if (state->slave_state == ICW2) {
      struct icw1 * cw1 =  (struct icw1 *)&(state->master_icw1);

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

      state->slave_icw3 = cw;

      if (cw1->ic4 == 1) {
	state->slave_state = ICW4;
      } else {
	state->slave_state = READY;
      }

    } else if (state->slave_state == ICW4) {
      state->slave_icw4 = cw;
      state->slave_state = READY;
    } else if (state->slave_state == READY) {
      state->slave_imr = cw;
    } else {
      // error
    }

    return 1;
}








int pic_init(struct vm_device * dev) {
  struct pic_internal * state = (struct pic_internal*)dev->private_data;

  set_intr_controller(dev->vm, &intr_ops, state);

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


  dev_hook_io(dev, MASTER_PORT1, &read_master_port1, &write_master_port1);
  dev_hook_io(dev, MASTER_PORT2, &read_master_port2, &write_master_port2);
  dev_hook_io(dev, SLAVE_PORT1, &read_slave_port1, &write_slave_port1);
  dev_hook_io(dev, SLAVE_PORT2, &read_slave_port2, &write_slave_port2);

  return 0;
}


int pic_deinit(struct vm_device * dev) {
  dev_unhook_io(dev, MASTER_PORT1);
  dev_unhook_io(dev, MASTER_PORT2);
  dev_unhook_io(dev, SLAVE_PORT1);
  dev_unhook_io(dev, SLAVE_PORT2);

  return 0;
}







static struct vm_device_ops dev_ops = {
  .init = pic_init,
  .deinit = pic_deinit,
  .reset = NULL,
  .start = NULL,
  .stop = NULL,
};


struct vm_device * create_pic() {
  struct pic_internal * state = NULL;
  VMMMalloc(struct pic_internal *, state, sizeof(struct pic_internal));

  struct vm_device *device = create_device("8259A", &dev_ops, state);

  return device;
}




