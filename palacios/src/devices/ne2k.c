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

//#define TEST_PERFORMANCE 0

typedef enum {NIC_READY, NIC_REG_POSTED} nic_state_t;

struct ne2k_context{
    struct guest_info *vm;
    nic_state_t dev_state;

    struct ne2k_regs regs;
    uchar_t mac[6];
    uchar_t mem[NE2K_MEM_SIZE];
    struct pci_device *pci_dev;
    struct vm_device *pci;
};

#define NUM_NE2K 10

struct vm_device *ne2ks[NUM_NE2K];  	//the array of virtual network cards

static int nic_no = 0;

#if TEST_PERFORMANCE
static uint32_t exit_num = 0;
static uint32_t int_num = 0; 
#endif

#define compare_mac(src, dst) ({ \
	((src[0] == dst[0]) && \
	  (src[1] == dst[1]) && \
	  (src[2] == dst[2]) && \
	  (src[3] == dst[3]) && \
	  (src[4] == dst[4]) && \
	  (src[5] == dst[5]))? 1:0; \
	})

extern int V3_Send_pkt(uchar_t *buf, int length);
extern int V3_Register_pkt_event(int (*netif_input)(uchar_t * pkt, uint_t size));

#ifdef DEBUG_NE2K
static void dump_state(struct vm_device *dev)
{
  int i;
  uchar_t *p;
  struct ne2k_context *nic_state = (struct ne2k_context *)dev->private_data;

  PrintDebug("====NE2000: Dumping state Begin ==========\n");
  PrintDebug("Registers:\n");

  p = (uchar_t *)&nic_state->regs;
  for(i = 0; i < sizeof(struct ne2k_regs); i++)
     PrintDebug("Regs[%d] = 0x%2x\n", i, (int)p[i]);	
  
  PrintDebug("Memory:\n");	
  for(i = 0; i < 32; i++)
        PrintDebug("0x%02x ", nic_state->mem[i]);
  PrintDebug("\n");
  PrintDebug("====NE2000: Dumping state End==========\n");
}
#endif

#if 0
//no-pci version
static void ne2k_update_irq(struct vm_device *dev)
{
    int isr;
    struct ne2k_context *nic_state = (struct ne2k_context *)dev->private_data;
    struct guest_info *guest = dev->vm;
	
    isr = ((nic_state->regs.isr & nic_state->regs.imr) & 0x7f);

    if ((isr & 0x7f) != 0x0) {
    	v3_raise_irq(guest, NIC_DEF_IRQ);
	PrintDebug("Ne2k: RaiseIrq: isr: 0x%02x imr: 0x%02x\n", nic_state->regs.isr, nic_state->regs.imr);
    }   
}

#endif

#if 1
//pci version
static void ne2k_update_irq(struct vm_device *dev)
{
    int isr;
    struct ne2k_context *nic_state = (struct ne2k_context *)dev->private_data;
    struct pci_device *pdev = nic_state->pci_dev;
    int irqline = 0;

    if (pdev == NULL){
	PrintDebug("Ne2k: Device %p is not attached to any PCI Bus\n", nic_state);
	irqline = NE2K_DEF_IRQ;
    } else {
    	irqline = pdev->config_header.intr_line;
    }
	
    isr = ((nic_state->regs.isr & nic_state->regs.imr) & 0x7f);
    
    if (irqline == 0){
	PrintError("Ne2k: IRQ_LINE: %d\n", irqline);
	return;
    }

    PrintDebug("Ne2k: RaiseIrq: isr: 0x%02x imr: 0x%02x\n", nic_state->regs.isr, nic_state->regs.imr);
    PrintDebug("ne2k_update_irq: irq_line: %d\n", irqline);

    if ((isr & 0x7f) != 0x0) {
    	v3_raise_irq(nic_state->vm, irqline);
	PrintDebug("Ne2k: RaiseIrq: isr: 0x%02x imr: 0x%02x\n", nic_state->regs.isr, nic_state->regs.imr);

	#if TEST_PERFORMANCE
	if ((++int_num) % 50 == 0)
	     PrintError("Ne2k: Total Exit: %d, INT: %d\n", (int)exit_num, int_num);
	#endif
    }   
}

#endif

static void init_ne2k_context(struct vm_device *dev)
{
    struct ne2k_context *nic_state = (struct ne2k_context *)dev->private_data;
    int i;
    uchar_t mac[6] = {0x52, 0x54, 0x0, 0x12, 0x34, (0x60 + nic_no)};

    nic_state->vm = dev->vm;

    nic_state->regs.isr = ENISR_RESET;
    nic_state->regs.imr = 0x00;
    nic_state->regs.cmd = 0x22;

    for (i = 0; i < 5; i++)
	nic_state->regs.macaddr[i] = nic_state->mac[i] = mac[i];

    nic_state->regs.macaddr[5] = nic_state->mac[5] = mac[5] + nic_no;

    for (i = 0; i < 8; i++)
    	nic_state->regs.mult[i] = 0xff;

    for(i = 0; i < 32; i++) {
        nic_state->mem[i] = 0xff;
    }

    memcpy(nic_state->mem, nic_state->mac, 6);
    nic_state->mem[14] = 0x57;
    nic_state->mem[15] = 0x57;

#ifdef DEBUG_NE2K
    dump_state(dev);
#endif

}

static int ne2k_send_packet(struct vm_device *dev, uchar_t *pkt, int length)
{
    int i;
  
    PrintDebug("\nNe2k: Sending Packet\n");

    for (i = 0; i<length; i++)
	PrintDebug("%x ",pkt[i]);
    PrintDebug("\n");
	
    return V3_Send_pkt(pkt, length);
}

static int ne2k_rxbuf_full(struct vm_device *dev)
{
    int empty, index, boundary;
    struct ne2k_context *nic_state = (struct ne2k_context *)dev->private_data;

    index = nic_state->regs.curpag << 8;
    boundary = nic_state->regs.boundary << 8;
    if (index < boundary)
        empty = boundary - index;
    else
        empty = ((nic_state->regs.pgstop - nic_state->regs.pgstart) << 8) - (index - boundary);
	
    if (empty < (MAX_ETH_FRAME_SIZE + 4))
        return 1;
	
    return 0;
}

#define MIN_BUF_SIZE 60

static void ne2k_receive(struct vm_device *dev, const uchar_t *pkt, int length)
{
    struct ne2k_context *nic_state = (struct ne2k_context *)dev->private_data;
    struct ne2k_regs *nregs = &(nic_state->regs);
    uchar_t *p;
    uint32_t total_len, next, len, index, empty;
    uchar_t buf[60];
    uint32_t start, stop;

    start = nregs->pgstart << 8;
    stop = nregs->pgstop << 8;
   
    if (nregs->cmd & NE2K_STOP)
	return;

    if (ne2k_rxbuf_full(dev)){
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

    index = nregs->curpag << 8;
    //header, 4 bytes
    total_len = length + 4;
    //address for next packet (4 bytes for CRC)
    next = index + ((total_len + 4 + 255) & ~0xff);
    if (next >= stop)
        next -= stop - start;

    p = nic_state->mem + index;
    nregs->rsr = ENRSR_RXOK;

    if (pkt[0] & 0x01)
        nregs->rsr |= ENRSR_PHY;
	
    p[0] = nregs->rsr;
    p[1] = next >> 8;
    p[2] = total_len;
    p[3] = total_len >> 8;
    index += 4;

    while (length > 0) {
        if (index <= stop)
            empty = stop - index;
        else
            empty = 0;
        len = length;
        if (len > empty)
            len = empty;
        memcpy(nic_state->mem + index, pkt, len);
        pkt += len;
        index += len;
        if (index == stop)
            index = start;
        length -= len;
    }
    nregs->curpag = next >> 8;

    nregs->isr |= ENISR_RX;
    ne2k_update_irq(dev);
}

static int ne2k_hook_iospace(struct vm_device *vmdev, addr_t base_addr, int size, int type, void *data);

static struct pci_device * pci_ne2k_init(struct vm_device *vmdev, 
				struct vm_device *pci,  
				int bus_no, 
				int dev_num,
				int fn_num,
				int (*io_read)(ushort_t port, void * dst, uint_t length, struct vm_device * dev),
	    			int (*io_write)(ushort_t port, void * src, uint_t length, struct vm_device * dev))
{
    uchar_t *pci_conf;
    struct pci_device *pdev;
    struct v3_pci_bar ne2k_bar;

    ne2k_bar.type = PCI_BAR_IO;
    ne2k_bar.num_ports = 0x100;
    ne2k_bar.default_base_port = 0xc100;
    ne2k_bar.io_read = io_read;
    ne2k_bar.io_write = io_write;
  
    pdev = v3_pci_register_device(vmdev, 
				PCI_STD_DEVICE, 
				bus_no, 
				dev_num, 
				fn_num,
				"NE2000", 
				&ne2k_bar, 
				NULL, NULL, NULL, vmdev);

    if (pdev == NULL){
       PrintError("NIC: Register to PCI bus failed\n");
       return NULL;
    }

    pci_conf = pdev->config_space;
	
    pci_conf[0x00] = 0xec; // Realtek 8029
    pci_conf[0x01] = 0x10;
    pci_conf[0x02] = 0x29;
    pci_conf[0x03] = 0x80;
    pci_conf[0x0a] = 0x00; // ethernet network controller
    pci_conf[0x0b] = 0x02;
    pci_conf[0x0e] = 0x00; // header_type
    pci_conf[0x3d] = 1; // interrupt pin 0
    pci_conf[0x3c] = 11; //default IRQ Line

    pdev->vm_dev = vmdev;
    
    return pdev;
}

static int netif_input(uchar_t *pkt, uint_t size)
{
  struct ne2k_context *nic_state;
  struct ne2k_regs *nregs;
  static const uchar_t brocast_mac[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  int i;
  
  PrintDebug("\nNe2k: Packet Received:\nSource:");
  for (i = 6; i < 12; i++) {
  	PrintDebug("%x ", pkt[i]);
  }
  PrintDebug("\n");
  for(i= 0; i<size; i++)
  	PrintDebug("%x ", pkt[i]);
  

  for (i = 0; i < NUM_NE2K; i++){
	if (ne2ks[i] != NULL) {
		nic_state = (struct ne2k_context *)ne2ks[i]->private_data;
		nregs = &(nic_state->regs);
			
		if (nregs->rcr & 0x10) {//promiscuous mode
		        ne2k_receive(ne2ks[i], pkt, size);
		} else {
		       if (compare_mac(pkt,  brocast_mac) && (nregs->rcr & 0x04)){ //broadcast packet
		                ne2k_receive(ne2ks[i], pkt, size);
		       } else if (pkt[0] & 0x01) {
			     //TODO: multicast packet
			     if (nregs->rcr & 0x08)
			            ne2k_receive(ne2ks[i], pkt, size);
		       } else if (compare_mac(pkt, nic_state->mac)){
		            	ne2k_receive(ne2ks[i], pkt, size);
		       } else 
		       	continue;
	       }
	}
    }

  return 0;
}


static inline uint16_t cpu2le16(uint16_t val)
{
    uint16_t p;
    uchar_t *p1 = (uchar_t *)&p;

    p1[0] = val;
    p1[1] = val >> 8;

    return p;
}


static inline uint32_t cpu2le32(uint32_t val)
{
    uint32_t p;
    uchar_t *p1 = (uchar_t *)&p;

    p1[0] = val;
    p1[1] = val >> 8;
    p1[2] = val >> 16;
    p1[3] = val >> 24;

    return p;
}

static inline uint16_t le16_to_cpu(const uint16_t *p)
{
    const uchar_t *p1 = (const uchar_t *)p;
    return p1[0] | (p1[1] << 8);
}

static inline uint32_t le32_to_cpu(const uint32_t *p)
{
    const uchar_t *p1 = (const uchar_t *)p;
    return p1[0] | (p1[1] << 8) | (p1[2] << 16) | (p1[3] << 24);
}

static void 
ne2k_mem_writeb(struct ne2k_context *nic_state, 
			uint32_t addr,
			uint32_t val)
{
    uchar_t tmp;

    tmp = (uchar_t) (val & 0x000000ff);
    if (addr < 32 || (addr >= NE2K_PMEM_START && addr < NE2K_MEM_SIZE)) {
        nic_state->mem[addr] = tmp;
    }

    PrintDebug("wmem addr: %x val: %x\n", addr, val);
}

static void 
ne2k_mem_writew(struct ne2k_context *nic_state, 
			uint32_t addr,
			uint32_t val)
{
    addr &= ~1; //XXX: check exact behaviour if not even
    if (addr < 32 ||
        (addr >= NE2K_PMEM_START && addr < NE2K_MEM_SIZE)) {
        *(ushort_t *)(nic_state->mem + addr) = cpu2le16(val);
    }

    PrintDebug("wmem addr: %x val: %x\n", addr, val);
}

static void 
ne2k_mem_writel(struct ne2k_context *nic_state,
			uint32_t addr,
			uint32_t val)
{
    addr &= ~1; // XXX: check exact behaviour if not even
    if (addr < 32 ||
        (addr >= NE2K_PMEM_START && addr < NE2K_MEM_SIZE)) {
        *(uint32_t *)(nic_state->mem + addr) = cpu2le32(val);
    }

    PrintDebug("wmem addr: %x val: %x\n", addr, val);
}

static uchar_t 
ne2k_mem_readb(struct ne2k_context *nic_state, uint32_t addr)
{
    PrintDebug("rmem addr: %x\n", addr);
	
    if (addr < 32 ||
        (addr >= NE2K_PMEM_START && addr < NE2K_MEM_SIZE)) {
        return nic_state->mem[addr];
    } else {
        return 0xff;
    }
}

static ushort_t
ne2k_mem_readw(struct ne2k_context *nic_state, uint32_t addr)
{
    PrintDebug("rmem addr: %x\n", addr);
	
    addr &= ~1; //XXX: check exact behaviour if not even 
    if (addr < 32 ||
        (addr >= NE2K_PMEM_START && addr < NE2K_MEM_SIZE)) {
        return (ushort_t)le16_to_cpu((ushort_t *)(nic_state->mem + addr));
    } else {
        return 0xffff;
    }
}

static uint32_t 
ne2k_mem_readl(struct ne2k_context *nic_state, uint32_t addr)
{
    PrintDebug("rmem addr: %x\n", addr);

    addr &= ~1; //XXX: check exact behaviour if not even
    if (addr < 32 ||
        (addr >= NE2K_PMEM_START && addr < NE2K_MEM_SIZE)) {
        return (uint32_t)le32_to_cpu((uint32_t *)(nic_state->mem + addr));
    } else {
        return 0xffffffff;
    }
}

static void 
ne2k_dma_update(struct vm_device *dev, int len)
{		
    struct ne2k_context *nic_state = (struct ne2k_context *)dev->private_data;
	
    nic_state->regs.rsar += len;
    // wrap
    if (nic_state->regs.rsar == nic_state->regs.pgstop)
        nic_state->regs.rsar = nic_state->regs.pgstart;

    if (nic_state->regs.rbcr <= len) {
        nic_state->regs.rbcr = 0;
        nic_state->regs.isr |= ENISR_RDC;
        ne2k_update_irq(dev);
    } else {
        nic_state->regs.rbcr -= len;
    }
}


//for data port read/write
static int ne2k_data_read(ushort_t port,
			void * dst,
			uint_t length,
			struct vm_device *dev)
{
	uint32_t val;
	struct ne2k_context *nic_state = (struct ne2k_context *)dev->private_data;

        // current dma address
	uint32_t addr = nic_state->regs.rsar;

	switch(length){
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

	PrintDebug("ne2k_read: port:0x%x (%u bytes): 0x%x", port & 0x1f,length, val);

	#if TEST_PERFORMANCE
        if ((++exit_num) % 50 == 0)
	PrintError("Ne2k-Ne2k: Total Exit: %d, INT: %d\n", (int)exit_num, int_num);
        #endif

	return length;
}

static int ne2k_data_write(ushort_t port,
				 			void * src,
				 			uint_t length,
				 			struct vm_device *dev)
{
	uint32_t val;
	struct ne2k_context *nic_state = (struct ne2k_context *)dev->private_data;

	if (nic_state->regs.rbcr == 0)
		return length;

	memcpy(&val, src, length);

	uint32_t addr = nic_state->regs.rsar;
	
	switch (length){
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

	PrintDebug("ne2k_write: port:0x%x (%u bytes): 0x%x\n", port & 0x1f,length, val);

	#if TEST_PERFORMANCE
        if ((++exit_num) % 50 == 0)
		PrintError("Ne2k-Ne2k: Total Exit: %d, INT: %d\n", (int)exit_num, int_num);
        #endif
			
	return length;
}

static int ne2k_reset_device(struct vm_device * dev)
{
  
  PrintDebug("vnic: reset device\n");

  init_ne2k_context(dev);

  return 0;
}


//for 0xc11f port
static int ne2k_reset_port_read(ushort_t port,
				 			void * dst,
				 			uint_t length,
		  	 			       struct vm_device *dev)
{
	uint32_t val = 0x0;

	memcpy(dst, &val, length);

	PrintDebug("ne2k_read: port:0x%x (%u bytes): 0x%x\n", port,length, val);

	ne2k_reset_device(dev);

	#if TEST_PERFORMANCE
        if ((++exit_num) % 50 == 0)
		PrintError("Ne2k-Ne2k: Total Exit: %d, INT: %d\n", (int)exit_num, int_num);
        #endif

	return length;
}

static int ne2k_reset_port_write(ushort_t port,
				 			void * src,
				 			uint_t length,
				 			struct vm_device *dev)
{
	uint32_t val;

	memcpy(&val, src, length);

	PrintDebug("ne2k_write: port:0x%x (%u bytes): 0x%x\n", port,length, val);

	#if TEST_PERFORMANCE
        if ((++exit_num) % 50 == 0)
		PrintError("Ne2k-Ne2k: Total Exit: %d, INT: %d\n", (int)exit_num, int_num);
        #endif
			
	return length;
}

static int ne2k_ioport_write(ushort_t port,
			void * src,
			uint_t length,
			struct vm_device *dev)
{
    uchar_t  page;
    struct ne2k_context *nic_state = (struct ne2k_context* )dev->private_data;
    uchar_t val;
    int index;
	
    if (length == 1) {
	  memcpy(&val, src, 1);
    } else {
	  PrintError("ne2k_write error: length %d\n", length);  
	  return length;
    }

    PrintDebug("ne2k_write: port:0x%x  val: 0x%x\n", port, (int)val);
	
    port &= 0x1f;

    if (port == 0x10)
	return ne2k_data_write(port, src, length, dev);

    if (port == 0x1f)
	return ne2k_reset_port_write(port, src, length, dev);
		
    if (port == EN0_COMMAND) {
        nic_state->regs.cmd = val;
        if (!(val & NE2K_STOP)) {
            nic_state->regs.isr &= ~ENISR_RESET; 
            if ((val & (NE2K_DMAREAD | NE2K_DMAWRITE)) &&
                	nic_state->regs.rbcr == 0) {
                nic_state->regs.isr |= ENISR_RDC;
                ne2k_update_irq(dev);
            }
            if (val & NE2K_TRANSMIT) {
                index = (nic_state->regs.tpsr << 8);
                if (index >= NE2K_PMEM_END)
                    index -= NE2K_PMEM_SIZE;
                if (index + nic_state->regs.tbcr <= NE2K_PMEM_END) {
                    ne2k_send_packet(dev, nic_state->mem + index, nic_state->regs.tbcr);
                }
                nic_state->regs.tsr = ENTSR_PTX;
                nic_state->regs.isr |= ENISR_TX;
                nic_state->regs.cmd &= ~NE2K_TRANSMIT;
                ne2k_update_irq(dev);
            }
        }
    } else {
        page = nic_state->regs.cmd >> 6;
        if(page == 0){
	        switch(port) {
		        case EN0_STARTPG:
		            nic_state->regs.pgstart = val;
		            break;
		        case EN0_STOPPG:
		            nic_state->regs.pgstop = val;
		            break;
		        case EN0_BOUNDARY:
		            nic_state->regs.boundary = val;
		            break;
			case EN0_TPSR:
		            nic_state->regs.tpsr = val;
		            break;
			case EN0_TCNTLO:
		            nic_state->regs.tbcr = (nic_state->regs.tbcr & 0xff00) | val;
		            break;
		        case EN0_TCNTHI:
		            nic_state->regs.tbcr = (nic_state->regs.tbcr & 0x00ff) | (val << 8);
		            break;
			case EN0_ISR:
		            nic_state->regs.isr &= ~(val & 0x7f);
		            ne2k_update_irq(dev);
		            break;
			case EN0_RSARLO:
		            nic_state->regs.rsar = (nic_state->regs.rsar & 0xff00) | val;
		            break;
		        case EN0_RSARHI:
		            nic_state->regs.rsar = (nic_state->regs.rsar & 0x00ff) | (val << 8);
		            break;
			case EN0_RCNTLO:
		            nic_state->regs.rbcr = (nic_state->regs.rbcr & 0xff00) | val;
		            break;
		        case EN0_RCNTHI:
		            nic_state->regs.rbcr = (nic_state->regs.rbcr & 0x00ff) | (val << 8);
		            break;
		        case EN0_RXCR:
		            nic_state->regs.rcr = val;
		            break;
			case EN0_TXCR:
			     nic_state->regs.tcr = val;
		        case EN0_DCFG:
		            nic_state->regs.dcr = val;
		            break;	
		        case EN0_IMR:
		            nic_state->regs.imr = val;
			     //PrintError("ne2k_write error: write IMR:0x%x\n", (int)val);
		            ne2k_update_irq(dev);
		            break;
			default:
			     PrintError("ne2k_write error: invalid port:0x%x\n", port);
			     break;
	        	}
        	}
	 if(page == 1){
	 	switch(port) {
		        case EN1_PHYS ... EN1_PHYS + 5:
		            nic_state->regs.phys[port - EN1_PHYS] = val;
		            break;
		        case EN1_CURPAG:
		            nic_state->regs.curpag = val;
		            break;
		        case EN1_MULT ... EN1_MULT + 7:
			    // PrintError("ne2k_write error: write EN_MULT:0x%x\n", (int)val);
		            nic_state->regs.mult[port - EN1_MULT] = val;
		            break;
			default:
    			     PrintError("ne2k_write error: invalid port:0x%x\n", port);
			     break;
	 		}
	 	}
	if(page == 2){
	 	switch(port) {
			 case EN2_LDMA0:
			    nic_state->regs.clda = (nic_state->regs.clda & 0xff00) | val;
			    break;
			 case EN2_LDMA1:
			    nic_state->regs.clda = (nic_state->regs.clda & 0x00ff) | (val << 8);
			    break;
			 case EN2_RNPR:
			    nic_state->regs.rnpp = val;
			    break;
			 case EN2_LNRP:
			    nic_state->regs.lnpp = val;
			    break;
			 case EN2_ACNT0:
			    nic_state->regs.addcnt = (nic_state->regs.addcnt & 0xff00) | val;
			    break;
			 case EN2_ACNT1: 
			    nic_state->regs.addcnt = (nic_state->regs.addcnt & 0x00ff) | (val << 8);
			    break;
			 default:
			    PrintError("ne2k_write error: invalid port:0x%x\n", port);
			    break;
	 		}
		}
    }

    #if TEST_PERFORMANCE
    if ((++exit_num) % 50 == 0)
	PrintError("Ne2k-Ne2k: Total Exit: %d, INT: %d\n", (int)exit_num, int_num);
    #endif

    return length;
	
}

static int ne2k_ioport_read(ushort_t port,
			void * dst,
			uint_t length,
			struct vm_device *dev)
{
    uchar_t page, ret, offset;

    struct ne2k_context *nic_state = (struct ne2k_context* )dev->private_data;

    if (length > 1) {
	  PrintError("ne2k_read error: length %d\n", length);
	  return length;
    }

    offset = port;
    port &= 0x1f;

    if (port == 0x10)
	return ne2k_data_read(port, dst, length, dev);

    if (port == 0x1f)
	return ne2k_reset_port_read(port, dst, length, dev);

    if (port == EN0_COMMAND) {
        ret = nic_state->regs.cmd;
    } else {
        page = nic_state->regs.cmd >> 6;
        if (page == 0){
            switch(port) {		
		 case EN0_CLDALO:
	            ret = nic_state->regs.clda & 0x00ff;
	            break;
	         case EN0_CLDAHI:
	            ret = (nic_state->regs.clda & 0xff00) >> 8;
	            break;
	         case EN0_BOUNDARY:
	            ret = nic_state->regs.boundary;
	            break;
		 case EN0_TSR:
	            ret = nic_state->regs.tsr;
	            break;
		 case EN0_NCR:
	            ret = nic_state->regs.ncr;
	            break;
	         case EN0_FIFO:
	            ret = nic_state->regs.fifo;
	            break;
		 case EN0_ISR:
	            ret = nic_state->regs.isr;
	            ne2k_update_irq(dev);
	            break;
		 case EN0_CRDALO:
	            ret = nic_state->regs.crda & 0x00ff;
	            break;
	         case EN0_CRDAHI:
	            ret = (nic_state->regs.crda & 0xff00) >> 8;
	            break;
	         case EN0_RSR:
	            ret = nic_state->regs.rsr;
	            break;
		 case EN0_COUNTER0:
		     ret = nic_state->regs.cntr & 0x000000ff;
		     break;
	         case EN0_COUNTER1:
	            ret = (nic_state->regs.cntr & 0x0000ff00) >> 8;
		     break;	
	         case EN0_COUNTER2:
	            ret = (nic_state->regs.cntr & 0x00ff0000) >> 16;
		     break;
		 default:
    		    PrintError("ne2k_read error: invalid port:0x%x\n", port);
		    ret = 0x00;
		    break;
           }
        }
	 if (page == 1){
           switch(port) {
	         case EN1_PHYS ... EN1_PHYS + 5:
	            ret = nic_state->regs.phys[port - EN1_PHYS];
	            break;
	         case EN1_CURPAG:
	            ret = nic_state->regs.curpag;
	            break;
	         case EN1_MULT ... EN1_MULT + 7:
	            ret = nic_state->regs.mult[port - EN1_MULT];
	            break;
		 default:
		    PrintError("ne2k_read error: invalid port:0x%x\n", port);
		    ret = 0x00;
		    break;
           }
	 }
	 if (page == 2){
           switch(port) {
		 case EN2_STARTPG:
		    ret = nic_state->regs.pgstart;
		    break;
		 case EN2_STOPPG:
		    ret = nic_state->regs.pgstop;
		    break;
		 case EN2_RNPR:
		    ret = nic_state->regs.rnpp;
		    break;
		 case EN2_LNRP:
		    ret = nic_state->regs.lnpp;
		    break;
		 case EN2_TPSR:
		    ret = nic_state->regs.tpsr;
		    break;
		 case EN2_ACNT0:
		    ret = nic_state->regs.addcnt & 0x00ff;
		    break;
		 case EN2_ACNT1: 
		    ret = (nic_state->regs.addcnt & 0xff00) >> 8;
		    break;
		 case EN2_RCR:
		    ret = nic_state->regs.rcr;
		    break;
		 case EN2_TCR:
		    ret = nic_state->regs.tcr;
		    break;
		 case EN2_DCR:
		    ret = nic_state->regs.dcr;
		    break;
		 case EN2_IMR:
		    ret = nic_state->regs.imr;
		    break;
		 default:
		    PrintError("ne2k_read error: invalid port:0x%x\n", port);
		    ret = 0x00;
		    break;
           }
	 }
    }

    memcpy(dst, &ret, 1);

    PrintDebug("ne2k_read: port:0x%x  val: 0x%x\n", offset, (int)ret);

    #if TEST_PERFORMANCE
    if ((++exit_num) % 50 == 0)
	PrintError("Ne2k-Ne2k: Total Exit: %d, INT: %d\n", (int)exit_num, int_num);
    #endif

    return length;

}


static int ne2k_start_device(struct vm_device *dev)
{
  PrintDebug("vnic: start device\n");
  
  return 0;
}


static int ne2k_stop_device(struct vm_device *dev)
{
  PrintDebug("vnic: stop device\n");
  
  return 0;
}

static void  init_global_setting()
{	
  int i;
  
  V3_Register_pkt_event(&netif_input);
  
  for (i = 0; i < NUM_NE2K; i++)
  	ne2ks[i] = NULL;

}

static int ne2k_hook_iospace(struct vm_device *vmdev, addr_t base_addr, int size, int type, void *data)
{
  int i;

  if (base_addr <= 0)
  	return -1;

  PrintDebug("In NIC%d: Hook IO space starting from %x\n", nic_no, (int) base_addr);

  for (i = 0; i < 16; i++){	
  	v3_dev_hook_io(vmdev, base_addr + i, &ne2k_ioport_read, &ne2k_ioport_write);
  }
  v3_dev_hook_io(vmdev, base_addr + NIC_DATA_PORT, &ne2k_data_read, &ne2k_data_write);
  v3_dev_hook_io(vmdev, base_addr + NIC_RESET_PORT, &ne2k_reset_port_read, &ne2k_reset_port_write);

  return 0;

}

static int ne2k_init_device(struct vm_device * vmdev) 
{
  struct vm_device *pci = NULL;
  struct pci_device *pdev = NULL;
  struct ne2k_context *nic_state = (struct ne2k_context *)vmdev->private_data;

  PrintDebug("ne2k%d: init_device\n",  nic_no);

  if (nic_no == 0) // only initiate once
  	init_global_setting();

  init_ne2k_context(vmdev);

  pci = nic_state->pci;

  if (pci != NULL) {
  	PrintDebug("Ne2k: attach ne2k to the pci %p\n", pci);
        pdev = pci_ne2k_init(vmdev, pci, 0, -1, 0,  &ne2k_ioport_read, &ne2k_ioport_write);
	if (pdev == NULL)
		PrintError("Ne2k: initiate failure, failure to attach ne2k to the bus %p\n", pci);
  } else {
	PrintDebug("Ne2k: Not attached to any pci\n");
	ne2k_hook_iospace(vmdev, NE2K_DEF_BASE_ADDR, 100, 0, NULL);
  }

  nic_state->pci_dev = pdev;  
  ne2ks[nic_no ++] = vmdev;

  #ifdef DEBUG_NE2K
  dump_state(vmdev);
  #endif

  return 0;
}



static int ne2k_deinit_device(struct vm_device *vmdev)
{
  int i;
  
  for (i = 0; i<16; i++){		
  	v3_dev_unhook_io(vmdev, NE2K_DEF_BASE_ADDR + i);
  }

  v3_dev_unhook_io(vmdev, NE2K_DEF_BASE_ADDR + NIC_DATA_PORT);
  v3_dev_unhook_io(vmdev, NE2K_DEF_BASE_ADDR + NIC_RESET_PORT);
  
  return 0;
}


static struct vm_device_ops dev_ops = { 
  .init = ne2k_init_device, 
  .deinit = ne2k_deinit_device,
  .reset = ne2k_reset_device,
  .start = ne2k_start_device,
  .stop = ne2k_stop_device,
};


struct vm_device *v3_create_ne2k(struct vm_device *pci) 
{
  struct ne2k_context * nic_state = V3_Malloc(sizeof(struct ne2k_context));

  memset(nic_state, 0, sizeof(struct ne2k_context));

  PrintDebug("NE2K internal at %p\n", (void *)nic_state);

  nic_state->pci = pci;

  struct vm_device *device = v3_create_device("NE2K", &dev_ops, nic_state);

  return device;
}

