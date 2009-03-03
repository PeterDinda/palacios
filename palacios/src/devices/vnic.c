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

#include <devices/vnic.h>
#include <palacios/vmm.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm_io.h>
#include <palacios/vmm_debug.h>

#define DEBUG_NIC

#ifndef DEBUG_NIC
#undef PrintDebug()
#define PrintDebug(fmts, args...)
#endif

typedef enum {NIC_READY, NIC_REG_POSTED} nic_state_t;

struct nic_regs {
    uchar_t cmd;
    uchar_t pgstart;
    uchar_t pgstop;
    ushort_t clda;
    uchar_t boundary;
    uchar_t tsr;
    uchar_t tpsr;
    uchar_t ncr;
    ushort_t tbcr;
    uchar_t fifo;
    uchar_t isr;
    ushort_t crda;
    ushort_t rsar;
    ushort_t rbcr;
    uchar_t rsr;
    uchar_t rcr;
    uint32_t cntr;
    uchar_t tcr;
    uchar_t dcr;
    uchar_t imr;
    
    uchar_t phys[6]; //mac address 
    uchar_t curpag;
    uchar_t mult[8]; //multicast mask array 
    uchar_t rnpp;
    uchar_t lnpp;
    ushort_t addcnt;
    
    uchar_t macaddr[6];
};

struct nic_context{
    struct guest_info *vm;

    nic_state_t dev_state;

    struct nic_regs regs;

    uchar_t mac[6]; //the mac address of this nic

    uchar_t mem[NE2K_MEM_SIZE];	
};

struct vm_device *current_vnic;

#define compare_mac(src, dst) ({ \
	((src[0] == dst[0]) && \
	  (src[1] == dst[1]) && \
	  (src[2] == dst[2]) && \
	  (src[3] == dst[3]) && \
	  (src[4] == dst[4]) && \
	  (src[5] == dst[5]))? 1:0; \
	})

static void dump_state(struct vm_device *dev)
{
  int i;
  uchar_t *p;
  struct nic_context *nic_state = (struct nic_context *)dev->private_data;

  PrintDebug("====VNIC: Dumping state Begin ==========\n");
  PrintDebug("Registers:\n");

  p = (uchar_t *)&nic_state->regs;
  for(i = 0; i < sizeof(struct nic_regs); i++)
     PrintDebug("Regs[i] = 0x%2x\n", (int)p[i]);	
  
  PrintDebug("Memory:\n");	
  for(i = 0; i < 32; i++)
        PrintDebug("0x%02x ", nic_state->mem[i]);
  PrintDebug("\n");
  PrintDebug("====VNIC: Dumping state End==========\n");
}

static void vnic_update_irq(struct vm_device *dev)
{
    int isr;
    struct nic_context *nic_state = (struct nic_context *)dev->private_data;
    struct guest_info *guest = dev->vm;
	
    isr = ((nic_state->regs.isr & nic_state->regs.imr) & 0x7f);

    if ((isr & 0x7f) != 0x0) {
    	v3_raise_irq(guest, NIC_IRQ);
	PrintDebug("VNIC: RaiseIrq: isr: 0x%02x imr: 0x%02x\n", nic_state->regs.isr, nic_state->regs.imr);
    }   
}

static void init_vnic_context(struct vm_device *dev)
{
    struct nic_context *nic_state = (struct nic_context *)dev->private_data;
    int i;
    uchar_t mac[6] = {0x52, 0x54, 0x0, 0x12, 0x34, 0x56};

    nic_state->vm = dev->vm;

    nic_state->regs.isr = ENISR_RESET;
    nic_state->regs.imr = 0x00;
    nic_state->regs.cmd = 0x22;

    for (i = 0; i < 6; i++)
	nic_state->regs.macaddr[i] = nic_state->mac[i] = mac[i];

    for (i = 0; i < 8; i++)
    	nic_state->regs.mult[i] = 0xff;

    for(i = 0; i < 32; i++) {
        nic_state->mem[i] = 0xff;
    }

    memcpy(nic_state->mem, nic_state->mac, 6);
    nic_state->mem[14] = 0x57;
    nic_state->mem[15] = 0x57;

    dump_state(dev);

}
static int vnic_send_packet(struct vm_device *dev, uchar_t *pkt, int length)
{
    int i;
  
    PrintDebug("\nVNIC: Sending Packet\n");

    for (i = 0; i<length; i++)
	    PrintDebug("%x ",pkt[i]);
    PrintDebug("\n");
	
    return V3_SEND_PKT(pkt, length);
}


struct vm_device * get_rx_dev(uchar_t *dst_mac)
{
    struct nic_context *nic_state = (struct nic_context *)current_vnic->private_data;
    struct nic_regs *nregs = &(nic_state->regs);

    static const uchar_t brocast_mac[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

    if (nregs->rcr & 0x10) {
        // promiscuous model    
    } else {
        if (compare_mac(dst_mac,  brocast_mac)) { //broadcast address
            if (!(nregs->rcr & 0x04))
                return NULL;
        } else if (dst_mac[0] & 0x01) {
            // multicast packet, not fully done here
            // ==========
            if (!(nregs->rcr & 0x08))
                return NULL;
        } else if (!compare_mac(dst_mac, nic_state->mac)) {
            return NULL;
        } else {
            
        }
    }

    return current_vnic;
}

static int vnic_rxbuf_full(struct vm_device *dev)
{
    int empty, index, boundary;
    struct nic_context *nic_state = (struct nic_context *)dev->private_data;

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

static void vnic_receive(struct vm_device *dev, const uchar_t *pkt, int length)
{
    struct nic_context *nic_state = (struct nic_context *)dev->private_data;
    struct nic_regs *nregs = &(nic_state->regs);
    uchar_t *p;
    uint32_t total_len, next, len, index, empty;
    uchar_t buf[60];
    uint32_t start, stop;
    
    
    //PrintDebug("VNIC: received packet, len=%d\n", length);

    start = nregs->pgstart << 8;
    stop = nregs->pgstop << 8;
   
    if (nregs->cmd & NE2K_STOP)
	 return;

    if (vnic_rxbuf_full(dev)){
	  PrintDebug("VNIC: received buffer overflow\n");
        return;
    }

    // if too small buffer, expand it
    if (length < MIN_BUF_SIZE) {
        memcpy(buf, pkt, length);
        memset(buf + length, 0, MIN_BUF_SIZE - length);
        pkt = buf;
        length = MIN_BUF_SIZE;
    }

    index = nregs->curpag << 8;
    // 4 bytes header 
    total_len = length + 4;
    // address for next packet (4 bytes for CRC)
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
    vnic_update_irq(dev);
}

// =====begin here
#if 0
void pci_vnic_init(PCIBus *bus, NICInfo *nd, int devfn)
{
    PCINE2000State *d;
    NE2000State *s;
    uint8_t *pci_conf;
    struct pci_device *pdev;

    pdev = pci_register_device(bus,
                                              "NE2000", sizeof(PCINE2000State),
                                              devfn,
                                              NULL, NULL);
    pci_conf = d->dev.config;
    pci_conf[0x00] = 0xec; // Realtek 8029
    pci_conf[0x01] = 0x10;
    pci_conf[0x02] = 0x29;
    pci_conf[0x03] = 0x80;
    pci_conf[0x0a] = 0x00; // ethernet network controller
    pci_conf[0x0b] = 0x02;
    pci_conf[0x0e] = 0x00; // header_type
    pci_conf[0x3d] = 1; // interrupt pin 0

    pci_register_io_region(&d->dev, 0, 0x100,
                           PCI_ADDRESS_SPACE_IO, ne2000_map);
    s = &d->ne2000;
    s->irq = d->dev.irq[0];
    s->pci_dev = (PCIDevice *)d;
    memcpy(s->macaddr, nd->macaddr, 6);
    ne2000_reset(s);
    s->vc = qemu_new_vlan_client(nd->vlan, ne2000_receive,
                                 ne2000_can_receive, s);

    snprintf(s->vc->info_str, sizeof(s->vc->info_str),
             "ne2000 pci macaddr=%02x:%02x:%02x:%02x:%02x:%02x",
             s->macaddr[0],
             s->macaddr[1],
             s->macaddr[2],
             s->macaddr[3],
             s->macaddr[4],
             s->macaddr[5]);

    /* XXX: instance number ? */
    register_savevm("ne2000", 0, 3, ne2000_save, ne2000_load, s);
}
#endif
//End Here====================================

static int netif_input(uchar_t * pkt, uint_t size)
{
  uint_t i;
  struct vm_device *dev;
  
  PrintDebug("\nVNIC: Packet Received:\nSource:");
  for (i = 6; i < 12; i++) {
  	PrintDebug("%x ", pkt[i]);
  }

  dev = get_rx_dev(pkt);

  if (dev == NULL) 
  	return 0;

  PrintDebug("\n");
  for(i= 0; i<size; i++)
  	PrintDebug("%x ", pkt[i]);
  
  vnic_receive(dev, pkt, size);

  return 0;
}


static int vnic_ioport_write(ushort_t port,
				 			void * src,
				 			uint_t length,
				 			struct vm_device *dev)
{
    uchar_t  page;
    struct nic_context *nic_state = (struct nic_context* )dev->private_data;
    uchar_t val;
    int index;
	
    if (length == 1) {
	  memcpy(&val, src, 1);
    } else {
	  PrintDebug("vnic_write error: length %d\n", length);  
	  return length;
    }
	
    port &= 0x1f;
	
    PrintDebug("vnic_write: port:0x%x (%u bytes): 0x%x\n", port, length, (int)val);
	
    if (port == EN0_COMMAND) {
        nic_state->regs.cmd = val;
        if (!(val & NE2K_STOP)) {
            nic_state->regs.isr &= ~ENISR_RESET; 
            if ((val & (NE2K_DMAREAD | NE2K_DMAWRITE)) &&
                	nic_state->regs.rbcr == 0) {
                nic_state->regs.isr |= ENISR_RDC;
                vnic_update_irq(dev);
            }
            if (val & NE2K_TRANSMIT) {
                index = (nic_state->regs.tpsr << 8);
                if (index >= NE2K_PMEM_END)
                    index -= NE2K_PMEM_SIZE;
                if (index + nic_state->regs.tbcr <= NE2K_PMEM_END) {
                    vnic_send_packet(dev, nic_state->mem + index, nic_state->regs.tbcr);
                }
                nic_state->regs.tsr = ENTSR_PTX;
                nic_state->regs.isr |= ENISR_TX;
                nic_state->regs.cmd &= ~NE2K_TRANSMIT;
                vnic_update_irq(dev);
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
		            vnic_update_irq(dev);
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
		            vnic_update_irq(dev);
		            break;
			 default:
			     PrintDebug("vnic_write error: invalid port:0x%x\n", port);
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
		            nic_state->regs.mult[port - EN1_MULT] = val;
		            break;
			 default:
    			     PrintDebug("vnic_write error: invalid port:0x%x\n", port);
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
			    PrintDebug("vnic_write error: invalid port:0x%x\n", port);
			    break;
	 		}
		}
        }

       //dump_state(dev);
	
	return length;
	
}

static int vnic_ioport_read(ushort_t port,
				 			void * dst,
				 			uint_t length,
				 			struct vm_device *dev)
{
    uchar_t page, ret;

    struct nic_context *nic_state = (struct nic_context* )dev->private_data;

    if (length > 1) {
    	   PrintDebug("vnic_read error: length %d\n", length);
	   return length;
    }

    port &= 0x1f;

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
	            vnic_update_irq(dev);
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
    		     PrintDebug("vnic_read error: invalid port:0x%x\n", port);
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
		     PrintDebug("vnic_read error: invalid port:0x%x\n", port);
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
		    PrintDebug("vnic_read error: invalid port:0x%x\n", port);
		    ret = 0x00;
		    break;
           }
	 }
    }

    memcpy(dst, &ret, 1);

    PrintDebug("vnic_read: port:0x%x (%u bytes): 0x%x\n", port,length, (int)ret);

    //dump_state(dev);

    return length;

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

static void vnic_mem_writeb(struct nic_context *nic_state, 
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

static void vnic_mem_writew(struct nic_context *nic_state, 
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

static void vnic_mem_writel(struct nic_context *nic_state,
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

static uchar_t vnic_mem_readb(struct nic_context *nic_state, uint32_t addr)
{
    PrintDebug("rmem addr: %x\n", addr);
	
    if (addr < 32 ||
        (addr >= NE2K_PMEM_START && addr < NE2K_MEM_SIZE)) {
        return nic_state->mem[addr];
    } else {
        return 0xff;
    }
}

static ushort_t vnic_mem_readw(struct nic_context *nic_state, uint32_t addr)
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

static uint32_t vnic_mem_readl(struct nic_context *nic_state, uint32_t addr)
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

static void vnic_dma_update(struct vm_device *dev, int len)
{		
    struct nic_context *nic_state = (struct nic_context *)dev->private_data;
	
    nic_state->regs.rsar += len;
    // wrap
    if (nic_state->regs.rsar == nic_state->regs.pgstop)
        nic_state->regs.rsar = nic_state->regs.pgstart;

    if (nic_state->regs.rbcr <= len) {
        nic_state->regs.rbcr = 0;
        nic_state->regs.isr |= ENISR_RDC;
        vnic_update_irq(dev);
    } else {
        nic_state->regs.rbcr -= len;
    }
}


//for data port read/write
static int vnic_data_read(ushort_t port,
				 			void * dst,
				 			uint_t length,
		  	 			       struct vm_device *dev)
{
	uint32_t val;
	struct nic_context *nic_state = (struct nic_context *)dev->private_data;

       // current dma address
	uint32_t addr = nic_state->regs.rsar;

	switch(length){
		case 1:
			val = vnic_mem_readb(nic_state, addr);
			break;
		case 2:
			val = vnic_mem_readw(nic_state, addr);
			break;
		case 4:
			val = vnic_mem_readl(nic_state, addr);
			break;
		default:
			PrintDebug("vnic_data_read error: invalid length %d\n", length);
			val = 0x0;
	}

       vnic_dma_update(dev, length);

	memcpy(dst, &val, length);

	PrintDebug("vnic_read: port:0x%x (%u bytes): 0x%x", port & 0x1f,length, val);

	return length;
}

static int vnic_data_write(ushort_t port,
				 			void * src,
				 			uint_t length,
				 			struct vm_device *dev)
{
	uint32_t val;
	struct nic_context *nic_state = (struct nic_context *)dev->private_data;

	uint32_t addr = nic_state->regs.rsar;

	if (nic_state->regs.rbcr == 0)
		return length;

	memcpy(&val, src, length);

	//determine the starting address of reading/writing
	//addr= ??
	
	switch (length){
		case 1:
			vnic_mem_writeb(nic_state, addr, val);
			break;
		case 2:
			vnic_mem_writew(nic_state, addr, val);
			break;
		case 4:
			vnic_mem_writel(nic_state, addr, val);
			break;
		default:
    			PrintDebug("nic_data_write error: invalid length %d\n", length);
		}
	
	vnic_dma_update(dev, length);

	PrintDebug("vnic_write: port:0x%x (%u bytes): 0x%x\n", port & 0x1f,length, val);
			
	return length;
}

static int vnic_reset_device(struct vm_device * dev)
{
  
  PrintDebug("vnic: reset device\n");

  init_vnic_context(dev);

  return 0;
}


//for 0xc11f port
static int vnic_reset_port_read(ushort_t port,
				 			void * dst,
				 			uint_t length,
		  	 			       struct vm_device *dev)
{
	uint32_t val = 0x0;

	memcpy(dst, &val, length);

	PrintDebug("vnic_read: port:0x%x (%u bytes): 0x%x\n", port,length, val);

	vnic_reset_device(dev);

	return length;
}

static int vnic_reset_port_write(ushort_t port,
				 			void * src,
				 			uint_t length,
				 			struct vm_device *dev)
{
	uint32_t val;

	memcpy(&val, src, length);

	PrintDebug("vnic_write: port:0x%x (%u bytes): 0x%x\n", port,length, val);
			
	return length;
}


static int vnic_start_device(struct vm_device *dev)
{
  PrintDebug("vnic: start device\n");
  
  return 0;
}


static int vnic_stop_device(struct vm_device *dev)
{
  PrintDebug("vnic: stop device\n");
  
  return 0;
}

static void  init_phy_network()
{	

  V3_REGISTER_PKT_DELIVERY(&netif_input);
  
}

static int vnic_init_device(struct vm_device * dev) 
{
  int i;

  PrintDebug("vnic: init_device\n");

  init_phy_network();
  init_vnic_context(dev);

  current_vnic = dev;  

  for (i = 0; i < 16; i++){	
  	v3_dev_hook_io(dev, NIC_BASE_ADDR + i, &vnic_ioport_read, &vnic_ioport_write);
  }
  v3_dev_hook_io(dev, NIC_BASE_ADDR + NIC_DATA_PORT, &vnic_data_read, &vnic_data_write);
  v3_dev_hook_io(dev, NIC_BASE_ADDR + NIC_RESET_PORT, &vnic_reset_port_read, &vnic_reset_port_write);

  return 0;
}



static int vnic_deinit_device(struct vm_device *dev)
{
  int i;
  
  for (i = 0; i<16; i++){		
  	v3_dev_unhook_io(dev, NIC_BASE_ADDR + i);
  }

  v3_dev_unhook_io(dev, NIC_BASE_ADDR + NIC_DATA_PORT);
  v3_dev_unhook_io(dev, NIC_BASE_ADDR + NIC_RESET_PORT);

  //vnic_reset_device(dev);
  
  return 0;
}


static struct vm_device_ops dev_ops = { 
  .init = vnic_init_device, 
  .deinit = vnic_deinit_device,
  .reset = vnic_reset_device,
  .start = vnic_start_device,
  .stop = vnic_stop_device,
};


struct vm_device *v3_create_vnic() 
{
  struct nic_context * nic_state = V3_Malloc(sizeof(struct nic_context));

  //memset(nic_state, 0, sizeof(struct nic_context));

  //PrintDebug("VNIC internal at %x\n",(int)nic_state);

  struct vm_device *device = v3_create_device("VNIC", &dev_ops, nic_state);

  return device;
}

