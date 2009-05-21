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
 
/*
* Virtual NE2K Network Card 
*/

#include <devices/pci.h>
#include <devices/ne2k.h>
#include <palacios/vmm.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm_io.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_string.h>

#ifndef DEBUG_NE2K
#undef PrintDebug
#define PrintDebug(fmts, args...)
#endif




#define NE2K_DEFAULT_IRQ        11

#define MAX_ETH_FRAME_SIZE      1514


// What the hell is this crap?
#define NE2K_PMEM_SIZE          (32 * 1024)
#define NE2K_PMEM_START         (16 * 1024)
#define NE2K_PMEM_END           (NE2K_PMEM_SIZE + NE2K_PMEM_START)
#define NE2K_MEM_SIZE           NE2K_PMEM_END

#define NIC_REG_BASE_PORT       0xc100  	//Command register (for all pages) 
#define NIC_DATA_PORT 	        0xc110  	//Data read/write port
#define NIC_RESET_PORT 	        0xc11f  	//Data read/write port

// Page 0 registers
#define EN0_CLDALO		0x01	//Low byte of current local dma addr  RD 
#define EN0_STARTPG	 	0x01  	//Starting page of ring bfr WR 
#define EN0_CLDAHI	 	0x02	//High byte of current local dma addr  RD 
#define EN0_STOPPG	 	0x02    //Ending page +1 of ring bfr WR 
#define EN0_BOUNDARY	        0x03	//Boundary page of ring bfr RD WR 
#define EN0_TSR			0x04	//Transmit status reg RD 
#define EN0_TPSR		0x04 	//Transmit starting page WR 
#define EN0_NCR			0x05 	//Number of collision reg RD 
#define EN0_TCNTLO		0x05 	//Low  byte of tx byte count WR 
#define EN0_FIFO		0x06 	//FIFO RD 
#define EN0_TCNTHI		0x06	//High byte of tx byte count WR 
#define EN0_ISR			0x07 	//Interrupt status reg RD WR 
#define EN0_CRDALO		0x08 	//low byte of current remote dma address RD 
#define EN0_RSARLO		0x08 	//Remote start address reg 0 
#define EN0_CRDAHI		0x09 	//high byte, current remote dma address RD 
#define EN0_RSARHI		0x09 	//Remote start address reg 1 
#define EN0_RCNTLO		0x0a 	//Remote byte count reg WR 
#define EN0_RTL8029ID0	        0x0a 	//Realtek ID byte #1 RD 
#define EN0_RCNTHI		0x0b 	//Remote byte count reg WR 
#define EN0_RTL8029ID1	        0x0b	//Realtek ID byte #2 RD 
#define EN0_RSR			0x0c 	//rx status reg RD 
#define EN0_RXCR		0x0c 	//RX configuration reg WR 
#define EN0_TXCR		0x0d 	//TX configuration reg WR 
#define EN0_COUNTER0	        0x0d 	//Rcv alignment error counter RD 
#define EN0_DCFG		0x0e 	//Data configuration reg WR 
#define EN0_COUNTER1	        0x0e	//Rcv CRC error counter RD 
#define EN0_IMR			0x0f	//Interrupt mask reg WR 
#define EN0_COUNTER2	        0x0f	//Rcv missed frame error counter RD 

//Page 1 registers
#define EN1_PHYS        	0x01
#define EN1_CURPAG     	        0x07
#define EN1_MULT       	        0x08

//Page 2 registers
#define EN2_STARTPG	 	0x01	//Starting page of ring bfr RD 
#define EN2_STOPPG		0x02	//Ending page +1 of ring bfr RD 
#define EN2_LDMA0  		0x01   	//Current Local DMA Address 0 WR 
#define EN2_LDMA1  		0x02   	//Current Local DMA Address 1 WR 
#define EN2_RNPR  		0x03   	//Remote Next Packet Pointer RD WR 
#define EN2_TPSR  		0x04    	//Transmit Page Start Address RD 
#define EN2_LNRP  		0x05   	//Local Next Packet Pointer RD WR 
#define EN2_ACNT0  		0x06  	//Address Counter Upper WR 
#define EN2_ACNT1  		0x07  	//Address Counter Lower WR 
#define EN2_RCR  		0x0c  	//Receive Configuration Register RD 
#define EN2_TCR  		0x0d  	//Transmit Configuration Register RD 
#define EN2_DCR  		0x0e  	//Data Configuration Register RD 
#define EN2_IMR  		0x0f 	//Interrupt Mask Register RD 

//Page 3 registers
#define EN3_CONFIG0	 	0x03
#define EN3_CONFIG1	 	0x04
#define EN3_CONFIG2	 	0x05
#define EN3_CONFIG3	 	0x06



typedef enum {NIC_READY, NIC_REG_POSTED} nic_state_t;

struct cmd_reg {
    union {
	uint8_t val;
	struct {
	    uint8_t stop        : 1;
	    uint8_t start       : 1;
	    uint8_t tx_pkt      : 1;
	    uint8_t rem_dma_cmd : 3; // 0=Not allowed, 1=Read, 2=Write, 3=Send Pkt, 4=Abort/Complete DMA
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
	    uint8_t phy_match        : 1;   // 0=Physical Addr Match, 1=MCAST/BCAST Addr Match
	    uint8_t rx_disabled      : 1;
	    uint8_t deferring        : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct ne2k_context {
    struct guest_info * vm;

    nic_state_t dev_state;

    // Registers
    struct cmd_reg cmd;
    struct intr_status_reg isr;
    struct intr_mask_reg imr;
    struct data_cfg_reg dcr;
    struct tx_cfg_reg tcr;
    struct tx_status_reg tsr;
    struct rx_cfg_reg rcr;
    struct rx_status_reg rsr;  

    uint8_t      pgstart;      // page start reg
    uint8_t      pgstop;       // page stop reg
    uint8_t      boundary;     // boundary ptr
    uint8_t      tpsr;         // tx page start addr
    uint8_t      ncr;          // number of collisions
    uint8_t      fifo;         // FIFO...

    uint8_t      curpag;       // current page
    uint8_t      rnpp;         // rem next pkt ptr
    uint8_t      lnpp;         // local next pkt ptr

    uint8_t      cntr0;        // counter 0 (frame alignment errors)
    uint8_t      cntr1;        // counter 1 (CRC Errors)
    uint8_t      cntr2;        // counter 2 (missed pkt errors)

    union {                    // current local DMA Addr
	uint16_t     clda;
	struct {
	    uint8_t clda0;
	    uint8_t clda1;
	} __attribute__((packed));
    } __attribute__((packed));


    union {                   // current remote DMA addr
	uint16_t     crda;
	struct {
	    uint8_t crda0;
	    uint8_t crda1;
	} __attribute__((packed));
    } __attribute__((packed));


    union {                   // Remote Start Addr Reg
	uint16_t     rsar;
	struct {
	    uint8_t rsar0;
	    uint8_t rsar1;
	} __attribute__((packed));
    } __attribute__((packed));


    union {                    // TX Byte count Reg
	uint16_t     tbcr;
	struct {
	    uint8_t tbcr0;
	    uint8_t tbcr1;
	} __attribute__((packed));
    } __attribute__((packed));

    union {                    // Remote Byte count Reg
	uint16_t     rbcr;
	struct {
	    uint8_t rbcr0;
	    uint8_t rbcr1;
	} __attribute__((packed));
    } __attribute__((packed));



    union {                    // Address counter?
	uint16_t     addcnt;
	struct {
	    uint8_t addcnt0;
	    uint8_t addcnt1;
	} __attribute__((packed));
    } __attribute__((packed));





    uint8_t      mcast_addr[8];  // multicast mask array 
    uint8_t      mac_addr[6];    // MAC Addr




    uint8_t mem[NE2K_MEM_SIZE];

    struct pci_device * pci_dev;
    struct vm_device * pci_bus;
};

#define compare_mac(src, dst) !memcmp(src, dst, 6)

#ifdef DEBUG_NE2K
static void dump_state(struct vm_device * dev) {
    struct ne2k_context *nic_state = (struct ne2k_context *)dev->private_data;
    int i;


    PrintDebug("====NE2000: Dumping state Begin ==========\n");
    PrintDebug("Registers:\n");

    // JRL: Dump Registers

    PrintDebug("Memory:\n");	

    for(i = 0; i < 32; i++) {
        PrintDebug("0x%02x ", nic_state->mem[i]);
    } 

    PrintDebug("\n");
    PrintDebug("====NE2000: Dumping state End==========\n");
}
#endif



static int ne2k_update_irq(struct vm_device *dev) {
    struct ne2k_context * nic_state = (struct ne2k_context *)dev->private_data;
    struct pci_device * pci_dev = nic_state->pci_dev;
    int irq_line = 0;

    if (pci_dev == NULL){
	PrintDebug("Ne2k: Device %p is not attached to any PCI Bus\n", nic_state);
	irq_line = NE2K_DEFAULT_IRQ;
    } else {
    	irq_line = pdev->config_header.intr_line;
    }
	    
    if (irq_line == 0){
	PrintError("Ne2k: IRQ_LINE: %d\n", irq_line);
	return -1;
    }

    PrintDebug("Ne2k: RaiseIrq: isr: 0x%02x imr: 0x%02x\n", nic_state->isr.val, nic_state->imr.val);
    PrintDebug("ne2k_update_irq: irq_line: %d\n", irq_line);


    // The top bit of the ISR/IMR is reserved and does not indicate and irq event
    // We mask the bit out of the irq pending check
    if ((nic_state->isr.val & nic_state->imr.val) & 0x7f) {
    	v3_raise_irq(nic_state->vm, irq_line);
	PrintDebug("Ne2k: RaiseIrq: isr: 0x%02x imr: 0x%02x\n", nic_state->isr.val, nic_state->imr.val);
    }

    return 0;
}



static void ne2k_init_state(struct vm_device * dev) {
    struct ne2k_context * nic_state = (struct ne2k_context *)dev->private_data;
    int i;
    uchar_t mac[6] = {0x52, 0x54, 0x0, 0x12, 0x34, 0x60};

    nic_state->vm = dev->vm;

    nic_state->isr.reset = 1;
    nic_state->imr.val = 0x00;
    nic_state->cmd.val = 0x22;

    for (i = 0; i < 5; i++) {
	nic_state->mac_addr[i] = mac[i];
    }

    memset(nic_state->mcast_addr, 0xff, sizeof(nic_state->mcast_addr));

    // Not sure what this is about....
    memset(nic_state->mem, 0xff, 32); 

    memcpy(nic_state->mem, nic_state->mac_addr, 6);
    nic_state->mem[14] = 0x57;
    nic_state->mem[15] = 0x57;

#ifdef DEBUG_NE2K
    dump_state(dev);
#endif

}

static int ne2k_send_packet(struct vm_device *dev, uchar_t *pkt, int length) {
    int i;
  
    PrintDebug("\nNe2k: Sending Packet\n");

    for (i = 0; i < length; i++) {
	PrintDebug("%x ",pkt[i]);
    }

    PrintDebug("\n");
	
    PrintError("Implement Send Packet interface\n");

    return -1;
}

static int ne2k_rxbuf_full(struct vm_device *dev) {
    int empty;
    int index;
    int boundary;
    struct ne2k_context * nic_state = (struct ne2k_context *)dev->private_data;

    index = nic_state->curpag << 8;
    boundary = nic_state->boundary << 8;

    if (index < boundary) {
        empty = boundary - index;
    } else {
        empty = ((nic_state->pgstop - nic_state->pgstart) << 8) - (index - boundary);
    }

    if (empty < (MAX_ETH_FRAME_SIZE + 4)) {
        return 1;
    }

    return 0;
}

#define MIN_BUF_SIZE 60


// This needs to be completely redone...
static void ne2k_receive(struct vm_device * dev, const uchar_t * pkt, int length) {
    struct ne2k_context * nic_state = (struct ne2k_context *)dev->private_data;
    uchar_t * p;
    uint32_t total_len;
    uint32_t next;
    uint32_t len;
    uin32_t index;
    uint32_t empty;
    uchar_t buf[60];
    uint32_t start;
    uint32_t stop;

    start = nic_state->pgstart << 8;
    stop = nic_state->pgstop << 8;
   
    if (nic_state->cmd.stop) {
	return;
    }

    if (ne2k_rxbuf_full(dev)) {
	PrintError("Ne2k: received buffer overflow\n");
	return;
    }


    //packet too small, expand it
    if (length < MIN_BUF_SIZE) {
        memcpy(buf, pkt, length);
        memset(buf + length, 0, MIN_BUF_SIZE - length);
        pkt = buf;
        length = MIN_BUF_SIZE;
    }

    index = nic_state->curpag << 8;

    //header, 4 bytes
    total_len = length + 4;

    //address for next packet (4 bytes for CRC)
    next = index + ((total_len + 4 + 255) & ~0xff);

    if (next >= stop) {
        next -= (stop - start);
    }

    p = nic_state->mem + index;
    nic_state->rsr.val = 0;
    nic_state->rsr.rx_pkt_ok = 1;

    if (pkt[0] & 0x01) {
        nic_state->rsr.phy = 1;
    }

    p[0] = nic_state->rsr.val;
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

    nic_state->curpag = next >> 8;

    nic_state->isr.pkt_rx = 1;
    ne2k_update_irq(dev);
}




static int netif_input(uchar_t *pkt, uint_t size) {
    struct ne2k_context * nic_state;
    static const uchar_t brocast_mac[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    int i;
  
#ifdef DEBUG_NE2K
    PrintDebug("\nNe2k: Packet Received:\nSource:");
    for (i = 6; i < 12; i++) {
  	PrintDebug("%x ", pkt[i]);
    }

    PrintDebug("\n");

    for(i = 0; i < size; i++) {
  	PrintDebug("%x ", pkt[i]);
    }    
    PrintDebug("\n");
#endif    


    if (nic_state->rcr.prom_phys_enable == 1) {
	//promiscuous mode
	ne2k_receive(ne2ks[i], pkt, size);
    } else if (compare_mac(pkt,  brocast_mac) && (nic_state->rcr.bcast_ok)) {
	//broadcast packet
	ne2k_receive(ne2ks[i], pkt, size);
    } else if ((pkt[0] & 0x01) && (nic_state->rcr.mcast_ok)) {
	//TODO: multicast packet
	ne2k_receive(ne2ks[i], pkt, size);
    } else if (compare_mac(pkt, nic_state->mac_addr)) {
	ne2k_receive(ne2ks[i], pkt, size);
    }
    
    return 0;
}


static void ne2k_mem_writeb(struct ne2k_context * nic_state, uint32_t addr, uint32_t val) {
    uchar_t tmp;

    tmp = (uchar_t) (val & 0x000000ff);

    if ((addr < 32) || (addr >= NE2K_PMEM_START && addr < NE2K_MEM_SIZE)) {
        nic_state->mem[addr] = tmp;
    }

    PrintDebug("wmem addr: %x val: %x\n", addr, val);
}

static void ne2k_mem_writew(struct ne2k_context * nic_state, 
			    uint32_t addr,
			    uint32_t val) {
    addr &= ~1; //XXX: check exact behaviour if not even

    if ((addr < 32) || (addr >= NE2K_PMEM_START && addr < NE2K_MEM_SIZE)) {
        *(ushort_t *)(nic_state->mem + addr) = cpu2le16(val);
    }

    PrintDebug("wmem addr: %x val: %x\n", addr, val);
}

static void ne2k_mem_writel(struct ne2k_context *nic_state,
			    uint32_t addr,
			    uint32_t val) {
    addr &= ~1; // XXX: check exact behaviour if not even

    if ((addr < 32) || (addr >= NE2K_PMEM_START && addr < NE2K_MEM_SIZE)) {
        *(uint32_t *)(nic_state->mem + addr) = cpu2le32(val);
    }

    PrintDebug("wmem addr: %x val: %x\n", addr, val);
}

static uchar_t  ne2k_mem_readb(struct ne2k_context *nic_state, uint32_t addr) {
    PrintDebug("rmem addr: %x\n", addr);
	
    if ((addr < 32) || (addr >= NE2K_PMEM_START && addr < NE2K_MEM_SIZE)) {
        return nic_state->mem[addr];
    } else {
        return 0xff;
    }
}

static ushort_t ne2k_mem_readw(struct ne2k_context *nic_state, uint32_t addr) {
    PrintDebug("rmem addr: %x\n", addr);
	
    addr &= ~1; //XXX: check exact behaviour if not even 

    if ((addr < 32) || (addr >= NE2K_PMEM_START && addr < NE2K_MEM_SIZE)) {
        return (ushort_t)le16_to_cpu((ushort_t *)(nic_state->mem + addr));
    } else {
        return 0xffff;
    }
}

static uint32_t ne2k_mem_readl(struct ne2k_context *nic_state, uint32_t addr) {
    PrintDebug("rmem addr: %x\n", addr);

    addr &= ~1; //XXX: check exact behaviour if not even

    if ((addr < 32) || (addr >= NE2K_PMEM_START && addr < NE2K_MEM_SIZE)) {
        return (uint32_t)le32_to_cpu((uint32_t *)(nic_state->mem + addr));
    } else {
        return 0xffffffff;
    }
}


static void ne2k_dma_update(struct vm_device *dev, int len) {		
    struct ne2k_context * nic_state = (struct ne2k_context *)dev->private_data;
	
    nic_state->rsar += len;

    // wrap
    if (nic_state->rsar == nic_state->pgstop) {
        nic_state->rsar = nic_state->pgstart;
    }

    if (nic_state->rbcr <= len) {
        nic_state->rbcr = 0;
        nic_state->isr.rem_dma_done = 1;
        ne2k_update_irq(dev);
    } else {
        nic_state->rbcr -= len;
    }
}


//for data port read/write
static int ne2k_data_read(ushort_t port, void * dst, uint_t length, struct vm_device *dev) {
    uint32_t val;
    struct ne2k_context * nic_state = (struct ne2k_context *)dev->private_data;
    
    // current dma address
    uint32_t addr = nic_state->rsar;

    switch (length){
	case 1:
	    val = ne2k_mem_readb(nic_state, addr);
	    break;
	case 2:
	    val = ne2k_mem_readw(nic_state, addr);
	    break;
	case 4:
	    val = ne2k_mem_readl(nic_state, addr);
	    break;
	default:
	    PrintError("ne2k_data_read error: invalid length %d\n", length);
	    val = 0x0;
    }
    
    ne2k_dma_update(dev, length);

    memcpy(dst, &val, length);

    PrintDebug("ne2k_read: port:0x%x (%u bytes): 0x%x", port & 0x1f, length, val);

    return length;
}

static int ne2k_data_write(ushort_t port, void * src, uint_t length, struct vm_device *dev) {
    uint32_t val;
    struct ne2k_context * nic_state = (struct ne2k_context *)dev->private_data;
    uint32_t addr = nic_state->rsar;

    if (nic_state->rbcr == 0) {
	return length;
    }

    memcpy(&val, src, length);

    switch (length) {
	case 1:
	    ne2k_mem_writeb(nic_state, addr, val);
	    break;
	case 2:
	    ne2k_mem_writew(nic_state, addr, val);
	    break;
	case 4:
	    ne2k_mem_writel(nic_state, addr, val);
	    break;
	default:
	    PrintError("nic_data_write error: invalid length %d\n", length);
    }
    
    ne2k_dma_update(dev, length);

    PrintDebug("ne2k_write: port:0x%x (%u bytes): 0x%x\n", port & 0x1f, length, val);
    
    return length;
}

static int ne2k_reset_device(struct vm_device * dev) {  
    PrintDebug("vnic: reset device\n");

    init_ne2k_context(dev);

    return 0;
}


//for 0xc11f port
static int ne2k_reset_port_read(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
    PrintDebug("ne2k_read: port:0x%x (%u bytes): 0x%x\n", port, length, val);

    memset(dst, 0, length);
    ne2k_reset_device(dev);

    return length;
}

static int ne2k_reset_port_write(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    PrintDebug("ne2k_write: port:0x%x (%u bytes): 0x%x\n", port, length, val);			
    return length;
}



static int ne2k_cmd_write(uint16_t port, void * src, uint_t length, struct vm_device * dev) {
    struct ne2k_context * nic_state = (struct ne2k_context *)dev->private_data;

    if (length != 1) {
	PrintError("Invalid write length to ne2k Command register\n");
	return -1;
    }

    nic_state->cmd.val = *(uint8_t *)src;

    if (!(nic_state->cmd.stop)) {
	nic_state->isr.reset = 0;
	

	// if ((send pkt) && (dma byte count == 0)) 
	if ((nic_state.rem_dma_cmd & 0x3) && (nic_state->rbcr == 0)) {
	    nic_state->isr.rem_dma_done = 1;
	    ne2k_update_irq(dev);
	}
	
	if (nic_state->cmd.tx_pkt) {
	    int offset = (nic_state->tpsr << 8);
	    
	    if (offset >= NE2K_PMEM_END) {
		offset -= NE2K_PMEM_SIZE;
	    }

	    if (offset + nic_state->tbcr <= NE2K_PMEM_END) {
		ne2k_send_packet(dev, nic_state->mem + offset, nic_state->tbcr);
	    }

	    
	    nic_state->tsr.val = 0;        // clear the tx status reg
	    nic_state->tsr.pkt_tx_ok = 1;  // indicate successful tx

	    nic_state->isr.pkt_tx = 1;     // irq due to pkt tx
	    nic_state->cmd.tx_pkt = 0;     // reset cmd bit
	    ne2k_update_irq(dev);
	}
    } else {
	// stop the controller
    }

    return length;
}

static int ne2k_cmd_read(uint16_t port, void * src, uint_t length, struct vm_device * dev) {
    *(uint8_t *)dst = nic_state->cmd.val;

    PrintDebug("ne2k_read: port:0x%x  val: 0x%x\n", port, *(uint8_t *)dst);
    return length;
}

static int ne2k_std_write(uint16_t port, void * src, uint_t length, struct vm_device * dev) {
    struct ne2k_context * nic_state = (struct ne2k_context *)dev->private_data;
    int index = port & 0x1f;
    uint8_t page = nic_state->cmd.pg_sel;

    if (length != 1
	PrintError("ne2k_write error: length %d\n", length);  
	return -1;
    }

    PrintDebug("ne2k_write: port:0x%x  val: 0x%x\n", port, (int)val);
    

    if (page == 0) {
	switch (port) {
	    case EN0_STARTPG:
		nic_state->pgstart = val;
		break;
	    case EN0_STOPPG:
		nic_state->pgstop = val;
		break;
	    case EN0_BOUNDARY:
		nic_state->boundary = val;
		break;
	    case EN0_TPSR:
		nic_state->tpsr = val;
		break;
	    case EN0_TCNTLO:
		nic_state->tbcr0 = val;
		break;
	    case EN0_TCNTHI:
		nic_state->tbcr1 = val;
		break;
	    case EN0_ISR:
		nic_state->isr.val &= ~(val & 0x7f);
		ne2k_update_irq(dev);
		break;
	    case EN0_RSARLO:
		nic_state->rsar0 = val;
		break;
	    case EN0_RSARHI:
		nic_state->rsar1 = val;
		break;
	    case EN0_RCNTLO:
		nic_state->rbcr0 = val;
		break;
	    case EN0_RCNTHI:
		nic_state->rbcr1 = val;
		break;
	    case EN0_RXCR:
		nic_state->rcr.val = val;
		break;
	    case EN0_TXCR:
		nic_state->tcr.val = val;
		break;
	    case EN0_DCFG:
		nic_state->dcr.val = val;
		break;	
	    case EN0_IMR:
		nic_state->imr.val = val;
		//PrintError("ne2k_write error: write IMR:0x%x\n", (int)val);
		ne2k_update_irq(dev);
		break;
	    default:
		PrintError("ne2k_write error: invalid port:0x%x\n", port);
		return -1;
	}
    } else if (page == 1) {
	switch (port) {
	    case EN1_PHYS ... EN1_PHYS + 5:
		nic_state->mac_addr[port - EN1_PHYS] = val;
		break;
	    case EN1_CURPAG:
		nic_state->curpag = val;
		break;
	    case EN1_MULT ... EN1_MULT + 7:
		// PrintError("ne2k_write error: write EN_MULT:0x%x\n", (int)val);
		nic_state->mcast_addr[port - EN1_MULT] = val;
		break;
	    default:
		PrintError("ne2k_write error: invalid port:0x%x\n", port);
		return -1;
	}
    } else if (page == 2) {
	switch (port) {
	    case EN2_LDMA0:
		nic_state->clda0 = val;
		break;
	    case EN2_LDMA1:
		nic_state->clda1 = val;
		break;
	    case EN2_RNPR:
		nic_state->rnpp = val;
		break;
	    case EN2_LNRP:
		nic_state->lnpp = val;
		break;
	    case EN2_ACNT0:
		nic_state->addcnt0 = val;
		break;
	    case EN2_ACNT1: 
		nic_state->addcnt1 = val;
		break;
	    default:
		PrintError("ne2k_write error: invalid port:0x%x\n", port);
		return -1;
	}
    } else {
	PrintError("Invalid Register Page Value\n");
	return -1;
    }


    return length;
	
}

static int ne2k_std_read(uint16_t port, void * dst, uint_t length, struct vm_device *dev) {
    struct ne2k_context * nic_state = (struct ne2k_context *)(dev->private_data);
    uint16_t index = port & 0x1f;
    uint8_t page = nic_state->cmd.pg_sel;

    if (length > 1) {
	PrintError("ne2k_read error: length %d\n", length);
	return length;
    }

    if (page == 0) {

	switch (index) {		
	    case EN0_CLDALO:
		*(uint8_t *)dst = nic_state->clda0;
		break;
	    case EN0_CLDAHI:
		*(uint8_t *)dst = nic_state->clda1;
		break;
	    case EN0_BOUNDARY:
		*(uint8_t *)dst = nic_state->boundary;
		break;
	    case EN0_TSR:
		*(uint8_t *)dst = nic_state->tsr.val;
		break;
	    case EN0_NCR:
		*(uint8_t *)dst = nic_state->ncr;
		break;
	    case EN0_FIFO:
		*(uint8_t *)dst = nic_state->fifo;
		break;
	    case EN0_ISR:
		*(uint8_t *)dst = nic_state->isr.val;
		ne2k_update_irq(dev);
		break;
	    case EN0_CRDALO:
		*(uint8_t *)dst = nic_state->crda0;
		break;
	    case EN0_CRDAHI:
		*(uint8_t *)dst = nic_state->crda1;
		break;
	    case EN0_RSR:
		*(uint8_t *)dst = nic_state->rsr.val;
		break;
	    case EN0_COUNTER0:
		*(uint8_t *)dst = nic_state->cntr0;
		break;
	    case EN0_COUNTER1:
		*(uint8_t *)dst = nic_state->cntr1;
		break;	
	    case EN0_COUNTER2:
		*(uint8_t *)dst = nic_state->cntr2;
		break;
	    default:
		PrintError("ne2k_read error: invalid port:0x%x\n", port);
		return -1;
	}

    } else if (page == 1) {

	switch (index) {
	    case EN1_PHYS ... EN1_PHYS + 5:
		*(uint8_t *)dst = nic_state->mac_addr[index - EN1_PHYS];
		break;
	    case EN1_CURPAG:
		*(uint8_t *)dst = nic_state->curpag;
		break;
	    case EN1_MULT ... EN1_MULT + 7:
		*(uint8_t *)dst = nic_state->mcast_addr[index - EN1_MULT];
		break;
	    default:
		PrintError("ne2k_read error: invalid port:0x%x\n", port);
		return -1;
	}

    } else if (page == 2) {

	switch (index) {
	    case EN2_STARTPG:
		*(uint8_t *)dst = nic_state->pgstart;
		break;
	    case EN2_STOPPG:
		*(uint8_t *)dst = nic_state->pgstop;
		break;
	    case EN2_RNPR:
		*(uint8_t *)dst = nic_state->rnpp;
		break;
	    case EN2_LNRP:
		*(uint8_t *)dst = nic_state->lnpp;
		break;
	    case EN2_TPSR:
		*(uint8_t *)dst = nic_state->tpsr;
		break;
	    case EN2_ACNT0:
		*(uint8_t *)dst = nic_state->addcnt0;
		break;
	    case EN2_ACNT1: 
		*(uint8_t *)dst = nic_state->addcnt1;
		break;
	    case EN2_RCR:
		*(uint8_t *)dst = nic_state->rcr.val;
		break;
	    case EN2_TCR:
		*(uint8_t *)dst = nic_state->tcr.val;
		break;
	    case EN2_DCR:
		*(uint8_t *)dst = nic_state->dcr.val;
		break;
	    case EN2_IMR:
		*(uint8_t *)dst = nic_state->imr.val;
		break;
	    default:
		PrintError("ne2k_read error: invalid port:0x%x\n", port);
		return -1;
	}
    } else {
	PrintError("Invalid Register Page Value\n");
	return -1;
    }

    
    PrintDebug("ne2k_read: port:0x%x  val: 0x%x\n", port, *(uint8_t *)dst);

    return length;
}


static int ne2k_start_device(struct vm_device * dev) {
    PrintDebug("vnic: start device\n");
  
    return 0;
}


static int ne2k_stop_device(struct vm_device * dev) {
    PrintDebug("vnic: stop device\n");
  
    return 0;
}




static int ne2k_init_device(struct vm_device * dev) {
    struct ne2k_context * nic_state = (struct ne2k_context *)(dev->private_data);
    
    PrintDebug("Initializing NE2K\n");

    init_ne2k_context(dev);

    if (nic_state->pci_bus == NULL) {
	PrintDebug("NE2k: Not attached to pci\n");

	v3_dev_hook_io(dev, NIC_REG_BASE_PORT , &ne2k_cmd_read, &ne2k_cmd_write);

	for (i = 1; i < 16; i++){	
	    v3_dev_hook_io(dev, NIC_REG_BASE_PORT + i, &ne2k_std_read, &ne2k_std_write);
	}

	v3_dev_hook_io(dev, NIC_DATA_PORT, &ne2k_data_read, &ne2k_data_write);
	v3_dev_hook_io(dev, NIC_RESET_PORT, &ne2k_reset_read, &ne2k_reset_write);

    } else {

	struct v3_pci_bar bars[6];
	struct pci_device * pci_dev = NULL;
	int i;

  	PrintDebug("NE2k: PCI Enabled\n");

	for (i = 0; i < 6; i++) {
	    bars[i].type = PCI_BAR_NONE;
	}

	bars[0].type = PCI_BAR_IO;
	bars[0].default_base_port = NIC_REG_BASE_PORT;
	bars[0].num_ports = 256;

	bars[0].io_read = ne2k_pci_read;
	bars[0].io_write = ne2k_pci_write;

	pci_dev = v3_pci_register_device(nic_state->pci_bus, PCI_STD_DEVICE, 0, -1, 0, 
					 "NE2000", bars,
					 pci_config_update, NULL, NULL, dev);

	if (pci_dev == NULL) {
	    PrintError("Failed to register NE2K with PCI\n");
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
    }
    

#ifdef DEBUG_NE2K
    dump_state(dev);
#endif

    return 0;
}



static int ne2k_deinit_device(struct vm_device *dev) {
    int i;
  
    for (i = 0; i < 16; i++){		
  	v3_dev_unhook_io(dev, NIC_REG_BASE_PORT + i);
    }
    
    v3_dev_unhook_io(dev, NIC_DATA_PORT);
    v3_dev_unhook_io(dev, NIC_RESET_PORT);
  
    return 0;
}


static struct vm_device_ops dev_ops = { 
    .init = ne2k_init_device, 
    .deinit = ne2k_deinit_device,
    .reset = ne2k_reset_device,
    .start = ne2k_start_device,
    .stop = ne2k_stop_device,
};


struct vm_device * v3_create_ne2k(struct vm_device * pci) {
    struct ne2k_context * nic_state = V3_Malloc(sizeof(struct ne2k_context));

    memset(nic_state, 0, sizeof(struct ne2k_context));

    nic_state->pci_bus = pci;

    struct vm_device * device = v3_create_device("NE2K", &dev_ops, nic_state);
    
    return device;
}

