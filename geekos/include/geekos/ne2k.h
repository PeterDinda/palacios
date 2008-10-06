/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Matt Wojcik
 * Copyright (c) 2008, Peter Kamm
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Matt Wojcik
 * Author: Peter Kamm
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef GEEKOS_NE2K_H
#define GEEKOS_NE2K_H

#include <geekos/malloc.h>

#define NE2K_PAGE0	0x00
#define NE2K_PAGE1	0x40
#define NE2K_PAGE2	0x80
#define NE2K_PAGE3	0xc0

#define NE2K_BASE_ADDR 	0xc100		/* Starting address of the card */
#define NE2K_CR		NE2K_BASE_ADDR	/* Command register */
#define NE2K_DATAPORT	(NE2K_CR + 0x10)
#define NE2K_RESET	(NE2K_CR + 0x1f)

/* Page 0 register offsets */
#define NE2K CLDA0	(NE2K_CR + 0x01)
#define NE2K_PSTART	(NE2K_CR + 0x01)	/* Page start register */
#define NE2K_CLDA1	(NE2K_CR + 0x02)
#define NE2K_PSTOP	(NE2K_CR + 0x02)	/* Page stop register */
#define NE2K_BNRY	(NE2K_CR + 0x03)	/* Boundary register */
#define NE2K_TSR	(NE2K_CR + 0x04)
#define NE2K_TPSR	(NE2K_CR + 0x04)
#define NE2K_NCR	(NE2K_CR + 0x05)
#define NE2K_TBCR0	(NE2K_CR + 0x05)
#define NE2K_FIFO	(NE2K_CR + 0x06)
#define NE2K_TBCR1	(NE2K_CR + 0x06)
#define NE2K_ISR	(NE2K_CR + 0x07)	/* Interrupt status register */
#define NE2K_CRDA0	(NE2K_CR + 0x08)
#define NE2K_RSAR0	(NE2K_CR + 0x08)	/* Remote start address registers */
#define NE2K_CRDA1	(NE2K_CR + 0x09)
#define NE2K_RSAR1	(NE2K_CR + 0x09)
#define NE2K_RBCR0	(NE2K_CR + 0x0a)	/* Remote byte count registers */
#define NE2K_RBCR1	(NE2K_CR + 0x0b)
#define NE2K_RSR	(NE2K_CR + 0x0c)
#define NE2K_RCR	(NE2K_CR + 0x0c)	/* Receive configuration register */
#define NE2K_CNTR0	(NE2K_CR + 0x0d)
#define NE2K_TCR	(NE2K_CR + 0x0d)	/* Transmit configuration register */
#define NE2K_CNTR1	(NE2K_CR + 0x0e)
#define NE2K_DCR	(NE2K_CR + 0x0e)	/* Data configuration register */
#define NE2K_CNTR2	(NE2K_CR + 0x0f)
#define NE2K_IMR	(NE2K_CR + 0x0f)	/* Interrupt mask register */

/* Page 1 register offsets */
#define NE2K_PAR0	(NE2K_CR + 0x01)
#define NE2K_PAR1	(NE2K_CR + 0x02)
#define NE2K_PAR2	(NE2K_CR + 0x03)
#define NE2K_PAR3	(NE2K_CR + 0x04)
#define NE2K_PAR4	(NE2K_CR + 0x05)
#define NE2K_PAR5	(NE2K_CR + 0x06)
#define NE2K_CURR	(NE2K_CR + 0x07)
#define NE2K_MAR0	(NE2K_CR + 0x08)
#define NE2K_MAR1	(NE2K_CR + 0x09)
#define NE2K_MAR2	(NE2K_CR + 0x0a)
#define NE2K_MAR3	(NE2K_CR + 0x0b)
#define NE2K_MAR4	(NE2K_CR + 0x0c)
#define NE2K_MAR5	(NE2K_CR + 0x0d)
#define NE2K_MAR6	(NE2K_CR + 0x0e)
#define NE2K_MAR7	(NE2K_CR + 0x0f)

#define NE2K_IRQ	11		/* Interrupt channel */


/* Physical Address of Network Card */
#define PHY_ADDR1 0x52
#define PHY_ADDR2 0x54
#define PHY_ADDR3 0x00
#define PHY_ADDR4 0x12
#define PHY_ADDR5 0x34
#define PHY_ADDR6 0x58


struct NE2K_REGS {
	uchar_t cr;
	uchar_t isr;
	uchar_t imr;
	uchar_t dcr;
	uchar_t tcr;
	uchar_t tsr;
	uchar_t rcr;
	uchar_t rsr;
};

struct _CR {  //COMMAND REG
        uint_t stp: 1;  //STOP- software reset
        uint_t sta: 1;  //START- activates NIC
        uint_t txp: 1;  //TRANSMIT- set to send
        uint_t rd:  3;  //REMOTE DMA
        uint_t ps:  2;  //PAGE SELECT
}__attribute__((__packed__)) __attribute__((__aligned__(1)));

struct _ISR{  //INTERRUPT STATUS REG
        uint_t prx: 1;  //PACKET RECIEVED
        uint_t ptx: 1;  //PACKET TRANSMITTED
        uint_t rxe: 1;  //TRANSMIT ERROR
        uint_t txe: 1;  //RECEIVE ERROR
        uint_t ovw: 1;  //OVERWRITE WARNING
        uint_t cnt: 1;  //COUNTER OVERFLOW
        uint_t rdc: 1;  //REMOTE DMA COMPLETE
        uint_t rst: 1;  //RESET STATUS
}__attribute__((__packed__)) __attribute__((__aligned__(1)));

struct _IMR {  //INTERRUPT MASK REG
        uint_t prxe: 1;  //PACKET RX INTRPT
        uint_t ptxe: 1;  //PACKET TX INTRPT
        uint_t rxee: 1;  //RX ERROR INTRPT
        uint_t txee: 1;  //TX ERROR INTRPt
        uint_t ovwe: 1;  //OVERWRITE WARNING INTRPT
        uint_t cnte: 1;  //COUNTER OVERFLOW INTRPT
        uint_t rdce: 1;  //DMA COMLETE INTRPT
	uint_t rsvd: 1;
}__attribute__((__packed__)) __attribute__((__aligned__(1)));

struct _DCR {  //DATA CONFIG REGISTER
        uint_t wts: 1;  //WORD TRANSFER SELECT
        uint_t bos: 1;  //BYTE ORDER SELECT
        uint_t las: 1;  //LONG ADDR SELECT
        uint_t ls:  1;  //LOOPBACK SELECT
        uint_t arm: 1;  //AUTO-INITIALIZE REMOTE
        uint_t ft:  2;  //FIFO THRESH SELECT
}__attribute__((__packed__)) __attribute__((__aligned__(1)));

struct _TCR {  //TX CONFIG REGISTER
        uint_t crc:  1;  //INHIBIT CRC
        uint_t lb:   2;  //ENCODED LOOPBACK
        uint_t atd:  1;  //AUTO TRANSMIT
        uint_t ofst: 1;  //COLLISION OFFSET ENABLE
	uint_t rsvd: 3;
}__attribute__((__packed__)) __attribute__((__aligned__(1)));

struct _TSR {
        uint_t ptx:  1;  //PACKET TX
	uint_t rsvd: 1;
        uint_t col:  1;  //TX COLLIDED
        uint_t abt:  1;  //TX ABORTED
        uint_t crs:  1;  //CARRIER SENSE LOST
        uint_t fu:   1;  //FIFO UNDERRUN
        uint_t cdh:  1;  //CD HEARTBEAT
        uint_t owc:  1;  //OUT OF WINDOW COLLISION
}__attribute__((__packed__)) __attribute__((__aligned__(1)));

struct _RCR {  //RECEIVE CONFIGURATION REGISTER
        uint_t sep:  1;  //SAVE ERRORED PACKETS
        uint_t ar:   1;  //ACCEPT RUNT PACKETS
        uint_t ab:   1;  //ACCEPT BROADCAST
        uint_t am:   1;  //ACCEPT MULTICAST
        uint_t pro:  1;  //PROMISCUOUS PHYSICAL
        uint_t mon:  1;  //MONITOR MODE
	uint_t rsvd: 2;
}__attribute__((__packed__)) __attribute__((__aligned__(1)));
  
struct _RSR {  //RECEIVE STATUS REG
        uint_t prx: 1;  //PACKET RX INTACT
        uint_t crc: 1;  //CRC ERROR
        uint_t fae: 1;  //FRAME ALIGNMENT ERROR
        uint_t fo:  1;  //FIFO OVERRUN
        uint_t mpa: 1;  //MISSED PACKET
        uint_t phy: 1;  //PHYSICAL/MULTICAST ADDR
        uint_t dis: 1;  //RX DISABLED
        uint_t dfr: 1;  //DEFERRING
}__attribute__((__packed__)) __attribute__((__aligned__(1)));

struct NE2K_Packet_Info {
  uchar_t status;
  uint_t size;
  uchar_t src[6];
  uchar_t dest[6];
};

int Init_Ne2k();
int NE2K_Receive();
int NE2K_Transmit(uint_t size);
int NE2K_Send_Packet(uchar_t *packet, uint_t size);
int NE2K_Send(uchar_t src[], uchar_t dest[], uint_t type, uchar_t *data, uint_t size);

#endif  /* GEEKOS_NE2K_H */
