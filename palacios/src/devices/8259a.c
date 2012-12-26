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


static int pic_raise_intr(struct v3_vm_info * vm, void * private_data, struct v3_irq * irq) {
    struct pic_internal * state = (struct pic_internal*)private_data;
    uint8_t irq_num = irq->irq;

    if (irq_num == 2) {
	irq_num = 9;
	state->master_irr |= 0x04;
    }

    PrintDebug("8259 PIC: Raising irq %d in the PIC\n", irq_num);

    if (irq_num <= 7) {
	state->master_irr |= 0x01 << irq_num;
    } else if ((irq_num > 7) && (irq_num < 16)) {
	state->slave_irr |= 0x01 << (irq_num - 8);
    } else {
	PrintDebug("8259 PIC: Invalid IRQ raised (%d)\n", irq_num);
	return -1;
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


    PrintDebug("[pic_lower_intr] IRQ line %d now low\n", irq_num);
    if (irq_num <= 7) {

	state->master_irr &= ~(1 << irq_num);
	if ((state->master_irr & ~(state->master_imr)) == 0) {
	    PrintDebug("\t\tFIXME: Master maybe should do sth\n");
	}
    } else if ((irq_num > 7) && (irq_num < 16)) {

	state->slave_irr &= ~(1 << (irq_num - 8));
	if ((state->slave_irr & (~(state->slave_imr))) == 0) {
	    PrintDebug("\t\tFIXME: Slave maybe should do sth\n");
	}
    }
    return 0;
}



static int pic_intr_pending(struct guest_info * info, void * private_data) {
    struct pic_internal * state = (struct pic_internal*)private_data;

    if ((state->master_irr & ~(state->master_imr)) || 
	(state->slave_irr & ~(state->slave_imr))) {
	return 1;
    }

    return 0;
}

static int pic_get_intr_number(struct guest_info * info, void * private_data) {
    struct pic_internal * state = (struct pic_internal *)private_data;
    int i = 0;
    int irq = -1;

    PrintDebug("8259 PIC: getnum: master_irr: 0x%x master_imr: 0x%x\n", state->master_irr, state->master_imr);
    PrintDebug("8259 PIC: getnum: slave_irr: 0x%x slave_imr: 0x%x\n", state->slave_irr, state->slave_imr);

    for (i = 0; i < 16; i++) {
	if (i <= 7) {
	    if (((state->master_irr & ~(state->master_imr)) >> i) & 0x01) {
		//state->master_isr |= (0x1 << i);
		// reset the irr
		//state->master_irr &= ~(0x1 << i);
		PrintDebug("8259 PIC: IRQ: %d, master_icw2: %x\n", i, state->master_icw2);
		irq = i + state->master_icw2;
		break;
	    }
	} else {
	    if (((state->slave_irr & ~(state->slave_imr)) >> (i - 8)) & 0x01) {
		//state->slave_isr |= (0x1 << (i - 8));
		//state->slave_irr &= ~(0x1 << (i - 8));
		PrintDebug("8259 PIC: IRQ: %d, slave_icw2: %x\n", i, state->slave_icw2);
		irq = (i - 8) + state->slave_icw2;
		break;
	    }
	}
    }

#if 1
    if ((i == 15) || (i == 6)) { 
	DumpPICState(state);
    }
#endif
  
    if (i == 16) { 
	return -1;
    } else {
	PrintDebug("8259 PIC: get num is returning %d\n",irq);
	return irq;
    }
}



/* The IRQ number is the number returned by pic_get_intr_number(), not the pin number */
static int pic_begin_irq(struct guest_info * info, void * private_data, int irq) {
    struct pic_internal * state = (struct pic_internal*)private_data;
    
    if ((irq >= state->master_icw2) && (irq <= state->master_icw2 + 7)) {
       irq &= 0x7;
    } else if ((irq >= state->slave_icw2) && (irq <= state->slave_icw2 + 7)) {
       irq &= 0x7;
       irq += 8;
    } else {
       //    PrintError("8259 PIC: Could not find IRQ (0x%x) to Begin\n",irq);
       return -1;
    }
    
    if (irq <= 7) {
	// This should always be true: See pic_get_intr_number
       if (((state->master_irr & ~(state->master_imr)) >> irq) & 0x01) {
           state->master_isr |= (0x1 << irq);

           if (!(state->master_elcr & (0x1 << irq))) {
               state->master_irr &= ~(0x1 << irq);
           }
       } else {
	   PrintDebug("8259 PIC: (master) Ignoring begin_irq for %d since I don't own it\n", irq);
       }

    } else {
	// This should always be true: See pic_get_intr_number
	if (((state->slave_irr & ~(state->slave_imr)) >> (irq - 8)) & 0x01) {
	   state->slave_isr |= (0x1 << (irq - 8));
	   
	   if (!(state->slave_elcr & (0x1 << (irq - 8)))) {
	       state->slave_irr &= ~(0x1 << (irq - 8));
	   }
	} else {
	   PrintDebug("8259 PIC: (slave) Ignoring begin_irq for %d since I don't own it\n", irq);
	}
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
	PrintError("8259 PIC: Invalid Read length (rd_Master1)\n");
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
	PrintError("8259 PIC: Invalid Read length (rd_Master2)\n");
	return -1;
    }

    *(uint8_t *)dst = state->master_imr;

    return 1;
  
}

static int read_slave_port1(struct guest_info * core, ushort_t port, void * dst, uint_t length, void * priv_data) {
    struct pic_internal * state = (struct pic_internal *)priv_data;

    if (length != 1) {
	PrintError("8259 PIC: Invalid Read length (rd_Slave1)\n");
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
	PrintError("8259 PIC: Invalid Read length  (rd_Slave2)\n");
	return -1;
    }

    *(uint8_t *)dst = state->slave_imr;

    return 1;
}


static int write_master_port1(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    struct pic_internal * state = (struct pic_internal *)priv_data;
    uint8_t cw = *(uint8_t *)src;

    PrintDebug("8259 PIC: Write master port 1 with 0x%x\n",cw);

    if (length != 1) {
        PrintError("8259 PIC: Invalid Write length (wr_Master1)\n");
        return -1;
    }

    v3_clear_pending_intr(core);

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


		/*
		// ack the irq if requested
		if (state->irq_ack_cbs[irq].ack) {
		    state->irq_ack_cbs[irq].ack(info, irq, state->irq_ack_cbs[irq].private_data);
		}
		*/

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
            } else if (!(cw2->EOI) && (cw2->R) && (cw2->SL)) {
                PrintDebug("8259 PIC: Ignoring set-priority, priorities not implemented (level=%d, wr_Master1)\n", cw2->level);
            } else if (!(cw2->EOI) && !(cw2->R) && (cw2->SL)) {
                PrintDebug("8259 PIC: Ignoring no-op (level=%d, wr_Master1)\n", cw2->level);
	    } else {
                PrintError("8259 PIC: Command not handled, or in error (wr_Master1)\n");
                return -1;
            }

	    if (cw2->EOI) {
		if (pic_get_intr_number(core,  state) != -1) {
		    PrintError("Interrupt pending after EOI\n");
		}
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

static int write_master_port2(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    struct pic_internal * state = (struct pic_internal *)priv_data;
    uint8_t cw = *(uint8_t *)src;    

    PrintDebug("8259 PIC: Write master port 2 with 0x%x\n",cw);

    if (length != 1) {
        PrintError("8259 PIC: Invalid Write length (wr_Master2)\n");
        return -1;
    }

    v3_clear_pending_intr(core);

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
    } else if ((state->master_state == ICW1) || (state->master_state == READY)) {
        PrintDebug("8259 PIC: Setting IMR = %x (wr_Master2)\n", cw);
        state->master_imr = cw;
    } else {
        // error
        PrintError("8259 PIC: Invalid master PIC State (wr_Master2) (state=%d)\n", 
                state->master_state);
        return -1;
    }

    return 1;
}

static int write_slave_port1(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    struct pic_internal * state = (struct pic_internal *)priv_data;
    uint8_t cw = *(uint8_t *)src;

    PrintDebug("8259 PIC: Write slave port 1 with 0x%x\n",cw);

    if (length != 1) {
	// error
	PrintError("8259 PIC: Invalid Write length (wr_Slave1)\n");
	return -1;
    }

    v3_clear_pending_intr(core);

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

	    if (cw2->EOI) {
		if (pic_get_intr_number(core,  state) != -1) {
		    PrintError("Interrupt pending after EOI\n");
		}
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

static int write_slave_port2(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    struct pic_internal * state = (struct pic_internal *)priv_data;
    uint8_t cw = *(uint8_t *)src;    

    PrintDebug("8259 PIC: Write slave port 2 with 0x%x\n",cw);

    if (length != 1) {
        PrintError("8259 PIC: Invalid write length (wr_Slave2)\n");
        return -1;
    }

    v3_clear_pending_intr(core);


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
    } else if ((state->slave_state == ICW1) || (state->slave_state == READY)) {
        PrintDebug("8259 PIC: Setting IMR = %x (wr_Slave2)\n", cw);
        state->slave_imr = cw;
    } else {
        PrintError("8259 PIC: Invalid State at write (wr_Slave2)\n");
        return -1;
    }

    return 1;
}




static int read_elcr_port(struct guest_info * core, ushort_t port, void * dst, uint_t length, void * priv_data) {
    struct pic_internal * state = (struct pic_internal *)priv_data;
    
    if (length != 1) {
	PrintError("ELCR read of invalid length %d\n", length);
	return -1;
    }

    if (port == ELCR1_PORT) {
	// master
	*(uint8_t *)dst = state->master_elcr;
    } else if (port == ELCR2_PORT) {
	*(uint8_t *)dst = state->slave_elcr;
    } else {
	PrintError("Invalid port %x\n", port);
	return -1;
    }

    return length;
}


static int write_elcr_port(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    struct pic_internal * state = (struct pic_internal *)priv_data;
    
    if (length != 1) {
	PrintError("ELCR read of invalid length %d\n", length);
	return -1;
    }

    if (port == ELCR1_PORT) {
	// master
	state->master_elcr  = (*(uint8_t *)src) & state->master_elcr_mask;
    } else if (port == ELCR2_PORT) {
	state->slave_elcr  = (*(uint8_t *)src) & state->slave_elcr_mask;
    } else {
	PrintError("Invalid port %x\n", port);
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
    PrintError("Failed to save PIC\n");
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
    PrintError("Failed to load PIC\n");
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
	PrintError("Cannot allocate in init\n");
	return -1;
    }

    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, state);

    if (dev == NULL) {
	PrintError("Could not add device %s\n", dev_id);
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
	PrintError("Error hooking io ports\n");
	v3_remove_device(dev);
	return -1;
    }

    return 0;
}



device_register("8259A", pic_init);
