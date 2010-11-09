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
 *             Lei Xia <lxia@northwestern.edu>
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


#ifndef CONFIG_DEBUG_SERIAL
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


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
#define STATUS_IRQ_LSR_OE_SET   0x3
#define STATUS_IRQ_LSR_PE_SET   0x3
#define STATUS_IRQ_LSR_FE_SET   0x3
#define STATUS_IRQ_LSR_BI_SET   0x3
#define RX_IRQ_DR               0x2
#define RX_IRQ_TRIGGER_LEVEL    0x2
#define FIFO_IRQ                0x6
#define TX_IRQ_THRE             0x1
#define MODEL_IRQ_DELTA_SET     0x0

//COMs IRQ ID
#define COM1_IRQ  0x4
#define COM2_IRQ  0x3
#define COM3_IRQ  0x4
#define COM4_IRQ  0x3

#define RX_BUFFER 0x1
#define TX_BUFFER 0x2

//initial value for registers

#define  IER_INIT_VAL 0x3
//receive data available interrupt and THRE interrupt are enabled
#define  IIR_INIT_VAL 0x1
//No Pending Interrupt bit is set.
#define  FCR_INIT_VAL 0xc0
//fifo control register is set to 0
#define  LCR_INIT_VAL 0x3
#define  MCR_INIT_VAL 0x0
#define  LSR_INIT_VAL 0x60
#define  MSR_INIT_VAL 0x0
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
	    uint8_t erbfi   : 1;   // Enable Receiver Buffer full interrupt
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
	    uint8_t stick_parity   : 1;  // Stick Parity Select
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
	    uint8_t dr      : 1;  // data ready
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


    void * backend_data;
    struct v3_dev_char_ops * ops;

};


struct serial_state {
    struct serial_port coms[4];

};



static struct serial_port * get_com_from_port(struct serial_state * serial, uint16_t port) {
    if ((port >= COM1_DATA_PORT) && (port <= COM1_SCRATCH_PORT)) {
	return &(serial->coms[0]);
    } else if ((port >= COM2_DATA_PORT) && (port <= COM2_SCRATCH_PORT)) {
	return &(serial->coms[1]);
    } else if ((port >= COM3_DATA_PORT) && (port <= COM3_SCRATCH_PORT)) {
	return &(serial->coms[2]);
    } else if ((port >= COM4_DATA_PORT) && (port <= COM4_SCRATCH_PORT)) {
	return &(serial->coms[3]);
    } else {
	PrintError("Error: Could not find serial port associated with IO port %d\n", port);
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

static int updateIRQ(struct v3_vm_info * vm, struct serial_port * com) {
    
    if ( (com->ier.erbfi == 0x1) && 
	 (receive_buffer_trigger( getNumber(&(com->rx_buffer)), com->fcr.rx_trigger)) ) {

	PrintDebug("UART: receive buffer interrupt(trigger level reached)");

	com->iir.iid = RX_IRQ_TRIGGER_LEVEL;
	v3_raise_irq(vm, com->irq_number);
    }
    
    if ( (com->iir.iid == RX_IRQ_TRIGGER_LEVEL) && 
	 (!(receive_buffer_trigger( getNumber(&(com->rx_buffer)), com->fcr.rx_trigger))) ) {

	com->iir.iid = 0x0;   //reset interrupt identification register
	com->iir.pending = 0x1;
    }
    
    if ( (com->iir.iid == TX_IRQ_THRE) && 
	 (getNumber(&(com->tx_buffer)) == SERIAL_BUF_LEN)) {

	com->iir.iid = 0x0; //reset interrupt identification register
	com->iir.pending = 0x1;

    } else if ( (com->ier.etbei == 0x1) && 
		(getNumber(&(com->tx_buffer)) != SERIAL_BUF_LEN )) {
	
	PrintDebug("UART: transmit buffer interrupt(buffer not full)");

	com->iir.iid = TX_IRQ_THRE;
	com->iir.pending = 0;

	v3_raise_irq(vm, com->irq_number);
    }

    return 1;
}


static int queue_data(struct v3_vm_info * vm, struct serial_port * com,
		      struct serial_buffer * buf, uint8_t data) {
    int next_loc = (buf->head + 1) % SERIAL_BUF_LEN;    

    if (buf->full == 1) {
	PrintDebug("Buffer is full!\n");

	if (buf == &(com->rx_buffer)) {
	    com->lsr.oe = 1; //overrun error bit set
	}

	return 0;
    }
    
    buf->buffer[next_loc] = data;
    buf->head = next_loc;
    
    if (buf->head == buf->tail) {
	buf->full = 1;
    }
    
    if (buf == &(com->rx_buffer)) {
	com->lsr.dr = 1; //as soon as new data arrives at receive buffer, set data ready bit in lsr.
    }
    
    if (buf == &(com->tx_buffer)) {
	com->lsr.thre = 0; //reset thre and temt bits.
	com->lsr.temt = 0;
    }
    
    updateIRQ(vm, com);
    
    return 0;
}

static int dequeue_data(struct v3_vm_info * vm, struct serial_port * com,
			struct serial_buffer * buf, uint8_t * data) {

    int next_tail = (buf->tail + 1) % SERIAL_BUF_LEN;


    if ( (buf->head == buf->tail) && (buf->full != 1) ) {
	PrintDebug("no data to delete!\n");
	return -1;
    }
    
    if (buf->full == 1) {
	buf->full = 0;
    }
    
        
    *data = buf->buffer[next_tail];
    buf->buffer[next_tail] = 0;
    buf->tail = next_tail;
    
    if ( (buf == &(com->rx_buffer)) && (getNumber(&(com->rx_buffer)) == 0) ) {
	com->lsr.dr = 0;
    }
    
    if ((buf == &(com->tx_buffer)) && (getNumber(&(com->tx_buffer)) == 0)) {
	com->lsr.thre = 1;
	com->lsr.temt = 1;
    }
    
    updateIRQ(vm, com);
    
    return 0;
}

static int write_data_port(struct guest_info * core, uint16_t port, 
			   void * src, uint_t length, struct vm_device * dev) {
    struct serial_state * state = (struct serial_state *)dev->private_data;
    uint8_t * val = (uint8_t *)src;
    struct serial_port * com_port = NULL;

    PrintDebug("Write to Data Port 0x%x (val=%x)\n", port, *val);
    
    if (length != 1) {
	PrintDebug("Invalid length(%d) in write to 0x%x\n", length, port);
	return -1;
    }

    if ((port != COM1_DATA_PORT) && (port != COM2_DATA_PORT) && 
	(port != COM3_DATA_PORT) && (port != COM4_DATA_PORT)) {
	PrintError("Serial Read data port for illegal port Number (%d)\n", port);
	return -1;
    }

    com_port = get_com_from_port(state, port);

    if (com_port == NULL) {
	PrintDebug("UART:read from NOBODY");
	return -1;
    }
    

    // dlab is always checked first
    if (com_port->lcr.dlab == 1) {
	com_port->dll.data = *val;
    }  else {
	

	/* JRL: Some buffering would probably be a good idea here.... */
	if (com_port->ops) {
	    com_port->ops->write(val, 1, com_port->backend_data);
	} else {
	    queue_data(core->vm_info, com_port, &(com_port->tx_buffer), *val);
	}
    }
    
    return length;
}



static int read_data_port(struct guest_info * core, uint16_t port, 
			  void * dst, uint_t length, struct vm_device * dev) {
    struct serial_state * state = (struct serial_state *)dev->private_data;
    uint8_t * val = (uint8_t *)dst;
    struct serial_port * com_port = NULL;

    PrintDebug("Read from Data Port 0x%x\n", port);
    
    if (length != 1) {
	PrintDebug("Invalid length(%d) in write to 0x%x\n", length, port);
	return -1;
    }
    
    if ((port != COM1_DATA_PORT) && (port != COM2_DATA_PORT) && 
	(port != COM3_DATA_PORT) && (port != COM4_DATA_PORT)) {
	PrintError("Serial Read data port for illegal port Number (%d)\n", port);
	return -1;
    }

    com_port = get_com_from_port(state, port);

    if (com_port == NULL) {
	PrintDebug("UART:read from NOBODY");
	return -1;
    }
    
    if (com_port->lcr.dlab == 1) {
	*val = com_port->dll.data;
    } else {
	dequeue_data(core->vm_info, com_port, &(com_port->rx_buffer), val);
    }    
	
    return length;
}



static int handle_fcr_write(struct serial_port * com, uint8_t value) {

    com->fcr.enable = value & 0x1;
    
    if (com->fcr.enable == 0x1) {
	com->fcr.val = value;

	com->fcr.enable = 1; // Do we need to set this??

	//if rfres set, clear receive buffer.
	if (com->fcr.rfres == 0x1) {
	    com->rx_buffer.head = 0;
	    com->rx_buffer.tail = 0;
	    com->rx_buffer.full = 0;
	    memset(com->rx_buffer.buffer, 0, SERIAL_BUF_LEN);
	    com->fcr.rfres = 0;
	}

	//if xfres set, clear transmit buffer.
	if (com->fcr.xfres == 0x1) {
	    com->tx_buffer.head = 0;
	    com->tx_buffer.tail = 0;
	    com->tx_buffer.full = 0;
	    memset(com->tx_buffer.buffer, 0, SERIAL_BUF_LEN);
	    com->fcr.xfres = 0;
	}
    } else {
	//clear both buffers.
	com->tx_buffer.head = 0;
	com->tx_buffer.tail = 0;
	com->tx_buffer.full = 0;
	com->rx_buffer.head = 0;
	com->rx_buffer.tail = 0;
	com->rx_buffer.full = 0;
	
	memset(com->rx_buffer.buffer, 0, SERIAL_BUF_LEN);
	memset(com->tx_buffer.buffer, 0, SERIAL_BUF_LEN);
    }
    
    return 1;
}





static int write_ctrl_port(struct guest_info * core, uint16_t port, void * src, 
			   uint_t length, struct vm_device * dev) {
    struct serial_state * state = (struct serial_state *)dev->private_data;
    uint8_t val = *(uint8_t *)src;
    struct serial_port * com_port = NULL;

    PrintDebug("UART:Write to Control Port (val=%x)\n", val);
    
    if (length != 1) {
	PrintDebug("UART:Invalid Write length to control port%d\n", port);
	return -1;
    }

    com_port = get_com_from_port(state, port);

    if (com_port == NULL) {
	PrintError("Could not find serial port corresponding to IO port %d\n", port);
	return -1;
    }
    
    //always check dlab first
    switch (port) {
	case COM1_IRQ_ENABLE_PORT:
	case COM2_IRQ_ENABLE_PORT:
	case COM3_IRQ_ENABLE_PORT:
	case COM4_IRQ_ENABLE_PORT: {
	    PrintDebug("UART:Write to IER/LATCH port: dlab is %x\n", com_port->lcr.dlab);

	    if (com_port->lcr.dlab == 1) {
		com_port->dlm.data = val;
	    } else {
		com_port->ier.val = val;
	    }

	    break;
	}	    
	case COM1_FIFO_CTRL_PORT:
	case COM2_FIFO_CTRL_PORT:
	case COM3_FIFO_CTRL_PORT:
	case COM4_FIFO_CTRL_PORT: {
	    PrintDebug("UART:Write to FCR");

	    if (handle_fcr_write(com_port, val) == -1) {
		return -1;
	    }

	    break;
	}
	case COM1_LINE_CTRL_PORT:
	case COM2_LINE_CTRL_PORT:
	case COM3_LINE_CTRL_PORT:
	case COM4_LINE_CTRL_PORT: {
	    PrintDebug("UART:Write to LCR");
	    com_port->lcr.val = val;
	    break;
	}
	case COM1_MODEM_CTRL_PORT:
	case COM2_MODEM_CTRL_PORT:
	case COM3_MODEM_CTRL_PORT:
	case COM4_MODEM_CTRL_PORT: {
	    PrintDebug("UART:Write to MCR");
	    com_port->mcr.val = val;
	    break;
	}
	case COM1_SCRATCH_PORT:
	case COM2_SCRATCH_PORT:
	case COM3_SCRATCH_PORT:
	case COM4_SCRATCH_PORT: {
	    PrintDebug("UART:Write to SCRATCH");
	    com_port->scr.data = val;
	    break;
	}
	default:
	    PrintDebug("UART:Write to NOBODY, ERROR");
	    return -1;
    }
    

    return length;
}




static int read_ctrl_port(struct guest_info * core, uint16_t port, void * dst, 
			  uint_t length, struct vm_device * dev) {
    struct serial_state * state = (struct serial_state *)dev->private_data;
    uint8_t * val = (uint8_t *)dst;
    struct serial_port * com_port = NULL;

    PrintDebug("Read from Control Port\n");
    
    if (length != 1) {
	PrintDebug("Invalid Read length to control port\n");
	return -1;
    }

    com_port = get_com_from_port(state, port);

    if (com_port == NULL) {
	PrintError("Could not find serial port corresponding to IO port %d\n", port);
	return -1;
    }
    
    //always check dlab first
    switch (port) {
	case COM1_IRQ_ENABLE_PORT:
	case COM2_IRQ_ENABLE_PORT:
	case COM3_IRQ_ENABLE_PORT:
	case COM4_IRQ_ENABLE_PORT: {
	    PrintDebug("UART:read from IER");

	    if (com_port->lcr.dlab == 1) {
		*val = com_port->dlm.data;
	    } else {
		*val = com_port->ier.val;
	    }
	    break;
	}

	case COM1_FIFO_CTRL_PORT:
	case COM2_FIFO_CTRL_PORT:
	case COM3_FIFO_CTRL_PORT:
	case COM4_FIFO_CTRL_PORT:
	    PrintDebug("UART:read from FCR");
	    *val = com_port->fcr.val;
	    break;

	case COM1_LINE_CTRL_PORT:
	case COM2_LINE_CTRL_PORT:
	case COM3_LINE_CTRL_PORT:
	case COM4_LINE_CTRL_PORT:
	    PrintDebug("UART:read from LCR");
	    *val = com_port->lcr.val;
	    break;

	case COM1_MODEM_CTRL_PORT:
	case COM2_MODEM_CTRL_PORT:
	case COM3_MODEM_CTRL_PORT:
	case COM4_MODEM_CTRL_PORT:
	    PrintDebug("UART:read from MCR");
	    *val = com_port->mcr.val;
	    break;

	case COM1_SCRATCH_PORT:
	case COM2_SCRATCH_PORT:
	case COM3_SCRATCH_PORT:
	case COM4_SCRATCH_PORT:
	    PrintDebug("UART:read from SCRATCH");
	    *val = com_port->scr.data;
	    break;

	default:
	    PrintDebug("UART:read from NOBODY");
	    return -1;
    }

    return length;
}


static int write_status_port(struct guest_info * core, uint16_t port, void * src, 
			     uint_t length, struct vm_device * dev) {
    struct serial_state * state = (struct serial_state *)dev->private_data;
    uint8_t val = *(uint8_t *)src;
    struct serial_port * com_port = NULL;

    PrintDebug("Write to Status Port (val=%x)\n", val);

    if (length != 1) {
	PrintDebug("Invalid Write length to status port %d\n", port);
	return -1;
    }

    com_port = get_com_from_port(state, port);

    if (com_port == NULL) {
	PrintError("Could not find serial port corresponding to IO port %d\n", port);
	return -1;
    }

    switch (port) {
	case COM1_LINE_STATUS_PORT:
	case COM2_LINE_STATUS_PORT:
	case COM3_LINE_STATUS_PORT:
	case COM4_LINE_STATUS_PORT:
	    PrintDebug("UART:write to LSR");
	    com_port->lsr.val = val;
	    break;

	case COM1_MODEM_STATUS_PORT:
	case COM2_MODEM_STATUS_PORT:
	case COM3_MODEM_STATUS_PORT:
	case COM4_MODEM_STATUS_PORT:
	    PrintDebug("UART:write to MSR");
	    com_port->msr.val = val;
	    break;

	default:
	    PrintDebug("UART:write to NOBODY");
	    return -1;
    }

    return length;
}

static int read_status_port(struct guest_info * core, uint16_t port, void * dst, 
			    uint_t length, struct vm_device * dev) {
    struct serial_state * state = (struct serial_state *)dev->private_data;
    uint8_t * val = (uint8_t *)dst;
    struct serial_port * com_port = NULL;

    PrintDebug("Read from Status Port 0x%x\n", port);

    if (length != 1) {
	PrintDebug("Invalid Read length to control port\n");
	return -1;
    }

    com_port = get_com_from_port(state, port);

    if (com_port == NULL) {
	PrintError("Could not find serial port corresponding to IO port %d\n", port);
	return -1;
    }

    switch (port) {
	case COM1_LINE_STATUS_PORT:
	case COM2_LINE_STATUS_PORT:
	case COM3_LINE_STATUS_PORT:
	case COM4_LINE_STATUS_PORT:
	    PrintDebug("UART:read from LSR");

	    *val = com_port->lsr.val;
	    com_port->lsr.oe = 0;     // Why do we clear this??

	    break;

	case COM1_MODEM_STATUS_PORT:
	case COM2_MODEM_STATUS_PORT:
	case COM3_MODEM_STATUS_PORT:
	case COM4_MODEM_STATUS_PORT:
	    PrintDebug("UART:read from COM4 MSR");
	    *val = com_port->msr.val;
	    break;

	default:
	    PrintDebug("UART:read from NOBODY");
	    return -1;
    }

    return length;
}

static int serial_deinit(struct vm_device * dev) {


    v3_dev_unhook_io(dev, COM1_DATA_PORT);
    v3_dev_unhook_io(dev, COM1_IRQ_ENABLE_PORT);
    v3_dev_unhook_io(dev, COM1_FIFO_CTRL_PORT);
    v3_dev_unhook_io(dev, COM1_LINE_CTRL_PORT);
    v3_dev_unhook_io(dev, COM1_MODEM_CTRL_PORT);
    v3_dev_unhook_io(dev, COM1_LINE_STATUS_PORT);
    v3_dev_unhook_io(dev, COM1_MODEM_STATUS_PORT);
    v3_dev_unhook_io(dev, COM1_SCRATCH_PORT);

    v3_dev_unhook_io(dev, COM2_DATA_PORT);
    v3_dev_unhook_io(dev, COM2_IRQ_ENABLE_PORT);
    v3_dev_unhook_io(dev, COM2_FIFO_CTRL_PORT);
    v3_dev_unhook_io(dev, COM2_LINE_CTRL_PORT);
    v3_dev_unhook_io(dev, COM2_MODEM_CTRL_PORT);
    v3_dev_unhook_io(dev, COM2_LINE_STATUS_PORT);
    v3_dev_unhook_io(dev, COM2_MODEM_STATUS_PORT);
    v3_dev_unhook_io(dev, COM2_SCRATCH_PORT);

    v3_dev_unhook_io(dev, COM3_DATA_PORT);
    v3_dev_unhook_io(dev, COM3_IRQ_ENABLE_PORT);
    v3_dev_unhook_io(dev, COM3_FIFO_CTRL_PORT);
    v3_dev_unhook_io(dev, COM3_LINE_CTRL_PORT);
    v3_dev_unhook_io(dev, COM3_MODEM_CTRL_PORT);
    v3_dev_unhook_io(dev, COM3_LINE_STATUS_PORT);
    v3_dev_unhook_io(dev, COM3_MODEM_STATUS_PORT);
    v3_dev_unhook_io(dev, COM3_SCRATCH_PORT);

    v3_dev_unhook_io(dev, COM4_DATA_PORT);
    v3_dev_unhook_io(dev, COM4_IRQ_ENABLE_PORT);
    v3_dev_unhook_io(dev, COM4_FIFO_CTRL_PORT);
    v3_dev_unhook_io(dev, COM4_LINE_CTRL_PORT);
    v3_dev_unhook_io(dev, COM4_MODEM_CTRL_PORT);
    v3_dev_unhook_io(dev, COM4_LINE_STATUS_PORT);
    v3_dev_unhook_io(dev, COM4_MODEM_STATUS_PORT);
    v3_dev_unhook_io(dev, COM4_SCRATCH_PORT);

    return 0;
}




static struct v3_device_ops dev_ops = {
    //.init = serial_init,
    .free = serial_deinit,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};



static int init_serial_port(struct serial_port * com) {

    com->ier.val = IER_INIT_VAL;
    com->iir.val = IIR_INIT_VAL;
    com->fcr.val = FCR_INIT_VAL;
    com->lcr.val = LCR_INIT_VAL;
    com->mcr.val = MCR_INIT_VAL;
    com->lsr.val = LSR_INIT_VAL;
    com->msr.val = MSR_INIT_VAL;

    com->dll.data =  DLL_INIT_VAL;
    com->dlm.data =  DLM_INIT_VAL;
    
    com->tx_buffer.head = 0;
    com->tx_buffer.tail = 0;
    com->tx_buffer.full = 0;
    memset(com->tx_buffer.buffer, 0, SERIAL_BUF_LEN);

    com->rx_buffer.head = 0;
    com->rx_buffer.tail = 0;
    com->rx_buffer.full = 0;
    memset(com->rx_buffer.buffer, 0, SERIAL_BUF_LEN);
    
    com->ops = NULL;
    com->backend_data = NULL;

    return 0;
}

static int serial_input(struct v3_vm_info * vm, uint8_t * buf, uint64_t len, void * priv_data){
    struct serial_port * com_port = (struct serial_port *)priv_data;
    int i;

    for(i = 0; i < len; i++){
    	queue_data(vm, com_port, &(com_port->rx_buffer), buf[i]);
    }

    return len;
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
	PrintError("Invalid Serial frontend config: missing \"com_port\"\n");
	return -1;
    }
    
    com_idx = atoi(com_port) - 1;

    if ((com_idx > 3) || (com_idx < 0)) {
	PrintError("Invalid Com port (%s) \n", com_port);
	return -1;
    }

    com = &(serial->coms[com_idx]);

    com->ops = ops;
    com->backend_data = private_data;

    com->ops->push = serial_input;
    *push_fn_arg = com;

    return 0;
}

static int serial_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct serial_state * state = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");

    state = (struct serial_state *)V3_Malloc(sizeof(struct serial_state));
    
    if (state == NULL) {
	PrintError("Could not allocate Serial Device\n");
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


    struct vm_device * dev = v3_allocate_device(dev_id, &dev_ops, state);

    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", dev_id);
	return -1;
    }

    PrintDebug("Serial device attached\n");

    v3_dev_hook_io(dev, COM1_DATA_PORT, &read_data_port, &write_data_port);
    v3_dev_hook_io(dev, COM1_IRQ_ENABLE_PORT, &read_ctrl_port, &write_ctrl_port);
    v3_dev_hook_io(dev, COM1_FIFO_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    v3_dev_hook_io(dev, COM1_LINE_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    v3_dev_hook_io(dev, COM1_MODEM_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    v3_dev_hook_io(dev, COM1_LINE_STATUS_PORT, &read_status_port, &write_status_port);
    v3_dev_hook_io(dev, COM1_MODEM_STATUS_PORT, &read_status_port, &write_status_port);
    v3_dev_hook_io(dev, COM1_SCRATCH_PORT, &read_ctrl_port, &write_ctrl_port);

    v3_dev_hook_io(dev, COM2_DATA_PORT, &read_data_port, &write_data_port);
    v3_dev_hook_io(dev, COM2_IRQ_ENABLE_PORT, &read_ctrl_port, &write_ctrl_port);
    v3_dev_hook_io(dev, COM2_FIFO_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    v3_dev_hook_io(dev, COM2_LINE_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    v3_dev_hook_io(dev, COM2_MODEM_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    v3_dev_hook_io(dev, COM2_LINE_STATUS_PORT, &read_status_port, &write_status_port);
    v3_dev_hook_io(dev, COM2_MODEM_STATUS_PORT, &read_status_port, &write_status_port);
    v3_dev_hook_io(dev, COM2_SCRATCH_PORT, &read_ctrl_port, &write_ctrl_port);

    v3_dev_hook_io(dev, COM3_DATA_PORT, &read_data_port, &write_data_port);
    v3_dev_hook_io(dev, COM3_IRQ_ENABLE_PORT, &read_ctrl_port, &write_ctrl_port);
    v3_dev_hook_io(dev, COM3_FIFO_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    v3_dev_hook_io(dev, COM3_LINE_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    v3_dev_hook_io(dev, COM3_MODEM_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    v3_dev_hook_io(dev, COM3_LINE_STATUS_PORT, &read_status_port, &write_status_port);
    v3_dev_hook_io(dev, COM3_MODEM_STATUS_PORT, &read_status_port, &write_status_port);
    v3_dev_hook_io(dev, COM3_SCRATCH_PORT, &read_ctrl_port, &write_ctrl_port);

    v3_dev_hook_io(dev, COM4_DATA_PORT, &read_data_port, &write_data_port);
    v3_dev_hook_io(dev, COM4_IRQ_ENABLE_PORT, &read_ctrl_port, &write_ctrl_port);
    v3_dev_hook_io(dev, COM4_FIFO_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    v3_dev_hook_io(dev, COM4_LINE_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    v3_dev_hook_io(dev, COM4_MODEM_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
    v3_dev_hook_io(dev, COM4_LINE_STATUS_PORT, &read_status_port, &write_status_port);
    v3_dev_hook_io(dev, COM4_MODEM_STATUS_PORT, &read_status_port, &write_status_port);
    v3_dev_hook_io(dev, COM4_SCRATCH_PORT, &read_ctrl_port, &write_ctrl_port);

    PrintDebug("Serial ports hooked\n");



    if (v3_dev_add_char_frontend(vm, dev_id, connect_fn, (void *)state) == -1) {
	PrintError("Could not register %s as frontend\n", dev_id);
	return -1;
    }


    return 0;
}





device_register("SERIAL", serial_init)
