/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Lei Xia <lxia@northwestern.edu> 
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author:  Lei Xia <lxia@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <devices/ethernet.h>
#include <devices/pci.h>
#include <palacios/vmm.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm_io.h>
#include <palacios/vmm_debug.h>
#include <palacios/vm_guest_mem.h>


#ifndef CONFIG_DEBUG_RTL8139
#undef PrintDebug
#define PrintDebug(fmts, args...)
#endif



#define NIC_BASE_ADDR	0xc100
#define NIC_IRQ		11

#define RTL8139_IDR0    (0x00)		/* ID Registers */
#define RTL8139_IDR1    (0x01)
#define RTL8139_IDR2    (0x02)
#define RTL8139_IDR3    (0x03)
#define RTL8139_IDR4    (0x04)
#define RTL8139_IDR5    (0x05)

#define RTL8139_MAR0    (0x08)	/* Mulicast Registers*/
#define RTL8139_MAR1    (0x09)
#define RTL8139_MAR2    (0x0a)
#define RTL8139_MAR3    (0x0b)
#define RTL8139_MAR4    (0x0c)
#define RTL8139_MAR5    (0x0d)
#define RTL8139_MAR6    (0x0e)
#define RTL8139_MAR7    (0x0f)

#define RTL8139_TSD0    (0x10)	/* Tx Status of Descriptors */
#define RTL8139_TSD1    (0x14)
#define RTL8139_TSD2    (0x18)
#define RTL8139_TSD3    (0x1c)

#define RTL8139_TSAD0   (0x20)	/* Tx Start Address of Descriptors */
#define RTL8139_TSAD1   (0x24)
#define RTL8139_TSAD2   (0x28)
#define RTL8139_TSAD3   (0x2c)

#define RTL8139_RBSTART (0x30)	/* Rx Buffer Start Address */
#define RTL8139_ERBCR   (0x34)	/* Early Rx Byte Count Register */
#define RTL8139_ERSR    (0x36)	/* Early Rx Status Register */
#define RTL8139_CR      (0x37)	/* Command Register */
#define RTL8139_CAPR    (0x38)	/* Current Address of Pkt Read */
#define RTL8139_CBR     (0x3a)	/* Current Buffer Address */
#define RTL8139_IMR     (0x3c)	/* Intrpt Mask Reg */
#define RTL8139_ISR     (0x3e)	/* Intrpt Status Reg */
#define RTL8139_TCR     (0x40)	/* Tx Config Reg */
#define RTL8139_RCR     (0x44)	/* Rx Config Reg */
#define RTL8139_TCTR    (0x48)	/* Timer Count Reg */
#define RTL8139_MPC     (0x4c)	/* Missed Pkt Counter */
#define RTL8139_9346CR  (0x50)	/* 9346 Command Reg */
#define RTL8139_CONFIG0 (0x51)	/* Config Reg */
#define RTL8139_CONFIG1 (0x52)
#define RTL8139_TimerInt    (0x54)	/* Timer Intrpt Reg */
#define RTL8139_MSR     (0x58)	/* Media Status Reg */
#define RTL8139_CONFIG3 (0x59)	
#define RTL8139_CONFIG4 (0x5a)
#define RTL8139_MULINT  (0x5c)	/* Multiple Intrpt Select */
#define RTL8139_RERID   (0x5e)	
#define RTL8139_TSAD    (0x60)	/* Tx Status of All Descriptors */
#define RTL8139_BMCR    (0x62)	/* Basic Mode Control Register */
#define RTL8139_BMSR    (0x64)	/* Basic Mode Status Register */
#define RTL8139_ANAR    (0x66)	/* Auto-Negotiation Advertisement Register */
#define RTL8139_ANLPAR  (0x68)	/* Auto-Negotiation Link Partner Register */
#define RTL8139_ANER    (0x6a)	/* Auto-Negotiation Expansion Register */
#define RTL8139_DIS     (0x6c)	/* Disconnect Counter */
#define RTL8139_FCSC    (0x6e)	/* False Carrier Sense Counter */
#define RTL8139_NWAYTR  (0x70)	/* N-way Test Register */
#define RTL8139_REC     (0x72)	/* RX ER Counter */
#define RTL8139_CSCR    (0x74)	/* CS Config Register */
#define RTL8139_PHY1_PARM   (0x78)	/* PHY parameter */
#define RTL8139_TW_PARM (0x7c)	/* Twister parameter */
#define RTL8139_PHY2_PARM   (0x80)

#define RTL8139_CRC0    (0x84)	/* Power Management CRC Reg for wakeup frame */
#define RTL8139_CRC1    (0x85)
#define RTL8139_CRC2    (0x86)
#define RTL8139_CRC3    (0x87)
#define RTL8139_CRC4    (0x88)
#define RTL8139_CRC5    (0x89)
#define RTL8139_CRC6    (0x8a)
#define RTL8139_CRC7    (0x8b)

#define RTL8139_Wakeup0	(0x8c)	/* Power Management wakeup frame */
#define RTL8139_Wakeup1	(0x94)
#define RTL8139_Wakeup2	(0x9c)
#define RTL8139_Wakeup3	(0xa4)
#define RTL8139_Wakeup4	(0xac)
#define RTL8139_Wakeup5	(0xb4)
#define RTL8139_Wakeup6	(0xbc)
#define RTL8139_Wakeup7	(0xc4)

#define RTL8139_LSBCRO0 (0xcc)	/* LSB of the mask byte of wakeup frame */
#define RTL8139_LSBCRO1 (0xcd)
#define RTL8139_LSBCRO2 (0xce)
#define RTL8139_LSBCRO3 (0xcf)
#define RTL8139_LSBCRO4 (0xd0)
#define RTL8139_LSBCRO5 (0xd1)
#define RTL8139_LSBCRO6 (0xd2)
#define RTL8139_LSBCRO7 (0xd3)

#define RTL8139_Config5 (0xd8)

/* Interrupts */
#define PKT_RX      0x0001
#define RX_ERR      0x0002
#define TX_OK       0x0004
#define TX_ERR      0x0008
#define RX_BUFF_OF  0x0010
#define RX_UNDERRUN 0x0020
#define RX_FIFO_OF  0x0040
#define CABLE_LEN_CHNG  0x2000
#define TIME_OUT    0x4000
#define SERR        0x8000

#define DESC_SIZE 2048
#define TX_FIFO_SIZE (DESC_SIZE * 4)
#define RX_FIFO_SIZE (DESC_SIZE * 4)

typedef enum {NIC_READY, NIC_REG_POSTED} nic_state_t;

enum TxStatusBits
{
    TSD_Own = 0x2000,
    TSD_Tun = 0x4000,
    TSD_Tok = 0x8000,
    TSD_Cdh = 0x10000000,
    TSD_Owc = 0x20000000,
    TSD_Tabt = 0x40000000,
    TSD_Crs = 0x80000000,
};

enum RxStatusBits {
    Rx_Multicast = 0x8000,
    Rx_Physical = 0x4000,
    Rx_Broadcast = 0x2000,
    Rx_BadSymbol = 0x0020,
    Rx_Runt = 0x0010,
    Rx_TooLong = 0x0008,
    Rx_CRCErr = 0x0004,
    Rx_BadAlign = 0x0002,
    Rx_StatusOK = 0x0001,
};


/* Transmit Status of All Descriptors (TSAD) Register */
enum TSAD_bits {
 TSAD_TOK3 = 1<<15, // TOK bit of Descriptor 3
 TSAD_TOK2 = 1<<14, // TOK bit of Descriptor 2
 TSAD_TOK1 = 1<<13, // TOK bit of Descriptor 1
 TSAD_TOK0 = 1<<12, // TOK bit of Descriptor 0
 TSAD_TUN3 = 1<<11, // TUN bit of Descriptor 3
 TSAD_TUN2 = 1<<10, // TUN bit of Descriptor 2
 TSAD_TUN1 = 1<<9, // TUN bit of Descriptor 1
 TSAD_TUN0 = 1<<8, // TUN bit of Descriptor 0
 TSAD_TABT3 = 1<<07, // TABT bit of Descriptor 3
 TSAD_TABT2 = 1<<06, // TABT bit of Descriptor 2
 TSAD_TABT1 = 1<<05, // TABT bit of Descriptor 1
 TSAD_TABT0 = 1<<04, // TABT bit of Descriptor 0
 TSAD_OWN3 = 1<<03, // OWN bit of Descriptor 3
 TSAD_OWN2 = 1<<02, // OWN bit of Descriptor 2
 TSAD_OWN1 = 1<<01, // OWN bit of Descriptor 1
 TSAD_OWN0 = 1<<00, // OWN bit of Descriptor 0
};

enum ISRBits
{
    ISR_Rok = 0x1,
    ISR_Rer = 0x2,
    ISR_Tok = 0x4,
    ISR_Ter = 0x8,
    ISR_Rxovw = 0x10,
    ISR_Pun = 0x20,
    ISR_Fovw = 0x40,
    ISR_Lenchg = 0x2000,
    ISR_Timeout = 0x4000,
    ISR_Serr = 0x8000,
};

enum CMDBits
{
    CMD_Bufe = 0x1,
    CMD_Te = 0x4,
    CMD_Re = 0x8,
    CMD_Rst = 0x10,
};

enum CMD9346Bits {
	CMD9346_Lock = 0x00,
	CMD9346_Unlock = 0xC0,
};

// Bits in TxConfig.
enum TXConfig_bits{

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

enum CSCRBits {
    CSCR_Testfun = 1<<15, /* 1 = Auto-neg speeds up internal timer, WO, def 0 */
    CSCR_LD  = 1<<9,  /* Active low TPI link disable signal. When low, TPI still transmits link pulses and TPI stays in good link state. def 1*/
    CSCR_HEART_BIT = 1<<8,  /* 1 = HEART BEAT enable, 0 = HEART BEAT disable. HEART BEAT function is only valid in 10Mbps mode. def 1*/
    CSCR_JBEN = 1<<7,  /* 1 = enable jabber function. 0 = disable jabber function, def 1*/
    CSCR_F_LINK_100 = 1<<6, /* Used to login force good link in 100Mbps for diagnostic purposes. 1 = DISABLE, 0 = ENABLE. def 1*/
    CSCR_F_Connect  = 1<<5,  /* Assertion of this bit forces the disconnect function to be bypassed. def 0*/
    CSCR_Con_status = 1<<3, /* This bit indicates the status of the connection. 1 = valid connected link detected; 0 = disconnected link detected. RO def 0*/
    CSCR_Con_status_En = 1<<2, /* Assertion of this bit configures LED1 pin to indicate connection status. def 0*/
    CSCR_PASS_SCR = 1<<0, /* Bypass Scramble, def 0*/
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

#define RTL8139_PCI_REVID_8139      0x10

#define SET_MASKED(input, mask, curr) \
    (((input) & ~(mask)) | ((curr) & (mask)))

/* arg % size for size which is a power of 2 */
#define MOD2(input, size) \
    ((input) & (size - 1))


/* Size is 64 * 16bit words */
#define EEPROM_9346_ADDR_BITS 6
#define EEPROM_9346_SIZE  (1 << EEPROM_9346_ADDR_BITS)
#define EEPROM_9346_ADDR_MASK (EEPROM_9346_SIZE - 1)

enum Chip9346Operation
{
    Chip9346_op_mask = 0xc0,          /* 10 zzzzzz */
    Chip9346_op_read = 0x80,          /* 10 AAAAAA */
    Chip9346_op_write = 0x40,         /* 01 AAAAAA D(15)..D(0) */
    Chip9346_op_ext_mask = 0xf0,      /* 11 zzzzzz */
    Chip9346_op_write_enable = 0x30,  /* 00 11zzzz */
    Chip9346_op_write_all = 0x10,     /* 00 01zzzz */
    Chip9346_op_write_disable = 0x00, /* 00 00zzzz */
};

enum Chip9346Mode
{
    Chip9346_none = 0,
    Chip9346_enter_command_mode,
    Chip9346_read_command,
    Chip9346_data_read,      /* from output register */
    Chip9346_data_write,     /* to input register, then to contents at specified address */
    Chip9346_data_write_all, /* to input register, then filling contents */
};

struct EEprom9346
{
    uint16_t contents[EEPROM_9346_SIZE];
    int      mode;
    uint32_t tick;
    uint8_t  address;
    uint16_t input;
    uint16_t output;

    uint8_t eecs;
    uint8_t eesk;
    uint8_t eedi;
    uint8_t eedo;
};

struct rtl8139_regs {
  union{
	uint8_t mem[256];
	
	struct {
	    uint8_t id[6];
	    uint8_t reserved;
	    uint8_t mult[8];
	    uint32_t tsd[4];
	    uint32_t tsad[4];
	    uint32_t rbstart;
	    uint16_t erbcr;
	    uint8_t ersr;
	    uint8_t cmd;
	    uint16_t capr;
	    uint16_t cbr;
	    uint16_t imr;
	    uint16_t isr;
	    uint32_t tcr;
	    uint32_t rcr;
	    uint32_t tctr;
	    uint16_t mpc;
	    uint8_t cmd9346;
	    uint8_t config[2];
	    uint32_t timer_int;
	    uint8_t msr;
	    uint8_t config3[2];
	    uint16_t mulint;
	    uint16_t rerid;
	    uint16_t txsad;
	    uint16_t bmcr;
	    uint16_t bmsr;
	    uint16_t anar;
	    uint16_t anlpar;
	    uint16_t aner;
	    uint16_t dis;
	    uint16_t fcsc;
	    uint16_t nwaytr;
	    uint16_t rec;
	    uint32_t cscr;
	    uint32_t phy1_parm;
	    uint16_t tw_parm;
	    uint32_t  phy2_parm;
	    uint8_t crc[8];
	    uint32_t wakeup[16];
	    uint8_t isbcr[8];
	    uint8_t config5;
	}__attribute__((packed));
   }__attribute__((packed));
};



struct rtl8139_state
{	
    nic_state_t dev_state;

    struct v3_vm_info * vm;
    struct pci_device * pci_dev;
    struct vm_device * pci_bus;
    struct vm_device * dev;

    struct nic_statistics statistic;

    struct rtl8139_regs regs;
    struct EEprom9346 eeprom; 

    uint8_t tx_fifo[TX_FIFO_SIZE];
    uint8_t rx_fifo[RX_FIFO_SIZE];
    uint32_t rx_bufsize;

    uint8_t mac[ETH_ALEN];

    struct v3_dev_net_ops *net_ops;
    void * backend_data;
};


static void rtl8139_reset(struct vm_device *dev);

static void dump_state(struct vm_device *dev)
{
    struct nic_context *nic_state = (struct nic_context *)dev->private_data;
    PrintDebug("====RTL8139: Dumping State Begin==========\n");
    PrintDebug("Registers\n");
    int i;
    uchar_t *reg;
	reg = (uchar_t *)&nic_state->regs;

    for(i = 0; i < sizeof(struct nic_regs); i++)
    {
        PrintDebug("Register[%d] = 0x%2x\n", i, (int)reg[i]);
    }

    PrintDebug("====RTL8139: Dumping State End==========\n");
}

static void rtl8139_update_irq(struct rtl8139_state *nic_state)
{
    struct pci_device *pdev = nic_state->pci_dev;
    int irq_line;

    irq_line = pdev->configs[PCI_INTERRUPT_LINE];
    int isr = ((nic_state->regs.isr & nic_state->regs.imr) & 0xffff);

    if(isr & 0xffff)
    {
        if (irq_line != 0){
        	v3_raise_irq(dev->vm, irq_line);
       	PrintDebug("VNIC: RaiseIrq %d: isr: 0x%04x imr : 0x%04x\n", irq_line, nic_state->regs.isr, nic_state->regs.imr);
    	 } else {
    	      PrintError("RTL8139: IRQ_Line: %d\n", irq_line);
    	 }
    }
}

#if 1

static void prom9346_decode_command(struct EEprom9346 *eeprom, uint8_t command)
{
    PrintDebug("RTL8139: eeprom command 0x%02x\n", command);

    switch (command & Chip9346_op_mask)
    {
        case Chip9346_op_read:
        {
            eeprom->address = command & EEPROM_9346_ADDR_MASK;
            eeprom->output = eeprom->contents[eeprom->address];
            eeprom->eedo = 0;
            eeprom->tick = 0;
            eeprom->mode = Chip9346_data_read;
            PrintDebug("RTL8139: eeprom read from address 0x%02x data=0x%04x\n",
                   eeprom->address, eeprom->output);
        }
        break;

        case Chip9346_op_write:
        {
            eeprom->address = command & EEPROM_9346_ADDR_MASK;
            eeprom->input = 0;
            eeprom->tick = 0;
            eeprom->mode = Chip9346_none; /* Chip9346_data_write */
            PrintDebug("RTL8139: eeprom begin write to address 0x%02x\n",
                   eeprom->address);
        }
        break;
        default:
            eeprom->mode = Chip9346_none;
            switch (command & Chip9346_op_ext_mask)
            {
                case Chip9346_op_write_enable:
                    PrintDebug("RTL8139: eeprom write enabled\n");
                    break;
                case Chip9346_op_write_all:
                    PrintDebug("RTL8139: eeprom begin write all\n");
                    break;
                case Chip9346_op_write_disable:
                    PrintDebug("RTL8139: eeprom write disabled\n");
                    break;
            }
            break;
    }
}

static void prom9346_shift_clock(struct EEprom9346 *eeprom)
{
    int bit = eeprom->eedi?1:0;

    ++ eeprom->tick;

    PrintDebug("eeprom: tick %d eedi=%d eedo=%d\n", eeprom->tick, eeprom->eedi, eeprom->eedo);

    switch (eeprom->mode)
    {
        case Chip9346_enter_command_mode:
            if (bit)
            {
                eeprom->mode = Chip9346_read_command;
                eeprom->tick = 0;
                eeprom->input = 0;
                PrintDebug("eeprom: +++ synchronized, begin command read\n");
            }
            break;

        case Chip9346_read_command:
            eeprom->input = (eeprom->input << 1) | (bit & 1);
            if (eeprom->tick == 8)
            {
                prom9346_decode_command(eeprom, eeprom->input & 0xff);
            }
            break;

        case Chip9346_data_read:
            eeprom->eedo = (eeprom->output & 0x8000)?1:0;
            eeprom->output <<= 1;
            if (eeprom->tick == 16)
            {
#if 1
        // the FreeBSD drivers (rl and re) don't explicitly toggle
        // CS between reads (or does setting Cfg9346 to 0 count too?),
        // so we need to enter wait-for-command state here
                eeprom->mode = Chip9346_enter_command_mode;
                eeprom->input = 0;
                eeprom->tick = 0;

                PrintDebug("eeprom: +++ end of read, awaiting next command\n");
#else
        // original behaviour
                ++eeprom->address;
                eeprom->address &= EEPROM_9346_ADDR_MASK;
                eeprom->output = eeprom->contents[eeprom->address];
                eeprom->tick = 0;

                DEBUG_PRINT(("eeprom: +++ read next address 0x%02x data=0x%04x\n",
                       eeprom->address, eeprom->output));
#endif
            }
            break;

        case Chip9346_data_write:
            eeprom->input = (eeprom->input << 1) | (bit & 1);
            if (eeprom->tick == 16)
            {
                PrintDebug("RTL8139: eeprom write to address 0x%02x data=0x%04x\n",
                       eeprom->address, eeprom->input);

                eeprom->contents[eeprom->address] = eeprom->input;
                eeprom->mode = Chip9346_none; /* waiting for next command after CS cycle */
                eeprom->tick = 0;
                eeprom->input = 0;
            }
            break;

        case Chip9346_data_write_all:
            eeprom->input = (eeprom->input << 1) | (bit & 1);
            if (eeprom->tick == 16)
            {
                int i;
                for (i = 0; i < EEPROM_9346_SIZE; i++)
                {
                    eeprom->contents[i] = eeprom->input;
                }
                PrintDebug("RTL8139: eeprom filled with data=0x%04x\n",
                       eeprom->input);

                eeprom->mode = Chip9346_enter_command_mode;
                eeprom->tick = 0;
                eeprom->input = 0;
            }
            break;

        default:
            break;
    }
}

static int prom9346_get_wire(struct vm_device *dev)
{
    struct nic_context *nic_state = (struct nic_context *)dev->private_data;
    struct EEprom9346 *eeprom = &(nic_state->eeprom);

    if (!eeprom->eecs)
        return 0;

    return eeprom->eedo;
}

static void prom9346_set_wire(struct vm_device *dev, int eecs, int eesk, int eedi)
{
    struct nic_context *nic_state = (struct nic_context *)dev->private_data;
    struct EEprom9346 *eeprom = &(nic_state->eeprom);
    uint8_t old_eecs = eeprom->eecs;
    uint8_t old_eesk = eeprom->eesk;

    eeprom->eecs = eecs;
    eeprom->eesk = eesk;
    eeprom->eedi = eedi;

    PrintDebug("eeprom: +++ wires CS=%d SK=%d DI=%d DO=%d\n",
                 eeprom->eecs, eeprom->eesk, eeprom->eedi, eeprom->eedo);

    if (!old_eecs && eecs)
    {
        /* Synchronize start */
        eeprom->tick = 0;
        eeprom->input = 0;
        eeprom->output = 0;
        eeprom->mode = Chip9346_enter_command_mode;

        PrintDebug("=== eeprom: begin access, enter command mode\n");
    }

    if (!eecs)
    {
        PrintDebug("=== eeprom: end access\n");
        return;
    }

    if (!old_eesk && eesk)
    {
        /* SK front rules */
        prom9346_shift_clock(eeprom);
    }
}

static void rtl8139_9346cr_write(struct vm_device *dev, uint32_t val)
{
    struct nic_context *nic_state = (struct nic_context *)dev->private_data;

    val &= 0xff;

    PrintDebug("RTL8139: 9346CR write val=0x%02x\n", val);

    /* mask unwriteable bits */
    val = SET_MASKED(val, 0x31, nic_state->regs.cmd9346);

    uint32_t opmode = val & 0xc0;
    uint32_t eeprom_val = val & 0xf;

    if (opmode == 0x80) {
        /* eeprom access */
        int eecs = (eeprom_val & 0x08)?1:0;
        int eesk = (eeprom_val & 0x04)?1:0;
        int eedi = (eeprom_val & 0x02)?1:0;
        prom9346_set_wire(dev, eecs, eesk, eedi);
    } else if (opmode == 0x40) {
        /* Reset.  */
        val = 0;
        rtl8139_reset(dev);
    }

    nic_state->regs.cmd9346 = val;
}

static uint32_t rtl8139_9346cr_read(struct vm_device *dev)
{
    struct nic_context *nic_state = (struct nic_context *)dev->private_data;
    uint32_t ret = nic_state->regs.cmd9346;
    uint32_t opmode = ret & 0xc0;

    if (opmode == 0x80)
    {
        /* eeprom access */
        int eedo = prom9346_get_wire(dev);
        if (eedo)
        {
            ret |=  0x01;
        }
        else
        {
            ret &= ~0x01;
        }
    }

    PrintDebug("RTL8139: 9346CR read val=0x%02x\n", ret);

    return ret;
}

#endif

static int rtl8139_receiver_enabled(struct vm_device *dev)
{
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);

    return nic_state->regs.cmd & CMD_Re;
}

static int rtl8139_rxwrap(struct vm_device *dev)
{
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);

    // wrapping enabled; assume 1.5k more buffer space if size < 64K
    return (nic_state->regs.rcr & (1 << 7));
}

static void rtl8139_rxbuf_write(struct vm_device *dev, const void *buf, int size)
{
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);
    struct nic_regs *regs = &(nic_state->regs);
    int wrap;
    addr_t guestpa, host_rxbuf;

    guestpa = (addr_t)regs->rbstart;
    guest_pa_to_host_va(dev->vm, guestpa, &host_rxbuf);   

    //wrap to the front of rx buffer
    if (regs->cbr + size > nic_state->rx_bufsize)
    {
        wrap = MOD2(regs->cbr + size, nic_state->rx_bufsize);

        if (wrap && !(nic_state->rx_bufsize < 64*1024 && rtl8139_rxwrap(dev)))
        {
            PrintDebug("RTL8139: rx packet wrapped in buffer at %d\n", size-wrap);

            if (size > wrap)
            {
                memcpy((void *)(host_rxbuf + regs->cbr), buf, size-wrap);
            }

            // reset buffer pointer
            regs->cbr = 0;

            memcpy((void *)(host_rxbuf + regs->cbr), buf + (size-wrap), wrap);

            regs->cbr = wrap;

            return;
        }
    }

    memcpy((void *)(host_rxbuf + regs->cbr), buf, size);

    regs->cbr += size;
}

#define POLYNOMIAL 0x04c11db6

/* From FreeBSD */
static int compute_mcast_idx(const uint8_t *ep)
{
    uint32_t crc;
    int carry, i, j;
    uint8_t b;

    crc = 0xffffffff;
    for (i = 0; i < 6; i++) {
        b = *ep++;
        for (j = 0; j < 8; j++) {
            carry = ((crc & 0x80000000L) ? 1 : 0) ^ (b & 0x01);
            crc <<= 1;
            b >>= 1;
            if (carry)
                crc = ((crc ^ POLYNOMIAL) | carry);
        }
    }
    return (crc >> 26);
}
static void vnic_receive(struct vm_device *dev, const uchar_t *pkt, uint_t length)
{
    struct nic_context *nic_state = (struct nic_context *)dev->private_data;
    struct nic_regs *regs = &(nic_state->regs);
    uint_t rxbufsize = nic_state->rx_bufsize;
    uint32_t header, val;
    uint8_t bcast_addr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

    if (regs->rcr & AcceptAllPhys) {
        PrintDebug("RTL8139: packet received in promiscuous mode\n");
    } else {
        if (!memcmp(pkt,  bcast_addr, 6)) {
            if (!(regs->rcr & AcceptBroadcast))
            {
                PrintDebug("RTL8139: broadcast packet rejected\n");
                return;
            }
            header |= Rx_Broadcast;
            PrintDebug("RTL8139: broadcast packet received\n");
        } else if (pkt[0] & 0x01) {
            // multicast
            if (!(regs->rcr & AcceptMulticast))
            {
                PrintDebug("RTL8139: multicast packet rejected\n");
                return;
            }

            int mcast_idx = compute_mcast_idx(pkt);

            if (!(regs->mult[mcast_idx >> 3] & (1 << (mcast_idx & 7))))
            {
                PrintDebug("RTL8139: multicast address mismatch\n");
                return;
            }
            header |= Rx_Multicast;
            PrintDebug("RTL8139: multicast packet received\n");
        } else if (regs->id[0] == pkt[0] &&
                   regs->id[1] == pkt[1] &&
                   regs->id[2] == pkt[2] &&
                   regs->id[3] == pkt[3] &&
                   regs->id[4] == pkt[4] &&
                   regs->id[5] == pkt[5]) {
            if (!(regs->rcr & AcceptMyPhys))
            {
                PrintDebug("RTL8139: rejecting physical address matching packet\n");
                return;
            }

            header |= Rx_Physical;
            PrintDebug("RTL8139: physical address matching packet received\n");
        } else {
            PrintDebug("RTL8139: unknown packet\n");
            return;
        }
    }

    if(1){
        PrintDebug("RTL8139: in ring Rx mode\n");

        int avail = MOD2(rxbufsize + regs->capr - regs->cbr, rxbufsize);

        if (avail != 0 && length + 8 >= avail)
        {
            PrintError("rx overflow: rx buffer length %d head 0x%04x read 0x%04x === available 0x%04x need 0x%04x\n",
                   rxbufsize, regs->cbr, regs->capr, avail, length + 8);

            regs->isr |= ISR_Rxovw;
            ++ regs->mpc;
            rtl8139_update_irq(dev);
            return;
        }

        header |= Rx_StatusOK;
        header |= ((length << 16) & 0xffff0000);

        rtl8139_rxbuf_write(dev, (uint8_t *)&header, 4);

        rtl8139_rxbuf_write(dev, pkt, length);

        val = V3_Crc32(0, (char *)pkt, length);

        rtl8139_rxbuf_write(dev, (uint8_t *)&val, 4);

        // correct buffer write pointer 
        regs->cbr = MOD2((regs->cbr + 3) & ~0x3, rxbufsize);

        PrintDebug("RTL8139: received: rx buffer length %d CBR: 0x%04x CAPR: 0x%04x\n",
               		rxbufsize, regs->cbr, regs->capr);
    }

    regs->isr |= ISR_Rok;

    nic_state->pkts_rcvd++;
	
    rtl8139_update_irq(dev);   
}

static int netif_input(uchar_t *pkt, uint_t size)
{
    PrintDebug("RTL8139: packet received!\n");

    if (!rtl8139_receiver_enabled(current_vnic)){
		PrintDebug("RTL8139: receiver disabled\n");
		return 0;
    }
    vnic_receive(current_vnic, pkt, size);

    return 0;
}

static void rtl8139_reset_rxbuf(struct vm_device *dev, uint32_t bufsize)
{
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);
    struct nic_regs *regs = &(nic_state->regs);
	
    nic_state->rx_bufsize = bufsize;
    regs->capr  = 0;
    regs->cbr = 0;
}

static void rtl8139_rcr_write(struct vm_device *dev, uint32_t val)
{
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);

    PrintDebug("RTL8139: RCR write val=0x%08x\n", val);

    val = SET_MASKED(val, 0xf0fc0040, nic_state->regs.rcr);

    nic_state->regs.rcr = val;

#if 0
    uchar_t rblen = (regs->rcr >> 11) & 0x3;
    switch(rblen)
    {
        case 0x0:
            rxbufsize = 1024 * 8 + 16;
            break;
        case 0x1:
            rxbufsize = 1024 * 16 + 16;
            break;
        case 0x2:
            rxbufsize = 1024 * 32 + 16;
            break;
        default:
            rxbufsize = 1024 * 64 + 16;
            break;
    }
#endif

    // reset buffer size and read/write pointers
    rtl8139_reset_rxbuf(dev, 8192 << ((nic_state->regs.rcr >> 11) & 0x3));

    PrintDebug("RTL8139: RCR write reset buffer size to %d\n", nic_state->rx_bufsize);
}

static void rtl8139_reset(struct rtl8139_state *nic_state)
{
    struct rtl8139_regs *regs = &(nic_state->regs);
    int i;

    PrintDebug("Rtl8139: Reset\n");

    /* restore MAC address */
    memcpy(regs->id, nic_state->mac, ETH_ALEN);
    memset(regs->mult, 0xff, 8);

    regs->isr = 0;
    regs->imr = 0;

    rtl8139_update_irq(nic_state);

    // prepare eeprom
    nic_state->eeprom.contents[0] = 0x8129;
	
    // PCI vendor and device ID
    nic_state->eeprom.contents[1] = 0x10ec;
    nic_state->eeprom.contents[2] = 0x8139;
    //Mac address
    nic_state->eeprom.contents[7] = nic_state->mac[0] | nic_state->mac[1] << 8;
    nic_state->eeprom.contents[8] = nic_state->mac[2] | nic_state->mac[3] << 8;
    nic_state->eeprom.contents[9] = nic_state->mac[4] | nic_state->mac[5] << 8;

    for (i = 0; i < 4; ++i)
    {
        regs->tsd[i] = TSD_Own;
    }

    regs->rbstart = 0;

    rtl8139_reset_rxbuf(nic_state, 1024*8);

    /* ACK the reset */
    regs->tcr = 0;

    regs->tcr |= ((0x1d << 26) | (0x1 << 22)); // RTL-8139D
    regs->rerid = RTL8139_PCI_REVID_8139;

    regs->cmd = CMD_Rst; //RxBufEmpty bit is calculated on read from ChipCmd 

    regs->config[0] = 0x0 | (1 << 4); // No boot ROM 
    regs->config[1] = 0xC; //IO mapped and MEM mapped registers available
    //regs->config[1] = 0x4; //Only IO mapped registers available
    regs->config3[0] = 0x1; // fast back-to-back compatible
    regs->config3[1] = 0x0;
    regs->config5 = 0x0;
	
    regs->cscr = CSCR_F_LINK_100 | CSCR_HEART_BIT | CSCR_LD;

    //0x3100 : 100Mbps, full duplex, autonegotiation.  0x2100 : 100Mbps, full duplex
    regs->bmcr = 0x1000; // autonegotiation

    regs->bmsr  = 0x7809;
    regs->bmsr |= 0x0020; // autonegotiation completed
    regs->bmsr |= 0x0004; // link is up

    regs->anar = 0x05e1;     // all modes, full duplex
    regs->anlpar = 0x05e1;   // all modes, full duplex
    regs->aner = 0x0001;     // autonegotiation supported

    // reset timer and disable timer interrupt
    regs->tctr = 0;
    regs->timer_int = 0;

    nic_state->pkts_rcvd = 0;
}

static void init_rtl8139_regs(struct rtl8139_state *nic_state)
{
    nic_state->regs.imr = 0x00;
    nic_state->regs.tsd[0] = nic_state->regs.tsd[1] = nic_state->regs.tsd[2] = nic_state->regs.tsd[3] = TSD_Own;
    nic_state->pkts_rcvd = 0;

    int i;
    for(i = 0; i < 6; i++)
        nic_state->regs.id[i] = nic_state->mac_addr[i] = mac[i];
    for(i = 0; i < 8; i++)
        nic_state->regs.mult[i] = 0xff;

    nic_state->regs.rerid = RTL8139_PCI_REVID_8139;
    nic_state->regs.tcr |= ((0x1d << 26) | (0x1 << 22));

    rtl8139_reset(dev);
}


#if 0

static int rtl8139_config_writeable(struct vm_device *dev)
{
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);
	
    if (nic_state->regs.cmd9346 & CMD9346_Unlock)
    {
        return 1;
    }

    PrintDebug("RTL8139: Configuration registers are unwriteable\n");

    return 0;
}

#endif

static int rtl8139_transmitter_enabled(struct vm_device *dev)
{
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);

    return nic_state->regs.cmd & CMD_Te;
}

static bool rtl8139_rxbufempty(struct vm_device *dev)
{
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);
    struct nic_regs *regs = &(nic_state->regs);
    int unread;

    unread = MOD2(regs->cbr + nic_state->rx_bufsize - regs->capr, nic_state->rx_bufsize);
   
    if (unread != 0)
    {
        PrintDebug("RTL8139: receiver buffer data available 0x%04x\n", unread);
        return false;
    }

    PrintDebug("RTL8139: receiver buffer is empty\n");

    return true;
}

static uint32_t rtl8139_cmd_read(struct vm_device *dev)
{
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);
    uint32_t ret = nic_state->regs.cmd;

    if (rtl8139_rxbufempty(dev))
        ret |= CMD_Bufe;

    PrintDebug("RTL8139: Cmd read val=0x%04x\n", ret);

    return ret;
}

static void rtl8139_cmd_write(struct vm_device *dev, uint32_t val)
{
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);

    val &= 0xff;

    PrintDebug("RTL8139: Cmd write val=0x%08x\n", val);

    if (val & CMD_Rst)
    {
        PrintDebug("RTL8139: Cmd reset\n");
        rtl8139_reset(dev);
    }
    if (val & CMD_Re)
    {
        PrintDebug("RTL8139: Cmd enable receiver\n");

        //s->currCPlusRxDesc = 0;
    }
    if (val & CMD_Te)
    {
        PrintDebug("RTL8139: Cmd enable transmitter\n");

        //s->currCPlusTxDesc = 0;
    }

    val = SET_MASKED(val, 0xe3, nic_state->regs.cmd);
    val &= ~CMD_Rst;

    nic_state->regs.cmd = val;
}

static int rtl8139_send_packet(struct vm_device *dev, int descriptor)
{
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);
    struct nic_regs *regs = &(nic_state->regs);
    int txsize;
    uint8_t *pkt;
    addr_t pkt_gpa = 0, hostva = 0;
    int i;

    if (!rtl8139_transmitter_enabled(dev))
    {
        PrintError("RTL8139: fail to send from descriptor %d: transmitter disabled\n", descriptor);
        return 0;
    }

    if (regs->tsd[descriptor] & TSD_Own)
    {
        PrintError("RTL8139: fail to send from descriptor %d: owned by host\n", descriptor);
        return 0;
    }

    txsize = regs->tsd[descriptor] & 0x1fff;
    pkt_gpa = (addr_t) regs->tsad[descriptor];

    PrintDebug("RTL8139: sending %d bytes from guest memory at 0x%08x\n", txsize, regs->tsad[descriptor]);

    guest_pa_to_host_va(dev->vm, (addr_t)pkt_gpa, &hostva);
    pkt = (uchar_t *)hostva;

    for(i = 0; i < txsize; i++)
    {
        PrintDebug("%x ", pkt[i]);
    }
    PrintDebug("\n");

    if (TxLoopBack == (regs->tcr & TxLoopBack)){ //loopback test
        PrintDebug(("RTL8139: transmit loopback mode\n"));
        vnic_receive(dev, pkt, txsize);
    } else{       
	    if (V3_SEND_PKT(pkt, txsize) == 0){
		 PrintDebug("RTL8139: Sent %d bytes from descriptor %d\n", txsize, descriptor);
	    } else {
	        PrintError("Rtl8139: Sending packet error: 0x%p\n", pkt);
	    }
    }

    regs->tsd[descriptor] |= TSD_Tok;
    regs->tsd[descriptor] |= TSD_Own;

    nic_state->regs.isr |= ISR_Tok;
    rtl8139_update_irq(dev);

    return 0;
}

//write to transmit status registers
static void rtl8139_tsd_write(struct vm_device *dev, uint8_t descriptor, uint32_t val)
{ 
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);

#if 0
    if (rtl8139_transmitter_enabled(dev))
    {
        PrintDebug("RTL8139: TxStatus write val=0x%08x descriptor=%d\n", val, descriptor);

        nic_state->regs.tsd[descriptor] = val;

        return;
    }
#endif

    PrintDebug("RTL8139: TSD write val=0x%08x descriptor=%d\n", val, descriptor);

    // mask read-only bits
    val &= ~0xff00c000;
    val = SET_MASKED(val, 0x00c00000,  nic_state->regs.tsd[descriptor]);

    nic_state->regs.tsd[descriptor] = val;

    rtl8139_send_packet(dev, descriptor);
}

//transmit status of all descriptors
static uint16_t rtl8139_tsad_read(struct vm_device *dev)
{
    uint16_t ret = 0;
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);
    struct nic_regs *regs = &(nic_state->regs);

    ret = ((regs->tsd[3] & TSD_Tok)?TSD_Tok:0)
         |((regs->tsd[2] & TSD_Tok)?TSAD_TOK2:0)
         |((regs->tsd[1] & TSD_Tok)?TSAD_TOK1:0)
         |((regs->tsd[0] & TSD_Tok)?TSAD_TOK0:0)

         |((regs->tsd[3] & TSD_Tun)?TSAD_TUN3:0)
         |((regs->tsd[2] & TSD_Tun)?TSAD_TUN2:0)
         |((regs->tsd[1] & TSD_Tun)?TSAD_TUN1:0)
         |((regs->tsd[0] & TSD_Tun)?TSAD_TUN0:0)

         |((regs->tsd[3] & TSD_Tabt)?TSAD_TABT3:0)
         |((regs->tsd[2] & TSD_Tabt)?TSAD_TABT2:0)
         |((regs->tsd[1] & TSD_Tabt)?TSAD_TABT1:0)
         |((regs->tsd[0] & TSD_Tabt)?TSAD_TABT0:0)

         |((regs->tsd[3] & TSD_Own)?TSAD_OWN3:0)
         |((regs->tsd[2] & TSD_Own)?TSAD_OWN2:0)
         |((regs->tsd[1] & TSD_Own)?TSAD_OWN1:0)
         |((regs->tsd[0] & TSD_Own)?TSAD_OWN0:0) ;


    PrintDebug("RTL8139: tsad read val=0x%04x\n", (int)ret);

    return ret;
}

//interrupt mask register
static void rtl8139_imr_write(struct vm_device *dev, uint32_t val)
{
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);
	
    PrintDebug("RTL8139: IMR write val=0x%04x\n", val);

    /* mask unwriteable bits */
    val = SET_MASKED(val, 0x1e00, nic_state->regs.imr);

    nic_state->regs.imr = val;

    rtl8139_update_irq(dev);
}

static void rtl8139_isr_write(struct vm_device *dev, uint32_t val)
{
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);
    struct nic_regs *regs = &(nic_state->regs);

    PrintDebug("RTL8139: ISR write val=0x%04x\n", val);

#if 0

    // writing to ISR has no effect

    return;

#else
    uint16_t newisr = regs->isr & ~val;

    /* mask unwriteable bits */
    newisr = SET_MASKED(newisr, 0x1e00, regs->isr);

    /* writing 1 to interrupt status register bit clears it */
    regs->isr = 0;
    rtl8139_update_irq(dev);

    regs->isr = newisr;
    rtl8139_update_irq(dev);
#endif
}

static uint32_t rtl8139_isr_read(struct vm_device *dev)
{
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);
    uint32_t ret = (uint32_t)nic_state->regs.isr;

    PrintDebug("RTL8139: ISR read val=0x%04x\n", ret);

#if 0
    // reading ISR clears all interrupts
    nic_state->regs.isr = 0;

    rtl8139_update_irq(dev);

#endif

    return ret;
}

static void rtl8139_capr_write(struct vm_device *dev, uint32_t val)
{
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);

    PrintDebug("RTL8139: CAPR write val=0x%04x\n", val);

    // this value is off by 16
    nic_state->regs.capr = MOD2(val + 0x10, nic_state->rx_bufsize);

    PrintDebug("RTL 8139: CAPR write: rx buffer length %d head 0x%04x read 0x%04x\n",
           nic_state->rx_bufsize, nic_state->regs.cbr, nic_state->regs.capr);
}

static uint32_t rtl8139_capr_read(struct vm_device *dev)
{
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);

    /* this value is off by 16 */
    uint32_t ret = nic_state->regs.capr - 0x10;

    PrintDebug("RTL8139: CAPR read val=0x%04x\n", ret);

    return ret;
}

typedef enum {read, write} opr_t; 
static bool need_hook(int port, opr_t op)
{
	if (op == read){ 
		switch (port) {
			case RTL8139_IMR:
			case RTL8139_ISR:
				return true;
			default:
				break;
		}
	}
	if (op == write){
		switch (port) {
			case RTL8139_TSD0:
			case RTL8139_TSD1:
			case RTL8139_TSD2:
			case RTL8139_TSD3:
			case RTL8139_CR:
			case RTL8139_IMR:
			case RTL8139_ISR:
			case RTL8139_TCR:
			case RTL8139_RCR:
			case RTL8139_CSCR:
			case RTL8139_Config5:
				return true;
			default:
				break;
		}
	}
	
	return false;
}

static int rtl8139_mmio_write(addr_t guest_addr, void * src, uint_t length, void * priv_data)
{
    int port;
    uint32_t val;
    struct vm_device *dev = (struct vm_device *)priv_data;
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);

    port = guest_addr & 0xff;

    memcpy(&val, src, length);

    PrintDebug("rtl8139 mmio write: addr:0x%x (%u bytes): 0x%x\n", (int)guest_addr, length, val);
	
    switch(port) {
	    case RTL8139_IDR0:
	        nic_state->regs.id[0] = val & 0xff;
	        break;
	    case RTL8139_IDR1:
	        nic_state->regs.id[1] = val & 0xff;
	        break;
	    case RTL8139_IDR2:
	        nic_state->regs.id[2] = val & 0xff;
	        break;
	    case RTL8139_IDR3:
	        nic_state->regs.id[3] = val & 0xff;
	        break;
	    case RTL8139_IDR4:
	        nic_state->regs.id[4] = val & 0xff;
	        break;
	    case RTL8139_IDR5:
	        nic_state->regs.id[5] = val & 0xff;
	        break;
	    case RTL8139_MAR0:
	        nic_state->regs.mult[0] = val & 0xff;
	        break;
	    case RTL8139_MAR1:
	        nic_state->regs.mult[1] = val & 0xff;
	        break;
	    case RTL8139_MAR2:
	        nic_state->regs.mult[2] = val & 0xff;
	        break;
	    case RTL8139_MAR3:
	        nic_state->regs.mult[3] = val & 0xff;
	        break;
	    case RTL8139_MAR4:
	        nic_state->regs.mult[4] = val & 0xff;
	        break;
	    case RTL8139_MAR5:
	        nic_state->regs.mult[5] = val & 0xff;
	        break;
	    case RTL8139_MAR6:
	        nic_state->regs.mult[6] = val & 0xff;
	        break;
	    case RTL8139_MAR7:
	        nic_state->regs.mult[7] = val & 0xff;
	        break;
	    case RTL8139_TSD0:
	    case RTL8139_TSD1:
	    case RTL8139_TSD2:
	    case RTL8139_TSD3:
	        rtl8139_tsd_write(dev, (port - RTL8139_TSD0)/4, val);
	        break;
	    case RTL8139_TSAD0:
	    case RTL8139_TSAD1:
	    case RTL8139_TSAD2:
	    case RTL8139_TSAD3:
	        nic_state->regs.tsad[(port - RTL8139_TSAD0)/4] = val;
	        break;
	    case RTL8139_RBSTART:
	        nic_state->regs.rbstart = val;
	        break;
	    case RTL8139_ERBCR:
	        nic_state->regs.erbcr = val & 0xffff;
	        break;
	    case RTL8139_ERSR:
	        //nic_state->regs.ersr = val & 0xff;
	        nic_state->regs.ersr &= (~val) & 0x0c;
	        break;
	    case RTL8139_CR:
		  rtl8139_cmd_write(dev, val);
	        break;
	    case RTL8139_CAPR:
	        rtl8139_capr_write(dev, val & 0xffff);
	        break;
	    case RTL8139_CBR: //this is read only =====
	        //nic_state->regs.cbr = val & 0xffff;
	        break;
	    case RTL8139_IMR:
		  rtl8139_imr_write(dev, val);
	        break;
	    case RTL8139_ISR:
	        rtl8139_isr_write(dev, val);
	        break;
	    case RTL8139_TCR:
	        nic_state->regs.tcr = val;
	        break;
	    case RTL8139_RCR:
	        rtl8139_rcr_write(dev, val);
		 break;
	    case RTL8139_TCTR:
	        nic_state->regs.tctr = 0; //write clear current tick
	        break;
	    case RTL8139_MPC:
	        nic_state->regs.mpc = 0; //clear on write
	        break;
	    case RTL8139_9346CR:
		  rtl8139_9346cr_write(dev, val);
	        break;
	    case RTL8139_CONFIG0:
	        nic_state->regs.config[0] = val & 0xff;
	        break;
	    case RTL8139_CONFIG1:
	        nic_state->regs.config[1] = val & 0xff;
	        break;
	    case RTL8139_TimerInt:
	        nic_state->regs.timer_int = val;
	        break;
	    case RTL8139_MSR:
	        nic_state->regs.msr = val & 0xff;
	        break;
	    case RTL8139_CONFIG3:
	        nic_state->regs.config3[0] = val & 0xff;
	        break;
	    case RTL8139_CONFIG4:
	        nic_state->regs.config3[1] = val & 0xff;
	        break;
	    case RTL8139_MULINT:
	        nic_state->regs.mulint = val & 0xffff;
	        break;
	    case RTL8139_RERID:
	        nic_state->regs.rerid = val & 0xffff;
	        break;
	    case RTL8139_TSAD:
	        nic_state->regs.txsad = val & 0xffff;
	        break;
	    case RTL8139_BMCR:
	        nic_state->regs.bmcr = val & 0xffff;
	        break;
	    case RTL8139_BMSR:
	        nic_state->regs.bmsr = val & 0xffff;
	        break;
	    case RTL8139_ANAR:
	        nic_state->regs.anar = val & 0xffff;
	        break;
	    case RTL8139_ANLPAR:
	        nic_state->regs.anlpar = val & 0xffff;
	        break;
	    case RTL8139_ANER:
	        nic_state->regs.aner = val & 0xffff;
	        break;
	    case RTL8139_DIS:
	        nic_state->regs.dis = val & 0xffff;
	        break;
	    case RTL8139_FCSC:
	        nic_state->regs.fcsc = val & 0xffff;
	        break;
	    case RTL8139_NWAYTR:
	        nic_state->regs.nwaytr = val & 0xffff;
	        break;
	    case RTL8139_REC:
	        nic_state->regs.rec = val & 0xffff;
	        break;
	    case RTL8139_CSCR:
	        nic_state->regs.cscr = val;
	        break;
	    case RTL8139_PHY1_PARM:
	        nic_state->regs.phy1_parm = val;
	        break;
	    case RTL8139_TW_PARM:
	        nic_state->regs.tw_parm = val & 0xffff;
	        break;
	    case RTL8139_PHY2_PARM:
	        nic_state->regs.phy2_parm = val;
	        break;
	    case RTL8139_CRC0:
	        nic_state->regs.crc[0] = val & 0xff;
	        break;
	    case RTL8139_CRC1:
	        nic_state->regs.crc[1] = val & 0xff;
	        break;
	    case RTL8139_CRC2:
	        nic_state->regs.crc[2] = val & 0xff;
	        break;
	    case RTL8139_CRC3:
	        nic_state->regs.crc[3] = val & 0xff;
	        break;
	    case RTL8139_CRC4:
	        nic_state->regs.crc[4] = val & 0xff;
	        break;
	    case RTL8139_CRC5:
	        nic_state->regs.crc[5] = val & 0xff;
	        break;
	    case RTL8139_CRC6:
	        nic_state->regs.crc[6] = val & 0xff;
	        break;
	    case RTL8139_CRC7:
	        nic_state->regs.crc[7] = val & 0xff;
	        break;
	    case RTL8139_Config5:
	        nic_state->regs.config5 = val & 0xff;
	        break;
	    default:
	        PrintDebug("rtl8139 write error: invalid port: 0x%x\n", port);
	}

	
#if TEST_PERFORMANCE
    if (need_hook(port, read))
		io_hooked ++;
    if (( ++io_total) % 50 == 0)
	 PrintError("RTL8139: Total IO: %d, Hooked: %d, INT: %d\n", io_total, io_hooked, int_total);
#endif
        
    return length;
}

static int rtl8139_mmio_read(addr_t guest_addr, void * dst, uint_t length, void * priv_data)
{
    ulong_t port;
    uint32_t val;
    struct vm_device *dev = (struct vm_device *)priv_data;
    struct nic_context *nic_state = (struct nic_context *)(dev->private_data);

    port = guest_addr & 0xff;

    switch(port) {
	    case RTL8139_IDR0:
	        val = nic_state->regs.id[0];
	        break;
	    case RTL8139_IDR1:
	        val = nic_state->regs.id[1];
	        break;
	    case RTL8139_IDR2:
	        val = nic_state->regs.id[2];
	        break;
	    case RTL8139_IDR3:
	        val = nic_state->regs.id[3];
	        break;
	    case RTL8139_IDR4:
	        val = nic_state->regs.id[4];
	        break;
	    case RTL8139_IDR5:
	        val = nic_state->regs.id[5];
	        break;
	    case RTL8139_MAR0:
	        val = nic_state->regs.mult[0];
	        break;
	    case RTL8139_MAR1:
	        val = nic_state->regs.mult[1];
	        break;
	    case RTL8139_MAR2:
	        val = nic_state->regs.mult[2];
	        break;
	    case RTL8139_MAR3:
	        val = nic_state->regs.mult[3];
	        break;
	    case RTL8139_MAR4:
	        val = nic_state->regs.mult[4];
	        break;
	    case RTL8139_MAR5:
	        val = nic_state->regs.mult[5];
	        break;
	    case RTL8139_MAR6:
	        val = nic_state->regs.mult[6];
	        break;
	    case RTL8139_MAR7:
	        val = nic_state->regs.mult[7];
	        break;
	    case RTL8139_TSD0:
	        val = nic_state->regs.tsd[0];
	        break;
	    case RTL8139_TSD1:
	        val = nic_state->regs.tsd[1];
	        break;
	    case RTL8139_TSD2:
	        val = nic_state->regs.tsd[2];
	        break;
	    case RTL8139_TSD3:
	        val = nic_state->regs.tsd[3];
	        break;
	    case RTL8139_TSAD0:
	        val = nic_state->regs.tsad[0];
	        break;
	    case RTL8139_TSAD1:
	        val = nic_state->regs.tsad[1];
	        break;
	    case RTL8139_TSAD2:
	        val = nic_state->regs.tsad[2];
	        break;
	    case RTL8139_TSAD3:
	        val = nic_state->regs.tsad[3];
	        break;
	    case RTL8139_RBSTART:
	        val = nic_state->regs.rbstart;
	        break;
	    case RTL8139_ERBCR:
	        val = nic_state->regs.erbcr;
	        break;
	    case RTL8139_ERSR:
	        val = nic_state->regs.ersr;
	        break;
	    case RTL8139_CR:
	        val = rtl8139_cmd_read(dev);
	        break;
	    case RTL8139_CAPR:
	        val = rtl8139_capr_read(dev);
	        break;
	    case RTL8139_CBR:
	        val = nic_state->regs.cbr;
	        break;
	    case RTL8139_IMR:
	        val = nic_state->regs.imr;
	        break;
	    case RTL8139_ISR:
	        val = rtl8139_isr_read(dev);
	        break;
	    case RTL8139_TCR:
	        val = nic_state->regs.tcr;
	        break;
	    case RTL8139_RCR:
	        val = nic_state->regs.rcr;
	        break;
	    case RTL8139_TCTR:
	        val = nic_state->regs.tctr;
	        break;
	    case RTL8139_MPC:
	        val = nic_state->regs.mpc;
	        break;
	    case RTL8139_9346CR:
	        val = rtl8139_9346cr_read(dev);
	        break;
	    case RTL8139_CONFIG0:
	        val = nic_state->regs.config[0];
	        break;
	    case RTL8139_CONFIG1:
	        val = nic_state->regs.config[1];
	        break;
	    case RTL8139_TimerInt:
	        val = nic_state->regs.timer_int;
	        break;
	    case RTL8139_MSR:
	        val = nic_state->regs.msr;
	        break;
	    case RTL8139_CONFIG3:
	        val = nic_state->regs.config3[0];
	        break;
	    case RTL8139_CONFIG4:
	        val = nic_state->regs.config3[1];
	        break;
	    case RTL8139_MULINT:
	        val = nic_state->regs.mulint;
	        break;
	    case RTL8139_RERID:
	        val = nic_state->regs.rerid;
	        break;
	    case RTL8139_TSAD: 
	        val = rtl8139_tsad_read(dev);
	        break;
	    case RTL8139_BMCR:
	        val = nic_state->regs.bmcr;
	        break;
	    case RTL8139_BMSR:
	        val = nic_state->regs.bmsr;
	        break;
	    case RTL8139_ANAR:
	        val = nic_state->regs.anar;
	        break;
	    case RTL8139_ANLPAR:
	        val = nic_state->regs.anlpar;
	        break;
	    case RTL8139_ANER:
	        val = nic_state->regs.aner;
	        break;
	    case RTL8139_DIS:
	        val = nic_state->regs.dis;
	        break;
	    case RTL8139_FCSC:
	        val = nic_state->regs.fcsc;
	        break;
	    case RTL8139_NWAYTR:
	        val = nic_state->regs.nwaytr;
	        break;
	    case RTL8139_REC:
	        val = nic_state->regs.rec;
	        break;
	    case RTL8139_CSCR:
	        val = nic_state->regs.cscr;
	        break;
	    case RTL8139_PHY1_PARM:
	        val = nic_state->regs.phy1_parm;
	        break;
	    case RTL8139_TW_PARM:
	        val = nic_state->regs.tw_parm;
	        break;
	    case RTL8139_PHY2_PARM:
	        val = nic_state->regs.phy2_parm;
	        break;
	    case RTL8139_CRC0:
	        val = nic_state->regs.crc[0];
	        break;
	    case RTL8139_CRC1:
	        val = nic_state->regs.crc[1];
	        break;
	    case RTL8139_CRC2:
	        val = nic_state->regs.crc[2];
	        break;
	    case RTL8139_CRC3:
	        val = nic_state->regs.crc[3];
	        break;
	    case RTL8139_CRC4:
	        val = nic_state->regs.crc[4];
	        break;
	    case RTL8139_CRC5:
	        val = nic_state->regs.crc[5];
	        break;
	    case RTL8139_CRC6:
	        val = nic_state->regs.crc[6];
	        break;
	    case RTL8139_CRC7:
	        val = nic_state->regs.crc[7];
	        break;
	    case RTL8139_Config5:
	        val = nic_state->regs.config5;
	        break;
	    default:
		 val = 0x0;
		 break;
    }

    memcpy(dst, &val, length);
	
    PrintDebug("rtl8139 mmio read: port:0x%x (%u bytes): 0x%x\n", (int)guest_addr, length, val);

#if TEST_PERFORMANCE
    if (need_hook(port, read))
		io_hooked ++;
    if (( ++io_total) % 50 == 0)
	 PrintError("RTL8139: Total IO: %d, Hooked: %d, INT: %d\n", io_total, io_hooked, int_total);
#endif

    return length;
}


static int rtl8139_ioport_write(struct guest_info * core, uint16_t port, void *src, uint_t length, void * private_data)
{
    PrintDebug("rtl8139 pio write: port:0x%x (%u bytes)\n", port, length);
	
    rtl8139_mmio_write((addr_t)port, src, length, private_data);
	
    return length;
}

static int rtl8139_ioport_read(uint16_t port, void *dst, uint_t length, struct vm_device *dev)
{
    PrintDebug("rtl8139 pio read: port:0x%x (%u bytes)\n", port, length);

    rtl8139_mmio_read((addr_t)port, dst, length, (void *)dev);
    
    return length;
}

static int rtl8139_reset_device(struct vm_device *dev)
{
    struct nic_context *nic_state = (struct nic_context *)dev->private_data;
    struct nic_regs *regs = &(nic_state->regs);
    regs->cmd |= CMD_Rst;
    init_rtl8139(dev);
    regs->cmd &= ~CMD_Rst;
    return 0;
}

static int rtl8139_start_device(struct vm_device *dev)
{
    PrintDebug("rtl8139: start device\n");
    struct nic_context *nic_state = (struct nic_context *)dev->private_data;
    struct nic_regs *regs = &(nic_state->regs);
    regs->cmd |= CMD_Re | CMD_Te;
    return 0;
}

static int rtl8139_stop_device(struct vm_device *dev)
{
    PrintDebug("rtl8139: stop device\n");
    struct nic_context *nic_state = (struct nic_context *)dev->private_data;
    struct nic_regs *regs = &(nic_state->regs);
    regs->cmd &= ~(CMD_Re | CMD_Te);
    return 0;
}

static void init_phy_network()
{
    V3_REGISTER_PKT_DELIVERY(&netif_input);
}

static int rtl8139_hook_iospace(struct vm_device *vmdev, addr_t base_addr, int size, int type, void *data)
{
    int i;

    if (base_addr <= 0)
    {
	PrintError("In RTL8139: Fail to Hook IO Space, base address 0x%x\n", (int) base_addr);
	return -1;
    }

    if (type == PCI_ADDRESS_SPACE_IO){
    	PrintDebug("In RTL8139: Hook IO ports starting from %x, size %d\n", (int) base_addr, size);

    	for (i = 0; i < 0xff; i++)
    	{	
  	    v3_dev_hook_io(vmdev, base_addr + i, &rtl8139_ioport_read, &rtl8139_ioport_write);
    	}
    } else if (type == PCI_ADDRESS_SPACE_MEM) {
    	PrintDebug("In RTL8139: Hook memory space starting from %x, size %d\n", (int) base_addr, size);
	
	//hook memory mapped I/O	
    	v3_hook_full_mem(vmdev->vm, base_addr, base_addr + 0xff,
                                     &rtl8139_mmio_read, &rtl8139_mmio_write, vmdev);
    } else {
       PrintError("In RTL8139: unknown memory type: start %x, size %d\n", (int) base_addr, size);
    }
	
    return 0;
}

static int rtl8139_unhook_iospace()
{

    return 0;
}


static struct pci_device * rtl8139_pci_init(struct vm_device *vmdev, struct pci_bus *bus, int devfn)
{
    uchar_t *pci_conf;
    struct pci_device *pdev;

    pdev = v3_pci_register_device(vmdev, bus, "REALTEK8139", devfn, NULL, NULL);

    if (pdev == NULL)
    {
        PrintError("NIC: Register to PCI bus failed\n");
      	 return NULL;
    }

    pci_conf = pdev->configs;
	
    pci_conf[0x00] = 0xec;
    pci_conf[0x01] = 0x10;
    pci_conf[0x02] = 0x39;
    pci_conf[0x03] = 0x81;
    pci_conf[0x04] = 0x05; /* command = I/O space, Bus Master */
    pci_conf[0x08] = RTL8139_PCI_REVID_8139; /* PCI revision ID; >=0x20 is for 8139C+ */
    pci_conf[0x0a] = 0x00; // ethernet network controller
    pci_conf[0x0b] = 0x02;
    pci_conf[0x0e] = 0x00; // header_type
    pci_conf[0x3d] = 1; // interrupt pin 0
    pci_conf[0x3c] = 12;
    pci_conf[0x34] = 0xdc;

    pdev->vmdev = vmdev;
	
    v3_pci_register_io_region(pdev, 0, 0x100, PCI_ADDRESS_SPACE_IO, &rtl8139_hook_iospace);
    v3_pci_register_io_region(pdev, 1, 0x100, PCI_ADDRESS_SPACE_MEM, &rtl8139_hook_iospace);
    
    return pdev;
}

static int rtl8139_init_state(struct rtl8139_state *nic_state)
{
    PrintDebug("rtl8139: init_state\n");
	
    init_phy_network();
    init_rtl8139(dev);
    current_vnic = dev;
    bus = v3_get_pcibus(dev->vm, 0);

    if (bus != NULL) 
    {
  	    PrintDebug("Find PCI bus in guest, attach nic to the bus %p\n", bus);
           pdev = rtl8139_pci_init(dev, bus, -1);
	    if (pdev == NULL)
		    PrintError("Failure to attach nic to the bus %p\n", bus);
    }

    nic_state->pci_dev = pdev;

    //rtl8139_hook_iospace(dev, 0x2000, 0x100, 1, NULL);

    return 0;
}

static int rtl8139_deinit_device(struct vm_device *dev)
{
	
    rtl8139_unhook_iospace(); 

    return 0;
}

static struct vm_device_ops dev_ops =
{
    .init = rtl8139_init_device,
    .deinit = rtl8139_deinit_device,
    .reset = rtl8139_reset_device,
    .start = rtl8139_start_device,
    .stop = rtl8139_stop_device,
};

struct vm_device *v3_create_rtl8139()
{
    struct nic_context *nic_state = V3_Malloc(sizeof(struct nic_context));
    PrintDebug("rtl8139 internal at %p\n", (void *)nic_state);
    struct vm_device *dev = v3_create_device("RTL8139", &dev_ops, nic_state);
    return dev;
};



static int register_dev(struct ne2k_state * nic_state) 
{
    int i;

    if (nic_state->pci_bus != NULL) {
	struct v3_pci_bar bars[6];
	struct pci_device * pci_dev = NULL;

  	PrintDebug("NE2000: PCI Enabled\n");

	for (i = 0; i < 6; i++) {
	    bars[i].type = PCI_BAR_NONE;
	}

	bars[0].type = PCI_BAR_IO;
	bars[0].default_base_port = NIC_REG_BASE_PORT;
	bars[0].num_ports = 256;

	bars[0].io_read = ne2k_pci_read;
	bars[0].io_write = ne2k_pci_write;
       bars[0].private_data = nic_state;

	pci_dev = v3_pci_register_device(nic_state->pci_bus, PCI_STD_DEVICE, 0, -1, 0, 
					 "NE2000", bars,
					 pci_config_update, NULL, NULL, nic_state);


    	if (pci_dev == NULL) {
	    PrintError("NE2000: Could not register PCI Device\n");
	    return -1;
    	}
	
	pci_dev->config_header.vendor_id = 0x10ec;
	pci_dev->config_header.device_id = 0x8029;
	pci_dev->config_header.revision = 0x00;

	pci_dev->config_header.subclass = 0x00;
	pci_dev->config_header.class = 0x02;
	pci_dev->config_header.header_type = 0x00;

	pci_dev->config_header.intr_line = 11;
	pci_dev->config_header.intr_pin = 1;

	nic_state->pci_dev = pci_dev;
    }else {
	PrintDebug("NE2000: Not attached to PCI\n");

	v3_dev_hook_io(nic_state->dev, NIC_REG_BASE_PORT , &ne2k_cmd_read, &ne2k_cmd_write);

	for (i = 1; i < 16; i++){	
	    v3_dev_hook_io(nic_state->dev, NIC_REG_BASE_PORT + i, &ne2k_std_read, &ne2k_std_write);
	}

	v3_dev_hook_io(nic_state->dev, NIC_DATA_PORT, &ne2k_data_read, &ne2k_data_write);
	v3_dev_hook_io(nic_state->dev, NIC_RESET_PORT, &ne2k_reset_port_read, &ne2k_reset_port_write);
    }


    return 0;
}

static int connect_fn(struct v3_vm_info * info, 
		      void * frontend_data, 
		      struct v3_dev_net_ops * ops, 
		      v3_cfg_tree_t * cfg, 
		      void * private_data) {
    struct ne2k_state * nic_state = (struct ne2k_state *)frontend_data;

    rtl8139_init_state(nic_state);
    register_dev(nic_state);

    nic_state->net_ops = ops;
    nic_state->backend_data = private_data;	

    ops->recv = ne2k_rx;
    ops->poll = NULL;
    ops->start_tx = NULL;
    ops->stop_tx = NULL;
    ops->frontend_data = nic_state;
    memcpy(ops->fnt_mac, nic_state->mac, ETH_ALEN);

    return 0;
}


static int rtl8139_free(struct ne2k_state * nic_state) {
    int i;

    /* dettached from backend */

    if(nic_state->pci_bus == NULL){
    	for (i = 0; i < 16; i++){		
  	    v3_dev_unhook_io(nic_state->dev, NIC_REG_BASE_PORT + i);
    	}
    
       v3_dev_unhook_io(nic_state->dev, NIC_DATA_PORT);
       v3_dev_unhook_io(nic_state->dev, NIC_RESET_PORT);
    }else {
       /* unregistered from PCI? */
    }
  
    return 0;

    V3_Free(nic_state);
	
    return 0;
}


static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))rtl8139_free,
};


static int rtl8139_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct vm_device * pci_bus = v3_find_dev(vm, v3_cfg_val(cfg, "bus"));
    struct rtl8139_state * nic_state = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");
    char * macstr = v3_cfg_val(cfg, "mac");

    nic_state  = (struct rtl8139_state *)V3_Malloc(sizeof(struct rtl8139_state));
    memset(nic_state, 0, sizeof(struct ne2k_state));

    nic_state->pci_bus = pci_bus;
    nic_state->vm = vm;

    if (macstr != NULL && !str2mac(macstr, nic_state->mac)) {
	PrintDebug("RTL8139: Mac specified %s\n", macstr);
    }else {
    	PrintDebug("RTL8139: MAC not specified\n");
	random_ethaddr(nic_state->mac);
    }

    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, nic_state);

    if (dev == NULL) {
	PrintError("RTL8139: Could not attach device %s\n", dev_id);
	V3_Free(nic_state);
	return -1;
    }

    nic_state->dev = dev;

    if (v3_dev_add_net_frontend(vm, dev_id, connect_fn, (void *)nic_state) == -1) {
	PrintError("RTL8139: Could not register %s as net frontend\n", dev_id);
	v3_remove_device(dev);
	V3_Free(nic_state);
	return -1;
    }
	    
    return 0;
}

device_register("RTL8139", rtl8139_init)
