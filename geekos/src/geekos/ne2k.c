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

#include <geekos/ne2k.h>
#include <geekos/debug.h>
#include <geekos/io.h>
#include <geekos/irq.h>
#include <geekos/malloc.h>
#include <geekos/string.h>

#ifdef UIP
#include <uip/uip.h>
#include <uip/uip_arp.h>
#endif

#define DEBUG 1
#define TX_START_BUFF 	0x40
#define RX_START_BUFF 	0x4c
#define RX_END_BUFF	0x80

static uint_t next = (RX_START_BUFF << 8);
static uint_t received = 0;
static uint_t send_done = 1;

struct NE2K_REGS* regs;

struct callback {
  int (*packet_received)(struct NE2K_Packet_Info *info, uchar_t *packet);
} callbacks;

#if DEBUG
static void Dump_Registers()
{
  uint_t data;
  PrintBoth("Dumping NIC registers for page %x...\n", (In_Byte(NE2K_CR) & 0xc0) >> 6);
  uint_t i = 0;
  for(i = 0; i <= 0x0f; i += 0x01) {
    data = In_Byte(NE2K_BASE_ADDR+i);
    PrintBoth("\t%x: %x\n", NE2K_BASE_ADDR + i, data);
  }
}
#endif

static void NE2K_Interrupt_Handler(struct Interrupt_State * state)
{

  Begin_IRQ(state);
  PrintBoth("NIC Interrupt Occured!\n");
  uchar_t isr_content = In_Byte(NE2K_ISR);

  PrintBoth("Contents of ISR: %x\n", isr_content);

  if(isr_content & 0x01) /* A packet has been received. */
  {
        uchar_t current;
	Out_Byte(NE2K_CR, 0x4a);	/* Page 1 */
	current = In_Byte(NE2K_CURR);
	Out_Byte(NE2K_CR, 0x0a);	/* Page 0 */
        NE2K_Receive();

        /* When CURR equals BNRY, all packets in the receive ring buffer have been read, and
           the packet received bit in the interrupt status register can be cleared. */
        if(current == In_Byte(NE2K_BNRY))
          Out_Byte(NE2K_ISR, 0x01);
  }

  End_IRQ(state);
  if(isr_content & 0x02) /* A packet has been successfully transmitted. */
  {
    send_done = 1;  
    Out_Byte(NE2K_ISR, 0x02);
  }

  //Out_Byte(NE2K_ISR, 0xff); /* Clear all interrupts. */
}

int Init_Ne2k(int (*rcvd_fn)(struct NE2K_Packet_Info *info, uchar_t *packet))
{
  callbacks.packet_received = rcvd_fn;

  PrintBoth("Initializing network card...\n");
  Out_Byte(NE2K_CR+0x1f, In_Byte(NE2K_CR+0x1f));  /* Reset */

  regs = Malloc(sizeof(struct NE2K_REGS));
  struct _CR * cr = (struct _CR *)&(regs->cr);
  struct _RCR * rcr = (struct _RCR*)&(regs->rcr);
  struct _IMR * imr = (struct _IMR *)&(regs->imr);

  regs->cr = 0x21;
  regs->dcr = 0x49; /* Word-wide DMA transfer. */
  regs->isr = 0xff; /* Clear all interrupts. */
  regs->rcr = 0x20; /* Accept packets shorter than 64 bytes. */
  regs->tcr = 0x02; /* Internal loopback mode. */

  Out_Byte(NE2K_CR, regs->cr);
  Out_Byte(NE2K_DCR, regs->dcr);
  Out_Byte(NE2K_ISR, regs->isr);
  Out_Byte(NE2K_RCR, regs->rcr);
  Out_Byte(NE2K_TCR, regs->tcr);
  Out_Byte(NE2K_IMR, regs->imr);

  /* Remote byte count registers. */
  Out_Byte(NE2K_RBCR0, 0x00);
  Out_Byte(NE2K_RBCR1, 0x00);
  /* Remote start address registers. */
  Out_Byte(NE2K_RSAR0, 0x00);
  Out_Byte(NE2K_RSAR1, 0x00);

  Out_Byte(NE2K_TPSR, TX_START_BUFF);		/* Transmit page start register */
  Out_Byte(NE2K_PSTART, RX_START_BUFF);		/* Page start register */
  Out_Byte(NE2K_PSTOP, RX_END_BUFF);     		/* Page stop register */
  Out_Byte(NE2K_BNRY, RX_START_BUFF);		/* Boundary register */	

  cr->ps = 0x01; /* Switch to reg page 1. */
  Out_Byte(NE2K_CR, regs->cr);
  /* Current page register: points to first free page that can be used for packet reception. */
  Out_Byte(NE2K_CURR, RX_START_BUFF);
  cr->ps = 0x00; /* Switch to page 0 */

  Out_Byte(NE2K_CR, regs->cr);
  Out_Byte(NE2K_ISR, regs->isr);

  /* Interrupt mask register: setting a bit to 1 enables the
     corresponding interrupt in ISR. */
  imr->prxe = 0x1;
  imr->ptxe = 0x1;
  imr->rxee = 0x1;
  imr->txee = 0x1;
  imr->ovwe = 0x1;
  imr->cnte = 0x1;
  Out_Byte(NE2K_IMR, regs->imr);

  cr->ps = 0x01;  /* Switch to reg page 1 */
  Out_Byte(NE2K_CR, regs->cr);

  /* Set the physical address of the card */
  Out_Byte(NE2K_CR+0x01, PHY_ADDR1);
  Out_Byte(NE2K_CR+0x02, PHY_ADDR2);
  Out_Byte(NE2K_CR+0x03, PHY_ADDR3);
  Out_Byte(NE2K_CR+0x04, PHY_ADDR4);
  Out_Byte(NE2K_CR+0x05, PHY_ADDR5);
  Out_Byte(NE2K_CR+0x06, PHY_ADDR6);

  /* Set the multicast address register to all 1s; accepts all multicast packets */
  uint_t i;
  for(i = 0x08; i <= 0x0f; i++) {
    Out_Byte(NE2K_CR+i, 0xff);
  }

  regs->cr = 0x21;  //set CR to start value
  Out_Byte(NE2K_CR, regs->cr);

  regs->tcr = 0x00;
  Out_Byte(NE2K_TCR, regs->tcr);

  rcr->sep = 0x1;
  rcr->ar = 0x1;
  rcr->ab = 0x1;
  rcr->am = 0x1;
  rcr->pro = 0x1; /* Promiscuous mode: accept all packets. */
  rcr->mon = 0x0;
  Out_Byte(NE2K_RCR, regs->rcr);

  cr->sta = 0x1;  // toggle start bit
  cr->stp = 0x0;
  Out_Byte(NE2K_CR, regs->cr);

#if DEBUG
  Dump_Registers();

  cr->ps = 0x01;
  Out_Byte(NE2K_CR, regs->cr);
  Dump_Registers();

  cr->ps = 0x02;
  Out_Byte(NE2K_CR, regs->cr);
  Dump_Registers();

  cr->ps = 0x00;
  Out_Byte(NE2K_CR, regs->cr);
#endif

  Install_IRQ(NE2K_IRQ, NE2K_Interrupt_Handler);
  Enable_IRQ(NE2K_IRQ);
#if 0
  for(i = 0; i < 0; i++)
  {
    NE2K_Transmit(regs);
    PrintBoth("Transmitting a packet\n");
  }
#endif
/*
  uchar_t src_addr[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x58 };
  uchar_t dest_addr[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };

  uint_t size = 64;
  uchar_t *data = Malloc(size);
  data = "This is a 64-byte string that will be used to test transmission.";

  for(i = 0; i < 0; i++) {
    NE2K_Send(regs, src_addr, dest_addr, 0x01, data, size);
  }
*/
  //Free(data);  // Why does this crash?

  return 0;
}

/* 
 * This function is called when there is data in uip_buf that's ready to be sent.
 * uip_arp_out() is used to translate the destination IP address to a MAC address.
 * If the corresponding MAC address isn't in the cache, the packet is replaced with
 * an ARP packet, which is sent out instead.  The original packet will need to be
 * retransmitted at some point in the future.
 */
#ifdef UIP

int NE2K_Transmit(uint_t size)
{
  uip_arp_out();
  uchar_t *data;
  data = Malloc(size);

  /* Based on example code from the uIP documentation... */
  if(size <= UIP_LLH_LEN + UIP_TCPIP_HLEN) {
    memcpy(data, &uip_buf[0], size);
  } else {
    memcpy(data, &uip_buf[0], UIP_LLH_LEN + UIP_TCPIP_HLEN);
    memcpy(data + UIP_LLH_LEN + UIP_TCPIP_HLEN, uip_appdata, size - UIP_TCPIP_HLEN - UIP_LLH_LEN);
  }

  /* Manually copy in the source MAC address for now. */
  uchar_t src_addr[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x58 };
  memcpy(data + 6, src_addr, 6);
  if(*(data+12) != 0x08 || *(data+13) != 0x06)
  {
    /* This is not an ARP packet. Fill in te size of the packet manually. */
    *(data+12) = size & 0xff;
    *(data+13) = (size >> 8) & 0xff;
  }

  NE2K_Send_Packet(data, size);
  Free(data);
  return 0;
}

#endif

int NE2K_Send_Packet(uchar_t *packet, uint_t size)
{
  struct _CR * cr = (struct _CR*)&(regs->cr);
  regs->cr = 0x21; /* Turn off remote DMA, stop command */
  cr->stp = 0x0;  /* toggle start on */
  cr->sta = 0x1;
  Out_Byte(NE2K_CR, regs->cr);
  
  // Read-before-write bug fix?
  Out_Byte(NE2K_RBCR0, 0x42);
  Out_Byte(NE2K_RBCR1, 0x00);
  Out_Byte(NE2K_RSAR0, 0x42);
  Out_Byte(NE2K_RSAR1, 0x00);

  cr->rd = 0x01;  /* set remote DMA to 'remote read' */
  Out_Byte(NE2K_CR, regs->cr);

  regs->isr = 0x40;  /* clear 'remote DMA complete' interrupt */
  Out_Byte(NE2K_ISR, regs->isr);
  
  /* Set remote byte count registers */
  Out_Byte(NE2K_RBCR0, size & 0xff);
  Out_Byte(NE2K_RBCR1, (size >> 8) & 0xff);

  /* Set transmit byte count registers. */
  Out_Byte(NE2K_TBCR0, size & 0xff);
  Out_Byte(NE2K_TBCR1, (size >> 8) & 0xff);

  /* Set remote start address registers to the first page of the transmit ring buffer. */
  Out_Byte(NE2K_RSAR0, 0x00);
  Out_Byte(NE2K_RSAR1, TX_START_BUFF);

  cr->rd = 0x02; /* Set remote DMA to 'remote write' */
  Out_Byte(NE2K_CR, regs->cr);

  /* Push the packet data to into the dataport */
  uint_t i;
  for(i = 0; i < size; i += 2) {
    Out_Word(NE2K_CR + 0x10, (*(packet + i + 1) << 8) | *(packet + i));
  }

  cr->txp = 0x1; /* Start transmission */
  Out_Byte(NE2K_CR, regs->cr);

  return 0;
}

#if 1
/* Assumes src and dest are arrays of 6 characters. */
int NE2K_Send(uchar_t src[], uchar_t dest[], uint_t type, uchar_t *data, uint_t size)
{
  struct _CR * cr = (struct _CR*)&(regs->cr);
  uint_t packet_size = size + 16;
  regs->cr = 0x21; /* Turn off remote DMA, stop command */
  cr->stp = 0x0;  /* toggle start on */
  cr->sta = 0x1;
  Out_Byte(NE2K_CR, regs->cr);
  
  // Read-before-write bug fix?
  Out_Byte(NE2K_RBCR0, 0x42);
  Out_Byte(NE2K_RBCR1, 0x00);
  Out_Byte(NE2K_RSAR0, 0x42);
  Out_Byte(NE2K_RSAR1, 0x00);

  cr->rd = 0x01;  /* set remote DMA to 'remote read' */
  Out_Byte(NE2K_CR, regs->cr);

  regs->isr = 0x40;  /* clear 'remote DMA complete' interrupt */
  Out_Byte(NE2K_ISR, regs->isr);
  
  /* Set remote byte count registers */
  Out_Byte(NE2K_RBCR0, packet_size & 0xff);
  Out_Byte(NE2K_RBCR1, (packet_size >> 8) & 0xff);

  /* Set transmit byte count registers. */
  Out_Byte(NE2K_TBCR0, packet_size & 0xff);
  Out_Byte(NE2K_TBCR1, (packet_size >> 8) & 0xff);

  /* Set remote start address registers to the first page of the transmit ring buffer. */
  Out_Byte(NE2K_RSAR0, 0x00);
  Out_Byte(NE2K_RSAR1, TX_START_BUFF);

  cr->rd = 0x02; /* Set remote DMA to 'remote write' */
  Out_Byte(NE2K_CR, regs->cr);

  /* Begin pushing the packet into the dataport (located at 0x10 from the base address). */
  /* Destination Address */
  Out_Word(NE2K_CR + 0x10, (dest[1] << 8) | dest[0]);
  Out_Word(NE2K_CR + 0x10, (dest[3] << 8) | dest[2]);
  Out_Word(NE2K_CR + 0x10, (dest[5] << 8) | dest[4]);

  /* Source Address */
  Out_Word(NE2K_CR + 0x10, (src[1] << 8) | src[0]);
  Out_Word(NE2K_CR + 0x10, (src[3] << 8) | src[2]);
  Out_Word(NE2K_CR + 0x10, (src[5] << 8) | src[4]);

  /* Type */
  Out_Word(NE2K_CR + 0x10, packet_size);

  /* Packet data */
  uint_t i;
  for(i = 0; i < size; i += 2) {
    Out_Word(NE2K_CR + 0x10, (*(data + i + 1) << 8) | *(data + i));
  }

  cr->txp = 0x1; /* Start transmission */
  Out_Byte(NE2K_CR, regs->cr);

  return 0;
}
#endif

int NE2K_Receive()
{
  PrintBoth("Packet Received\n");

  Out_Byte(NE2K_CR, 0x22);
  /* Set RSAR to the start address of the received packet. */
  Out_Byte(NE2K_RSAR0, next & 0xff);
  Out_Byte(NE2K_RSAR1, next >> 8);
  Out_Byte(NE2K_CR, 0x0a);

  /* 
   * A four byte header is added to the beginning of each received packet by the NIC.
   * The first byte is the location of the next packet in the ring buffer.
   * The second byte is the receive status code.
   * The third and fourth bytes are the size of the packet.
   */

  uint_t i;
  uint_t data;
  data = In_Word(NE2K_CR + 0x10);

#if DEBUG
  PrintBoth("\nPacket data:\n\t");
  PrintBoth("%x ", data);
#endif

  /* Get the location of the next packet */
  next = data & 0xff00;

  /* Retrieve the packet size from the header, and store it in RBCR. */
  uint_t packet_size =  In_Word(NE2K_CR + 0x10) - 4;
  uchar_t *packet = Malloc(packet_size);
  Out_Byte(NE2K_RBCR0, packet_size & 0xff);
  Out_Byte(NE2K_RBCR1, (packet_size>>8) & 0xff);

#if DEBUG
	PrintBoth("packetsize = %x\n\t", packet_size);
#endif

  /* Copy the received packet over from the ring buffer. */
  for(i = 0; i < packet_size; i+=2) {
    data = In_Word(NE2K_CR + 0x10);
    *(packet + i) = data & 0x00ff;
    *(packet + i + 1) = (data & 0xff00) >> 8;
#if 0
    PrintBoth("BNRY = %x\n", In_Byte(NE2K_BNRY));
    Out_Byte(NE2K_CR, 0x4a);
    PrintBoth("CURR = %x\n", In_Byte(NE2K_CURR));  
    Out_Byte(NE2K_CR, 0x0a);
#endif
  }

//Out_Byte(NE2K_RBCR0, (In_Byte(NE2K_RBCR0))-2);
//Out_Byte(NE2K_RSAR0, (In_Byte(NE2K_RSAR0))+2);

  PrintBoth("\n%d packets have been received", ++received);
  PrintBoth("\n\n");

  Out_Byte(NE2K_ISR, 0x40); /* Clear the remote DMA complete interrupt. */

  /* The BNRY register stores the location of the first packet that hasn't been read yet */
  Out_Byte(NE2K_BNRY, next >> 8);

  struct NE2K_Packet_Info *info = Malloc(sizeof(struct NE2K_Packet_Info));
  info->size = packet_size;
  info->status = 0;
  memcpy(info->dest, packet, 6);
  memcpy(info->src, packet + 6, 6);
  callbacks.packet_received(info, packet);

  return 0;
}




