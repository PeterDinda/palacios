/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National
 * Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at
 * http://www.v3vee.org
 *
 * Copyright (c) 2010, Rumou Duan <duanrumou@gmail.com>
 * Copyright (c) 2010, The V3VEE Project <http://www.v3vee.org>
 * All rights reserved.
 *
 * Author: Rumou Duan <duanrumou@gmail.com>
 *         Lei Xia <lxia@northwestern.edu>
 &         Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_types.h>

#include <palacios/vmm_ringbuffer.h>
#include <palacios/vmm_lock.h>
#include <palacios/vmm_intr.h>
#include <palacios/vm_guest.h>

#include <devices/serial.h>


#ifndef V3_CONFIG_DEBUG_SERIAL
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

/*

  This is an implementation of a 16450 and 16550A-compatible UART (fifo-capable), based on
  the TI 16550D spec (http://www.ti.com/lit/ds/symlink/pc16550d.pdf) and the Altera
  16450 spec (ftp://ftp.altera.com/pub/lit_req/document/ds/ds16450.pdf)

*/

// This is not yet tested - enable at your own risk!
#define BE_16550A  0  



#define COM1_DATA_PORT           0x3f8
#define COM1_IRQ_ENABLE_PORT     0x3f9
#define COM1_DIV_LATCH_LSB_PORT  0x3f8
#define COM1_DIV_LATCH_MSB_PORT  0x3f9
#define COM1_IIR_PORT            0x3fa
#define COM1_FIFO_CTRL_PORT      0x3fa
#define COM1_LINE_CTRL_PORT      0x3fb
#define COM1_MODEM_CTRL_PORT     0x3fc
#define COM1_LINE_STATUS_PORT    0x3fd
#define COM1_MODEM_STATUS_PORT   0x3fe
#define COM1_SCRATCH_PORT        0x3ff

#define COM2_DATA_PORT           0x2f8
#define COM2_IRQ_ENABLE_PORT     0x2f9
#define COM2_DIV_LATCH_LSB_PORT  0x2f8
#define COM2_DIV_LATCH_MSB_PORT  0x2f9
#define COM2_IIR_PORT            0x2fa
#define COM2_FIFO_CTRL_PORT      0x2fa
#define COM2_LINE_CTRL_PORT      0x2fb
#define COM2_MODEM_CTRL_PORT     0x2fc
#define COM2_LINE_STATUS_PORT    0x2fd
#define COM2_MODEM_STATUS_PORT   0x2fe
#define COM2_SCRATCH_PORT        0x2ff

#define COM3_DATA_PORT           0x3e8
#define COM3_IRQ_ENABLE_PORT     0x3e9
#define COM3_DIV_LATCH_LSB_PORT  0x3e8
#define COM3_DIV_LATCH_MSB_PORT  0x3e9
#define COM3_IIR_PORT            0x3ea
#define COM3_FIFO_CTRL_PORT      0x3ea
#define COM3_LINE_CTRL_PORT      0x3eb
#define COM3_MODEM_CTRL_PORT     0x3ec
#define COM3_LINE_STATUS_PORT    0x3ed
#define COM3_MODEM_STATUS_PORT   0x3ee
#define COM3_SCRATCH_PORT        0x3ef

#define COM4_DATA_PORT           0x2e8
#define COM4_IRQ_ENABLE_PORT     0x2e9
#define COM4_DIV_LATCH_LSB_PORT  0x2e8
#define COM4_DIV_LATCH_MSB_PORT  0x2e9
#define COM4_IIR_PORT            0x2ea
#define COM4_FIFO_CTRL_PORT      0x2ea
#define COM4_LINE_CTRL_PORT      0x2eb
#define COM4_MODEM_CTRL_PORT     0x2ec
#define COM4_LINE_STATUS_PORT    0x2ed
#define COM4_MODEM_STATUS_PORT   0x2ee
#define COM4_SCRATCH_PORT        0x2ef


// Interrupt IDs (in priority order, highest is first)
#define RX_IRQ_STATUS           0x3
#define RX_IRQ_DR               0x2
#define RX_IRQ_TRIGGER_LEVEL    0x2
#define FIFO_IRQ                0x6
#define TX_IRQ_THRE             0x1
#define MODEM_IRQ_DELTA_SET     0x0

//COMs IRQ ID
#define COM1_IRQ  0x4
#define COM2_IRQ  0x3
#define COM3_IRQ  0x4
#define COM4_IRQ  0x3

#define RX_BUFFER 0x1
#define TX_BUFFER 0x2

//initial value for registers

// Per TI spec: http://www.ti.com/lit/ds/symlink/pc16550d.pdf
// all interrupt enables off
#define  IER_INIT_VAL 0x0 
// no pending interrupts (active low flag)
#define  IIR_INIT_VAL 0x1 
// fifos disabled (comes up in 16450 mode)
#define  FCR_INIT_VAL 0x0
// 5 bits, 1 stop bit, no parity, no break, divisor latch off
#define  LCR_INIT_VAL 0x0
// not DTR, not RTS, not OUT1, not OUT2, not loopback
#define  MCR_INIT_VAL 0x0
// tx registers empty, no errors, no data available
#define  LSR_INIT_VAL 0x60
// not cts, not dsr, not ring, etc.
#define  MSR_INIT_VAL 0x0
// baud rate 115200 (divisor is 1)
#define  DLL_INIT_VAL 0x1
#define  DLM_INIT_VAL 0x0

//receiver buffer register
struct rbr_register {
    uint8_t data;
};

// transmitter holding register
struct thr_register {
    uint8_t data;
};

//interrupt enable register
struct ier_register {
    union {
	uint8_t val;
	struct {
	    uint8_t erbfi   : 1;  // Enable Receiver Buffer full interrupt
	    uint8_t etbei   : 1;  // Enable Transmit buffer empty interrupt
	    uint8_t elsi    : 1;  // Enable Line Status Interrupt
	    uint8_t edssi   : 1;  // Enable Delta Status signals interrupt
	    uint8_t rsvd    : 4;   // MBZ
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


//interrupt identification register
struct iir_register {
    union {
	uint8_t val;
	struct {
	    uint8_t pending : 1; // Interrupt pending (0=interrupt pending)
	    uint8_t iid     : 3; // Interrupt Identification
	    uint8_t rsvd    : 2; // MBZ
	    uint8_t fifo_en : 2; // FIFO enable
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

//FIFO control register
struct fcr_register {
    union {
	uint8_t val;
	struct {
	    uint8_t enable  : 1; // enable fifo
	    uint8_t rfres   : 1; // RX FIFO reset
	    uint8_t xfres   : 1; // TX FIFO reset
	    uint8_t dma_sel : 1; // DMA mode select
	    uint8_t rsvd    : 2; // MBZ
	    uint8_t rx_trigger: 2; // RX FIFO trigger level select
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


//line control register
struct lcr_register {
    union {
	uint8_t val;
	struct {
	    uint8_t word_len       : 2;  // word length select
	    uint8_t stop_bits      : 1;  // Stop Bit select
	    uint8_t parity_enable  : 1;  // Enable parity
	    uint8_t even_sel       : 1;  // Even Parity Select
	    uint8_t stick_parity   : 1;  // Stick Parity Select (e.g., force mark or space)
	    uint8_t sbr            : 1;  // Set Break
	    uint8_t dlab           : 1;  // Divisor latch access bit
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


//modem control register
struct mcr_register {
    union {
	uint8_t val;
	struct {
	    uint8_t dtr      : 1;
	    uint8_t rts      : 1;
	    uint8_t out1     : 1;
	    uint8_t out2     : 1;
	    uint8_t loop     : 1;  // loopback mode
	    uint8_t rsvd     : 3;  // MBZ
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


//line status register
struct lsr_register {
    union {
	uint8_t val;
	struct {
	    uint8_t dr      : 1;   // data ready
	    uint8_t oe       : 1;  // Overrun error
	    uint8_t pe       : 1;  // Parity Error
	    uint8_t fe       : 1;  // Framing Error
	    uint8_t brk      : 1;  // broken line detected
	    uint8_t thre     : 1;  // Transmitter holding register empty
	    uint8_t temt     : 1;  // Transmitter Empty
	    uint8_t fifo_err : 1;  // at least one error is pending in the RX FIFO chain
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct msr_register {
    union {
	uint8_t val;
	struct {
	    uint8_t dcts     : 1;  // Delta Clear To Send
	    uint8_t ddsr     : 1;  // Delta Data Set Ready
	    uint8_t teri     : 1;  // Trailing Edge Ring Indicator
	    uint8_t ddcd     : 1;  // Delta Data Carrier Detect
	    uint8_t cts      : 1;  // Clear to Send
	    uint8_t dsr      : 1;  // Data Set Ready
	    uint8_t ri       : 1;  // Ring Indicator
	    uint8_t dcd      : 1;  // Data Carrier Detect
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

//scratch register
struct scr_register {
    uint8_t data;
};

//divisor latch LSB
struct dll_register {
    uint8_t data;
};

//divisor latch MSB
struct dlm_register {
    uint8_t data;
};
#define SERIAL_BUF_LEN 16   

struct serial_buffer {
    int head; // most recent data
    int tail; // oldest char
    int full;
    uint8_t buffer[SERIAL_BUF_LEN];
};

struct serial_port {
    struct rbr_register     rbr;
    struct thr_register     thr;
    struct ier_register     ier;
    struct iir_register     iir;
    struct fcr_register     fcr;
    struct lcr_register     lcr;
    struct mcr_register     mcr;
    struct lsr_register     lsr;
    struct msr_register     msr;
    struct scr_register     scr;
    struct dll_register     dll;
    struct dlm_register     dlm;
    
    
    struct serial_buffer tx_buffer;
    struct serial_buffer rx_buffer;
    uint_t irq_number;

    /*
      Multiple interrupt conditions can be active simultaneously. 
      The chip does a priority encoding to determine which one shows up in the iid

      INT_RX_STAT      1      line status change   cleared by LSR read
      INT_RX_DATA      2      rx data available    cleared by RBR read or when FIFO below trigger level
      INT_RX_TIMEOUT   2      rx fifo lonely       cleared by RBR read
      INT_TX_EMPTY     3      tx space available   cleared by IIR read (if in iid) or write to THR
      INT_MD_STAT      4      modem status change  cleared by MSR read

    */
#define INT_RX_STAT     1     
#define INT_RX_DATA     2
#define INT_RX_TIMEOUT  4
#define INT_TX_EMPTY    8
#define INT_MD_STAT     16
#define INT_MASK        0x1f
// whether we currently have an IRQ raised
#define IRQ_RAISED_MASK  128

    uint8_t int_state;  // all currently signaled interrupts, not just the one being delivered

    v3_lock_t  lock;

    void * backend_data;
    struct v3_dev_char_ops * ops;

};


struct serial_state {
    struct serial_port coms[4];

};



static struct serial_port * get_com_from_port(struct serial_state * serial, uint16_t port) {
    if ((port >= COM1_DATA_PORT) && (port <= COM1_SCRATCH_PORT)) {
        PrintDebug(VM_NONE, VCORE_NONE, "UART: COM1\n");
	return &(serial->coms[0]);
    } else if ((port >= COM2_DATA_PORT) && (port <= COM2_SCRATCH_PORT)) {
        PrintDebug(VM_NONE, VCORE_NONE, "UART: COM2\n");
	return &(serial->coms[1]);
    } else if ((port >= COM3_DATA_PORT) && (port <= COM3_SCRATCH_PORT)) {
        PrintDebug(VM_NONE, VCORE_NONE, "UART: COM3\n");
	return &(serial->coms[2]);
    } else if ((port >= COM4_DATA_PORT) && (port <= COM4_SCRATCH_PORT)) {
        PrintDebug(VM_NONE, VCORE_NONE, "UART: COM4\n");
	return &(serial->coms[3]);
    } else {
	PrintError(VM_NONE, VCORE_NONE, "UART: Error: Could not find serial port associated with IO port %d\n", port);
	return NULL;
    }
}

static inline bool receive_buffer_trigger(int number, int trigger_number) {

    switch (trigger_number) {
	case 0:
	    return (number >= 1);
	case 1:
	    return (number >= 4);
	case 2:
	    return (number >= 8);
	case 3:
	    return (number >= 14);
    }

    return false;
}


static int getNumber(struct serial_buffer * buf) {
    int number = buf->head - buf->tail;
  
    if (buf->full == 1) {
	return SERIAL_BUF_LEN;
    } else if (number >= 0) {
	return number;
    } else {
	return SERIAL_BUF_LEN + number;
    }
}


							     

static inline int can_queue(struct serial_buffer * buf) 
{
  return !(buf->full);
}

static inline int can_dequeue(struct serial_buffer * buf) 
{
  return !( (buf->head == buf->tail) && (buf->full != 1) );

}

static inline int peek_queue(struct serial_buffer *buf, uint8_t *data)
{
  int next_tail = (buf->tail + 1) % SERIAL_BUF_LEN;
 
  *data = buf->buffer[next_tail];
  
  return 0;
}


//  0 = successfully queued byte
// <0 = full
// caller responsible for locking
// caller responsible for updateIRQ
static int queue_data(struct v3_vm_info * vm, struct serial_port * com,
		      struct serial_buffer * buf, uint8_t data) {
    int next_loc = (buf->head + 1) % SERIAL_BUF_LEN;    

    PrintDebug(vm, VCORE_NONE,"UART: queue 0x%x ('%c') to %s buffer\n", data, data, 
	       buf == &(com->rx_buffer) ? "RX" : "TX");

    if (!can_queue(buf)) {
	PrintDebug(vm, VCORE_NONE, "UART: Buffer is full!\n");

	// We will not consider this an overrun, instead,
	// we push the problem back to the caller

	return -1;

    } else {
    
	buf->buffer[next_loc] = data;
	buf->head = next_loc;
	
	if (buf->head == buf->tail) {
	    buf->full = 1;
	}
	
	if (buf == &(com->rx_buffer)) {
	    com->lsr.dr = 1; //as soon as new data arrives at receive buffer, set data ready bit in lsr.
	    
	    // Set INT_RX_DATA if fifo is now above trigger level or fifos are off
	    
	    if ( (com->fcr.enable && (receive_buffer_trigger( getNumber(&(com->rx_buffer)), com->fcr.rx_trigger)))   
		 || !com->fcr.enable) {
		com->lsr.dr = 1;
		com->int_state |= INT_RX_DATA;

		PrintDebug(vm, VCORE_NONE, "UART: Set data ready and raised INT_RX_DATA\n");
		
	    }
	    
	}
	
	if ((buf == &(com->tx_buffer))) {
	    // at least one item in the TX queue now
	    
	    com->lsr.thre = 0; //reset thre and temt bits.
	    com->lsr.temt = 0;
	    
	    // TX EMPTY deasserted 
	    com->int_state &= ~INT_TX_EMPTY;
	    
	    PrintDebug(vm, VCORE_NONE, "UART: lowered INT_TX_EMPTY\n");
	}

	return 0;

    }
    
}
//  0 = data returned
// <0 = data not returned
// caller responsible for locking
// caller responsible for doing updateIRQ
static int dequeue_data(struct v3_vm_info * vm, struct serial_port * com,
			struct serial_buffer * buf, uint8_t * data)
{


    PrintDebug(vm, VCORE_NONE,"UART: dequeue from %s buffer\n",
	       buf == &(com->rx_buffer) ? "RX" : "TX");

    if (!can_dequeue(buf)) {
	PrintDebug(vm, VCORE_NONE, "UART: queue is empty - no state change, returning '!'\n");
	*data='!'; // just in case it uses what's there, blindly
	// for both rx and tx queues, we signaled when we droped to zero
	// so we don't resignal here, so no need to call updateIRQ
	return -1;
    } else {

	int next_tail = (buf->tail + 1) % SERIAL_BUF_LEN;
	
	
	if (buf->full == 1) {
	    buf->full = 0;
	}
	
        
	*data = buf->buffer[next_tail];
	
	buf->buffer[next_tail] = 0;
	buf->tail = next_tail;
	
	PrintDebug(vm,VCORE_NONE,"UART: dequeue will return 0x%x ('%c')\n", *data, *data);

	if (buf == &(com->rx_buffer)) { 
	    
	    // Reset INT_RX_DATA if RBR read with if fifo is now below trigger level or fifos are off
	    
	    if ( (com->fcr.enable && (!receive_buffer_trigger( getNumber(&(com->rx_buffer)), com->fcr.rx_trigger)))   
		 || !com->fcr.enable) {
		PrintDebug(vm, VCORE_NONE, "UART: lowering INT_RX_DATA since we're below threshold\n");
		com->int_state &= ~INT_RX_DATA;
		
	    }

	    // Reset DR if we have dropped to zero bytes
	    if (getNumber(&(com->rx_buffer)) ==0) { 
		PrintDebug(vm, VCORE_NONE, "UART: setting DR to zero since we have zero bytes\n");
		com->lsr.dr = 0;
		// should have lowered INT_RX_DATA earlier
	    }
	    
	    // reset timeout
	    com->int_state &= ~INT_RX_TIMEOUT;
	    
	}
	
	if ((buf == &(com->tx_buffer)) && (getNumber(&(com->tx_buffer)) == 0)) {
	    com->lsr.thre = 1;
	    com->lsr.temt = 1;
	    
	    // Aassert TX empty when we truly drop to zero; 
	    com->int_state |= INT_TX_EMPTY;
	}

	return 0;
    }
}

//
// Caller is assumed to have acquired the lock
//
static int pump_transmit(struct v3_vm_info * vm, struct serial_port * com) 
// Now we will pump transmit data to the backend, if any
{
  uint8_t buf;
  

  while (can_dequeue(&(com->tx_buffer))) {
    if (com->ops) { // do we have a back-end to toss it to?
      int rc;
      // let's take a peek and see if we can send it
      peek_queue(&(com->tx_buffer),&buf);
      rc = com->ops->output(&buf, 1, com->backend_data);
      if (rc<0) {
	PrintError(vm, VCORE_NONE, "UART: backend write returned error\n");
	// we need to give up at this point - 
	break;
      } else if (rc==0) { 
	// no room to send it
	PrintDebug(vm, VCORE_NONE, "UART: backend write would block\n");
	// we do nothing but we don't want to iterate again
	break; // out we go
      } else {
	// it was sent, now we need to remove it from the queue for real
	// as well as update the device state
	if (dequeue_data(vm,com,&(com->tx_buffer),&buf)) { 
	  PrintError(vm, VCORE_NONE, "UART: uh... dequeue_data failed after successful peek?!\n");
	  break;  // out we go
	}
	// we have already sent the byte to the backend, so we
	// just discard it now and continue to the next one
      }
    } else { // there is no backend
      if (dequeue_data(vm,com,&(com->tx_buffer),&buf)) { 
	PrintError(vm, VCORE_NONE, "UART: uh... dequeue_data failed after successful can_dequeue?!\n");
      }
      // no backend, so just discard the data
    }
  }
  return 0;
}

static int updateIRQ(struct v3_vm_info * vm, struct serial_port * com) {



  PrintDebug(vm,VCORE_NONE, "UART: updateIRQ before pending check: iir.pending=%d iir.iid=%d int_state=0x%x\n", 
	     com->iir.pending, com->iir.iid, com->int_state);

  // if we have have raised an irq, we need to lower it if it's been handled
  // we should also presumably lower it if the guest decided to 
  // disable the interrupt
  if (!com->iir.pending && (com->int_state & IRQ_RAISED_MASK)) {  // active low pending, and we have raised
      switch (com->iir.iid) {
	  case RX_IRQ_STATUS:
	      if (!(com->int_state & INT_RX_STAT) ||                     // now low or
		  ((com->int_state & INT_RX_STAT) && !com->ier.elsi))  { // no longer enabled
		  // no longer asserted
		  v3_lower_irq(vm, com->irq_number);
		  com->int_state &= ~IRQ_RAISED_MASK;
		  com->iir.pending=1;
		  com->iir.iid=0;
		  PrintDebug(vm, VCORE_NONE, "UART: lowered irq on reset of INT_RX_STAT\n");
	      }
	      break;
	  case RX_IRQ_DR:    
	  // case RX_IRQ_TRIGGER_LEVEL: is the smae
	      if (!(com->int_state & INT_RX_DATA) || 
		  ((com->int_state & INT_RX_STAT) && !com->ier.erbfi)) {
		  // no longer asserted
		  v3_lower_irq(vm, com->irq_number);
		  com->int_state &= ~IRQ_RAISED_MASK;
		  com->iir.pending=1;
		  com->iir.iid=0;
		  PrintDebug(vm, VCORE_NONE, "UART: lowered irq on reset of INT_RX_DATA\n");
	      }
	      break;
	  case FIFO_IRQ:
	      if (!(com->int_state & INT_RX_TIMEOUT) ||
		  ((com->int_state & INT_RX_TIMEOUT) && !com->ier.erbfi)) {
		  // no longer asserted
		  v3_lower_irq(vm, com->irq_number);
		  com->int_state &= ~IRQ_RAISED_MASK;
		  com->iir.pending=1;
		  com->iir.iid=0;
		  PrintDebug(vm, VCORE_NONE, "UART: lowered irq on reset of INT_RX_TIMEOUT\n");
	      }
	      break;
	  case TX_IRQ_THRE:
	      if (!(com->int_state & INT_TX_EMPTY)  ||
		  ((com->int_state & INT_TX_EMPTY) && !com->ier.etbei)) {
		  // no longer asserted
		  v3_lower_irq(vm, com->irq_number);
		  com->int_state &= ~IRQ_RAISED_MASK;
		  com->iir.pending=1;
		  com->iir.iid=0;
		  PrintDebug(vm, VCORE_NONE, "UART: lowered irq on reset of INT_TX_EMPTY\n");
	      }
	      break;
	  case MODEM_IRQ_DELTA_SET:
	      if (!(com->int_state & INT_MD_STAT) ||
		  ((com->int_state & INT_MD_STAT) && !com->ier.edssi)) {
		  // no longer asserted
		  v3_lower_irq(vm, com->irq_number);
		  com->int_state &= ~IRQ_RAISED_MASK;
		  com->iir.pending=1;
		  com->iir.iid=0;
		  PrintDebug(vm, VCORE_NONE, "UART: lowered irq on reset of INT_MD_STAT\n");
	      }
	      break;
      }
  }

  PrintDebug(vm,VCORE_NONE, "UART: updateIRQ before transmit-pump: iir.pending=%d iir.iid=%d int_state=0x%x\n", 
	     com->iir.pending, com->iir.iid, com->int_state);
  
  if (pump_transmit(vm,com)) {
    PrintError(vm,VCORE_NONE, "UART: pump_transmit failed - eh?\n");
  }

  // At this point, INT_TX_EMPTY might have also been raised
  if (com->lsr.temt) { 
      // even if pump_transmit wasn't able to do anything, we need to
      // raise the interrupt - this is the case, for example, if the
      // guest does an interrupt enable of the tx empty interrupt before
      // writing anything
      com->int_state |= INT_TX_EMPTY;
  }

  PrintDebug(vm,VCORE_NONE, "UART: updateIRQ before priority encode:  iir.pending=%d iir.iid=%d int_state=0x%x\n", 
	     com->iir.pending, com->iir.iid, com->int_state);


  if (!(com->int_state & IRQ_RAISED_MASK)) { 
      // We can inject a new interrupt since the last one is done
      // Now we do the priority encode
      if ((com->int_state & INT_RX_STAT) && com->ier.elsi ) { 
	  // highest priority
	  com->iir.pending=0;
	  com->iir.iid = RX_IRQ_STATUS;
	  v3_raise_irq(vm,com->irq_number);
	  com->int_state |= IRQ_RAISED_MASK;
	  PrintDebug(vm, VCORE_NONE, "UART: raised irq on set of INT_RX_STAT\n");
      } else if ((com->int_state & INT_RX_DATA) && com->ier.erbfi) { 
	  // 2nd highest priority
	  com->iir.pending=0;
	  com->iir.iid = RX_IRQ_DR;
	  v3_raise_irq(vm,com->irq_number);
	  com->int_state |= IRQ_RAISED_MASK;
	  PrintDebug(vm, VCORE_NONE, "UART: raised irq on set of INT_RX_DATA\n");
      } else if ((com->int_state & INT_RX_TIMEOUT) && com->ier.erbfi) { 
	  // Also 2nd highest priority
	  com->iir.pending=0;
	  com->iir.iid = FIFO_IRQ;
	  v3_raise_irq(vm,com->irq_number);
	  com->int_state |= IRQ_RAISED_MASK;
	  PrintDebug(vm, VCORE_NONE, "UART: raised irq on set of INT_RX_TIMEOUT\n");
      }	else if ((com->int_state & INT_TX_EMPTY) && com->ier.etbei) { 
	  // 3rd highest priority
	  com->iir.pending=0;
	  com->iir.iid = TX_IRQ_THRE;
	  v3_raise_irq(vm,com->irq_number);
	  com->int_state |= IRQ_RAISED_MASK;
	  PrintDebug(vm, VCORE_NONE, "UART: raised irq on set of INT_TX_EMPTY\n");
      } else if ((com->int_state & INT_MD_STAT) && com->ier.edssi) { 
	  // 4th highest priority
	  com->iir.pending=0;
	  com->iir.iid = MODEM_IRQ_DELTA_SET;
	  v3_raise_irq(vm,com->irq_number);
	  com->int_state |= IRQ_RAISED_MASK;
	  PrintDebug(vm, VCORE_NONE, "UART: raised irq on set of INT_MD_STAT\n");
      } else {
	  // nothing to do
      }
  }
  
  return 0;
}



static int write_data_port(struct guest_info * core, uint16_t port, 
			   void * src, uint_t length, void * priv_data) {
    struct serial_state * state = priv_data;
    uint8_t * val = (uint8_t *)src;
    struct serial_port * com_port = NULL;
    addr_t irq_status;

    PrintDebug(core->vm_info, core, "UART: Write to Data Port 0x%x (val=%x, '%c')\n", port, *val, *val);
    
    if (length != 1) {
	PrintError(core->vm_info, core, "UART: Invalid length(%d) in write to 0x%x\n", length, port);
	return -1;
    }

    if ((port != COM1_DATA_PORT) && (port != COM2_DATA_PORT) && 
	(port != COM3_DATA_PORT) && (port != COM4_DATA_PORT)) {
	PrintError(core->vm_info, core, "UART: Serial write data port for illegal port Number (%d)\n", port);
	return -1;
    }

    com_port = get_com_from_port(state, port);

    if (com_port == NULL) {
	PrintError(core->vm_info, core, "UART: write of unknown port %d - lookup failed?!\n",port);
	return -1;
    }
    
    irq_status = v3_lock_irqsave(com_port->lock);
    
    // dlab is always checked first
    if (com_port->lcr.dlab == 1) {
	PrintDebug(core->vm_info, core, "UART:  Write to DLM, old DLM is 0x%x\n",com_port->dlm.data);
	com_port->dll.data = *val;
	PrintDebug(core->vm_info, core, "UART:  Write to DLM, new DLM is 0x%x\n",com_port->dlm.data);
    }  else {
	// queue data to send and update interrupts
	PrintDebug(core->vm_info, core, "UART: queue transmission of 0x%x ('%c')\n",*val,*val);
	if (queue_data(core->vm_info, com_port, &(com_port->tx_buffer), *val)) { 
	  PrintError(core->vm_info,core, "UART: no room for transmitted data - dropped\n");
	  // note that it must "succeed" since this is a device port
	} else {
	  updateIRQ(core->vm_info, com_port);
	}
    }

    v3_unlock_irqrestore(com_port->lock, irq_status);

    return length;
}



static int read_data_port(struct guest_info * core, uint16_t port, 
			  void * dst, uint_t length, void * priv_data) {
    struct serial_state * state = priv_data;
    uint8_t * val = (uint8_t *)dst;
    struct serial_port * com_port = NULL;
    addr_t irq_status;

    PrintDebug(core->vm_info, core, "UART: Read from Data Port 0x%x\n", port);
    
    if (length != 1) {
	PrintError(core->vm_info, core, "UART: Invalid length(%d) in write to 0x%x\n", length, port);
	return -1;
    }
    
    if ((port != COM1_DATA_PORT) && (port != COM2_DATA_PORT) && 
	(port != COM3_DATA_PORT) && (port != COM4_DATA_PORT)) {
	PrintError(core->vm_info, core, "UART: Serial Read data port for illegal port Number (%d)\n", port);
	return -1;
    }

    com_port = get_com_from_port(state, port);

    if (com_port == NULL) {
	PrintError(core->vm_info, core, "UART: write of unknown port %d - lookup failed?!\n",port);
	return -1;
    }

    irq_status = v3_lock_irqsave(com_port->lock);

    if (com_port->lcr.dlab == 1) {
	*val = com_port->dll.data;
	PrintDebug(core->vm_info, core, "UART: read of DLL returning 0x%x\n",*val);
    } else {
      if (dequeue_data(core->vm_info, com_port, &(com_port->rx_buffer), val)) { 
	PrintError(core->vm_info, core, "UART: no received data available - returning garbaage\n");
	// note that it must "succeed" since this is device port read
      } else {
	updateIRQ(core->vm_info, com_port);
      }
      PrintDebug(core->vm_info, core, "UART: dequeued received data 0x%x ('%c')\n",*val,*val);
    }    
	
    v3_unlock_irqrestore(com_port->lock, irq_status);

    return length;
}


static void flush_buffer(struct serial_buffer *buf)
{
    buf->head = 0;
    buf->tail = 0;
    buf->full = 0;
    memset(buf->buffer, 0, SERIAL_BUF_LEN);
}

//
// caller is assumed to have acquired the lock
// caller is responsible for updateIRQ
static void handle_fcr_write(struct serial_port * com, uint8_t value) {

#if BE_16550A

    if ((!com->fcr.enable && (value & 0x1)) || (com->fcr.enable && !(value & 0x1))) { 
	// switch of modes from 16450<->16550
	// flush fifos, reset state
	flush_buffer(&(com->rx_buffer));
	flush_buffer(&(com->tx_buffer));
	com->lsr.dr = 0;
	com->int_state &= ~INT_RX_DATA; // no data in rx buffer - lower

	com->lsr.thre = 1;
	com->lsr.temt = 1;
	com->int_state |= INT_TX_EMPTY; // tx buffer empty - raise
    }

    if (!(value & 0x1)) { // disabling
	// enable->disable requires flush and state reset, handled above
	// disable->disable doesn't require flush, I don't think 
	com->fcr.enable = 0;
	com->iir.fifo_en = 0;
	// reset does not change rest of fcr register
    } else { // enabling
	com->fcr.val = value;
	com->fcr.rsvd = 0; // we are not some weird chip
	
	//if rfres set, clear receive buffer.
	if (com->fcr.rfres == 0x1) {
	    flush_buffer(&(com->rx_buffer));
	    com->fcr.rfres = 0; // bit is self-clearing
	    com->lsr.dr = 0;
	    com->int_state &= ~INT_RX_DATA; // no data in rx buffer - lower
	}
	
	//if xfres set, clear transmit buffer.
	if (com->fcr.xfres == 0x1) {
	    flush_buffer(&(com->tx_buffer));
	    com->fcr.xfres = 0; // bit is self-clearing
	    com->lsr.thre = 1;
	    com->lsr.temt = 1;
	    com->int_state |= INT_TX_EMPTY; // tx buffer empty -raise
	}

	com->iir.fifo_en = 0x3; // We are a 16550A
	com->iir.fifo_en = 0x0; // We are a 16450A

	// 00  => 8250/16450 (latter if have scratchpad)
	// 10  => 16550
	// 11  => 16550A
    }   

#else
    // does nothing since a 16450 has no fcr
#endif

    // caller must update irq!
}





static int handle_multiple(int (*func)(struct guest_info *core,
				       uint16_t port, void *dst,
				       uint_t length, void *priv_data),
			   struct guest_info *core, uint16_t port, 
			   void *dst, uint_t length, void *priv_data) 
{
  uint16_t i;
  int rc;

  if (length == 1) { 
    return func(core,port,dst,length,priv_data);
  } else {
    for (i=0;i<length;i++) {
      rc = func(core,port+i,dst+i,1,priv_data);
      if (rc!=1) { 
	return i;
      }
    }
    return length;
  }
}

static int write_ctrl_port(struct guest_info * core, uint16_t port, void * src, 
			   uint_t length, void * priv_data) {
    struct serial_state * state = priv_data;
    uint8_t val = *(uint8_t *)src;
    struct serial_port * com_port = NULL;
    addr_t irq_status;
    int ret;

    PrintDebug(core->vm_info, core, "UART: Write to Control Port (val=%x)\n", val);
    
    if (length != 1) {
	PrintError(core->vm_info, core, "UART: Invalid Write length to control port%d\n", port);
	return -1;
    }

    com_port = get_com_from_port(state, port);

    if (com_port == NULL) {
	PrintError(core->vm_info, core, "UART: Could not find serial port corresponding to IO port %d\n", port);
	return -1;
    }

    irq_status = v3_lock_irqsave(com_port->lock);
  
    ret = 1;

    //always check dlab first
    switch (port) {
	case COM1_IRQ_ENABLE_PORT:
	case COM2_IRQ_ENABLE_PORT:
	case COM3_IRQ_ENABLE_PORT:
	case COM4_IRQ_ENABLE_PORT: {
	    PrintDebug(core->vm_info, core, "UART: Write to IER/LATCH port: dlab is %x\n", com_port->lcr.dlab);

	    if (com_port->lcr.dlab == 1) {
		PrintDebug(core->vm_info, core, "UART:  Write to DLM, old DLM is 0x%x\n",com_port->dlm.data);
		com_port->dlm.data = val;
		PrintDebug(core->vm_info, core, "UART:  Write to DLM, new DLM is 0x%x\n",com_port->dlm.data);
	    } else {
		PrintDebug(core->vm_info, core, "UART: Write to IER, old IER is erbfi=%d etbei=%d elsi=%d edssi=%d\n",
			   com_port->ier.erbfi,com_port->ier.etbei,com_port->ier.elsi,com_port->ier.edssi);
		com_port->ier.val = val;
		com_port->ier.rsvd = 0; // we are not some weird chip
		PrintDebug(core->vm_info, core, "UART: Write to IER, new IER is erbfi=%d etbei=%d elsi=%d edssi=%d\n",
			   com_port->ier.erbfi,com_port->ier.etbei,com_port->ier.elsi,com_port->ier.edssi);
		// some signaled interrupt might now need to fire
	    }

	    updateIRQ(core->vm_info,com_port);

	    break;
	}	    
	case COM1_FIFO_CTRL_PORT:
	case COM2_FIFO_CTRL_PORT:
	case COM3_FIFO_CTRL_PORT:
	case COM4_FIFO_CTRL_PORT: {
	    PrintDebug(core->vm_info, core, "UART: Write to FCR, old FCR is enable=%d rfres=%d xfres=%d dma_sel=%d rx_trigger=%d\n",
		       com_port->fcr.enable, com_port->fcr.rfres, com_port->fcr.xfres, com_port->fcr.dma_sel, com_port->fcr.rx_trigger);

	    handle_fcr_write(com_port, val); // cannot fail
	    
	    PrintDebug(core->vm_info, core, "UART: Write to FCR, new FCR is enable=%d rfres=%d xfres=%d dma_sel=%d rx_trigger=%d\n",
		       com_port->fcr.enable, com_port->fcr.rfres, com_port->fcr.xfres, com_port->fcr.dma_sel, com_port->fcr.rx_trigger);

	    updateIRQ(core->vm_info,com_port);
	    
	    break;
	}
	case COM1_LINE_CTRL_PORT:
	case COM2_LINE_CTRL_PORT:
	case COM3_LINE_CTRL_PORT:
	case COM4_LINE_CTRL_PORT: {
	    PrintDebug(core->vm_info, core, "UART: Write to LCR, old LCR is word_len=%d stop_bits=%d parity_enable=%d even_sel=%d stick_parity=%d sbr=%d dlab=%d\n",
		       com_port->lcr.word_len,com_port->lcr.stop_bits,com_port->lcr.parity_enable,com_port->lcr.even_sel,com_port->lcr.stick_parity,com_port->lcr.sbr,com_port->lcr.dlab);

	    com_port->lcr.val = val;
	    // no reserved bits
	    
	    PrintDebug(core->vm_info, core, "UART: Write to LCR, new LCR is word_len=%d stop_bits=%d parity_enable=%d even_sel=%d stick_parity=%d sbr=%d dlab=%d\n",
		       com_port->lcr.word_len,com_port->lcr.stop_bits,com_port->lcr.parity_enable,com_port->lcr.even_sel,com_port->lcr.stick_parity,com_port->lcr.sbr,com_port->lcr.dlab);

	    updateIRQ(core->vm_info, com_port);

	    break;
	}
	case COM1_MODEM_CTRL_PORT:
	case COM2_MODEM_CTRL_PORT:
	case COM3_MODEM_CTRL_PORT:
	case COM4_MODEM_CTRL_PORT: {
	    PrintDebug(core->vm_info, core, "UART: Write to MCR, old MCR is dtr=%d rts=%d out1=%d out2=%d loop=%d\n",
		       com_port->mcr.dtr,com_port->mcr.rts,com_port->mcr.out1,com_port->mcr.out2,com_port->mcr.loop);

	    com_port->mcr.val = val;
	    com_port->mcr.rsvd = 0; // we are not some weird chip

	    PrintDebug(core->vm_info, core, "UART: Write to MCR, new MCR is dtr=%d rts=%d out1=%d out2=%d loop=%d\n",
		       com_port->mcr.dtr,com_port->mcr.rts,com_port->mcr.out1,com_port->mcr.out2,com_port->mcr.loop);
	    
	    updateIRQ(core->vm_info, com_port);

	    break;
	}
	case COM1_SCRATCH_PORT:
	case COM2_SCRATCH_PORT:
	case COM3_SCRATCH_PORT:
	case COM4_SCRATCH_PORT: {
	    PrintDebug(core->vm_info, core, "UART: Write to SCRATCH, old value is 0x%x\n",com_port->scr.data);

	    com_port->scr.data = val;

	    PrintDebug(core->vm_info, core, "UART: Write to SCRATCH, new value is 0x%x\n",com_port->scr.data);
	    break;
	}
	default:
	    PrintError(core->vm_info, core, "UART: Write to unknown port %d, ERROR\n",port);
	    ret = -1;
    }
    
    v3_unlock_irqrestore(com_port->lock,irq_status);

    return ret;
}




static int read_ctrl_port(struct guest_info * core, uint16_t port, void * dst, 
			  uint_t length, void * priv_data) {
    struct serial_state * state = priv_data;
    uint8_t * val = (uint8_t *)dst;
    struct serial_port * com_port = NULL;
    addr_t irq_status;
    int ret;

    PrintDebug(core->vm_info, core, "UART: Read from Control Port\n");
    
    if (length != 1) {
	PrintError(core->vm_info, core, "UART: Invalid Read length to control port\n");
	return -1;
    }

    com_port = get_com_from_port(state, port);

    if (com_port == NULL) {
	PrintError(core->vm_info, core, "UART: Could not find serial port corresponding to IO port %d\n", port);
	return -1;
    }
    
    irq_status = v3_lock_irqsave(com_port->lock);

    ret = 1;

    //always check dlab first
    switch (port) {
	case COM1_IRQ_ENABLE_PORT:
	case COM2_IRQ_ENABLE_PORT:
	case COM3_IRQ_ENABLE_PORT:
	case COM4_IRQ_ENABLE_PORT: {
	    PrintDebug(core->vm_info, core, "UART: Read from IER/LATCH port: dlab is %x\n", com_port->lcr.dlab);

	    if (com_port->lcr.dlab == 1) {
		PrintDebug(core->vm_info, core, "UART: Read of DLM, value 0x%x\n",com_port->dlm.data);
		*val = com_port->dlm.data;
	    } else {
		*val = com_port->ier.val;
		PrintDebug(core->vm_info, core, "UART: Read of IER is val=0x%x - erbfi=%d etbei=%d elsi=%d edssi=%d\n",
			   com_port->ier.val, com_port->ier.erbfi,com_port->ier.etbei,com_port->ier.elsi,com_port->ier.edssi);
	    }
	    break;
	}

	case COM1_IIR_PORT:
	case COM2_IIR_PORT:
	case COM3_IIR_PORT:
	case COM4_IIR_PORT:
	    PrintDebug(core->vm_info, core, "UART: read from IIR is val=0x%x - pending=%d iid=%d fifo_en=%d\n",
		       com_port->iir.val,com_port->iir.pending,com_port->iir.iid,com_port->iir.fifo_en);
	    *val = com_port->iir.val;

	    if ((com_port->int_state & IRQ_RAISED_MASK) && 
		!com_port->iir.pending && com_port->iir.iid==TX_IRQ_THRE) { 
		// we are firing a TX_IRQ_THRE interrupt, therefore
		// this read resets it

		com_port->int_state &= ~INT_TX_EMPTY;
		
		updateIRQ(core->vm_info,com_port);
	    }

	    break;

	case COM1_LINE_CTRL_PORT:
	case COM2_LINE_CTRL_PORT:
	case COM3_LINE_CTRL_PORT:
	case COM4_LINE_CTRL_PORT:
	    PrintDebug(core->vm_info, core, "UART: read from LCR is val=0x%x - word_len=%d stop_bits=%d parity_enable=%d even_sel=%d stick_parity=%d sbr=%d dlab=%d\n",
		       com_port->lcr.val,com_port->lcr.word_len, com_port->lcr.stop_bits,com_port->lcr.parity_enable,com_port->lcr.even_sel,com_port->lcr.stick_parity,com_port->lcr.sbr,com_port->lcr.dlab);
	    *val = com_port->lcr.val;
	    break;

	case COM1_MODEM_CTRL_PORT:
	case COM2_MODEM_CTRL_PORT:
	case COM3_MODEM_CTRL_PORT:
	case COM4_MODEM_CTRL_PORT:
	    PrintDebug(core->vm_info, core, "UART: read from MCR is val=0x%x - dtr=%d rts=%d out1=%d out2=%d loop=%d\n",
		       com_port->mcr.val,com_port->mcr.dtr,com_port->mcr.rts,com_port->mcr.out1,com_port->mcr.out2,com_port->mcr.loop);
	    *val = com_port->mcr.val;
	    break;

	case COM1_SCRATCH_PORT:
	case COM2_SCRATCH_PORT:
	case COM3_SCRATCH_PORT:
	case COM4_SCRATCH_PORT:
	    PrintDebug(core->vm_info, core, "UART: read from SCRATCH is val=0x%x\n",com_port->scr.data);
	    *val = com_port->scr.data;
	    break;

	default:
	    PrintError(core->vm_info, core, "UART: read from unknown port %d\n",port);
	    ret = -1;
    }

    v3_unlock_irqrestore(com_port->lock,irq_status);

    return ret;
}


static int write_status_port(struct guest_info * core, uint16_t port, void * src, 
			     uint_t length, void * priv_data) {
    struct serial_state * state = priv_data;
    uint8_t val = *(uint8_t *)src;
    struct serial_port * com_port = NULL;
    addr_t irq_status;
    int ret;

    PrintDebug(core->vm_info, core, "UART: Write to Status Port (val=0x%x)\n", val);

    if (length != 1) {
	PrintError(core->vm_info, core, "UART: Invalid Write length (%d) to status port %d\n", length, port);
	return -1;
    }

    com_port = get_com_from_port(state, port);

    if (com_port == NULL) {
	PrintError(core->vm_info, core, "UART: Could not find serial port corresponding to IO port %d\n", port);
	return -1;
    }

    irq_status = v3_lock_irqsave(com_port->lock);
    
    ret = 1;

    switch (port) {
	case COM1_LINE_STATUS_PORT:
	case COM2_LINE_STATUS_PORT:
	case COM3_LINE_STATUS_PORT:
	case COM4_LINE_STATUS_PORT:
	    PrintDebug(core->vm_info, core, "UART: Write to LSR, old value is 0x%x - dr=%d oe=%d pe=%d fe=%d brk=%d thre=%d temt=%d fifo_err=%d\n",
		       com_port->lsr.val,com_port->lsr.dr,com_port->lsr.oe,com_port->lsr.pe,com_port->lsr.fe,com_port->lsr.brk,com_port->lsr.thre,com_port->lsr.temt,com_port->lsr.fifo_err);

	    com_port->lsr.val = val;
	    // no reserved bits

	    PrintDebug(core->vm_info, core, "UART: Write to LSR, new value is 0x%x - dr=%d oe=%d pe=%d fe=%d brk=%d thre=%d temt=%d fifo_err=%d\n",
		       com_port->lsr.val,com_port->lsr.dr,com_port->lsr.oe,com_port->lsr.pe,com_port->lsr.fe,com_port->lsr.brk,com_port->lsr.thre,com_port->lsr.temt,com_port->lsr.fifo_err);

	    updateIRQ(core->vm_info, com_port);

	    break;

	case COM1_MODEM_STATUS_PORT:
	case COM2_MODEM_STATUS_PORT:
	case COM3_MODEM_STATUS_PORT:
	case COM4_MODEM_STATUS_PORT:

	    PrintDebug(core->vm_info,core,"UART: Write to MSR, old value is 0x%x - dcts=%d ddsr=%d teri=%d ddcd=%d cts=%d dsr=%d ri=%d dcd=%d\n",
		       com_port->msr.val,com_port->msr.dcts,com_port->msr.ddsr,com_port->msr.teri,com_port->msr.ddcd,com_port->msr.cts,com_port->msr.dsr,com_port->msr.ri,com_port->msr.dcd);

	    com_port->msr.val = val;
	    // no reserved bits

	    PrintDebug(core->vm_info,core,"UART: Write to MSR, new value is 0x%x - dcts=%d ddsr=%d teri=%d ddcd=%d cts=%d dsr=%d ri=%d dcd=%d\n",
		       com_port->msr.val,com_port->msr.dcts,com_port->msr.ddsr,com_port->msr.teri,com_port->msr.ddcd,com_port->msr.cts,com_port->msr.dsr,com_port->msr.ri,com_port->msr.dcd);
	    
	    updateIRQ(core->vm_info, com_port);

	    break;

	default:
	    PrintError(core->vm_info, core, "UART: write to unsupported port %d\n",port);
	    ret = -1;
    }

    v3_unlock_irqrestore(com_port->lock,irq_status);

    return ret;
}


static int read_status_port(struct guest_info * core, uint16_t port, void * dst, 
			    uint_t length, void * priv_data) {
    struct serial_state * state = priv_data;
    uint8_t * val = (uint8_t *)dst;
    struct serial_port * com_port = NULL;
    addr_t irq_status;
    int ret;

    if (length==2 && (port==COM1_MODEM_STATUS_PORT ||
		      port==COM2_MODEM_STATUS_PORT ||
		      port==COM3_MODEM_STATUS_PORT ||
		      port==COM4_MODEM_STATUS_PORT) ) {
      return handle_multiple(read_status_port,core,port,dst,length, priv_data);
    } else if (length!=1) { 
      PrintError(core->vm_info, core, "UART: Invalid Read length (%d) from status port %d\n", length, port);
      return -1;
    }
      
    PrintDebug(core->vm_info, core, "UART: Read from Status Port 0x%x\n", port);

    com_port = get_com_from_port(state, port);

    if (com_port == NULL) {
	PrintError(core->vm_info, core, "UART: Could not find serial port corresponding to IO port %d\n", port);
	return -1;
    }
    
    irq_status = v3_lock_irqsave(com_port->lock);
    ret = 1;
    
    switch (port) {
	case COM1_LINE_STATUS_PORT:
	case COM2_LINE_STATUS_PORT:
	case COM3_LINE_STATUS_PORT:
	case COM4_LINE_STATUS_PORT:

	    PrintDebug(core->vm_info, core, "UART: Read from LSR is 0x%x - dr=%d oe=%d pe=%d fe=%d brk=%d thre=%d temt=%d fifo_err=%d\n",
		       com_port->lsr.val,com_port->lsr.dr,com_port->lsr.oe,com_port->lsr.pe,com_port->lsr.fe,com_port->lsr.brk,com_port->lsr.thre,com_port->lsr.temt,com_port->lsr.fifo_err);

	    *val = com_port->lsr.val;


	    // Reading the LSR resets the line error state
	    com_port->lsr.oe = 0;     
	    com_port->lsr.pe = 0;     
	    com_port->lsr.fe = 0;     
	    com_port->lsr.brk = 0;     
	    com_port->lsr.fifo_err = 0;     

	    // and clears any interrupt we set due to it
	    com_port->int_state &= ~INT_RX_STAT;
	    
	    updateIRQ(core->vm_info,com_port);


	    break;

	case COM1_MODEM_STATUS_PORT:
	case COM2_MODEM_STATUS_PORT:
	case COM3_MODEM_STATUS_PORT:
	case COM4_MODEM_STATUS_PORT:
	    PrintDebug(core->vm_info,core,"UART: read of MSR is 0x%x - dcts=%d ddsr=%d teri=%d ddcd=%d cts=%d dsr=%d ri=%d dcd=%d\n",
		       com_port->msr.val,com_port->msr.dcts,com_port->msr.ddsr,com_port->msr.teri,com_port->msr.ddcd,com_port->msr.cts,com_port->msr.dsr,com_port->msr.ri,com_port->msr.dcd);

	    *val = com_port->msr.val;

	    // MSR read resets any interrupt due to modem error
	    com_port->int_state &= ~INT_MD_STAT;

	    updateIRQ(core->vm_info,com_port);

	    break;

	default:
	    PrintError(core->vm_info, core, "UART: read of unknown port %d (length = %d)\n", port, length);
	    ret = -1; 
    }

    v3_unlock_irqrestore(com_port->lock, irq_status);

    return ret;
}


static int deinit_serial_port(struct serial_port *);

static int serial_free(struct serial_state * state) {

    deinit_serial_port(&(state->coms[0]));
    deinit_serial_port(&(state->coms[1]));
    deinit_serial_port(&(state->coms[2]));
    deinit_serial_port(&(state->coms[3]));

    V3_Free(state);
    return 0;
}



#ifdef V3_CONFIG_CHECKPOINT

#include <palacios/vmm_sprintf.h>

static int serial_buffer_save(struct v3_chkpt_ctx * ctx, int port, struct serial_buffer *sb, char * bufname) {
  
  char keyname[128];
  
  snprintf(keyname,128,"COM%d_%s_HEAD", port, bufname);
  V3_CHKPT_SAVE(ctx,keyname,sb->head,failout);
  snprintf(keyname,128,"COM%d_%s_TAIL", port, bufname);
  V3_CHKPT_SAVE(ctx,keyname,sb->tail,failout);
  snprintf(keyname,128,"COM%d_%s_FULL", port, bufname);
  V3_CHKPT_SAVE(ctx,keyname,sb->full,failout);
  snprintf(keyname,128,"COM%d_%s_DATA", port, bufname);
  V3_CHKPT_SAVE(ctx,keyname,sb->buffer,failout);
  
  return 0;
  
 failout:
  PrintError(VM_NONE, VCORE_NONE, "Failed to save serial buffer\n");
  return -1;
}

 
static int serial_buffer_load(struct v3_chkpt_ctx * ctx, int port,  struct serial_buffer *sb, char * bufname) {

  char keyname[128];

  snprintf(keyname,128,"COM%d_%s_HEAD", port, bufname);
  V3_CHKPT_LOAD(ctx,keyname,sb->head,failout);
  snprintf(keyname,128,"COM%d_%s_TAIL", port, bufname);
  V3_CHKPT_LOAD(ctx,keyname,sb->tail,failout);
  snprintf(keyname,128,"COM%d_%s_FULL", port, bufname);
  V3_CHKPT_LOAD(ctx,keyname,sb->full,failout);
  snprintf(keyname,128,"COM%d_%s_DATA", port, bufname);
  V3_CHKPT_LOAD(ctx,keyname,sb->buffer,failout);
  
  return 0;
  
 failout:
  PrintError(VM_NONE, VCORE_NONE, "Failed to load serial buffer\n");
  return -1;
}
 
static int serial_save(struct v3_chkpt_ctx * ctx, void * private_data) {
  struct serial_state *state = (struct serial_state *)private_data;
  struct serial_port *serial;
  char keyname[128];
  int i;
  
  for (i=0;i<4;i++) { 
    serial = &(state->coms[i]);
    snprintf(keyname, 128,"COM%d_RBR",i);
    V3_CHKPT_SAVE(ctx, keyname, serial->rbr.data,failout);
    snprintf(keyname, 128,"COM%d_THR",i);
    V3_CHKPT_SAVE(ctx, keyname, serial->thr.data,failout);
    snprintf(keyname, 128,"COM%d_IER",i);
    V3_CHKPT_SAVE(ctx, keyname, serial->ier.val,failout);
    snprintf(keyname, 128,"COM%d_IIR",i);
    V3_CHKPT_SAVE(ctx, keyname, serial->iir.val,failout);
    snprintf(keyname, 128,"COM%d_FCR",i);
    V3_CHKPT_SAVE(ctx, keyname, serial->fcr.val,failout);
    snprintf(keyname, 128,"COM%d_LCR",i);
    V3_CHKPT_SAVE(ctx, keyname, serial->lcr.val,failout);
    snprintf(keyname, 128,"COM%d_MCR",i);
    V3_CHKPT_SAVE(ctx, keyname, serial->mcr.val,failout);
    snprintf(keyname, 128,"COM%d_LSR",i);
    V3_CHKPT_SAVE(ctx, keyname, serial->lsr.val,failout);
    snprintf(keyname, 128,"COM%d_MSR",i);
    V3_CHKPT_SAVE(ctx, keyname, serial->msr.val,failout);
    snprintf(keyname, 128,"COM%d_SCR",i);
    V3_CHKPT_SAVE(ctx, keyname, serial->scr.data,failout);
    snprintf(keyname, 128,"COM%d_DLL",i);
    V3_CHKPT_SAVE(ctx, keyname, serial->dll.data,failout);
    snprintf(keyname, 128,"COM%d_DLM",i);
    V3_CHKPT_SAVE(ctx, keyname, serial->dlm.data,failout);
    snprintf(keyname, 128,"COM%d_int_state",i);
    V3_CHKPT_SAVE(ctx, keyname, serial->int_state,failout);
    
    if (serial_buffer_save(ctx, i, &(serial->tx_buffer), "TX")) { 
      PrintError(VM_NONE, VCORE_NONE, "Failed to save serial tx buffer %d\n",i);
      goto failout;
    }
    
    if (serial_buffer_save(ctx, i, &(serial->rx_buffer), "RX")) { 
      PrintError(VM_NONE, VCORE_NONE, "Failed to save serial rx buffer %d\n",i);
      goto failout;
    }
    
    snprintf(keyname, 128,"COM%d_IRQ_NUM",i);
    V3_CHKPT_SAVE(ctx, keyname, serial->irq_number,failout);
  }

  return 0;

 failout:
  PrintError(VM_NONE, VCORE_NONE, "Failed to save serial device\n");
  return -1;
  
}
 
static int serial_load(struct v3_chkpt_ctx * ctx, void * private_data) {
  struct serial_state *state = (struct serial_state *)private_data;
  struct serial_port *serial;
  char keyname[128];
  int i;
  
  for (i=0;i<4;i++) { 
    serial = &(state->coms[i]);
    snprintf(keyname, 128,"COM%d_RBR",i);
    V3_CHKPT_LOAD(ctx, keyname, serial->rbr.data,failout);
    snprintf(keyname, 128,"COM%d_THR",i);
    V3_CHKPT_LOAD(ctx, keyname, serial->thr.data,failout);
    snprintf(keyname, 128,"COM%d_IER",i);
    V3_CHKPT_LOAD(ctx, keyname, serial->ier.val,failout);
    snprintf(keyname, 128,"COM%d_IIR",i);
    V3_CHKPT_LOAD(ctx, keyname, serial->iir.val,failout);
    snprintf(keyname, 128,"COM%d_FCR",i);
    V3_CHKPT_LOAD(ctx, keyname, serial->fcr.val,failout);
    snprintf(keyname, 128,"COM%d_LCR",i);
    V3_CHKPT_LOAD(ctx, keyname, serial->lcr.val,failout);
    snprintf(keyname, 128,"COM%d_MCR",i);
    V3_CHKPT_LOAD(ctx, keyname, serial->mcr.val,failout);
    snprintf(keyname, 128,"COM%d_LSR",i);
    V3_CHKPT_LOAD(ctx, keyname, serial->lsr.val,failout);
    snprintf(keyname, 128,"COM%d_MSR",i);
    V3_CHKPT_LOAD(ctx, keyname, serial->msr.val,failout);
    snprintf(keyname, 128,"COM%d_SCR",i);
    V3_CHKPT_LOAD(ctx, keyname, serial->scr.data,failout);
    snprintf(keyname, 128,"COM%d_DLL",i);
    V3_CHKPT_LOAD(ctx, keyname, serial->dll.data,failout);
    snprintf(keyname, 128,"COM%d_DLM",i);
    V3_CHKPT_LOAD(ctx, keyname, serial->dlm.data,failout);
    snprintf(keyname, 128,"COM%d_int_state",i);
    V3_CHKPT_LOAD(ctx, keyname, serial->int_state,failout);

    if (serial_buffer_load(ctx, i, &(serial->tx_buffer), "TX")) { 
      PrintError(VM_NONE, VCORE_NONE, "Failed to load serial tx buffer %d\n",i);
      goto failout;
    }
    
    if (serial_buffer_load(ctx, i, &(serial->rx_buffer), "RX")) { 
      PrintError(VM_NONE, VCORE_NONE, "Failed to load serial rx buffer %d\n",i);
      goto failout;
    }
    
    snprintf(keyname, 128,"COM%d_IRQ_NUM",i);
    V3_CHKPT_LOAD(ctx, keyname, serial->irq_number,failout);
  }

  return 0;

 failout:
  PrintError(VM_NONE, VCORE_NONE,"Failed to load serial device\n");
  return -1;
  
}

#endif

static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))serial_free,
#ifdef V3_CONFIG_CHECKPOINT
    .save = serial_save, 
    .load = serial_load
#endif
};



static int init_serial_port(struct serial_port * com) {

    // zero all
    memset(com, 0, sizeof(*com));

    com->ier.val = IER_INIT_VAL;
    com->iir.val = IIR_INIT_VAL;
    com->fcr.val = FCR_INIT_VAL;
    com->lcr.val = LCR_INIT_VAL;
    com->mcr.val = MCR_INIT_VAL;
    com->lsr.val = LSR_INIT_VAL;
    com->msr.val = MSR_INIT_VAL;

    com->dll.data =  DLL_INIT_VAL;
    com->dlm.data =  DLM_INIT_VAL;

    flush_buffer(&(com->tx_buffer));
    flush_buffer(&(com->rx_buffer));

    v3_lock_init(&(com->lock));

    com->ops = NULL;
    com->backend_data = NULL;

    return 0;
}

static int deinit_serial_port(struct serial_port * com) {

    v3_lock_deinit(&(com->lock));

    return 0;
}

static sint64_t serial_input(struct v3_vm_info * vm, uint8_t * buf, sint64_t len, void * priv_data){
    struct serial_port * com_port = (struct serial_port *)priv_data;
    int i;
    addr_t irq_status;

    irq_status = v3_lock_irqsave(com_port->lock);
    
    for(i = 0; i < len; i++){
      if (queue_data(vm, com_port, &(com_port->rx_buffer), buf[i])) {
	break;
      }
    }

    updateIRQ(vm, com_port);

    v3_unlock_irqrestore(com_port->lock,irq_status);

    return i;
}


static int connect_fn(struct v3_vm_info * vm, 
		      void * frontend_data, 
		      struct v3_dev_char_ops * ops, 
		      v3_cfg_tree_t * cfg, 
		      void * private_data, 
		      void ** push_fn_arg) {

    struct serial_state * serial = (struct serial_state *)frontend_data;
    struct serial_port * com = NULL;
    char * com_port = v3_cfg_val(cfg, "com_port");
    int com_idx = 0;

    if (com_port == NULL) {
        PrintError(vm, VCORE_NONE, "UART: Invalid Serial frontend config: missing \"com_port\"\n");
	return -1;
    }
    
    com_idx = atoi(com_port) - 1;

    if ((com_idx > 3) || (com_idx < 0)) {
      PrintError(vm, VCORE_NONE, "UART: Invalid Com port (%s) \n", com_port);
	return -1;
    }

    com = &(serial->coms[com_idx]);

    com->ops = ops;
    com->backend_data = private_data;

    com->ops->input = serial_input;
    *push_fn_arg = com;

    return 0;
}

static int serial_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct serial_state * state = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");
    int ret = 0;

    state = (struct serial_state *)V3_Malloc(sizeof(struct serial_state));
    
    if (state == NULL) {
        PrintError(vm,VCORE_NONE, "UART: Could not allocate Serial Device\n");
	return -1;
    }
    
    memset(state, 0, sizeof(struct serial_state));

    init_serial_port(&(state->coms[0]));
    init_serial_port(&(state->coms[1]));
    init_serial_port(&(state->coms[2]));
    init_serial_port(&(state->coms[3]));

    state->coms[0].irq_number = COM1_IRQ;
    state->coms[1].irq_number = COM2_IRQ;
    state->coms[2].irq_number = COM3_IRQ;
    state->coms[3].irq_number = COM4_IRQ;


    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, state);

    if (dev == NULL) {
	PrintError(vm, VCORE_NONE, "UART: Could not attach device %s\n", dev_id);
	V3_Free(state);
	return -1;
    }

    PrintDebug(vm, VCORE_NONE, "UART: Serial device attached\n");

    ret |= v3_dev_hook_io(dev, COM1_DATA_PORT, &read_data_port, &write_data_port);
    ret |= v3_dev_hook_io(dev, COM1_IRQ_ENABLE_PORT, &read_ctrl_port, &write_ctrl_port);
    ret |= v3_dev_hook_io(dev, COM1_FIFO_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    ret |= v3_dev_hook_io(dev, COM1_LINE_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    ret |= v3_dev_hook_io(dev, COM1_MODEM_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    ret |= v3_dev_hook_io(dev, COM1_LINE_STATUS_PORT, &read_status_port, &write_status_port);
    ret |= v3_dev_hook_io(dev, COM1_MODEM_STATUS_PORT, &read_status_port, &write_status_port);
    ret |= v3_dev_hook_io(dev, COM1_SCRATCH_PORT, &read_ctrl_port, &write_ctrl_port);

    ret |= v3_dev_hook_io(dev, COM2_DATA_PORT, &read_data_port, &write_data_port);
    ret |= v3_dev_hook_io(dev, COM2_IRQ_ENABLE_PORT, &read_ctrl_port, &write_ctrl_port);
    ret |= v3_dev_hook_io(dev, COM2_FIFO_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    ret |= v3_dev_hook_io(dev, COM2_LINE_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    ret |= v3_dev_hook_io(dev, COM2_MODEM_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    ret |= v3_dev_hook_io(dev, COM2_LINE_STATUS_PORT, &read_status_port, &write_status_port);
    ret |= v3_dev_hook_io(dev, COM2_MODEM_STATUS_PORT, &read_status_port, &write_status_port);
    ret |= v3_dev_hook_io(dev, COM2_SCRATCH_PORT, &read_ctrl_port, &write_ctrl_port);

    ret |= v3_dev_hook_io(dev, COM3_DATA_PORT, &read_data_port, &write_data_port);
    ret |= v3_dev_hook_io(dev, COM3_IRQ_ENABLE_PORT, &read_ctrl_port, &write_ctrl_port);
    ret |= v3_dev_hook_io(dev, COM3_FIFO_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    ret |= v3_dev_hook_io(dev, COM3_LINE_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    ret |= v3_dev_hook_io(dev, COM3_MODEM_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    ret |= v3_dev_hook_io(dev, COM3_LINE_STATUS_PORT, &read_status_port, &write_status_port);
    ret |= v3_dev_hook_io(dev, COM3_MODEM_STATUS_PORT, &read_status_port, &write_status_port);
    ret |= v3_dev_hook_io(dev, COM3_SCRATCH_PORT, &read_ctrl_port, &write_ctrl_port);

    ret |= v3_dev_hook_io(dev, COM4_DATA_PORT, &read_data_port, &write_data_port);
    ret |= v3_dev_hook_io(dev, COM4_IRQ_ENABLE_PORT, &read_ctrl_port, &write_ctrl_port);
    ret |= v3_dev_hook_io(dev, COM4_FIFO_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    ret |= v3_dev_hook_io(dev, COM4_LINE_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    ret |= v3_dev_hook_io(dev, COM4_MODEM_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    ret |= v3_dev_hook_io(dev, COM4_LINE_STATUS_PORT, &read_status_port, &write_status_port);
    ret |= v3_dev_hook_io(dev, COM4_MODEM_STATUS_PORT, &read_status_port, &write_status_port);
    ret |= v3_dev_hook_io(dev, COM4_SCRATCH_PORT, &read_ctrl_port, &write_ctrl_port);

    if (ret != 0) {
	PrintError(vm, VCORE_NONE, "UART: Error hooking Serial IO ports\n");
	v3_remove_device(dev);
	return -1;
    }

    PrintDebug(vm, VCORE_NONE, "UART: Serial ports hooked\n");



    if (v3_dev_add_char_frontend(vm, dev_id, connect_fn, (void *)state) == -1) {
	PrintError(vm, VCORE_NONE, "UART: Could not register %s as frontend\n", dev_id);
	v3_remove_device(dev);
	return -1;
    }


    return 0;
}





device_register("SERIAL", serial_init)
