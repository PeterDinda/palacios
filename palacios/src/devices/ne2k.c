/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2009, Lei Xia <lxia@northwestern.edu> 
 * Copyright (c) 2009, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Lei Xia <lxia@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */
 
#include <devices/pci.h>
#include <palacios/vmm.h>
#include <palacios/vmm_io.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_string.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_ethernet.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_sprintf.h>

#ifndef V3_CONFIG_DEBUG_NE2K
#undef PrintDebug
#define PrintDebug(fmts, args...)
#endif


#define NE2K_DEFAULT_IRQ        11

// What the hell is this crap?
#define NE2K_PMEM_SIZE          (32 * 1024)
#define NE2K_PMEM_START         (16 * 1024)
#define NE2K_PMEM_END           (NE2K_PMEM_SIZE + NE2K_PMEM_START)
#define NE2K_MEM_SIZE           NE2K_PMEM_END

#define NIC_REG_BASE_PORT       0xc100  	/* Command register (for all pages) */

#define NE2K_CMD_OFFSET	0x00
#define NE2K_DATA_OFFSET	0x10
#define NE2K_RESET_OFFSET	0x1f

/* Page 0 registers */
#define EN0_CLDALO		0x01	/* Low byte of current local dma addr  RD */
#define EN0_STARTPG	 	0x01  	/* Starting page of ring bfr WR  */
#define EN0_CLDAHI	 	0x02	/* High byte of current local dma addr  RD  */
#define EN0_STOPPG	 	0x02    /* Ending page +1 of ring bfr WR */
#define EN0_BOUNDARY	        0x03	/* Boundary page of ring bfr RD WR */
#define EN0_TSR			0x04	/* Transmit status reg RD */
#define EN0_TPSR		0x04 	/* Transmit starting page WR */
#define EN0_NCR			0x05 	/* Number of collision reg RD */
#define EN0_TCNTLO		0x05 	/* Low  byte of tx byte count WR */
#define EN0_FIFO		0x06 	/* FIFO RD */
#define EN0_TCNTHI		0x06	/* High byte of tx byte count WR */
#define EN0_ISR			0x07 	/* Interrupt status reg RD WR */
#define EN0_CRDALO		0x08 	/* low byte of current remote dma address RD */
#define EN0_RSARLO		0x08 	/* Remote start address reg 0 */
#define EN0_CRDAHI		0x09 	/* high byte, current remote dma address RD */
#define EN0_RSARHI		0x09 	/* Remote start address reg 1 */
#define EN0_RCNTLO		0x0a 	/* Remote byte count reg WR */
#define EN0_RTL8029ID0	        0x0a 	/* Realtek ID byte #1 RD */
#define EN0_RCNTHI		0x0b 	/* Remote byte count reg WR */
#define EN0_RTL8029ID1	        0x0b	/* Realtek ID byte #2 RD */
#define EN0_RSR			0x0c 	/* rx status reg RD */
#define EN0_RXCR		0x0c 	/* RX configuration reg WR */
#define EN0_TXCR		0x0d 	/* TX configuration reg WR */
#define EN0_COUNTER0	        0x0d 	/* Rcv alignment error counter RD */
#define EN0_DCFG		0x0e 	/* Data configuration reg WR */
#define EN0_COUNTER1	        0x0e	/* Rcv CRC error counter RD */
#define EN0_IMR			0x0f	/* Interrupt mask reg WR */
#define EN0_COUNTER2	        0x0f	/* Rcv missed frame error counter RD */

/* Page 1 registers */
#define EN1_PHYS        	0x01
#define EN1_CURPAG     	        0x07
#define EN1_MULT       	        0x08

/* Page 2 registers */
#define EN2_STARTPG	 	0x01	/* Starting page of ring bfr RD */
#define EN2_STOPPG		0x02	/* Ending page +1 of ring bfr RD */
#define EN2_LDMA0  		0x01   	/* Current Local DMA Address 0 WR */
#define EN2_LDMA1  		0x02   	/* Current Local DMA Address 1 WR */
#define EN2_RNPR  		0x03   	/* Remote Next Packet Pointer RD WR */
#define EN2_TPSR  		0x04    	/* Transmit Page Start Address RD */
#define EN2_LNRP  		0x05   	/* Local Next Packet Pointer RD WR */
#define EN2_ACNT0  		0x06  	/* Address Counter Upper WR */
#define EN2_ACNT1  		0x07  	/* Address Counter Lower WR */
#define EN2_RCR  		0x0c  	/* Receive Configuration Register RD */
#define EN2_TCR  		0x0d  	/* Transmit Configuration Register RD */
#define EN2_DCR  		0x0e  	/* Data Configuration Register RD */
#define EN2_IMR  		0x0f 	/* Interrupt Mask Register RD */

/* Page 3 registers */
#define EN3_CONFIG0	 	0x03
#define EN3_CONFIG1	 	0x04
#define EN3_CONFIG2	 	0x05
#define EN3_CONFIG3	 	0x06


struct cmd_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t stop        : 1;
	    uint8_t start       : 1;
	    uint8_t tx_pkt      : 1;
	    uint8_t rem_dma_cmd : 3;    /* 0=Not allowed, 1=Read, 2=Write, 3=Send Pkt, 4=Abort/Complete DMA */
	    uint8_t pg_sel      : 2;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct intr_status_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t pkt_rx          : 1;
	    uint8_t pkt_tx          : 1;
	    uint8_t rx_err          : 1;
	    uint8_t tx_err          : 1;
	    uint8_t overwrite_warn  : 1;
	    uint8_t cnt_overflow    : 1;
	    uint8_t rem_dma_done    : 1;
	    uint8_t reset_status    : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct intr_mask_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t pkt_rx          : 1;
	    uint8_t pkt_tx          : 1;
	    uint8_t rx_err          : 1;
	    uint8_t tx_err          : 1;
	    uint8_t overwrite_warn  : 1;
	    uint8_t cnt_overflow    : 1;
	    uint8_t rem_dma_done    : 1;
	    uint8_t rsvd            : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct data_cfg_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t word_trans_sel   : 1;
	    uint8_t byte_order_sel   : 1;
	    uint8_t long_addr_sel    : 1;
	    uint8_t loopback_sel     : 1;
	    uint8_t auto_init_rem    : 1;
	    uint8_t fifo_thresh_sel  : 2;
	    uint8_t rsvd             : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct tx_cfg_reg { 
    union {
	uint8_t val;
	struct {
	    uint8_t inhibit_crc     : 1;
	    uint8_t enc_loop_ctrl   : 2;
	    uint8_t auto_tx_disable : 1;
	    uint8_t coll_offset_en  : 1;
	    uint8_t rsvd            : 3;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

struct tx_status_reg { 
    union {
	uint8_t val;
	struct {
	    uint8_t pkt_tx_ok       : 1;
	    uint8_t rsvd            : 1;
	    uint8_t tx_collision    : 1;
	    uint8_t tx_aborted      : 1;
	    uint8_t carrier_lost    : 1;
	    uint8_t fifo_underrun   : 1;
	    uint8_t cd_heartbeat    : 1;
	    uint8_t oow_collision   : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

struct rx_cfg_reg { 
    union {
	uint8_t val;
	struct {
	    uint8_t save_pkt_errs    : 1;
	    uint8_t runt_pkt_ok      : 1;
	    uint8_t bcast_ok         : 1;
	    uint8_t mcast_ok         : 1;
	    uint8_t prom_phys_enable : 1;
	    uint8_t mon_mode         : 1;
	    uint8_t rsvd             : 2;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct rx_status_reg { 
    union {
	uint8_t val;
	struct {
	    uint8_t pkt_rx_ok        : 1;
	    uint8_t crc_err          : 1;
	    uint8_t frame_align_err  : 1;
	    uint8_t fifo_overrun     : 1;
	    uint8_t missed_pkt       : 1;
	    uint8_t phy_match        : 1;   /* 0=Physical Addr Match, 1=MCAST/BCAST Addr Match */
	    uint8_t rx_disabled      : 1;
	    uint8_t deferring        : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct ne2k_registers {
    struct cmd_reg cmd;
    struct intr_status_reg isr;
    struct intr_mask_reg imr;
    struct data_cfg_reg dcr;
    struct tx_cfg_reg tcr;
    struct tx_status_reg tsr;
    struct rx_cfg_reg rcr;
    struct rx_status_reg rsr;  

    uint8_t      pgstart;      /* page start reg */
    uint8_t      pgstop;       /* page stop reg */
    uint8_t      boundary;     /* boundary ptr */
    uint8_t      tpsr;         /* tx page start addr */
    uint8_t      ncr;          /* number of collisions */
    uint8_t      fifo;         /* FIFO... */

    uint8_t      curpag;       /* current page */
    uint8_t      rnpp;         /* rem next pkt ptr */
    uint8_t      lnpp;         /* local next pkt ptr */

    uint8_t      cntr0;        /* counter 0 (frame alignment errors) */
    uint8_t      cntr1;        /* counter 1 (CRC Errors) */
    uint8_t      cntr2;        /* counter 2 (missed pkt errors) */

    union {                    /* current local DMA Addr */
	uint16_t     clda;
	struct {
	    uint8_t clda0;
	    uint8_t clda1;
	} __attribute__((packed));
    } __attribute__((packed));


    union {                    /* current remote DMA addr */
	uint16_t     crda;
	struct {
	    uint8_t crda0;
	    uint8_t crda1;
	} __attribute__((packed));
    } __attribute__((packed));


    union {                    /* Remote Start Addr Reg */
	uint16_t     rsar;
	struct {
	    uint8_t rsar0;
	    uint8_t rsar1;
	} __attribute__((packed));
    } __attribute__((packed));


    union {                    /* TX Byte count Reg */
	uint16_t     tbcr;
	struct {
	    uint8_t tbcr0;
	    uint8_t tbcr1;
	} __attribute__((packed));
    } __attribute__((packed));

    union {                    /* Remote Byte count Reg */
	uint16_t     rbcr;
	struct {
	    uint8_t rbcr0;
	    uint8_t rbcr1;
	} __attribute__((packed));
    } __attribute__((packed));

    union {                    /* Address counter? */
	uint16_t     addcnt;
	struct {
	    uint8_t addcnt0;
	    uint8_t addcnt1;
	} __attribute__((packed));
    } __attribute__((packed));
};


struct ne2k_state {
    struct v3_vm_info * vm;
    struct pci_device * pci_dev;
    struct vm_device * pci_bus;
    struct vm_device * dev;

    struct ne2k_registers context;
    uint8_t mem[NE2K_MEM_SIZE];

    uint8_t mcast_addr[8];
    uint8_t mac[ETH_ALEN];

    struct nic_statistics statistics;

    struct v3_dev_net_ops *net_ops;
    void * backend_data;
};

static int ne2k_update_irq(struct ne2k_state * nic_state) {
    struct pci_device * pci_dev = nic_state->pci_dev;

    if ((nic_state->context.isr.val & nic_state->context.imr.val) & 0x7f) {
       if (pci_dev == NULL){
	    v3_raise_virq(&(nic_state->vm->cores[0]), NE2K_DEFAULT_IRQ);
       } else {	    
	   v3_pci_raise_irq(nic_state->pci_bus, nic_state->pci_dev, 0);
       }

       nic_state->statistics.rx_interrupts ++;

       PrintDebug(VM_NONE, VCORE_NONE, "NE2000: Raise IRQ\n");
    }

    return 0;
}

static int tx_one_pkt(struct ne2k_state * nic_state, uchar_t *pkt, uint32_t length) {
	
#ifdef V3_CONFIG_DEBUG_NE2K
    PrintDebug(VM_NONE, VCORE_NONE, "NE2000: Send Packet:\n");
    v3_hexdump(pkt, length, NULL, 0);
#endif    

    if(nic_state->net_ops->send(pkt, length, nic_state->backend_data) >= 0){
	nic_state->statistics.tx_pkts ++;
	nic_state->statistics.tx_bytes += length;

	return 0;
    }
	
    nic_state->statistics.tx_dropped ++;

    return -1;
}

static int ne2k_rxbuf_full(struct ne2k_registers * regs) {
    int empty;
    int index;
    int boundary;

    index = regs->curpag << 8;
    boundary = regs->boundary << 8;

    if (index < boundary) {
        empty = boundary - index;
    } else {
        empty = ((regs->pgstop - regs->pgstart) << 8) - (index - boundary);
    }

    if (empty < (ETHERNET_PACKET_LEN + 4)) {
        return 1;
    }

    return 0;
}

#define MIN_BUF_SIZE 60


// This needs to be completely redone...
static int rx_one_pkt(struct ne2k_state * nic_state, const uchar_t * pkt,  uint32_t length) {
    struct ne2k_registers * regs = (struct ne2k_registers *)&(nic_state->context);
    uchar_t buf[MIN_BUF_SIZE];
    uchar_t * p;
    uint32_t total_len;
    uint32_t next;
    uint32_t len;
    uint32_t index;
    uint32_t empty;
    uint32_t start;
    uint32_t stop;

    start = regs->pgstart << 8;
    stop = regs->pgstop << 8;
   
    if (regs->cmd.stop) {
	return -1;
    }

    if (ne2k_rxbuf_full(regs)) {
	PrintError(VM_NONE, VCORE_NONE, "Ne2k: received buffer overflow\n");
	return -1;
    }

    //packet too small, expand it
    if (length < MIN_BUF_SIZE) {
        memcpy(buf, pkt, length);
        memset(buf + length, 0, MIN_BUF_SIZE - length);
        pkt = buf;
        length = MIN_BUF_SIZE;
    }

    index = regs->curpag << 8;

    //header, 4 bytes
    total_len = length + 4;

    //address for next packet (4 bytes for CRC)
    next = index + ((total_len + 4 + 255) & ~0xff);

    if (next >= stop) {
        next -= (stop - start);
    }

    p = nic_state->mem + index;
    regs->rsr.val = 0;
    regs->rsr.pkt_rx_ok = 1;

    if (pkt[0] & 0x01) {
        regs->rsr.phy_match = 1; /* TODO: Check this back */
    }

    p[0] = regs->rsr.val;
    p[1] = next >> 8;
    p[2] = total_len;
    p[3] = total_len >> 8;
    index += 4;

    while (length > 0) {
	if (index <= stop) {
	    empty = stop - index;
	} else {
	    empty = 0;
	}

	len = length;

	if (len > empty) {
	    len = empty;
	}

	memcpy(nic_state->mem + index, pkt, len);
	pkt += len;
	index += len;

	if (index == stop) {
	    index = start;
	}

	length -= len;
    }

    regs->curpag = next >> 8;

    regs->isr.pkt_rx = 1;
    ne2k_update_irq(nic_state);

    return 0;
}


static int ne2k_rx(uint8_t * buf, uint32_t size, void * private_data){
    struct ne2k_state * nic_state = (struct ne2k_state *)private_data;
  
#ifdef V3_CONFIG_DEBUG_NE2K
    PrintDebug(VM_NONE, VCORE_NONE, "\nNe2k: Packet Received:\n");
    v3_hexdump(buf, size, NULL, 0);
#endif    

    if(!rx_one_pkt(nic_state, buf, size)){
	nic_state->statistics.rx_pkts ++;
	nic_state->statistics.rx_bytes += size;
	
	return 0;
    }

    nic_state->statistics.rx_dropped ++;
	
    return -1;
}


static inline void mem_writeb(struct ne2k_state * nic_state, 
			uint32_t addr, 
			uint32_t val) {
    uchar_t tmp = (uchar_t) (val & 0x000000ff);

    if ((addr < 32) || (addr >= NE2K_PMEM_START && addr < NE2K_MEM_SIZE)) {
        nic_state->mem[addr] = tmp;
    }
}

static inline void mem_writew(struct ne2k_state * nic_state, 
			    uint32_t addr,
			    uint32_t val) {
    addr &= ~1;

    if ((addr < 32) || (addr >= NE2K_PMEM_START && addr < NE2K_MEM_SIZE)) {
        *(uint16_t *)(nic_state->mem + addr) = val;
    }
}

static inline void mem_writel(struct ne2k_state * nic_state,
			    uint32_t addr,
			    uint32_t val) {
    addr &= ~1;

    if ((addr < 32) || (addr >= NE2K_PMEM_START && addr < NE2K_MEM_SIZE)) {
        *(uint32_t *)(nic_state->mem + addr) = val;
    }
}

static inline uint8_t  mem_readb(struct ne2k_state * nic_state, uint32_t addr) {
	
    if ((addr < 32) || (addr >= NE2K_PMEM_START && addr < NE2K_MEM_SIZE)) {
        return nic_state->mem[addr];
    } else {
        return 0xff;
    }
}

static inline uint16_t mem_readw(struct ne2k_state * nic_state, uint32_t addr) {
    addr &= ~1;

    if ((addr < 32) || (addr >= NE2K_PMEM_START && addr < NE2K_MEM_SIZE)) {
        return *(uint16_t *)(nic_state->mem + addr);
    } else {
        return 0xffff;
    }
}

static uint32_t mem_readl(struct ne2k_state * nic_state, uint32_t addr) {
    addr &= ~1;

    if ((addr < 32) || (addr >= NE2K_PMEM_START && addr < NE2K_MEM_SIZE)) {
        return *(uint32_t *)(nic_state->mem + addr);
    } else {
        return 0xffffffff;
    }
}


static void dma_update( struct ne2k_state * nic_state, int len) {			
    struct ne2k_registers * regs = (struct ne2k_registers *)&(nic_state->context);

    regs->rsar += len;

    // wrap
    if (regs->rsar == regs->pgstop) {
        regs->rsar = regs->pgstart;
    }

    if (regs->rbcr <= len) {
        regs->rbcr = 0;
        regs->isr.rem_dma_done = 1;
        ne2k_update_irq(nic_state);
    } else {
        regs->rbcr -= len;
    }
}

static int ne2k_data_read(struct guest_info * core, 
			  uint16_t port, 
			  void * dst, 
			  uint_t length, 
			  void * private_data) {
    struct ne2k_state * nic_state = (struct ne2k_state *)private_data;
    uint32_t val;
    struct ne2k_registers * regs = (struct ne2k_registers *)&(nic_state->context);

    // current dma address
    uint32_t addr = regs->rsar;

    switch (length){
	case 1:
	    val = mem_readb(nic_state, addr);
	    break;
	case 2:
	    val = mem_readw(nic_state, addr);
	    break;
	case 4:
	    val = mem_readl(nic_state, addr);
	    break;
	default:
	    PrintError(VM_NONE, VCORE_NONE, "ne2k_data_read error: invalid length %d\n", length);
	    val = 0x0;
    }
    
    dma_update(nic_state, length);
    memcpy(dst, &val, length);

    PrintDebug(VM_NONE, VCORE_NONE, "NE2000 read: port:0x%x (%u bytes): 0x%x", port & 0x1f, length, val);

    return length;
}

static int ne2k_data_write(struct guest_info * core, 
			   uint16_t port, 
			   void * src, 
			   uint_t length, 
			   void * private_data) {
    struct ne2k_state * nic_state = (struct ne2k_state *)private_data;
    uint32_t val;
    struct ne2k_registers * regs = (struct ne2k_registers *)&(nic_state->context);
	
    uint32_t addr = regs->rsar;
	
    if (regs->rbcr == 0) {
	return length;
    }

    memcpy(&val, src, length);

    switch (length) {
	case 1:
	    mem_writeb(nic_state, addr, val);
	    break;
	case 2:
	    mem_writew(nic_state, addr, val);
	    break;
	case 4:
	    mem_writel(nic_state, addr, val);
	    break;
	default:
	    PrintError(VM_NONE, VCORE_NONE, "NE2000 port write error: invalid length %d\n", length);
    }
    
    dma_update(nic_state, length);

    PrintDebug(VM_NONE, VCORE_NONE, "NE2000: Write port:0x%x (%u bytes): 0x%x\n", port & 0x1f, length, val);
    
    return length;
}


static void ne2k_init_state(struct ne2k_state * nic_state) {


    /* Not sure what this is about....  */
    memset(nic_state->mem, 0xff, 32); 
	
    memcpy(nic_state->mem, nic_state->mac, ETH_ALEN);
    memset(nic_state->mcast_addr, 0xff, sizeof(nic_state->mcast_addr));
    nic_state->mem[14] = 0x57;
    nic_state->mem[15] = 0x57;

    /* initiate registers */
    nic_state->context.isr.reset_status = 1;
    nic_state->context.imr.val = 0x00;
    nic_state->context.cmd.val = 0x22;
}

static int reset_device(struct ne2k_state * nic_state) {  
    ne2k_init_state(nic_state);

    PrintDebug(VM_NONE, VCORE_NONE, "NE2000: Reset device\n");

    return 0;
}

//for 0xc11f port
static int ne2k_reset_port_read(struct guest_info * core, 
				uint16_t port, 
				void * dst, 
				uint_t length, 
				void * private_data) {
    struct ne2k_state * nic_state = (struct ne2k_state *)private_data;

    memset(dst, 0, length);
    reset_device(nic_state);

    return length;
}

static int ne2k_reset_port_write(struct guest_info * core, 
				 uint16_t port, 
				 void * src, 
				 uint_t length, 
				 void * private_data) {

    return length;
}



static int ne2k_cmd_write(struct guest_info * core, 
			  uint16_t port, 
			  void * src, 
			  uint_t length, 
			  void * private_data) {
    struct ne2k_state * nic_state = (struct ne2k_state *)private_data;
    struct ne2k_registers * regs = (struct ne2k_registers *)&(nic_state->context);

    if (length != 1) {
	PrintError(core->vm_info, core, "Invalid write length to NE2000 Command register\n");
	return -1;
    }

    regs->cmd.val = *(uint8_t *)src;

    if (!(regs->cmd.stop)) {
	regs->isr.reset_status = 0;
	
	// if ((send pkt) && (dma byte count == 0)) 
	if ((regs->cmd.rem_dma_cmd & 0x3) && (regs->rbcr == 0)) {
	    regs->isr.rem_dma_done = 1;
	    ne2k_update_irq(nic_state);
	}
	
	if (regs->cmd.tx_pkt) {
	    int offset = (regs->tpsr << 8);
	    
	    if (offset >= NE2K_PMEM_END) {
		offset -= NE2K_PMEM_SIZE;
	    }

	    if (offset + regs->tbcr <= NE2K_PMEM_END) {
		tx_one_pkt(nic_state, nic_state->mem + offset, regs->tbcr);
	    }

	    regs->tsr.val = 0;        /* clear the tx status reg */
	    regs->tsr.pkt_tx_ok = 1;  /* indicate successful tx */

	    regs->isr.pkt_tx = 1;     /* irq due to pkt tx */
	    regs->cmd.tx_pkt = 0;     /* reset cmd bit  */
	    
	    ne2k_update_irq(nic_state);
	}
    } else {
	/* stop the controller */
    }

    return length;
}

static int ne2k_cmd_read(struct guest_info * core, 
			 uint16_t port, 
			 void * dst, 
			 uint_t length, 
			 void * private_data) {
    struct ne2k_state * nic_state = (struct ne2k_state *)private_data;

    if (length != 1) {
	PrintError(core->vm_info, core, "Invalid read length to NE2000 Command register\n");
	return -1;
    }

    *(uint8_t *)dst = nic_state->context.cmd.val;

    PrintDebug(core->vm_info, core, "ne2k_read: port:0x%x  val: 0x%x\n", port, *(uint8_t *)dst);
    return length;
}

static int ne2k_std_write(struct guest_info * core, 
			  uint16_t port, 
			  void * src, 
			  uint_t length, 
			  void * private_data) {
    struct ne2k_state * nic_state = (struct ne2k_state *)private_data;
    struct ne2k_registers * regs = (struct ne2k_registers *)&(nic_state->context);
    int idx = port & 0x1f;
    uint8_t page = regs->cmd.pg_sel;

    if (length != 1){
	PrintError(core->vm_info, core, "NE2000 port write error: length %d port 0x%xnot equal to 1\n", length, port);  
	return -1;
    }

    uint8_t val = *(uint8_t *)src;
	
    PrintDebug(core->vm_info, core, "NE2000: write port:0x%x val: 0x%x\n", port, (uint8_t)val);
    
    if (page == 0) {
	switch (idx) {
	    case EN0_STARTPG:
		regs->pgstart = val;
		break;
	    case EN0_STOPPG:
		regs->pgstop = val;
		break;
	    case EN0_BOUNDARY:
		regs->boundary = val;
		break;
	    case EN0_TPSR:
		regs->tpsr = val;
		break;
	    case EN0_TCNTLO:
		regs->tbcr0 = val;
		break;
	    case EN0_TCNTHI:
		regs->tbcr1 = val;
		break;
	    case EN0_ISR:
		regs->isr.val &= ~(val & 0x7f);
		ne2k_update_irq(nic_state);
		break;
	    case EN0_RSARLO:
		regs->rsar0 = val;
		break;
	    case EN0_RSARHI:
		regs->rsar1 = val;
		break;
	    case EN0_RCNTLO:
		regs->rbcr0 = val;
		break;
	    case EN0_RCNTHI:
		regs->rbcr1 = val;
		break;
	    case EN0_RXCR:
		regs->rcr.val = val;
		break;
	    case EN0_TXCR:
		regs->tcr.val = val;
		break;
	    case EN0_DCFG:
		regs->dcr.val = val;
		break;	
	    case EN0_IMR:
		regs->imr.val = val;
		ne2k_update_irq(nic_state);
		break;

	    default:
		PrintError(core->vm_info, core, "NE2000 port write error: invalid port:0x%x\n", port);
		return -1;
	}
    } else if (page == 1) {
	switch (idx) {
	    case EN1_PHYS ... EN1_PHYS + ETH_ALEN -1:
		nic_state->mac[port - EN1_PHYS] = val;
		break;
	    case EN1_CURPAG:
		regs->curpag = val;
		break;
	    case EN1_MULT ... EN1_MULT + 7:
		nic_state->mcast_addr[port - EN1_MULT] = val;
		break;
		
	    default:
		PrintError(core->vm_info, core, "NE2000 write port error: invalid port:0x%x\n", port);
		return -1;
	}
    } else if (page == 2) {
	switch (idx) {
	    case EN2_LDMA0:
		regs->clda0 = val;
		break;
	    case EN2_LDMA1:
		regs->clda1 = val;
		break;
	    case EN2_RNPR:
		regs->rnpp = val;
		break;
	    case EN2_LNRP:
		regs->lnpp = val;
		break;
	    case EN2_ACNT0:
		regs->addcnt0 = val;
		break;
	    case EN2_ACNT1: 
		regs->addcnt1 = val;
		break;
		
	    default:
		PrintError(core->vm_info, core, "NE2000 write port error: invalid port:0x%x\n", port);
		return -1;
	}
    } else {
	PrintError(core->vm_info, core, "NE2000: Invalid Register Page Value\n");
	return -1;
    }


    return length;
	
}

static int ne2k_std_read(struct guest_info * core, 
			 uint16_t port, 
			 void * dst, 
			 uint_t length, 
			 void * private_data) {
    struct ne2k_state * nic_state = (struct ne2k_state *)private_data;
    struct ne2k_registers * regs = (struct ne2k_registers *)&(nic_state->context);
    uint16_t index = port & 0x1f;
    uint8_t page = regs->cmd.pg_sel;

    if (length > 1) {
	PrintError(core->vm_info, core, "ne2k_read error: length %d\n", length);
	return length;
    }

    if (page == 0) {
	switch (index) {		
	    case EN0_CLDALO:
		*(uint8_t *)dst = regs->clda0;
		break;
	    case EN0_CLDAHI:
		*(uint8_t *)dst = regs->clda1;
		break;
	    case EN0_BOUNDARY:
		*(uint8_t *)dst = regs->boundary;
		break;
	    case EN0_TSR:
		*(uint8_t *)dst = regs->tsr.val;
		break;
	    case EN0_NCR:
		*(uint8_t *)dst = regs->ncr;
		break;
	    case EN0_FIFO:
		*(uint8_t *)dst = regs->fifo;
		break;
	    case EN0_ISR:
		*(uint8_t *)dst = regs->isr.val;
		ne2k_update_irq(nic_state);
		break;
	    case EN0_CRDALO:
		*(uint8_t *)dst = regs->crda0;
		break;
	    case EN0_CRDAHI:
		*(uint8_t *)dst = regs->crda1;
		break;
	    case EN0_RSR:
		*(uint8_t *)dst = regs->rsr.val;
		break;
	    case EN0_COUNTER0:
		*(uint8_t *)dst = regs->cntr0;
		break;
	    case EN0_COUNTER1:
		*(uint8_t *)dst = regs->cntr1;
		break;	
	    case EN0_COUNTER2:
		*(uint8_t *)dst = regs->cntr2;
		break;
		
	    default:
		PrintError(core->vm_info, core, "NE2000 port read error: invalid port:0x%x\n", port);
		return -1;
	}
    } else if (page == 1) {
	switch (index) {
	    case EN1_PHYS ... EN1_PHYS + ETH_ALEN -1:
		*(uint8_t *)dst = nic_state->mac[index - EN1_PHYS];
		break;
	    case EN1_CURPAG:
		*(uint8_t *)dst = regs->curpag;
		break;
	    case EN1_MULT ... EN1_MULT + 7:
		*(uint8_t *)dst = nic_state->mcast_addr[index - EN1_MULT];
		break;
		
	    default:
		PrintError(core->vm_info, core, "ne2k_read error: invalid port:0x%x\n", port);
		return -1;
	}
    } else if (page == 2) {
	switch (index) {
	    case EN2_STARTPG:
		*(uint8_t *)dst = regs->pgstart;
		break;
	    case EN2_STOPPG:
		*(uint8_t *)dst = regs->pgstop;
		break;
	    case EN2_RNPR:
		*(uint8_t *)dst = regs->rnpp;
		break;
	    case EN2_LNRP:
		*(uint8_t *)dst = regs->lnpp;
		break;
	    case EN2_TPSR:
		*(uint8_t *)dst = regs->tpsr;
		break;
	    case EN2_ACNT0:
		*(uint8_t *)dst = regs->addcnt0;
		break;
	    case EN2_ACNT1: 
		*(uint8_t *)dst = regs->addcnt1;
		break;
	    case EN2_RCR:
		*(uint8_t *)dst = regs->rcr.val;
		break;
	    case EN2_TCR:
		*(uint8_t *)dst = regs->tcr.val;
		break;
	    case EN2_DCR:
		*(uint8_t *)dst = regs->dcr.val;
		break;
	    case EN2_IMR:
		*(uint8_t *)dst = regs->imr.val;
		break;
	    default:
		PrintError(core->vm_info, core, "NE2000 port read error: invalid port:0x%x\n", port);
		return -1;
	}
    } else {
	PrintError(core->vm_info, core, "NE2000 port read: Invalid Register Page Value\n");
	return -1;
    }

    PrintDebug(core->vm_info, core, "NE2000 port read: port:0x%x  val: 0x%x\n", port, *(uint8_t *)dst);

    return length;
}



static int ne2k_pci_write(struct guest_info * core, 
			  uint16_t port, 
			  void * src, 
			  uint_t length, 
			  void * private_data) {
    uint16_t idx = port & 0x1f;
    int ret;

    switch (idx) {
	case NE2K_CMD_OFFSET:
	    ret =  ne2k_cmd_write(core, port, src, length, private_data);
	    break;
	case NE2K_CMD_OFFSET+1 ... NE2K_CMD_OFFSET+15:
	    ret = ne2k_std_write(core, port, src, length, private_data);
	    break;
	case NE2K_DATA_OFFSET:
	    ret = ne2k_data_write(core, port, src, length, private_data);
	    break;
	case NE2K_RESET_OFFSET:
	    ret = ne2k_reset_port_write(core, port, src, length, private_data);
	    break;

	default:
	    PrintError(core->vm_info, core, "NE2000 port write error: invalid port:0x%x\n", port);
	    return -1;
    }

    return ret;
}

static int ne2k_pci_read(struct guest_info * core, 
			 uint16_t port, 
			 void * dst, 
			 uint_t length, 
			 void * private_data) {
    uint16_t idx = port & 0x1f;
    int ret;

    switch (idx) {
	case NE2K_CMD_OFFSET:
	    ret =  ne2k_cmd_read(core, port, dst, length, private_data);
	    break;
	case NE2K_CMD_OFFSET+1 ... NE2K_CMD_OFFSET+15:
	    ret = ne2k_std_read(core, port, dst, length, private_data);
	    break;
	case NE2K_DATA_OFFSET:
	    ret = ne2k_data_read(core, port, dst, length, private_data);
	    break;
	case NE2K_RESET_OFFSET:
	    ret = ne2k_reset_port_read(core, port, dst, length, private_data);
	    break;

	default:
	    PrintError(core->vm_info, core, "NE2000 port read error: invalid port:0x%x\n", port);
	    return -1;
    }

    return ret;


}

static int pci_config_update(struct pci_device * pci_dev,
                        uint32_t reg_num,
			     void * src, 
			     uint_t length,
			     void * private_data) {
    PrintDebug(VM_NONE, VCORE_NONE, "PCI Config Update\n");

    /* Do we need this? */

    return 0;
}


static int register_dev(struct ne2k_state * nic_state) 
{
    int i;

    if (nic_state->pci_bus != NULL) {
	struct v3_pci_bar bars[6];
	struct pci_device * pci_dev = NULL;

  	PrintDebug(VM_NONE, VCORE_NONE, "NE2000: PCI Enabled\n");

	for (i = 0; i < 6; i++) {
	    bars[i].type = PCI_BAR_NONE;
	}

	bars[0].type = PCI_BAR_IO;
	bars[0].default_base_port = -1;
	bars[0].num_ports = 256;

	bars[0].io_read = ne2k_pci_read;
	bars[0].io_write = ne2k_pci_write;
       bars[0].private_data = nic_state;

	pci_dev = v3_pci_register_device(nic_state->pci_bus, PCI_STD_DEVICE, 0, -1, 0, 
					 "NE2000", bars,
					 pci_config_update, NULL, NULL, NULL, nic_state);


    	if (pci_dev == NULL) {
	    PrintError(VM_NONE, VCORE_NONE, "NE2000: Could not register PCI Device\n");
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
	PrintDebug(VM_NONE, VCORE_NONE, "NE2000: Not attached to PCI\n");

	v3_dev_hook_io(nic_state->dev, NIC_REG_BASE_PORT , &ne2k_cmd_read, &ne2k_cmd_write);

	for (i = 1; i < 16; i++){	
	    v3_dev_hook_io(nic_state->dev, NIC_REG_BASE_PORT + i, &ne2k_std_read, &ne2k_std_write);
	}

	v3_dev_hook_io(nic_state->dev, NIC_REG_BASE_PORT + NE2K_DATA_OFFSET, &ne2k_data_read, &ne2k_data_write);
	v3_dev_hook_io(nic_state->dev, NIC_REG_BASE_PORT + NE2K_RESET_OFFSET, &ne2k_reset_port_read, &ne2k_reset_port_write);
    }


    return 0;
}

static int connect_fn(struct v3_vm_info * info, 
		      void * frontend_data, 
		      struct v3_dev_net_ops * ops, 
		      v3_cfg_tree_t * cfg, 
		      void * private_data) {
    struct ne2k_state * nic_state = (struct ne2k_state *)frontend_data;

    ne2k_init_state(nic_state);
    register_dev(nic_state);

    nic_state->net_ops = ops;
    nic_state->backend_data = private_data;	

    ops->recv = ne2k_rx;
    ops->poll = NULL;   
    ops->config.frontend_data = nic_state;
    ops->config.fnt_mac = nic_state->mac;

    return 0;
}


static int ne2k_free(struct ne2k_state * nic_state) {
    int i;

    /* dettached from backend */

    if(nic_state->pci_bus == NULL){
    	for (i = 0; i < 16; i++){		
  	    v3_dev_unhook_io(nic_state->dev, NIC_REG_BASE_PORT + i);
    	}
    
       v3_dev_unhook_io(nic_state->dev, NIC_REG_BASE_PORT + NE2K_DATA_OFFSET);
       v3_dev_unhook_io(nic_state->dev, NIC_REG_BASE_PORT + NE2K_RESET_OFFSET);
    }else {
       /* unregistered from PCI? */
    }

    V3_Free(nic_state);
	
    return 0;
}


static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))ne2k_free,
};


static int ne2k_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct vm_device * pci_bus = v3_find_dev(vm, v3_cfg_val(cfg, "bus"));
    struct ne2k_state * nic_state = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");
    char * macstr = v3_cfg_val(cfg, "mac");

    nic_state  = (struct ne2k_state *)V3_Malloc(sizeof(struct ne2k_state));

    if (!nic_state) {
	PrintError(vm, VCORE_NONE, "Cannot allocate in init\n");
	return -1;
    }

    memset(nic_state, 0, sizeof(struct ne2k_state));

    nic_state->pci_bus = pci_bus;
    nic_state->vm = vm;

    if (macstr != NULL && !str2mac(macstr, nic_state->mac)) {
	PrintDebug(vm, VCORE_NONE, "NE2000: Mac specified %s\n", macstr);
    }else {
    	PrintDebug(vm, VCORE_NONE, "NE2000: MAC not specified\n");
	random_ethaddr(nic_state->mac);
    }

    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, nic_state);

    if (dev == NULL) {
	PrintError(vm, VCORE_NONE, "NE2000: Could not attach device %s\n", dev_id);
	V3_Free(nic_state);
	return -1;
    }

    nic_state->dev = dev;

    if (v3_dev_add_net_frontend(vm, dev_id, connect_fn, (void *)nic_state) == -1) {
	PrintError(vm, VCORE_NONE, "NE2000: Could not register %s as net frontend\n", dev_id);
	v3_remove_device(dev);
	V3_Free(nic_state);
	return -1;
    }
	    
    return 0;
}

device_register("NE2000", ne2k_init)
