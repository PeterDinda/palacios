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

#ifndef GEEKOS_RTL8139_H
#define GEEKOS_RTL8139_H

#include <geekos/malloc.h>
 
#define RTL8139_IRQ	 11

#define RTL8139_BASE_ADDR 	0xc100		/* Starting address of the card (a guess right now)*/

#define RTL8139_IDR0	RTL8139_BASE_ADDR		/* ID Registers */
#define RTL8139_IDR1	(RTL8139_BASE_ADDR + 0x01)
#define RTL8139_IDR2	(RTL8139_BASE_ADDR + 0x02)
#define RTL8139_IDR3	(RTL8139_BASE_ADDR + 0x03)
#define RTL8139_IDR4	(RTL8139_BASE_ADDR + 0x04)
#define RTL8139_IDR5	(RTL8139_BASE_ADDR + 0x05)

#define RTL8139_MAR0	(RTL8139_BASE_ADDR + 0x08)	/* Mulicast Registers*/
#define RTL8139_MAR1	(RTL8139_BASE_ADDR + 0x09)
#define RTL8139_MAR2	(RTL8139_BASE_ADDR + 0x0a)
#define RTL8139_MAR3	(RTL8139_BASE_ADDR + 0x0b)
#define RTL8139_MAR4	(RTL8139_BASE_ADDR + 0x0c)
#define RTL8139_MAR5	(RTL8139_BASE_ADDR + 0x0d)
#define RTL8139_MAR6	(RTL8139_BASE_ADDR + 0x0e)
#define RTL8139_MAR7	(RTL8139_BASE_ADDR + 0x0f)

#define RTL8139_TSD0	(RTL8139_BASE_ADDR + 0x10)	/* Tx Status of Descriptors */
#define RTL8139_TSD1	(RTL8139_BASE_ADDR + 0x14)
#define RTL8139_TSD2	(RTL8139_BASE_ADDR + 0x18)
#define RTL8139_TSD3	(RTL8139_BASE_ADDR + 0x1c)

#define RTL8139_TSAD0	(RTL8139_BASE_ADDR + 0x20)	/* Tx Start Address of Descriptors */
#define RTL8139_TSAD1	(RTL8139_BASE_ADDR + 0x24)
#define RTL8139_TSAD2	(RTL8139_BASE_ADDR + 0x28)
#define RTL8139_TSAD3	(RTL8139_BASE_ADDR + 0x2c)

#define RTL8139_RBSTART	(RTL8139_BASE_ADDR + 0x30)	/* Rx Buffer Start Address */
#define RTL8139_ERBCR	(RTL8139_BASE_ADDR + 0x34)	/* Early Rx Byte Count Register */
#define RTL8139_ERSR	(RTL8139_BASE_ADDR + 0x36)	/* Early Rx Status Register */
#define RTL8139_CR	(RTL8139_BASE_ADDR + 0x37)	/* Command Register */
#define RTL8139_CAPR	(RTL8139_BASE_ADDR + 0x38)	/* Current Address of Pkt Read */
#define RTL8139_CBR	(RTL8139_BASE_ADDR + 0x3a)	/* Current Buffer Address */
#define RTL8139_IMR	(RTL8139_BASE_ADDR + 0x3c)	/* Intrpt Mask Reg */
#define RTL8139_ISR	(RTL8139_BASE_ADDR + 0x3e)	/* Intrpt Status Reg */
#define RTL8139_TCR	(RTL8139_BASE_ADDR + 0x40)	/* Tx Config Reg */
#define RTL8139_RCR	(RTL8139_BASE_ADDR + 0x44)	/* Rx Config Reg */
#define RTL8139_TCTR	(RTL8139_BASE_ADDR + 0x48)	/* Timer Count Reg */
#define RTL8139_MPC	(RTL8139_BASE_ADDR + 0x4c)	/* Missed Pkt Counter */
#define RTL8139_9346CR	(RTL8139_BASE_ADDR + 0x50)	/* 9346 Command Reg */
#define RTL8139_CONFIG0	(RTL8139_BASE_ADDR + 0x51)	/* Config Reg */
#define RTL8139_CONFIG1	(RTL8139_BASE_ADDR + 0x52)
#define RTL8139_TimerInt	(RTL8139_BASE_ADDR + 0x54)	/* Timer Intrpt Reg */
#define RTL8139_MSR	(RTL8139_BASE_ADDR + 0x58)	/* Media Status Reg */
#define RTL8139_CONFIG3	(RTL8139_BASE_ADDR + 0x59)	
#define RTL8139_CONFIG4	(RTL8139_BASE_ADDR + 0x5a)
#define RTL8139_MULINT	(RTL8139_BASE_ADDR + 0x5c)	/* Multiple Intrpt Select */
#define RTL8139_RERID	(RTL8139_BASE_ADDR + 0x5e)	
#define RTL8139_TSAD	(RTL8139_BASE_ADDR + 0x60)	/* Tx Status of All Descriptors */
#define RTL8139_BMCR	(RTL8139_BASE_ADDR + 0x62)	/* Basic Mode Control Register */
#define RTL8139_BMSR	(RTL8139_BASE_ADDR + 0x64)	/* Basic Mode Status Register */
#define RTL8139_ANAR	(RTL8139_BASE_ADDR + 0x66)	/* Auto-Negotiation Advertisement Register */
#define RTL8139_ANLPAR	(RTL8139_BASE_ADDR + 0x68)	/* Auto-Negotiation Link Partner Register */
#define RTL8139_ANER	(RTL8139_BASE_ADDR + 0x6a)	/* Auto-Negotiation Expansion Register */
#define RTL8139_DIS	(RTL8139_BASE_ADDR + 0x6c)	/* Disconnect Counter */
#define RTL8139_FCSC	(RTL8139_BASE_ADDR + 0x6e)	/* False Carrier Sense Counter */
#define RTL8139_NWAYTR	(RTL8139_BASE_ADDR + 0x70)	/* N-way Test Register */
#define RTL8139_REC	(RTL8139_BASE_ADDR + 0x72)	/* RX ER Counter */
#define RTL8139_CSCR	(RTL8139_BASE_ADDR + 0x74)	/* CS Config Register */
#define RTL8139_PHY1_PARM	(RTL8139_BASE_ADDR + 0x78)	/* PHY parameter */
#define RTL8139_TW_PARM	(RTL8139_BASE_ADDR + 0x7c)	/* Twister parameter */
#define RTL8139_PHY2_PARM	(RTL8139_BASE_ADDR + 0x80)

#define RTL8139_CRC0	(RTL8139_BASE_ADDR + 0x84)	/* Power Management CRC Reg for wakeup frame */
#define RTL8139_CRC1	(RTL8139_BASE_ADDR + 0x85)
#define RTL8139_CRC2	(RTL8139_BASE_ADDR + 0x86)
#define RTL8139_CRC3	(RTL8139_BASE_ADDR + 0x87)
#define RTL8139_CRC4	(RTL8139_BASE_ADDR + 0x88)
#define RTL8139_CRC5	(RTL8139_BASE_ADDR + 0x89)
#define RTL8139_CRC6	(RTL8139_BASE_ADDR + 0x8a)
#define RTL8139_CRC7	(RTL8139_BASE_ADDR + 0x8b)

#define RTL8139_Wakeup0	(RTL8139_BASE_ADDR + 0x8c)	/* Power Management wakeup frame */
#define RTL8139_Wakeup1	(RTL8139_BASE_ADDR + 0x94)
#define RTL8139_Wakeup2	(RTL8139_BASE_ADDR + 0x9c)
#define RTL8139_Wakeup3	(RTL8139_BASE_ADDR + 0xa4)
#define RTL8139_Wakeup4	(RTL8139_BASE_ADDR + 0xac)
#define RTL8139_Wakeup5	(RTL8139_BASE_ADDR + 0xb4)
#define RTL8139_Wakeup6	(RTL8139_BASE_ADDR + 0xbc)
#define RTL8139_Wakeup7	(RTL8139_BASE_ADDR + 0xc4)

#define RTL8139_LSBCRO0	(RTL8139_BASE_ADDR + 0xcc)	/* LSB of the mask byte of wakeup frame */
#define RTL8139_LSBCRO1	(RTL8139_BASE_ADDR + 0xcd)
#define RTL8139_LSBCRO2	(RTL8139_BASE_ADDR + 0xce)
#define RTL8139_LSBCRO3	(RTL8139_BASE_ADDR + 0xcf)
#define RTL8139_LSBCRO4	(RTL8139_BASE_ADDR + 0xd0)
#define RTL8139_LSBCRO5	(RTL8139_BASE_ADDR + 0xd1)
#define RTL8139_LSBCRO6	(RTL8139_BASE_ADDR + 0xd2)
#define RTL8139_LSBCRO7	(RTL8139_BASE_ADDR + 0xd3)

#define RTL8139_Config5	(RTL8139_BASE_ADDR + 0xd8)

/* Interrupts */
#define PKT_RX		0x0001
#define RX_ERR 		0x0002
#define TX_OK 		0x0004
#define TX_ERR 		0x0008
#define RX_BUFF_OF 	0x0010
#define RX_UNDERRUN	0x0020
#define RX_FIFO_OF	0x0040
#define CABLE_LEN_CHNG	0x2000
#define TIME_OUT	0x4000
#define SERR		0x8000


void Init_8139();
int rtl8139_Send(uchar_t *packet, uint_t size);
int rtl8139_Receive();
void rtl8139_Clear_IRQ(uint_t interrupts);

#endif










