/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2009, Jack Lange <jarusl@cs.northwestern.edu>
 * Copyright (c) 2009, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */ 
 

#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_intr.h>

#include <devices/pci.h>
#include <devices/southbridge.h>


struct iort_reg {
    union {
	uint8_t value;
	struct {
	    uint8_t times_16bit   : 2;
	    uint8_t enable_16bit  : 1;
	    uint8_t times_8bit    : 3;
	    uint8_t enable_8bit   : 1;
	    uint8_t dmaac         : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

struct xbcs_reg {
    union {
	uint16_t value;
	struct {
	    uint8_t rtc_addr_en        : 1;
	    uint8_t kb_ctrl_en         : 1;
	    uint8_t bioscs_wprot_en    : 1;
	    uint8_t rsvd1              : 1;
	    uint8_t irq12_mouse_fn_en  : 1;
	    uint8_t coproc_err_fn_en   : 1;
	    uint8_t lower_bios_en      : 1;
	    uint8_t ext_bios_en        : 1;
	    uint8_t apic_chip_sel      : 1;
	    uint8_t piix_rsvd          : 7;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct pirq_route_ctrl_req {
    union {
	uint8_t value;
	struct {
	    uint8_t irq_routing        : 4;
	    uint8_t rsvd               : 3;
	    uint8_t irq_route_en       : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

struct top_of_mem_reg {
    union {
	uint8_t value;
	struct {
	    uint8_t rsvd1                    : 1;
	    uint8_t isadma_reg_fwd_en        : 1;
	    uint8_t piix_rsvd                : 1;
	    uint8_t isadma_lo_bios_fwd_en    : 1;
	    uint8_t top_of_mem               : 4;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

// Miscellaneous Status register
struct misc_stat_reg {
    union {
	uint16_t value;
	struct {
	    uint8_t isa_clk_div              : 1;
	    uint8_t piix_rsvd1               : 1;
	    uint8_t pci_hdr_type_en          : 1;
	    uint8_t rsvd1                    : 1;
	    uint8_t usb_en                   : 1;
	    uint8_t rsvd2                    : 1;
	    uint8_t ext_sm_mode_en           : 1;
	    uint8_t nb_retry_en              : 1;
	    uint8_t rsvd3                    : 7;
	    uint8_t serr_gen_delayed_tranx   : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));



// Motherboard Device IRQ route control register
struct mb_irq_ctrl_reg {
    union {
	uint8_t value;
	struct {
	    uint8_t irq_routing              : 4;
	    uint8_t rsvd                     : 1;
	    uint8_t irq0_en                  : 1;
	    uint8_t irq_sharing_en           : 1;
	    uint8_t irq_routing_en           : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

// Motherboard Device DMA control register
struct mb_dma_ctrl_reg {
    union {
	uint8_t value;
	struct {
	    uint8_t type_f_dma_routing       : 3;
	    uint8_t piix_rsvd                : 1;
	    uint8_t rsvd                     : 3;
	    uint8_t type_f_dma_buf_en        : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


// Programmable Chip Select Control Register
struct prg_chip_sel_ctrl_reg {
    union {
	uint16_t value;
	struct {
	    uint8_t pcs_addr_mask            : 2;
	    uint16_t pcs_addr                : 14;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


// APIC base address relocation register
struct apic_base_addr_reg {
    union {
	uint8_t value;
	struct {
	    uint8_t y_base_addr             : 2;
	    uint8_t x_base_addr             : 4;
	    uint8_t a12_mask                : 1;
	    uint8_t rsvd                    : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


// Deterministic Latency control register
struct deter_lat_ctrl_reg {
    union {
	uint8_t value;
	struct {
	    uint8_t delayed_trans_en        : 1;
	    uint8_t pass_release_en         : 1;
	    uint8_t usb_pass_release_en     : 1;
	    uint8_t serr_gen_trans_tmout_en : 1;
	    uint8_t rsvd                    : 4;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

// SMI Control Register
struct smi_ctrl_reg {
    union {
	uint8_t value;
	struct {
	    uint8_t smi_gate               : 1;
	    uint8_t stpclk_sig_en          : 1;
	    uint8_t stpclk_scaling_en      : 1;
	    uint8_t fast_off_tmr_freeze    : 2;
	    uint8_t rsvd                   : 3;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

// SMI Enable register
struct smi_enable_reg {
    union {
	uint16_t value;
	struct {
	    uint8_t irq1_smi_en            : 1; // (keyboard irq)
	    uint8_t irq3_smi_en            : 1; // (COM1/COM3/Mouse irq)
	    uint8_t irq4_smi_en            : 1; // (COM2/COM4/Mouse irq)
	    uint8_t irq8_smi_en            : 1; // (RTC irq)
	    uint8_t irq12_smi_en           : 1; // (PS/2 mouse irq)
	    uint8_t fast_off_tmr_en        : 1;
	    uint8_t ext_smi_en             : 1;
	    uint8_t apic_wr_smi_en         : 1;
	    uint8_t legacy_usb_smi_en      : 1;
	    uint8_t rsvd                   : 7;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


// System Event Enable register
struct sys_evt_en_reg {
    union {
	uint32_t value;
	struct {
	    uint8_t firq0_en               : 1;
	    uint8_t firq1_en               : 1;
	    uint8_t rsvd1                  : 1;
	    uint8_t firq3_en               : 1;
	    uint8_t firq4_en               : 1;
	    uint8_t firq5_en               : 1;
	    uint8_t firq6_en               : 1;
	    uint8_t firq7_en               : 1;
	    uint8_t firq8_en               : 1;
	    uint8_t firq9_en               : 1;
	    uint8_t firq10_en              : 1;
	    uint8_t firq11_en              : 1;
	    uint8_t firq12_en              : 1;
	    uint8_t firq13_en              : 1;
	    uint8_t firq14_en              : 1;
	    uint8_t firq15_en              : 1;
	    uint16_t rsvd2                 : 12;
	    uint8_t fast_off_apic_en       : 1;
	    uint8_t fast_off_nmi_en        : 1;
	    uint8_t intr_en                : 1;
	    uint8_t fast_off_smi_en        : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


// SMI Request Register
struct smi_req_reg {
    union {
	uint16_t value;
	struct {
	    uint8_t irq1_req_smi_stat      : 1;
	    uint8_t irq3_req_smi_stat      : 1;
	    uint8_t irq4_req_smi_stat      : 1;
	    uint8_t irq8_req_smi_stat      : 1;
	    uint8_t irq12_req_smi_stat     : 1;
	    uint8_t fast_off_tmr_exp_stat  : 1;
	    uint8_t extsmi_stat            : 1;
	    uint8_t apm_smi_stat           : 1;
	    uint8_t legacy_usb_smi_stat    : 1;
	    uint8_t rsvd                   : 7;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct piix3_config_space {
    uint8_t rsvd1[12];            // 0x40 - 0x4b

    // ISA I/O Recovery timer register
    uint8_t iort;                 // 0x4c        (default 0x4d)
    uint8_t rsvd2;                // 0x4d

    // X-Bus Chip select register
    uint16_t xbcs;                // 0x4e - 0x4f (default: 0x03)
    uint8_t rsvd3[16];            // 0x50 - 0x5f

    // pirq route control register (IRQs A-D)
    uint8_t pirq_rc[4];           // 0x60 - 0x63 (default: 0x80) 
    uint8_t rsvd4[5];             // 0x64 - 0x68

    // top of memory register
    uint8_t top_of_mem;           // 0x69        (default: 0x02)

    // Miscellaneous status register
    uint16_t mstat;               // 0x6A - 0x6B (default: undefined)
    uint8_t rsvd5[4];             // 0x6c - 0x6f

    // Motherboard device IRQ route control register
    uint8_t mbirq0;                // 0x70        (default: 0x80)
    uint8_t rsvd6;                 // 0x71 (piix only)
    uint8_t rsvd7[4];              // 0x72 - 0x75

    // Motherboard Device DMA Control registers
    uint8_t mbdma0;                // 0x76        (default: 0x0c)
    uint8_t mbdma1;                // 0x77        (default: 0x0c)

    // Programmable Chip Select Control Register  
    uint16_t pcsc;                 // 0x78 - 0x79 (default: 0x0002)
    uint8_t rsvd8[6];              // 0x7A - 0x7F

    // APIC base address relocation register
    uint8_t apicbase;              // 0x80        (default: 0x00)
    uint8_t rsvd9;                 // 0x81


    // Deterministic Latency control register
    uint8_t dlc;                   // 0x82        (default: 0x00)
    uint8_t rsvd10[29];            // 0x83 - 0x9f


    // SMI Control Register
    uint8_t smicntl;               // 0xa0        (default: 0x08)
    uint8_t rsvd11;                // 0xa1

    // SMI Enable register
    uint16_t smien;                // 0xa2 - 0xa3 (default: 0x0000)

    // System Event Enable register
    uint32_t see;                  // 0xa4 - 0xa7 (default: 0x00000000)

    // Fast off timer register
    uint8_t ftmr;                  // 0xa8        (default: 0x0f)
    uint8_t rsvd12;                // 0xa9

    // SMI Request Register
    uint16_t smireq;               // 0xaa - 0xab (default: 0x0000)

    // Clock Scale stpclk low timer
    uint8_t ctltmr;                // 0xac        (default: 0x00)
    uint8_t rsvd13;                // 0xad

    // Slock Scale STPCLK high timer
    uint8_t cthtmr;                // 0xae        (default: 0x00)

} __attribute__((packed));



static int reset_piix3(struct vm_device * dev) {
    struct v3_southbridge * piix3 = (struct v3_southbridge *)(dev->private_data);
    struct pci_device * pci_dev = piix3->southbridge_pci;
    struct piix3_config_space * piix3_cfg = (struct piix3_config_space *)(pci_dev->config_data);

    pci_dev->config_header.command = 0x0007; // master, memory and I/O
    pci_dev->config_header.status = 0x0200;

    piix3_cfg->iort = 0x4d;
    piix3_cfg->xbcs = 0x0003;
    piix3_cfg->pirq_rc[0] = 0x80;
    piix3_cfg->pirq_rc[1] = 0x80;
    piix3_cfg->pirq_rc[2] = 0x80;
    piix3_cfg->pirq_rc[3] = 0x80;
    piix3_cfg->top_of_mem = 0x02;
    piix3_cfg->mbirq0 = 0x80;
    piix3_cfg->mbdma0 = 0x0c;
    piix3_cfg->mbdma1 = 0x0c;
    piix3_cfg->pcsc = 0x0002;
    piix3_cfg->apicbase = 0x00;
    piix3_cfg->dlc = 0x00;
    piix3_cfg->smicntl = 0x08;
    piix3_cfg->smien = 0x0000;
    piix3_cfg->see = 0x00000000;
    piix3_cfg->ftmr = 0x0f;
    piix3_cfg->smireq = 0x0000;
    piix3_cfg->ctltmr = 0x00;
    piix3_cfg->cthtmr = 0x00;

    return 0;
}


//irq is pirq_rc[intr_pin + pci_dev_num - 1] & 0x3

static int raise_pci_irq(struct vm_device * dev, struct pci_device * pci_dev) {
    struct v3_southbridge * piix3 = (struct v3_southbridge *)(dev->private_data);
    struct pci_device * piix3_pci = piix3->southbridge_pci;
    struct piix3_config_space * piix3_cfg = (struct piix3_config_space *)(piix3_pci->config_data);
    int intr_pin = pci_dev->config_header.intr_pin - 1;
    int irq_index = (intr_pin + pci_dev->dev_num - 1) & 0x3;
    
    //        PrintError("Raising PCI IRQ %d\n", piix3_cfg->pirq_rc[irq_index]);
    
    v3_raise_irq(dev->vm, piix3_cfg->pirq_rc[irq_index]);

    return 0;
}



static int lower_pci_irq(struct vm_device * dev, struct pci_device * pci_dev) {
    struct v3_southbridge * piix3 = (struct v3_southbridge *)(dev->private_data);
    struct pci_device * piix3_pci = piix3->southbridge_pci;
    struct piix3_config_space * piix3_cfg = (struct piix3_config_space *)(piix3_pci->config_data);
    int intr_pin = pci_dev->config_header.intr_pin - 1;
    int irq_index = (intr_pin + pci_dev->dev_num - 1) & 0x3;
    
    //    PrintError("Lowering PCI IRQ %d\n", piix3_cfg->pirq_rc[irq_index]);
    
    v3_lower_irq(dev->vm, piix3_cfg->pirq_rc[irq_index]);

    return 0;
}



static int piix_free(struct vm_device * dev) {
    return 0;
}


static struct v3_device_ops dev_ops = {
    .free = piix_free,
    .reset = reset_piix3,
    .start = NULL,
    .stop = NULL,
};




static int setup_pci(struct vm_device * dev) {
    struct v3_southbridge * piix3 = (struct v3_southbridge *)(dev->private_data);
    struct pci_device * pci_dev = NULL;
    struct v3_pci_bar bars[6];
    int i;
    int bus_num = 0;

    for (i = 0; i < 6; i++) {
	bars[i].type = PCI_BAR_NONE;
    }

    pci_dev = v3_pci_register_device(piix3->pci_bus, PCI_MULTIFUNCTION, 
				     bus_num, -1, 0, 
				     "PIIX3", bars, 
				     NULL, NULL, NULL, dev);
    if (pci_dev == NULL) {
	PrintError("Could not register PCI Device for PIIX3\n");
	return -1;
    }

    pci_dev->config_header.vendor_id = 0x8086;
    pci_dev->config_header.device_id = 0x7000; // PIIX4 is 0x7001
    pci_dev->config_header.class = PCI_CLASS_BRIDGE;
    pci_dev->config_header.subclass = PCI_BRIDGE_SUBCLASS_PCI_ISA; 

    piix3->southbridge_pci = pci_dev;

    v3_pci_set_irq_bridge(piix3->pci_bus, bus_num, raise_pci_irq, lower_pci_irq, dev);

    reset_piix3(dev);

    return 0;
}

static int piix3_init(struct guest_info * vm, v3_cfg_tree_t * cfg) {
    struct v3_southbridge * piix3 = (struct v3_southbridge *)V3_Malloc(sizeof(struct v3_southbridge));
    struct vm_device * dev = NULL;
    struct vm_device * pci = v3_find_dev(vm, v3_cfg_val(cfg, "bus"));
    char * name = v3_cfg_val(cfg, "name");

    if (!pci) {
	PrintError("Could not find PCI device\n");
	return -1;
    }

    piix3->pci_bus = pci;
    piix3->type = V3_SB_PIIX3;
    
    dev = v3_allocate_device(name, &dev_ops, piix3);

    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", name);
	return -1;
    }

    PrintDebug("Created PIIX3\n");

    return setup_pci(dev);
}


device_register("PIIX3", piix3_init)
