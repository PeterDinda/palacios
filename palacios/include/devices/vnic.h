/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Lei Xia <lxia@northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Lei Xia <lxia@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VNIC_H_
#define __VNIC_H_

#include <palacios/vm_dev.h>

#define NIC_BASE_ADDR 	0xc100

#define NIC_IRQ	11		/* Interrupt channel */

#define MAX_ETH_FRAME_SIZE 1514

#define NE2K_PMEM_SIZE    (32*1024)
#define NE2K_PMEM_START   (16*1024)
#define NE2K_PMEM_END     (NE2K_PMEM_SIZE+NE2K_PMEM_START)
#define NE2K_MEM_SIZE     NE2K_PMEM_END

#define EN0_COMMAND (0x00)  // The command register (for all pages) 

#define NIC_DATA_PORT (0x10)  // The data read/write port

#define NIC_RESET_PORT (0x1f)  // The data read/write port

// Page 0 registers
#define EN0_CLDALO	(0x01)	  // Low byte of current local dma addr  RD 
#define EN0_STARTPG	 (0x01)  // Starting page of ring bfr WR 
#define EN0_CLDAHI	 (0x02)	// High byte of current local dma addr  RD 
#define EN0_STOPPG	 (0x02)    //Ending page +1 of ring bfr WR 
#define EN0_BOUNDARY	(0x03)	  //Boundary page of ring bfr RD WR 
#define EN0_TSR		(0x04) 	//Transmit status reg RD 
#define EN0_TPSR	(0x04) 	//Transmit starting page WR 
#define EN0_NCR		(0x05) 	//Number of collision reg RD 
#define EN0_TCNTLO	(0x05) 	//Low  byte of tx byte count WR 
#define EN0_FIFO	(0x06) 	//FIFO RD 
#define EN0_TCNTHI	(0x06)	       //High byte of tx byte count WR 
#define EN0_ISR		(0x07) 	//Interrupt status reg RD WR 
#define EN0_CRDALO	(0x08) 	//low byte of current remote dma address RD 
#define EN0_RSARLO	(0x08) 	//Remote start address reg 0 
#define EN0_CRDAHI	(0x09) 	//high byte, current remote dma address RD 
#define EN0_RSARHI	(0x09) 	//Remote start address reg 1 
#define EN0_RCNTLO	(0x0a) 	//Remote byte count reg WR 
#define EN0_RTL8029ID0	(0x0a) 	//Realtek ID byte #1 RD 
#define EN0_RCNTHI	(0x0b) 	//Remote byte count reg WR 
#define EN0_RTL8029ID1	(0x0b)	//Realtek ID byte #2 RD 
#define EN0_RSR		(0x0c) 	//rx status reg RD 
#define EN0_RXCR	(0x0c) 	//RX configuration reg WR 
#define EN0_TXCR	(0x0d) 	//TX configuration reg WR 
#define EN0_COUNTER0	(0x0d) 	//Rcv alignment error counter RD 
#define EN0_DCFG	(0x0e) 	//Data configuration reg WR 
#define EN0_COUNTER1	(0x0e)	//Rcv CRC error counter RD 
#define EN0_IMR		(0x0f)	 //Interrupt mask reg WR 
#define EN0_COUNTER2	(0x0f) 	//Rcv missed frame error counter RD 

//Page 1 registers
#define EN1_PHYS        (0x01)
#define EN1_CURPAG      (0x07)
#define EN1_MULT       (0x08)

//Page 2 registers
#define EN2_STARTPG	 (0x01)	//Starting page of ring bfr RD 
#define EN2_STOPPG	(0x02)	//Ending page +1 of ring bfr RD 
#define EN2_LDMA0  (0x01)   //Current Local DMA Address 0 WR 
#define EN2_LDMA1  (0x02)   //Current Local DMA Address 1 WR 
#define EN2_RNPR  (0x03)   //Remote Next Packet Pointer RD WR 
#define EN2_TPSR  (0x04)    //Transmit Page Start Address RD 
#define EN2_LNRP  (0x05)   // Local Next Packet Pointer RD WR 
#define EN2_ACNT0  (0x06)  // Address Counter Upper WR 
#define EN2_ACNT1  (0x07)  // Address Counter Lower WR 
#define EN2_RCR  (0x0c)  // Receive Configuration Register RD 
#define EN2_TCR  (0x0d)  // Transmit Configuration Register RD 
#define EN2_DCR  (0x0e)  // Data Configuration Register RD 
#define EN2_IMR  (0x0f)  // Interrupt Mask Register RD 

//Page 3 registers
#define EN3_CONFIG0	 (0x03)
#define EN3_CONFIG1	 (0x04)
#define EN3_CONFIG2	 (0x05)
#define EN3_CONFIG3	 (0x06)

//Bits in EN0_ISR - Interrupt status register
#define ENISR_RX	0x01	//Receiver, no error 
#define ENISR_TX	0x02	//Transmitter, no error 
#define ENISR_RX_ERR	0x04	//Receiver, with error 
#define ENISR_TX_ERR	0x08	//Transmitter, with error 
#define ENISR_OVER	0x10	//Receiver overwrote the ring 
#define ENISR_COUNTERS	0x20	//Counters need emptying 
#define ENISR_RDC	0x40	//remote dma complete 
#define ENISR_RESET	0x80	//Reset completed 
#define ENISR_ALL	0x3f	//Interrupts we will enable 

//Bits in received packet status byte and EN0_RSR
#define ENRSR_RXOK	0x01	//Received a good packet 
#define ENRSR_CRC	0x02	//CRC error 
#define ENRSR_FAE	0x04	//frame alignment error 
#define ENRSR_FO	0x08	//FIFO overrun 
#define ENRSR_MPA	0x10	//missed pkt 
#define ENRSR_PHY	0x20	//physical/multicast address 
#define ENRSR_DIS	0x40	//receiver disable. set in monitor mode 
#define ENRSR_DEF	0x80	//deferring 

//Transmitted packet status, EN0_TSR
#define ENTSR_PTX 0x01	//Packet transmitted without error 
#define ENTSR_ND  0x02	//The transmit wasn't deferred. 
#define ENTSR_COL 0x04	//The transmit collided at least once. 
#define ENTSR_ABT 0x08  //The transmit collided 16 times, and was deferred. 
#define ENTSR_CRS 0x10	//The carrier sense was lost. 
#define ENTSR_FU  0x20  //A "FIFO underrun" occurred during transmit. 
#define ENTSR_CDH 0x40	//The collision detect "heartbeat" signal was lost. 
#define ENTSR_OWC 0x80  //There was an out-of-window collision. 

//command, Register accessed at EN0_COMMAND
#define NE2K_STOP 0x01
#define NE2K_START 0x02
#define NE2K_TRANSMIT 0x04
#define NE2K_DMAREAD	0x08	/* Remote read */
#define NE2K_DMAWRITE	0x10	/* Remote write  */
#define NE2K_DMASEND     0x18
#define NE2K_ABORTDMA	0x20	/* Abort/Complete DMA */
#define NE2K_PAGE0	0x00	/* Select page chip registers */
#define NE2K_PAGE1	0x40	/* using the two high-order bits */
#define NE2K_PAGE2	0x80
#define NE2K_PAGE    0xc0

struct vm_device *v3_create_vnic();

#endif
