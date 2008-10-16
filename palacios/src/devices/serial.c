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

#include <devices/serial.h>
#include <palacios/vmm.h>


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



struct irq_enable_reg {
  uint_t erbfi   : 1;  // Enable Receiver Buffer full interrupt
  uint_t etbei   : 1;  // Enable Transmit buffer empty interrupt
  uint_t elsi    : 1;  // Enable Line Status Interrupt
  uint_t edssi   : 1;  // Enable Delta Status signals interrupt
  uint_t rsvd    : 4;   // MBZ
};



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

struct irq_id_reg {
  uint_t pending : 1; // Interrupt pending (0=interrupt pending)
  uint_t iid     : 3; // Interrupt Identification
  uint_t rsvd    : 2; // MBZ
  uint_t fifo_en : 2; // FIFO enable
};

struct fifo_ctrl_reg {
  uint_t enable  : 1; // enable fifo
  uint_t rfres   : 1; // RX FIFO reset
  uint_t xfres   : 1; // TX FIFO reset
  uint_t dma_sel : 1; // DMA mode select
  uint_t rsvd    : 2; // MBZ
  uint_t rx_trigger: 2; // RX FIFO trigger level select
};

struct line_ctrl_reg {
  uint_t word_len       : 2;  // word length select
  uint_t stop_bits      : 1;  // Stop Bit select
  uint_t parity_enable  : 1;  // Enable parity 
  uint_t even_sel       : 1;  // Even Parity Select
  uint_t stick_parity   : 1;  // Stick Parity Select
  uint_t sbr            : 1;  // Set Break 
  uint_t dlab           : 1;  // Divisor latch access bit
};


struct modem_ctrl_reg { 
  uint_t dtr      : 1;
  uint_t rts      : 1;
  uint_t out1     : 1;
  uint_t out2     : 1;
  uint_t loop     : 1;  // loopback mode
  uint_t rsvd     : 3;  // MBZ
};


struct line_status_reg {
  uint_t rbf      : 1;  // Receiver Buffer Full
  uint_t oe       : 1;  // Overrun error
  uint_t pe       : 1;  // Parity Error
  uint_t fe       : 1;  // Framing Error
  uint_t brk      : 1;  // broken line detected
  uint_t thre     : 1;  // Transmitter holding register empty
  uint_t temt     : 1;  // Transmitter Empty
  uint_t fifo_err : 1;  // at least one error is pending in the RX FIFO chain
};


struct modem_status_reg {
  uint_t dcts     : 1;  // Delta Clear To Send
  uint_t ddsr     : 1;  // Delta Data Set Ready
  uint_t teri     : 1;  // Trailing Edge Ring Indicator
  uint_t ddcd     : 1;  // Delta Data Carrier Detect
  uint_t cts      : 1;  // Clear to Send
  uint_t dsr      : 1;  // Data Set Ready
  uint_t ri       : 1;  // Ring Indicator
  uint_t dcd      : 1;  // Data Carrier Detect
};


#define SERIAL_BUF_LEN 256

struct serial_buffer {
  uint_t head; // most recent data
  uint_t tail; // oldest char
  char buffer[SERIAL_BUF_LEN];
};

static int queue_data(struct serial_buffer * buf, char data) {
  uint_t next_loc = (buf->head + 1) % SERIAL_BUF_LEN;

  if (next_loc == buf->tail) {
    return -1;
  }

  buf->buffer[next_loc] = data;
  buf->head = next_loc;

  return 0;
}

static int dequeue_data(struct serial_buffer * buf, char * data) {
  uint_t next_tail = (buf->tail + 1) % SERIAL_BUF_LEN;

  if (buf->head == buf->tail) {
    return -1;
  }

  *data = buf->buffer[buf->tail];
  buf->tail = next_tail;

  return 0;
}


struct serial_port {
  char     ier;
  char     iir;
  char     fcr;
  char     lcr;
  char     mcr;
  char     lsr;
  char     msr;

  struct serial_buffer tx_buffer;
  struct serial_buffer rx_buffer;
};


struct serial_state {
  struct serial_port com1;
  struct serial_port com2;
  struct serial_port com3;
  struct serial_port com4;
};


static int write_data_port(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
  struct serial_state * state = (struct serial_state *)dev->private_data;
  char * val = (char *)src;
  PrintDebug("Write to Data Port 0x%x (val=%x)\n", port, *(char*)src);

  if (length != 1) {
    PrintDebug("Invalid length(%d) in write to 0x%x\n", length, port);
    return -1;
  }

  switch (port) {
  case COM1_DATA_PORT:
    queue_data(&(state->com1.tx_buffer), *val);
    break;
  case COM2_DATA_PORT:
    queue_data(&(state->com2.tx_buffer), *val);
    break;
  case COM3_DATA_PORT:
    queue_data(&(state->com3.tx_buffer), *val);
    break;
  case COM4_DATA_PORT:
    queue_data(&(state->com4.tx_buffer), *val);
    break;
  default:
    return -1;
  }
  

  return length;
}



static int read_data_port(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
  struct serial_state * state = (struct serial_state *)dev->private_data;
  char * val = (char *)dst;
  PrintDebug("Read from Data Port 0x%x\n", port);

  if (length != 1) {
    PrintDebug("Invalid length(%d) in write to 0x%x\n", length, port);
    return -1;
  }

  switch (port) {
  case COM1_DATA_PORT:
    dequeue_data(&(state->com1.tx_buffer), val);
    break;
  case COM2_DATA_PORT:
    dequeue_data(&(state->com2.tx_buffer), val);
    break;
  case COM3_DATA_PORT:
    dequeue_data(&(state->com3.tx_buffer), val);
    break;
  case COM4_DATA_PORT:
    dequeue_data(&(state->com4.tx_buffer), val);
    break;
  default:
    return -1;
  }
  

  return length;
}



static int handle_ier_write(struct serial_port * com, struct irq_enable_reg * ier) {
  

  return -1;
}


static int write_ctrl_port(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
  struct serial_state * state = (struct serial_state *)dev->private_data;
  char * val = (char *)src;
  PrintDebug("Write to Control Port (val=%x)\n", *(char *)src);

  if (length != 1) {
    PrintDebug("Invalid Write length to control port\n", port, port);
    return -1;
  }

  switch (port) {
  case COM1_IRQ_ENABLE_PORT:
    if (handle_ier_write(&(state->com1), (struct irq_enable_reg *)val) == -1) {
      return -1;
    }
    break;
  case COM2_IRQ_ENABLE_PORT:
    if (handle_ier_write(&(state->com2), (struct irq_enable_reg *)val) == -1) {
      return -1;
    }
    break;
  case COM3_IRQ_ENABLE_PORT:
    if (handle_ier_write(&(state->com3), (struct irq_enable_reg *)val) == -1) {
      return -1;
    }
    break;
  case COM4_IRQ_ENABLE_PORT:
    if (handle_ier_write(&(state->com4), (struct irq_enable_reg *)val) == -1) {
      return -1;
    }
    break;

  case COM1_FIFO_CTRL_PORT:
  case COM2_FIFO_CTRL_PORT:
  case COM3_FIFO_CTRL_PORT:
  case COM4_FIFO_CTRL_PORT:

  case COM1_LINE_CTRL_PORT:
  case COM2_LINE_CTRL_PORT:
  case COM3_LINE_CTRL_PORT:
  case COM4_LINE_CTRL_PORT:

  case COM1_MODEM_CTRL_PORT:
  case COM2_MODEM_CTRL_PORT:
  case COM3_MODEM_CTRL_PORT:
  case COM4_MODEM_CTRL_PORT:
    


  default:
    return -1;
  }


  return -1;
}




static int read_ctrl_port(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
  struct serial_state * state = (struct serial_state *)dev->private_data;
  char * val = (char *)dst;
  PrintDebug("Read from Control Port\n");

  if (length != 1) {
    PrintDebug("Invalid Read length to control port\n");
    return -1;
  }

  switch (port) {
  case COM1_IRQ_ENABLE_PORT:
    *val = state->com1.ier;
    break;
  case COM2_IRQ_ENABLE_PORT:
    *val = state->com2.ier;
    break;
  case COM3_IRQ_ENABLE_PORT:
    *val = state->com3.ier;
    break;
  case COM4_IRQ_ENABLE_PORT:
    *val = state->com4.ier;
    break;

  case COM1_FIFO_CTRL_PORT:
    *val = state->com1.fcr;
    break;
  case COM2_FIFO_CTRL_PORT:
    *val = state->com2.fcr;
    break;
  case COM3_FIFO_CTRL_PORT:
    *val = state->com3.fcr;
    break;
  case COM4_FIFO_CTRL_PORT:
    *val = state->com4.fcr;
    break;

  case COM1_LINE_CTRL_PORT:
    *val = state->com1.lcr;
    break;
  case COM2_LINE_CTRL_PORT:
    *val = state->com2.lcr;
    break;
  case COM3_LINE_CTRL_PORT:
    *val = state->com3.lcr;
    break;
  case COM4_LINE_CTRL_PORT:
    *val = state->com4.lcr;
    break;

  case COM1_MODEM_CTRL_PORT:
    *val = state->com1.mcr;
    break;
  case COM2_MODEM_CTRL_PORT:
    *val = state->com2.mcr;
    break;
  case COM3_MODEM_CTRL_PORT:
    *val = state->com3.mcr;
    break;
  case COM4_MODEM_CTRL_PORT:
    *val = state->com4.mcr;
    break;

  default:
    return -1;
  }

  return length;
}


static int write_status_port(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
  PrintDebug("Write to Status Port 0x%x (val=%x)\n", port, *(char *)src);

  return -1;
}

static int read_status_port(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
  struct serial_state * state = (struct serial_state *)dev->private_data;
  char * val = (char *)dst;
  PrintDebug("Read from Status Port 0x%x\n", port);
  
  if (length != 1) {
    PrintDebug("Invalid Read length to control port\n");
    return -1;
  }
  
  switch (port) {
  case COM1_LINE_STATUS_PORT:
    *val = state->com1.lsr;
    break;
  case COM2_LINE_STATUS_PORT:
    *val = state->com2.lsr;
    break;
  case COM3_LINE_STATUS_PORT:
    *val = state->com3.lsr;
    break;
  case COM4_LINE_STATUS_PORT:
    *val = state->com4.lsr;
    break;
    
  case COM1_MODEM_STATUS_PORT:
    *val = state->com1.msr;
    break;
  case COM2_MODEM_STATUS_PORT:
    *val = state->com2.msr;
    break;
  case COM3_MODEM_STATUS_PORT:
    *val = state->com3.msr;
    break;
  case COM4_MODEM_STATUS_PORT:
    *val = state->com4.msr;
    break;

  default:
    return -1;
  }



  return length;
}





static int init_serial_port(struct serial_port * com) {
  //struct irq_enable_reg * ier = (struct irq_enable_reg *)&(com->ier);
  //struct irq_id_reg * iir = (struct irq_id_reg *)&(com->iir);
  //struct fifo_ctrl_reg * fcr = (struct fifo_ctrl_reg *)&(com->fcr);
  //struct line_ctrl_reg * lcr = (struct line_ctrl_reg *)&(com->lcr);
  //struct modem_ctrl_reg * mcr = (struct modem_ctrl_reg *)&(com->mcr);
  //struct line_status_reg * lsr = (struct line_status_reg *)&(com->lsr);
  //struct modem_status_reg * msr = (struct modem_status_reg *)&(com->msr);

  com->ier = 0x00;
  com->iir = 0x01;
  com->fcr = 0x00;
  com->lcr = 0x00;
  com->mcr = 0x00;
  com->lsr = 0x60;
  com->msr = 0x00;

  return 0;
}

static int serial_init(struct vm_device * dev) {
  struct serial_state * state = (struct serial_state *)dev->private_data;


  init_serial_port(&(state->com1));
  init_serial_port(&(state->com2));
  init_serial_port(&(state->com3));
  init_serial_port(&(state->com4));

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

  return 0;
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



static struct vm_device_ops dev_ops = {
  .init = serial_init,
  .deinit = serial_deinit,
  .reset = NULL,
  .start = NULL,
  .stop = NULL,
};


struct vm_device * v3_create_serial(int num_ports) {
  struct serial_state * state = NULL;
  state = (struct serial_state *)V3_Malloc(sizeof(struct serial_state));
  V3_ASSERT(state != NULL);

  struct vm_device * device = v3_create_device("Serial UART", &dev_ops, state);

  return device;
}
