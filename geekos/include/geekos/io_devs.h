#ifndef _io_devs
#define _io_devs


//
//
// PIC: Programmable Interrupt Controller
//
#define PIC_MASTER_CMD_ISR_PORT 0x20   // Port where the master PIC command and status register is
#define PIC_MASTER_IMR_PORT     0x21   // Port where the master PIC interrupt mask register is
#define PIC_SLAVE_CMD_ISR_PORT  0xa0   // Port where the slave PIC command and status register is
#define PIC_SLAVE_IMR_PORT      0xa1   // Port where the slave PIC interrupt mask register is


#define BOOT_STATE_CARD_PORT    0x80   // hex codes sent here for display



#endif
