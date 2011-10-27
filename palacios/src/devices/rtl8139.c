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

#include <devices/pci.h>
#include <palacios/vmm.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm_io.h>
#include <palacios/vmm_debug.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_ethernet.h>
#include <palacios/vmm_sprintf.h>



#ifndef V3_CONFIG_DEBUG_RTL8139
#undef PrintDebug
#define PrintDebug(fmts, args...)
#endif

#define RTL8139_IDR0    (0x00)	/* ID Registers start, len 6*1bytes */
#define RTL8139_MAR0    (0x08)	/* Mulicast Registers start, len 8*1bytes */

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
#define RTL8139_TXSAD    (0x60)	/* Tx Status of All Descriptors */
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

#define RTL8139_CRC0    (0x84)	/* Power Management CRC Reg for wakeup frame 8*1bytes */

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

enum TxStatusBits {
    TSD_Own = 1<<13,
    TSD_Tun = 1<<14,
    TSD_Tok = 1<<15,
    TSD_Cdh = 1<<28,
    TSD_Owc = 1<<29,
    TSD_Tabt = 1<<30,
    TSD_Crs = 1<<31,
};

/* Transmit Status Register (TSD0-3) Offset: 0x10-0x1F */
struct tx_status_reg {
    union {
	uint16_t val;
	struct {
	    uint16_t size     : 13;
	    uint8_t own     : 1;
	    uint8_t tun     : 1;
	    uint8_t tok     : 1;	
	    uint8_t er_tx_th   : 6;
	    uint8_t reserved    : 2;
	    uint8_t ncc		   : 4;
	    uint8_t cdh	: 1;
	    uint8_t owc	: 1;
	    uint8_t tabt	: 1;
	    uint8_t crs	: 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


enum RxStatusBits {
    Rx_Multicast = 1<<15,
    Rx_Physical = 1<<14,
    Rx_Broadcast = 1<<13,
    Rx_BadSymbol = 1<<5,
    Rx_Runt = 1<<4,
    Rx_TooLong = 1<<3,
    Rx_CRCErr = 1<<2,
    Rx_BadAlign = 1<<1,
    Rx_OK = 1<<0,
};


/* Receive Status Register in RX Packet Header */
struct rx_status_reg {
    union {
	uint16_t val;
	struct {
	    uint16_t rx_ok     : 1;
	    uint16_t rx_bad_align     : 1;
	    uint16_t rx_crc_err     : 1;
	    uint16_t rx_too_long     : 1;	
	    uint16_t rx_runt	: 1;
	    uint16_t rx_bad_sym	: 1;
	    uint16_t reserved	: 7;
	    uint16_t rx_brdcast	: 1;
	    uint16_t rx_phys	: 1;
	    uint16_t rx_multi	: 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


/* ERSR - Early Rx Status Register Offset: 0x36*/
struct errx_status_reg{
    union {
	uint8_t val;
	struct {
	    uint8_t er_rx_ok     : 1;
	    uint8_t er_rx_ovw     : 1;
	    uint8_t er_rx_bad_pkt     : 1;
	    uint8_t er_rx_good     : 1;	
	    uint8_t reserved	: 4;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


/* Transmit Status of All Descriptors (TSAD) Register */
enum TSAD_bits {
    TSAD_TOK3 = 1<<15, /* TOK bits of Descriptors*/
    TSAD_TOK2 = 1<<14, 
    TSAD_TOK1 = 1<<13, 
    TSAD_TOK0 = 1<<12, 
    TSAD_TUN3 = 1<<11, /* TUN bits of Descriptors */
    TSAD_TUN2 = 1<<10, 
    TSAD_TUN1 = 1<<9, 
    TSAD_TUN0 = 1<<8,
    TSAD_TABT3 = 1<<7, /* TABT bits of Descriptors */
    TSAD_TABT2 = 1<<6,
    TSAD_TABT1 = 1<<5,
    TSAD_TABT0 = 1<<4,
    TSAD_OWN3 = 1<<3, /* OWN bits of Descriptors */
    TSAD_OWN2 = 1<<2,
    TSAD_OWN1 = 1<<1, 
    TSAD_OWN0 = 1<<0,
};


/* Transmit Status of All Descriptors (TSAD) Register Offset: 0x60-0x61*/
struct txsad_reg {
    union {
	uint16_t val;
	struct {
	    uint8_t own0     : 1;
	    uint8_t own1     : 1;
	    uint8_t own2     : 1;
	    uint8_t own3     : 1;	
	    uint8_t tabt0	: 1;
	    uint8_t tabt1	: 1;
	    uint8_t tabt2	: 1;
	    uint8_t tabt3	: 1;
	    uint8_t tun0	: 1;
	    uint8_t tun1	: 1;
	    uint8_t tun2	: 1;
	    uint8_t tun3	: 1;
	    uint8_t tok0	: 1;
	    uint8_t tok1	: 1;
	    uint8_t tok2	: 1;
	    uint8_t tok3	: 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));



enum ISRBits {
    ISR_Rok = 1<<0,
    ISR_Rer = 1<<1,
    ISR_Tok = 1<<2,
    ISR_Ter = 1<<3,
    ISR_Rxovw = 1<<4,
    ISR_Pun = 1<<5,
    ISR_Fovw = 1<<6,
    ISR_Lenchg = 1<<13,
    ISR_Timeout = 1<<14,
    ISR_Serr = 1<<15,
};

/* 
 * Interrupt Status Register (ISR) Offset: ox3e-0x3f
 * Interrupt Mask Register (IMR 0x3c-0x3d) shares the same structure
 */
struct isr_imr_reg {
    union {
	uint16_t val;
	struct {
	    uint16_t rx_ok	:1;
	    uint16_t rx_err     : 1;
	    uint16_t tx_ok        : 1;
	    uint16_t tx_err          : 1;
	    uint16_t rx_ovw          : 1;
	    uint16_t pun_linkchg          : 1;
	    uint16_t rx_fifo_ovw  : 1;
	    uint16_t reservd:     6;
	    uint16_t lenchg  :1;
	    uint16_t timeout   :1;
	    uint16_t syserr  :1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

enum CMDBits {
    CMD_Bufe = 1<<0,
    CMD_Te = 1<<2,
    CMD_Re = 1<<3,
    CMD_Rst = 1<<4,
};


/* Commmand Register Offset: 0x37 */
struct cmd_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t cmd_bufe      : 1;
	    uint8_t reservd_1        : 1;
	    uint8_t cmd_te          : 1;
	    uint8_t cmd_re          : 1;
	    uint8_t cmd_rst          : 1;
	    uint8_t reservd_2  : 3;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));




enum CMD9346Bits {
    CMD9346_Lock = 0x00,
    CMD9346_Unlock = 0xC0,
};



/* 93C46 Commmand Register Offset: 0x50 */
struct cmd9346_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t eedo      : 1;
	    uint8_t eedi        : 1;
	    uint8_t eesk          : 1;
	    uint8_t eecs          : 1;
	    uint8_t reserved    : 2;
	    uint8_t eem  : 2;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));



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


/* Transmit Configuration Register (TCR) Offset: 0x40-0x43 */
struct tx_config_reg {
    union {
	uint32_t val;
	struct {
	    uint8_t clr_abort	:1;
	    uint8_t reserved_1     : 3;
	    uint8_t tx_retry_cnt        : 4;
	    uint8_t max_dma          : 3;
	    uint8_t reserved_2          : 5;
	    uint8_t tx_crc          : 1;
	    uint8_t loop_test  : 2;
	    uint8_t reservd_3:     3;
	    uint8_t hw_verid_b  :2;
	    uint8_t ifg   :2;
	    uint8_t hw_verid_a  :5;
	    uint8_t reservd_4  :1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));




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


/* CS Configuration Register (CSCR) Offset: 0x74-0x75 */
struct cscr_reg {
    union {
	uint16_t val;
	struct {
	    uint8_t pass_scr	:1;
	    uint8_t reserved_1     : 1;
	    uint8_t con_status_en        : 1;
	    uint8_t con_status          : 1;
	    uint8_t reserved_2          : 1;
	    uint8_t f_connect          : 1;
	    uint8_t f_link_100  : 1;
	    uint8_t jben:     1;
	    uint8_t heart_beat  :1;
	    uint8_t ld   :1;
	    uint8_t reservd_3  :5;
	    uint8_t test_fun  :1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


/* Bits in RxConfig. */
enum rx_mode_bits {
    AcceptErr = 0x20,
    AcceptRunt = 0x10,
    AcceptBroadcast = 0x08,
    AcceptMulticast = 0x04,
    AcceptMyPhys = 0x02,
    AcceptAllPhys = 0x01,
};


/* Receive Configuration Register (RCR) Offset: 0x44-0x47 */
struct rx_config_reg {
    union {
	uint32_t val;
	struct {
    	    uint8_t all_phy   : 1;
	    uint8_t my_phy	: 1;
	    uint8_t all_multi     : 1;
	    uint8_t all_brdcast        : 1;
	    uint8_t acpt_runt          : 1;
	    uint8_t acpt_err          : 1;
	    uint8_t reserved_1          : 1;
	    uint8_t wrap  : 1;
	    uint8_t max_dma:     3;
	    uint8_t rx_buf_len  :2;
	    uint8_t rx_fifo_thresd   :3;
	    uint8_t rer8  :1;
	    uint8_t mul_er_intr  :1;
	    uint8_t reserved_2          : 6;
	    uint8_t eraly_rx_thresd   :4;
	    uint8_t reserved_3          : 4;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));




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

enum Chip9346Mode {
    Chip9346_none = 0,
    Chip9346_enter_command_mode,
    Chip9346_read_command,
    Chip9346_data_read,      /* from output register */
    Chip9346_data_write,     /* to input register, then to contents at specified address */
    Chip9346_data_write_all, /* to input register, then filling contents */
};

struct EEprom9346 {
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



struct rtl8139_state {	
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

static inline void rtl8139_update_irq(struct rtl8139_state * nic_state) {
    int isr = ((nic_state->regs.isr & nic_state->regs.imr) & 0xffff);

    if(isr & 0xffff){
	v3_pci_raise_irq(nic_state->pci_bus, 0, nic_state->pci_dev);
	nic_state->statistic.tx_interrupts ++;
    }
}

static void prom9346_decode_command(struct EEprom9346 * eeprom, uint8_t command) {
    PrintDebug("RTL8139: eeprom command 0x%02x\n", command);

    switch (command & Chip9346_op_mask) {
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
            switch (command & Chip9346_op_ext_mask) {
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

static void prom9346_shift_clock(struct EEprom9346 * eeprom) {
    int bit = eeprom->eedi?1:0;

    ++ eeprom->tick;

    PrintDebug("eeprom: tick %d eedi=%d eedo=%d\n", eeprom->tick, eeprom->eedi, eeprom->eedo);

    switch (eeprom->mode) {
        case Chip9346_enter_command_mode:
            if (bit) {
                eeprom->mode = Chip9346_read_command;
                eeprom->tick = 0;
                eeprom->input = 0;
                PrintDebug("eeprom: +++ synchronized, begin command read\n");
            }
            break;

        case Chip9346_read_command:
            eeprom->input = (eeprom->input << 1) | (bit & 1);
            if (eeprom->tick == 8) {
                prom9346_decode_command(eeprom, eeprom->input & 0xff);
            }
            break;

        case Chip9346_data_read:
            eeprom->eedo = (eeprom->output & 0x8000)?1:0;
            eeprom->output <<= 1;
            if (eeprom->tick == 16){
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
            if (eeprom->tick == 16) {
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
            if (eeprom->tick == 16) {
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

static int prom9346_get_wire(struct rtl8139_state * nic_state) {
    struct EEprom9346 *eeprom = &(nic_state->eeprom);

    if (eeprom->eecs == 0)
        return 0;

    return eeprom->eedo;
}

static void prom9346_set_wire(struct rtl8139_state * nic_state, 
			      int eecs, 
			      int eesk, 
			      int eedi) {
    struct EEprom9346 *eeprom = &(nic_state->eeprom);
    uint8_t old_eecs = eeprom->eecs;
    uint8_t old_eesk = eeprom->eesk;

    eeprom->eecs = eecs;
    eeprom->eesk = eesk;
    eeprom->eedi = eedi;

    PrintDebug("eeprom: +++ wires CS=%d SK=%d DI=%d DO=%d\n",
                 eeprom->eecs, eeprom->eesk, eeprom->eedi, eeprom->eedo);

    if (old_eecs == 0 && eecs) {
        /* Synchronize start */
        eeprom->tick = 0;
        eeprom->input = 0;
        eeprom->output = 0;
        eeprom->mode = Chip9346_enter_command_mode;

        PrintDebug("=== eeprom: begin access, enter command mode\n");
    }

    if (eecs == 0) {
        PrintDebug("=== eeprom: end access\n");
        return;
    }

    if (!old_eesk && eesk) {
        /* SK front rules */
        prom9346_shift_clock(eeprom);
    }
}


static inline void rtl8139_reset_rxbuf(struct rtl8139_state * nic_state, uint32_t bufsize) {
    nic_state->rx_bufsize = bufsize;
    nic_state->regs.capr = 0;
    nic_state->regs.cbr = 0;
}


static void rtl8139_reset(struct rtl8139_state *nic_state) {
    struct rtl8139_regs *regs = &(nic_state->regs);
    int i;

    PrintDebug("Rtl8139: Reset\n");

    /* restore MAC address */
    memcpy(regs->id, nic_state->mac, ETH_ALEN);
    memset(regs->mult, 0xff, 8);

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

    for (i = 0; i < 4; ++i) {
        regs->tsd[i] = TSD_Own;
    }

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
}



static void rtl8139_9346cr_write(struct rtl8139_state * nic_state, uint32_t val) {
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
        prom9346_set_wire(nic_state, eecs, eesk, eedi);
    } else if (opmode == 0x40) {
        /* Reset.  */
        val = 0;
        rtl8139_reset(nic_state);
    }

    nic_state->regs.cmd9346 = val;
}

static uint32_t rtl8139_9346cr_read(struct rtl8139_state * nic_state) {
    uint32_t ret = nic_state->regs.cmd9346;
    uint32_t opmode = ret & 0xc0;

    if (opmode == 0x80) {
        /* eeprom access */
        int eedo = prom9346_get_wire(nic_state);
        if (eedo){
            ret |=  0x01;
        } else {
            ret &= ~0x01;
        }
    }

    PrintDebug("RTL8139: 9346CR read val=0x%02x\n", ret);

    return ret;
}


static inline int rtl8139_receiver_enabled(struct rtl8139_state * nic_state) {
    return nic_state->regs.cmd & CMD_Re;
}

static inline int rtl8139_rxwrap(struct rtl8139_state * nic_state) {
    // wrapping enabled; assume 1.5k more buffer space if size < 64K
    return (nic_state->regs.rcr & (1 << 7));
}

static void rtl8139_rxbuf_write(struct rtl8139_state * nic_state, 
				const void * buf, 
				int size) {
    struct rtl8139_regs *regs = &(nic_state->regs);
    int wrap;
    addr_t guestpa, host_rxbuf;

    guestpa = (addr_t)regs->rbstart;
    v3_gpa_to_hva(&(nic_state->vm->cores[0]), guestpa, &host_rxbuf);   

    //wrap to the front of rx buffer
    if (regs->cbr + size > nic_state->rx_bufsize){
        wrap = MOD2(regs->cbr + size, nic_state->rx_bufsize);

        if (wrap && !(nic_state->rx_bufsize < 64*1024 && rtl8139_rxwrap(nic_state))){
            PrintDebug("RTL8139: rx packet wrapped in buffer at %d\n", size-wrap);

            if (size > wrap){
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
static inline int compute_mcast_idx(const uint8_t *ep) {
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
            if (carry){
                crc = ((crc ^ POLYNOMIAL) | carry);
	    }
        }
    }
    return (crc >> 26);
}


static int rx_one_pkt(struct rtl8139_state * nic_state, 
		      uint8_t * pkt, 
		      uint32_t len){
    struct rtl8139_regs *regs = &(nic_state->regs);
    uint_t rxbufsize = nic_state->rx_bufsize;
    uint32_t header, val;
    uint8_t bcast_addr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

    if (regs->rcr & AcceptAllPhys) {
	PrintDebug("RTL8139: packet received in promiscuous mode\n");
    } else {
	if (!memcmp(pkt,  bcast_addr, 6)) {
	    if (!(regs->rcr & AcceptBroadcast)){
		PrintDebug("RTL8139: broadcast packet rejected\n");
		return -1;
	    }
	    header |= Rx_Broadcast;
	    PrintDebug("RTL8139: broadcast packet received\n");
	} else if (pkt[0] & 0x01) {
            // multicast
            if (!(regs->rcr & AcceptMulticast)){
                PrintDebug("RTL8139: multicast packet rejected\n");
                return -1;
            }

            int mcast_idx = compute_mcast_idx(pkt);

            if (!(regs->mult[mcast_idx >> 3] & (1 << (mcast_idx & 7)))){
                PrintDebug("RTL8139: multicast address mismatch\n");
                return -1;
            }
            header |= Rx_Multicast;
            PrintDebug("RTL8139: multicast packet received\n");
        } else if (!compare_ethaddr(regs->id, pkt)){
            if (!(regs->rcr & AcceptMyPhys)){
                PrintDebug("RTL8139: rejecting physical address matching packet\n");
                return -1;
            }

            header |= Rx_Physical;
            PrintDebug("RTL8139: physical address matching packet received\n");
        } else {
            PrintDebug("RTL8139: unknown packet\n");
            return -1;
        }
    }

    if(1){
	PrintDebug("RTL8139: in ring Rx mode\n");

	int avail = MOD2(rxbufsize + regs->capr - regs->cbr, rxbufsize);

        if (avail != 0 && len + 8 >= avail){
            PrintError("rx overflow: rx buffer length %d head 0x%04x read 0x%04x === available 0x%04x need 0x%04x\n",
                   rxbufsize, regs->cbr, regs->capr, avail, len + 8);

            regs->isr |= ISR_Rxovw;
            ++ regs->mpc;
            rtl8139_update_irq(nic_state);
            return -1;
        }

        header |= Rx_OK;
        header |= ((len << 16) & 0xffff0000);

        rtl8139_rxbuf_write(nic_state, (uint8_t *)&header, 4);

        rtl8139_rxbuf_write(nic_state, pkt, len);

        /* CRC checksum */
        val = v3_crc32(0, pkt, len);

        rtl8139_rxbuf_write(nic_state, (uint8_t *)&val, 4);

        // correct buffer write pointer 
        regs->cbr = MOD2((regs->cbr + 3) & ~0x3, rxbufsize);

        PrintDebug("RTL8139: received: rx buffer length %d CBR: 0x%04x CAPR: 0x%04x\n",
               		rxbufsize, regs->cbr, regs->capr);
    }

    regs->isr |= ISR_Rok;

    nic_state->statistic.rx_pkts ++;
    nic_state->statistic.rx_bytes += len;
	
    rtl8139_update_irq(nic_state);   

    return 0;
}

static int rtl8139_rx(uint8_t * pkt, uint32_t len, void * private_data) {
    struct rtl8139_state *nic_state = (struct rtl8139_state *)private_data;

    if (!rtl8139_receiver_enabled(nic_state)){
	PrintDebug("RTL8139: receiver disabled\n");
	nic_state->statistic.rx_dropped ++;
		
	return 0;
    }
	
    if(rx_one_pkt(nic_state, pkt, len) >= 0){
	nic_state->statistic.rx_pkts ++;
	nic_state->statistic.rx_bytes += len;
    }else {
    	nic_state->statistic.rx_dropped ++;
    }

    return 0;
}

static void rtl8139_rcr_write(struct rtl8139_state * nic_state, uint32_t val) {
    PrintDebug("RTL8139: RCR write val=0x%08x\n", val);

    val = SET_MASKED(val, 0xf0fc0040, nic_state->regs.rcr);
    nic_state->regs.rcr = val;

#if 0
    uchar_t rblen = (regs->rcr >> 11) & 0x3;
    switch(rblen) {
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
    rtl8139_reset_rxbuf(nic_state, 8192 << ((nic_state->regs.rcr >> 11) & 0x3));

    PrintDebug("RTL8139: RCR write reset buffer size to %d\n", nic_state->rx_bufsize);
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

static inline int transmitter_enabled(struct rtl8139_state * nic_state){
    return nic_state->regs.cmd & CMD_Te;
}

static int rxbufempty(struct rtl8139_state * nic_state){
    struct rtl8139_regs *regs = &(nic_state->regs);
    int unread;

    unread = MOD2(regs->cbr + nic_state->rx_bufsize - regs->capr, nic_state->rx_bufsize);
   
    if (unread != 0){
        PrintDebug("RTL8139: receiver buffer data available 0x%04x\n", unread);
        return 0;
    }

    PrintDebug("RTL8139: receiver buffer is empty\n");

    return 1;
}

static void rtl8139_cmd_write(struct rtl8139_state * nic_state, uint32_t val){
    val &= 0xff;

    PrintDebug("RTL8139: Cmd write val=0x%08x\n", val);

    if (val & CMD_Rst){
        PrintDebug("RTL8139: Cmd reset\n");
        rtl8139_reset(nic_state);
    }
    if (val & CMD_Re){
        PrintDebug("RTL8139: Cmd enable receiver\n");
    }
    if (val & CMD_Te){
        PrintDebug("RTL8139: Cmd enable transmitter\n");
    }

    val = SET_MASKED(val, 0xe3, nic_state->regs.cmd);
    val &= ~CMD_Rst;

    nic_state->regs.cmd = val;
}

static int tx_one_packet(struct rtl8139_state * nic_state, int descriptor){
    struct rtl8139_regs *regs = &(nic_state->regs);
    int txsize;
    uint8_t *pkt;
    addr_t pkt_gpa = 0, hostva = 0;

    if (!transmitter_enabled(nic_state)){
        PrintError("RTL8139: fail to send from descriptor %d: transmitter disabled\n", descriptor);
        return 0;
    }

    if (regs->tsd[descriptor] & TSD_Own){
        PrintError("RTL8139: fail to send from descriptor %d: owned by host\n", descriptor);
        return 0;
    }

    txsize = regs->tsd[descriptor] & 0x1fff;
    pkt_gpa = (addr_t) regs->tsad[descriptor];

    PrintDebug("RTL8139: sending %d bytes from guest memory at 0x%08x\n", txsize, regs->tsad[descriptor]);
	
    v3_gpa_to_hva(&(nic_state->vm->cores[0]), (addr_t)pkt_gpa, &hostva);
    pkt = (uchar_t *)hostva;

#ifdef V3_CONFIG_DEBUG_RTL8139
    v3_hexdump(pkt, txsize, NULL, 0);
#endif

    if (TxLoopBack == (regs->tcr & TxLoopBack)){ /* loopback test */
        PrintDebug(("RTL8139: transmit loopback mode\n"));
        rx_one_pkt(nic_state, pkt, txsize);
    } else{       
        if (nic_state->net_ops->send(pkt, txsize, nic_state->backend_data) == 0){
	    PrintDebug("RTL8139: Sent %d bytes from descriptor %d\n", txsize, descriptor);
	    nic_state->statistic.tx_pkts ++;
	    nic_state->statistic.tx_bytes += txsize;
	} else {
	    PrintError("Rtl8139: Sending packet error: 0x%p\n", pkt);
	    nic_state->statistic.tx_dropped ++;
	}
    }

    regs->tsd[descriptor] |= TSD_Tok;
    regs->tsd[descriptor] |= TSD_Own;

    nic_state->regs.isr |= ISR_Tok;
    rtl8139_update_irq(nic_state);

    return 0;
}

/*transmit status registers*/
static void rtl8139_tsd_write(struct rtl8139_state * nic_state, 
			      uint8_t descriptor, 
			      uint32_t val){ 
    if (!transmitter_enabled(nic_state)){
        PrintDebug("RTL8139: TxStatus write val=0x%08x descriptor=%d, Transmitter not enabled\n", val, descriptor);
		
        return;
    }

    PrintDebug("RTL8139: TSD write val=0x%08x descriptor=%d\n", val, descriptor);

    // mask read-only bits
    val &= ~0xff00c000;
    val = SET_MASKED(val, 0x00c00000,  nic_state->regs.tsd[descriptor]);

    nic_state->regs.tsd[descriptor] = val;

    tx_one_packet(nic_state, descriptor);
}

/* transmit status of all descriptors */
static uint16_t rtl8139_txsad_read(struct rtl8139_state * nic_state){
    uint16_t ret = 0;
    struct rtl8139_regs *regs = &(nic_state->regs);

    ret = ((regs->tsd[3] & TSD_Tok)?TSAD_TOK3:0)
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

    PrintDebug("RTL8139: txsad read val=0x%04x\n", (int)ret);

    return ret;
}

static inline void rtl8139_isr_write(struct rtl8139_state * nic_state, uint32_t val) {
    struct rtl8139_regs *regs = &(nic_state->regs);

    PrintDebug("RTL8139: ISR write val=0x%04x\n", val);

    uint16_t newisr = regs->isr & ~val;

    /* mask unwriteable bits */
    newisr = SET_MASKED(newisr, 0x1e00, regs->isr);

    /* writing 1 to interrupt status register bit clears it */
    regs->isr = 0;
    rtl8139_update_irq(nic_state);

    regs->isr = newisr;
    rtl8139_update_irq(nic_state);
}

static int rtl8139_mmio_write(struct guest_info * core, 
			      addr_t guest_addr, 
			      void * src,
			      uint_t length, 
			      void * priv_data) {
    int idx;
    uint32_t val;
    struct rtl8139_state *nic_state = (struct rtl8139_state *)(priv_data);

    idx = guest_addr & 0xff;

    memcpy(&val, src, length);

    PrintDebug("rtl8139 mmio write: addr:0x%x (%u bytes): 0x%x\n", (int)guest_addr, length, val);
	
    switch(idx) {
	case RTL8139_IDR0 ... RTL8139_IDR0 + 5:
	    nic_state->regs.id[idx - RTL8139_IDR0] = val & 0xff;
	    break;

	case RTL8139_MAR0 ... RTL8139_MAR0 + 7:
	    nic_state->regs.mult[idx - RTL8139_MAR0] = val & 0xff;
	    break;

	case RTL8139_TSD0:
	case RTL8139_TSD1:
	case RTL8139_TSD2:
	case RTL8139_TSD3:
	    rtl8139_tsd_write(nic_state, (idx - RTL8139_TSD0)/4, val);
	    break;
		
	case RTL8139_TSAD0:
	case RTL8139_TSAD1:
	case RTL8139_TSAD2:
	case RTL8139_TSAD3:
	    nic_state->regs.tsad[(idx - RTL8139_TSAD0)/4] = val;
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
	    rtl8139_cmd_write(nic_state, val);
	    break;
	case RTL8139_CAPR:
	{
	    val &= 0xffff;
	    /* this value is off by 16 */
	    nic_state->regs.capr = MOD2(val + 0x10, nic_state->rx_bufsize);

	    PrintDebug("RTL 8139: CAPR write: rx buffer length %d head 0x%04x read 0x%04x\n",
	    nic_state->rx_bufsize, nic_state->regs.cbr, nic_state->regs.capr);	
	}
	    break;
	case RTL8139_CBR: /* read only */
	    //nic_state->regs.cbr = val & 0xffff;
	    break;
	case RTL8139_IMR:
	{
	    PrintDebug("RTL8139: IMR write val=0x%04x\n", val);

	    /* mask unwriteable bits */
	    val = SET_MASKED(val, 0x1e00, nic_state->regs.imr);
	    nic_state->regs.imr = val;
	    rtl8139_update_irq(nic_state);
	}
	    break;
	case RTL8139_ISR:
	    rtl8139_isr_write(nic_state, val);
	    break;
	case RTL8139_TCR:
	    nic_state->regs.tcr = val;
	    break;
	case RTL8139_RCR:
	    rtl8139_rcr_write(nic_state, val);
	    break;
	case RTL8139_TCTR:
	    nic_state->regs.tctr = 0; /* write clear current tick */
	    break;
	case RTL8139_MPC:
	    nic_state->regs.mpc = 0; /* clear on write */
	    break;
	case RTL8139_9346CR:
	    rtl8139_9346cr_write(nic_state, val);
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
	case RTL8139_TXSAD:
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
	case RTL8139_CRC0 ... RTL8139_CRC0 + 7:
	    nic_state->regs.crc[idx - RTL8139_CRC0] = val & 0xff;
	    break;

	case RTL8139_Config5:
	    nic_state->regs.config5 = val & 0xff;
	    break;
	default:
	    PrintDebug("rtl8139 write error: invalid port: 0x%x\n", idx);
	}
        
    return length;
}

static int rtl8139_mmio_read(struct guest_info * core, 
			     addr_t guest_addr, 
			     void * dst, 
			     uint_t length, 
			     void * priv_data) {
    uint16_t idx;
    uint32_t val;
    struct rtl8139_state *nic_state = (struct rtl8139_state *)priv_data;

    idx = guest_addr & 0xff;

    switch(idx) {
	case RTL8139_IDR0 ... RTL8139_IDR0 + 5:
	    val = nic_state->regs.id[idx - RTL8139_IDR0];
	    break;
		
	case RTL8139_MAR0 ... RTL8139_MAR0 + 7:
	    val = nic_state->regs.mult[idx - RTL8139_MAR0];
	    break;

	case RTL8139_TSD0:
	case RTL8139_TSD1:
	case RTL8139_TSD2:
	case RTL8139_TSD3:
	    val = nic_state->regs.tsd[(idx - RTL8139_TSD0)/4];
	    break;

	case RTL8139_TSAD0:
	case RTL8139_TSAD1:
	case RTL8139_TSAD2:
	case RTL8139_TSAD3:
	    val = nic_state->regs.tsad[(idx - RTL8139_TSAD0)/4];
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
	{
	    val = nic_state->regs.cmd;
	    if (rxbufempty(nic_state)){
	    	val |= CMD_Bufe;
	    }
	}
	    break;
	case RTL8139_CAPR:
	    /* this value is off by 16 -- don't know why - Lei*/
	    val = nic_state->regs.capr - 0x10;
	    break;
	case RTL8139_CBR:
	    val = nic_state->regs.cbr;
	    break;
	case RTL8139_IMR:
	    val = nic_state->regs.imr;
	    break;
	case RTL8139_ISR:
	    val = (uint32_t)nic_state->regs.isr;
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
	    val = rtl8139_9346cr_read(nic_state);
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
	case RTL8139_TXSAD: 
	    val = rtl8139_txsad_read(nic_state);
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
	case RTL8139_CRC0 ... RTL8139_CRC0 + 7:
	    val = nic_state->regs.crc[idx - RTL8139_CRC0];
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

    return length;
}


static int rtl8139_ioport_write(struct guest_info * core,
				uint16_t port, 
				void *src, 
				uint_t length, 
				void * private_data) {
    return rtl8139_mmio_write(core, (addr_t)port, 
			      src, length, private_data);
}

static int rtl8139_ioport_read(struct guest_info * core, 
			       uint16_t port, 
			       void *dst, 
			       uint_t length, 
			       void * private_data) {
    return rtl8139_mmio_read(core, (addr_t)port, 
			     dst, length, private_data);
}


static int rtl8139_init_state(struct rtl8139_state *nic_state) {
    PrintDebug("rtl8139: init_state\n");
	
    nic_state->regs.tsd[0] = nic_state->regs.tsd[1] = nic_state->regs.tsd[2] = nic_state->regs.tsd[3] = TSD_Own;

    nic_state->regs.rerid = RTL8139_PCI_REVID_8139;
    nic_state->regs.tcr |= ((0x1d << 26) | (0x1 << 22));

    rtl8139_reset(nic_state);

    return 0;
}


#if 0
static inline int rtl8139_reset_device(struct rtl8139_state * nic_state) {
    nic_state->regs.cmd |= CMD_Rst;
    rtl8139_init_state(nic_state);
    nic_state->regs.cmd &= ~CMD_Rst;
	
    return 0;
}

static inline int rtl8139_start_device(struct rtl8139_state * nic_state) {
    nic_state->regs.cmd |= CMD_Re | CMD_Te;
	
    return 0;
}

static int rtl8139_stop_device(struct rtl8139_state * nic_state) {
    PrintDebug("rtl8139: stop device\n");

    nic_state->regs.cmd &= ~(CMD_Re | CMD_Te);
	
    return 0;
}

static int rtl8139_hook_iospace(struct rtl8139_state * nic_state, 
				addr_t base_addr, 
				int size, 
				int type, 
				void *data) {
    int i;

    if (base_addr <= 0){
	PrintError("In RTL8139: Fail to Hook IO Space, base address 0x%x\n", (int) base_addr);
	return -1;
    }

    if (type == PCI_BAR_IO){
    	PrintDebug("In RTL8139: Hook IO ports starting from %x, size %d\n", (int) base_addr, size);

    	for (i = 0; i < 0xff; i++){	
  	    v3_dev_hook_io(nic_state->dev, base_addr + i, &rtl8139_ioport_read, &rtl8139_ioport_write);
    	}
    } else if (type == PCI_BAR_MEM32) {
    	PrintDebug("In RTL8139: Hook memory space starting from %x, size %d\n", (int) base_addr, size);
	
	//hook memory mapped I/O	
    	v3_hook_full_mem(nic_state->vm, nic_state->vm->cores[0].cpu_id, base_addr, base_addr + 0xff,
                                     &rtl8139_mmio_read, &rtl8139_mmio_write, nic_state);
    } else {
       PrintError("In RTL8139: unknown memory type: start %x, size %d\n", (int) base_addr, size);
    }
	
    return 0;
}
#endif

static int register_dev(struct rtl8139_state * nic_state)  {
    int i;

    if (nic_state->pci_bus == NULL) {
	PrintError("RTL8139: Not attached to any PCI bus\n");

	return -1;
    }

    struct v3_pci_bar bars[6];
    struct pci_device * pci_dev = NULL;

    for (i = 0; i < 6; i++) {
	bars[i].type = PCI_BAR_NONE;
    }

    bars[0].type = PCI_BAR_IO;
    bars[0].default_base_port = 0xc100;
    bars[0].num_ports = 0x100;

    bars[0].io_read = rtl8139_ioport_read;
    bars[0].io_write = rtl8139_ioport_write;
    bars[0].private_data = nic_state;

/*
    bars[1].type = PCI_BAR_MEM32;
    bars[1].default_base_addr = -1;
    bars[1].num_pages = 1;

    bars[1].mem_read = rtl8139_mmio_read;
    bars[1].mem_write = rtl8139_mmio_write;
    bars[1].private_data = nic_state;
*/

    pci_dev = v3_pci_register_device(nic_state->pci_bus, PCI_STD_DEVICE, 0, -1, 0, 
					 "RTL8139", bars,
					 NULL, NULL, NULL, nic_state);


    if (pci_dev == NULL) {
	PrintError("RTL8139: Could not register PCI Device\n");
	return -1;
    }
	
    pci_dev->config_header.vendor_id = 0x10ec;
    pci_dev->config_header.device_id = 0x8139;
    pci_dev->config_header.command = 0x05;
	
    pci_dev->config_header.revision = RTL8139_PCI_REVID_8139;

    pci_dev->config_header.subclass = 0x00;
    pci_dev->config_header.class = 0x02;
    pci_dev->config_header.header_type = 0x00;

    pci_dev->config_header.intr_line = 12;
    pci_dev->config_header.intr_pin = 1;
    pci_dev->config_space[0x34] = 0xdc;

    nic_state->pci_dev = pci_dev;
	
    return 0;
}

static int connect_fn(struct v3_vm_info * info, 
		      void * frontend_data, 
		      struct v3_dev_net_ops * ops, 
		      v3_cfg_tree_t * cfg, 
		      void * private_data) {
    struct rtl8139_state * nic_state = (struct rtl8139_state *)frontend_data;

    rtl8139_init_state(nic_state);
    register_dev(nic_state);

    nic_state->net_ops = ops;
    nic_state->backend_data = private_data;	

    ops->recv = rtl8139_rx;
    ops->poll = NULL;
    memcpy(ops->config.fnt_mac, nic_state->mac, ETH_ALEN);

    return 0;
}


static int rtl8139_free(void * private_data) {
    struct rtl8139_state * nic_state = (struct rtl8139_state *)private_data;

    /* dettached from backend */

    /* unregistered from PCI? */

    V3_Free(nic_state);
	
    return 0;
}


static struct v3_device_ops dev_ops = {
    .free = rtl8139_free,
};


static int rtl8139_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct vm_device * pci_bus = v3_find_dev(vm, v3_cfg_val(cfg, "bus"));
    struct rtl8139_state * nic_state = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");
    char * macstr = v3_cfg_val(cfg, "mac");

    nic_state  = (struct rtl8139_state *)V3_Malloc(sizeof(struct rtl8139_state));
    memset(nic_state, 0, sizeof(struct rtl8139_state));

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
