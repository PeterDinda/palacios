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


struct serial_port {
  struct irq_enable_reg     ier;
  struct irq_id_reg         iid;
  struct fifo_ctrl_reg      fcr;
  struct line_ctrl_reg      lcr;
  struct model_ctrl_reg     mcr;
  struct line_status_reg    lsr;
  struct model_status_reg   msr;


  char tx_buffer[256];
  char rx_buffer[256];
};


struct serial_state {
  struct serial_port com1;
  struct serial_port com2;
  struct serial_port com3;
  struct serial_port com4;
};


int write_data_port(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
  PrintDebug("Write to Data Port\n");

  return -1;
}

int read_data_port(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
  PrintDebug("Read from Data Port\n");
  return -1;
}


int write_ctrl_port(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
  PrintDebug("Write to Control Port\n");

  return -1;
}

int read_ctrl_port(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
  PrintDebug("Read from Control Port\n");
  return -1;
}


int write_status_port(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
  PrintDebug("Write to Status Port\n");

  return -1;
}

int read_status_port(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
  PrintDebug("Read from Status Port\n");
  return -1;
}







void serial_init(struct vm_device * dev) {
  struct serial_state * state = (struct serial_state *)dev->private_data;

  state->com1.ier.rsvd = 0;
  state->com1.iir.rsvd = 0;
  state->com1.fcr.rsvd = 0;
  state->com1.mcr.rsvd = 0;
  state->com1.iir.pending = 1;

  state->com2.ier.rsvd = 0;
  state->com2.iir.rsvd = 0;
  state->com2.fcr.rsvd = 0;
  state->com2.mcr.rsvd = 0;
  state->com2.iir.pending = 1;

  state->com3.ier.rsvd = 0;
  state->com3.iir.rsvd = 0;
  state->com3.fcr.rsvd = 0;
  state->com3.mcr.rsvd = 0;
  state->com3.iir.pending = 1;

  state->com4.ier.rsvd = 0;
  state->com4.iir.rsvd = 0;
  state->com4.fcr.rsvd = 0;
  state->com4.mcr.rsvd = 0;
  state->com4.iir.pending = 1;


  dev_hook_io(dev, COM1_DATA_PORT, &read_data_port, &write_data_port);
  dev_hook_io(dev, COM1_IRQ_ENABLE_PORT, &read_ctrl_port, &write_ctrl_port);
  dev_hook_io(dev, COM1_FIFO_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
  dev_hook_io(dev, COM1_LINE_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
  dev_hook_io(dev, COM1_MODEL_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
  dev_hook_io(dev, COM1_LINE_STATUS_PORT, &read_status_port, &write_status_port);
  dev_hook_io(dev, COM1_MODEL_STATUS_PORT, &read_status_port, &write_status_port);
  dev_hook_io(dev, COM1_SCRATCH_PORT, &read_ctrl_port, &write_ctrl_port);

  dev_hook_io(dev, COM2_DATA_PORT, &read_data_port, &write_data_port);
  dev_hook_io(dev, COM2_IRQ_ENABLE_PORT, &read_ctrl_port, &write_ctrl_port);
  dev_hook_io(dev, COM2_FIFO_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
  dev_hook_io(dev, COM2_LINE_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
  dev_hook_io(dev, COM2_MODEL_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
  dev_hook_io(dev, COM2_LINE_STATUS_PORT, &read_status_port, &write_status_port);
  dev_hook_io(dev, COM2_MODEL_STATUS_PORT, &read_status_port, &write_status_port);
  dev_hook_io(dev, COM2_SCRATCH_PORT, &read_ctrl_port, &write_ctrl_port);

  dev_hook_io(dev, COM3_DATA_PORT, &read_data_port, &write_data_port);
  dev_hook_io(dev, COM3_IRQ_ENABLE_PORT, &read_ctrl_port, &write_ctrl_port);
  dev_hook_io(dev, COM3_FIFO_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
  dev_hook_io(dev, COM3_LINE_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
  dev_hook_io(dev, COM3_MODEL_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
  dev_hook_io(dev, COM3_LINE_STATUS_PORT, &read_status_port, &write_status_port);
  dev_hook_io(dev, COM3_MODEL_STATUS_PORT, &read_status_port, &write_status_port);
  dev_hook_io(dev, COM3_SCRATCH_PORT, &read_ctrl_port, &write_ctrl_port);

  dev_hook_io(dev, COM4_DATA_PORT, &read_data_port, &write_data_port);
  dev_hook_io(dev, COM4_IRQ_ENABLE_PORT, &read_ctrl_port, &write_ctrl_port);
  dev_hook_io(dev, COM4_FIFO_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
  dev_hook_io(dev, COM4_LINE_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
  dev_hook_io(dev, COM4_MODEL_CTRL_PORT, &read_ctrl_port, &write_ctrl_port);
  dev_hook_io(dev, COM4_LINE_STATUS_PORT, &read_status_port, &write_status_port);
  dev_hook_io(dev, COM4_MODEL_STATUS_PORT, &read_status_port, &write_status_port);
  dev_hook_io(dev, COM4_SCRATCH_PORT, &read_ctrl_port, &write_ctrl_port);

}


void serial_deinit(struct vm_device * dev) {


  dev_unhook_io(dev, COM1_DATA_PORT);
  dev_unhook_io(dev, COM1_IRQ_ENABLE_PORT);
  dev_unhook_io(dev, COM1_FIFO_CTRL_PORT);
  dev_unhook_io(dev, COM1_LINE_CTRL_PORT);
  dev_unhook_io(dev, COM1_MODEL_CTRL_PORT);
  dev_unhook_io(dev, COM1_LINE_STATUS_PORT);
  dev_unhook_io(dev, COM1_MODEL_STATUS_PORT);
  dev_unhook_io(dev, COM1_SCRATCH_PORT);

  dev_unhook_io(dev, COM2_DATA_PORT);
  dev_unhook_io(dev, COM2_IRQ_ENABLE_PORT);
  dev_unhook_io(dev, COM2_FIFO_CTRL_PORT);
  dev_unhook_io(dev, COM2_LINE_CTRL_PORT);
  dev_unhook_io(dev, COM2_MODEL_CTRL_PORT);
  dev_unhook_io(dev, COM2_LINE_STATUS_PORT);
  dev_unhook_io(dev, COM2_MODEL_STATUS_PORT);
  dev_unhook_io(dev, COM2_SCRATCH_PORT);

  dev_unhook_io(dev, COM3_DATA_PORT);
  dev_unhook_io(dev, COM3_IRQ_ENABLE_PORT);
  dev_unhook_io(dev, COM3_FIFO_CTRL_PORT);
  dev_unhook_io(dev, COM3_LINE_CTRL_PORT);
  dev_unhook_io(dev, COM3_MODEL_CTRL_PORT);
  dev_unhook_io(dev, COM3_LINE_STATUS_PORT);
  dev_unhook_io(dev, COM3_MODEL_STATUS_PORT);
  dev_unhook_io(dev, COM3_SCRATCH_PORT);

  dev_unhook_io(dev, COM4_DATA_PORT);
  dev_unhook_io(dev, COM4_IRQ_ENABLE_PORT);
  dev_unhook_io(dev, COM4_FIFO_CTRL_PORT);
  dev_unhook_io(dev, COM4_LINE_CTRL_PORT);
  dev_unhook_io(dev, COM4_MODEL_CTRL_PORT);
  dev_unhook_io(dev, COM4_LINE_STATUS_PORT);
  dev_unhook_io(dev, COM4_MODEL_STATUS_PORT);
  dev_unhook_io(dev, COM4_SCRATCH_PORT);

}



static struct vm_device_ops dev_ops = {
  .init = serial_init,
  .deinit = serial_deini,
  .reset = NULL,
  .start = NULL,
  .stop = NULL,
};


struct vm_device * create_serial(int num_ports) {
  struct serial_state * state = NULL;
  state = (struct serial_state *)V3_Malloc(sizeof(struct serial_state));
  V3_ASSERT(state != NULL);

  struct vm_device * device = create_device("Serial UART", &dev_ops, state);

  return device;
}
