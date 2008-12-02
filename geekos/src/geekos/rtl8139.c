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

#include <geekos/rtl8139.h>
#include <geekos/debug.h>
#include <geekos/io.h>
#include <geekos/irq.h>
#include <geekos/malloc.h>
#include <geekos/string.h>
#include <uip/uip.h>
#include <uip/uip_arp.h>


#define RX_BUF_LEN (32768 + 16 + 2048)
#define TX_BUF_SIZE 1536
#define TX_FIFO_THRESH 256
uchar_t *rx_buf;
uchar_t *tx_buf;
uint_t cur_tx;
uint_t cur_rx;
uint_t tx_flag = (TX_FIFO_THRESH << 11) & 0x003f0000;
uint_t pkt_cntr = 0;

/* The macros below were copied straight from the linux 8139too.c driver. */
enum ClearBitMasks {
	MultiIntrClear = 0xF000,
	ChipCmdClear = 0xE2,
	Config1Clear = (1<<7)|(1<<6)|(1<<3)|(1<<2)|(1<<1),
};

enum ChipCmdBits {
	CmdReset = 0x10,
	CmdRxEnb = 0x08,
	CmdTxEnb = 0x04,
	RxBufEmpty = 0x01,
};

/* Interrupt register bits */
enum IntrStatusBits {
	PCIErr = 0x8000,
	PCSTimeout = 0x4000,
	RxFIFOOver = 0x40,
	RxUnderrun = 0x20,
	RxOverflow = 0x10,
	TxErr = 0x08,
	TxOK = 0x04,
	RxErr = 0x02,
	RxOK = 0x01,

	RxAckBits = RxFIFOOver | RxOverflow | RxOK,
};

enum TxStatusBits {
	TxHostOwns = 0x2000,
	TxUnderrun = 0x4000,
	TxStatOK = 0x8000,
	TxOutOfWindow = 0x20000000,
	TxAborted = 0x40000000,
	TxCarrierLost = 0x80000000,
};
enum RxStatusBits {
	RxMulticast = 0x8000,
	RxPhysical = 0x4000,
	RxBroadcast = 0x2000,
	RxBadSymbol = 0x0020,
	RxRunt = 0x0010,
	RxTooLong = 0x0008,
	RxCRCErr = 0x0004,
	RxBadAlign = 0x0002,
	RxStatusOK = 0x0001,
};

/* Bits in RxConfig. */
enum rx_mode_bits {
	AcceptErr = 0x20,
	AcceptRunt = 0x10,
	AcceptBroadcast = 0x08,
	AcceptMulticast = 0x04,
	AcceptMyPhys = 0x02,
	AcceptAllPhys = 0x01,
};

/* Bits in TxConfig. */
enum tx_config_bits {

        /* Interframe Gap Time. Only TxIFG96 doesn't violate IEEE 802.3 */
        TxIFGShift = 24,
        TxIFG84 = (0 << TxIFGShift),    /* 8.4us / 840ns (10 / 100Mbps) */
        TxIFG88 = (1 << TxIFGShift),    /* 8.8us / 880ns (10 / 100Mbps) */
        TxIFG92 = (2 << TxIFGShift),    /* 9.2us / 920ns (10 / 100Mbps) */
        TxIFG96 = (3 << TxIFGShift),    /* 9.6us / 960ns (10 / 100Mbps) */

	TxLoopBack = (1 << 18) | (1 << 17), /* enable loopback test mode */
	TxCRC = (1 << 16),	/* DISABLE appending CRC to end of Tx packets */
	TxClearAbt = (1 << 0),	/* Clear abort (WO) */
	TxDMAShift = 8,		/* DMA burst value (0-7) is shifted this many bits */
	TxRetryShift = 4,	/* TXRR value (0-15) is shifted this many bits */

	TxVersionMask = 0x7C800000, /* mask out version bits 30-26, 23 */
};

/* Bits in Config1 */
enum Config1Bits {
	Cfg1_PM_Enable = 0x01,
	Cfg1_VPD_Enable = 0x02,
	Cfg1_PIO = 0x04,
	Cfg1_MMIO = 0x08,
	LWAKE = 0x10,		/* not on 8139, 8139A */
	Cfg1_Driver_Load = 0x20,
	Cfg1_LED0 = 0x40,
	Cfg1_LED1 = 0x80,
	SLEEP = (1 << 1),	/* only on 8139, 8139A */
	PWRDN = (1 << 0),	/* only on 8139, 8139A */
};

/* Bits in Config3 */
enum Config3Bits {
	Cfg3_FBtBEn    = (1 << 0), /* 1 = Fast Back to Back */
	Cfg3_FuncRegEn = (1 << 1), /* 1 = enable CardBus Function registers */
	Cfg3_CLKRUN_En = (1 << 2), /* 1 = enable CLKRUN */
	Cfg3_CardB_En  = (1 << 3), /* 1 = enable CardBus registers */
	Cfg3_LinkUp    = (1 << 4), /* 1 = wake up on link up */
	Cfg3_Magic     = (1 << 5), /* 1 = wake up on Magic Packet (tm) */
	Cfg3_PARM_En   = (1 << 6), /* 0 = software can set twister parameters */
	Cfg3_GNTSel    = (1 << 7), /* 1 = delay 1 clock from PCI GNT signal */
};

/* Bits in Config4 */
enum Config4Bits {
	LWPTN = (1 << 2),	/* not on 8139, 8139A */
};

/* Bits in Config5 */
enum Config5Bits {
	Cfg5_PME_STS     = (1 << 0), /* 1 = PCI reset resets PME_Status */
	Cfg5_LANWake     = (1 << 1), /* 1 = enable LANWake signal */
	Cfg5_LDPS        = (1 << 2), /* 0 = save power when link is down */
	Cfg5_FIFOAddrPtr = (1 << 3), /* Realtek internal SRAM testing */
	Cfg5_UWF         = (1 << 4), /* 1 = accept unicast wakeup frame */
	Cfg5_MWF         = (1 << 5), /* 1 = accept multicast wakeup frame */
	Cfg5_BWF         = (1 << 6), /* 1 = accept broadcast wakeup frame */
};

enum RxConfigBits {
	/* rx fifo threshold */
	RxCfgFIFOShift = 13,
	RxCfgFIFONone = (7 << RxCfgFIFOShift),

	/* Max DMA burst */
	RxCfgDMAShift = 8,
	RxCfgDMAUnlimited = (7 << RxCfgDMAShift),

	/* rx ring buffer length */
	RxCfgRcv8K = 0,
	RxCfgRcv16K = (1 << 11),
	RxCfgRcv32K = (1 << 12),
	RxCfgRcv64K = (1 << 11) | (1 << 12),

	/* Disable packet wrap at end of Rx buffer. (not possible with 64k) */
	RxNoWrap = (1 << 7),
};

/* Twister tuning parameters from RealTek.
   Completely undocumented, but required to tune bad links on some boards. */
enum CSCRBits {
	CSCR_LinkOKBit = 0x0400,
	CSCR_LinkChangeBit = 0x0800,
	CSCR_LinkStatusBits = 0x0f000,
	CSCR_LinkDownOffCmd = 0x003c0,
	CSCR_LinkDownCmd = 0x0f3c0,
};

enum Cfg9346Bits {
	Cfg9346_Lock = 0x00,
	Cfg9346_Unlock = 0xC0,
};

static const uint_t rtl8139_intr_mask =
	PCIErr | PCSTimeout | RxUnderrun | RxOverflow | RxFIFOOver |
	TxErr | TxOK | RxErr | RxOK;

//static void Init_8139(int (*rcvd_fn)(struct NE2K_Packet_Info *info, uchar_t *packet))


static void rtl8139_interrupt( struct Interrupt_State * state )
{
	Begin_IRQ(state);

	uint_t status = In_Word(RTL8139_ISR);

	PrintBoth("Interrupt Received: %x\n", status);

	if( status == PKT_RX )
		rtl8139_Receive();

	rtl8139_Clear_IRQ(status);
	End_IRQ(state);

}

void rtl8139_Clear_IRQ(uint_t interrupts)
{
	Out_Word(RTL8139_ISR, interrupts);
}

void Init_8139()
{
	cur_tx = 0;
	cur_rx = 0;

	/* Reset the chip */
	Out_Byte(RTL8139_CR, CmdReset);

	//while( In_Byte(RTL8139_CR) != 0 )
		/*udelay(10)*/;

	/* Unlock Config[01234] and BMCR register writes */
	Out_Byte(RTL8139_9346CR, Cfg9346_Unlock);

	/* Enable Tx/Rx before setting transfer thresholds */
	Out_Byte(RTL8139_CR, CmdRxEnb | CmdTxEnb);

	/* Using 32K ring */
	Out_DWord(RTL8139_RCR, RxCfgRcv32K | RxNoWrap | (7 << RxCfgFIFOShift) | (7 << RxCfgDMAShift)
				 | AcceptBroadcast | AcceptMyPhys);

//	Out_DWord(RTL8139_TCR, RxCfgRcv32K | RxNoWrap | (7 << RxCfgFIFOShift) | (7 << RxCfgDMAShift));

	Out_DWord(RTL8139_TCR, TxIFG96 | (6 << TxDMAShift) | (8 << TxRetryShift));

	/* Lock Config[01234] and BMCR register writes */
	Out_Byte(RTL8139_9346CR, Cfg9346_Lock);

	/* init Rx ring buffer DMA address */
	rx_buf = Malloc(RX_BUF_LEN);
	Out_DWord(RTL8139_RBSTART, (uint_t)rx_buf);

	/* init Tx buffer DMA addresses (4 registers) */
	tx_buf = Malloc(TX_BUF_SIZE * 4);
	int i;
	for(i = 0; i < 4; i++)
		Out_DWord(RTL8139_TSAD0 + (i * 4), ((uint_t)tx_buf) + (i * TX_BUF_SIZE));

	/* missed packet counter */
	Out_DWord(RTL8139_MPC, 0);

	// rtl8139_set_rx_mode does some stuff here.
	Out_DWord(RTL8139_RCR, In_DWord(RTL8139_RCR) | AcceptBroadcast | AcceptMulticast | AcceptMyPhys |
				AcceptAllPhys);

	for(i = 0; i < 8; i++)
		Out_Byte(RTL8139_MAR0 + i, 0xff);

	/* no early-rx interrupts */
	Out_Word(RTL8139_MULINT, In_Word(RTL8139_MULINT) & MultiIntrClear);

	/* make sure RxTx has started */
	if(!(In_Byte(RTL8139_CR) & CmdRxEnb) || !(In_Byte(RTL8139_CR) & CmdTxEnb))
		Out_Byte(RTL8139_CR, CmdRxEnb | CmdTxEnb);

	/* Enable all known interrupts by setting the interrupt mask. */
	Out_Word(RTL8139_IMR, rtl8139_intr_mask);

	Install_IRQ(RTL8139_IRQ, rtl8139_interrupt);
  	Enable_IRQ(RTL8139_IRQ);

	PrintBoth("8139 initialized\n");
}

int rtl8139_Send(uchar_t *packet, uint_t size)
{
	uint_t entry = cur_tx % 4;
	memcpy(tx_buf + (entry * TX_BUF_SIZE), packet, size);
	Out_DWord(RTL8139_TSD0 + (entry * 4), tx_flag | size);
	cur_tx++;
	PrintBoth("Packet transmitted.  cur_tx = %d\n", cur_tx);
	return 0;
}

int rtl8139_Receive()
{
	PrintBoth("Packet Received\n");
	int i;

	while( (In_Byte(RTL8139_CR) & RxBufEmpty) == 0){

		uint_t ring_offset = cur_rx % RX_BUF_LEN;

		uint_t rx_status = ((uint_t)rx_buf + ring_offset);

		PrintBoth("RX Status = %x\n", rx_status);

		uint_t rx_size = (uint_t)((*(uchar_t *)(rx_status + 3) << 8) | (*(uchar_t *)(rx_status + 2)));
		uint_t pkt_len = rx_size - 4;

		PrintBoth("Packet Size = %d\n", pkt_len);
	
		for (i = 0; i < pkt_len; i++){
			PrintBoth(" %x ", *((uchar_t *)(rx_status + i)));
		}
		PrintBoth("\n");

		cur_rx = (cur_rx + rx_size + 4 + 3) & ~3;
		Out_Word(RTL8139_CAPR, (ushort_t)(cur_rx - 16));

		pkt_cntr++;

		PrintBoth("Packet counter at %d\n", pkt_cntr);
	}



	return 0;
}
