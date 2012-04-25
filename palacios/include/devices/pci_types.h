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
 * Author:  Jack Lange 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __DEVICES_PCI_TYPES_H__
#define __DEVICES_PCI_TYPES_H__


#include <palacios/vmm_types.h>

// struct pci_device_config
struct pci_config_header {
    uint16_t   vendor_id;
    uint16_t   device_id;
    
    uint16_t   command;
    uint16_t   status;
    
    
    uint8_t    revision;
    uint8_t    prog_if;
    uint8_t     subclass;
    uint8_t     class;

    uint8_t    cache_line_size;
    uint8_t    latency_time;
    uint8_t    header_type; // bits 6-0: 00: other, 01: pci-pci bridge, 02: pci-cardbus; bit 7: 1=multifunction
    uint8_t    BIST;  
    

    uint32_t   BAR0;
    uint32_t   BAR1;
    uint32_t   BAR2;
    uint32_t   BAR3;
    uint32_t   BAR4;
    uint32_t   BAR5;
    uint32_t   cardbus_cis_pointer;
    uint16_t   subsystem_vendor_id;
    uint16_t   subsystem_id;
    uint32_t expansion_rom_address;
    uint8_t cap_ptr;  // capabilities list offset in config space
    uint8_t rsvd1[3];
    uint32_t rsvd2;
    uint8_t    intr_line; // 00=none, 01=IRQ1, etc.
    uint8_t    intr_pin;  // 00=none, otherwise INTA# to INTD#
    uint8_t    min_grant; // min busmaster time - units of 250ns
    uint8_t    max_latency; // units of 250ns - busmasters
} __attribute__((packed));


struct pci_cmd_reg {
    union {
	uint16_t val;
	struct {
	    uint16_t io_enable         : 1;
	    uint16_t mem_enable        : 1;
	    uint16_t dma_enable        : 1;
	    uint16_t special_cycles    : 1;
	    uint16_t mem_wr_inv_enable : 1;
	    uint16_t vga_snoop         : 1;
	    uint16_t parity_err_resp   : 1;
	    uint16_t rsvd1             : 1;
	    uint16_t serr_enable       : 1;
	    uint16_t fast_b2b_enable   : 1;
	    uint16_t intx_disable      : 1;
	    uint16_t rsvd2             : 5;
	} __attribute__((packed));
    } __attribute__((packed));

} __attribute__((packed));


typedef enum { PCI_CLASS_PRE2 = 0x00, 
	       PCI_CLASS_STORAGE = 0x01, 
	       PCI_CLASS_NETWORK = 0x02,
	       PCI_CLASS_DISPLAY = 0x03,
	       PCI_CLASS_MMEDIA = 0x04,
	       PCI_CLASS_MEMORY = 0x05,
	       PCI_CLASS_BRIDGE = 0x06,
	       PCI_CLASS_COMM_CTRL = 0x07,
	       PCI_CLASS_BASE_PERIPH = 0x08,
	       PCI_CLASS_INPUT = 0x09, 
	       PCI_CLASS_DOCK = 0x0a,
	       PCI_CLASS_PROC = 0x0b, 
	       PCI_CLASS_SERIAL = 0x0c,
	       PCI_CLASS_MISC = 0xff } pci_class_t;

typedef enum { PCI_STORAGE_SUBCLASS_SCSI = 0x00,
	       PCI_STORAGE_SUBCLASS_IDE = 0x01,
	       PCI_STORAGE_SUBCLASS_FLOPPY = 0x02,
	       PCI_STORAGE_SUBCLASS_IPI = 0x03,
	       PCI_STORAGE_SUBCLASS_RAID = 0x04,
	       PCI_STORAGE_SUBCLASS_SATA = 0x06,
	       PCI_STORAGE_SUBCLASS_SAS = 0x07,
	       PCI_STORAGE_SUBCLASS_OTHER = 0x80 } pci_storage_subclass_t;



typedef enum { PCI_NET_SUBCLASS_ETHER = 0x00,
	       PCI_NET_SUBCLASS_TOKRING = 0x01,
	       PCI_NET_SUBCLASS_FDDI = 0x02,
	       PCI_NET_SUBCLASS_ATM = 0x03,
	       PCI_NET_SUBCLASS_OTHER = 0x80 } pci_network_subclass_t;

typedef enum { PCI_DISPLAY_SUBCLASS_VGA = 0x00,
	       PCI_DISPLAY_SUBCLASS_XGA = 0x01,
	       PCI_DISPLAY_SUBCLASS_OTHER = 0x80 } pci_display_subclass_t;

typedef enum { PCI_MMEDIA_SUBCLASS_VIDEO = 0x00,
	       PCI_MMEDIA_SUBCLASS_AUDIO = 0x01,
	       PCI_MMEDIA_SUBCLASS_OTHER = 0x80 } pci_multimedia_subclass_t;
	       
typedef enum { PCI_MEM_SUBCLASS_RAM = 0x00, 
	       PCI_MEM_SUBCLASS_FLASH = 0x01,
	       PCI_MEM_SUBCLASS_OTHER = 0x80 } pci_memory_subclass_t;

typedef enum { PCI_BRIDGE_SUBCLASS_HOST_PCI = 0x00,
	       PCI_BRIDGE_SUBCLASS_PCI_ISA = 0x01,
	       PCI_BRIDGE_SUBCLASS_PCI_EISA = 0x02,
	       PCI_BRIDGE_SUBCLASS_PCI_MICRO = 0x03,
	       PCI_BRIDGE_SUBCLASS_PCI_PCI = 0x04,
	       PCI_BRIDGE_SUBCLASS_PCI_PCMCIA = 0x05,
	       PCI_BRIDGE_SUBCLASS_PCI_NUBUS = 0x06,
	       PCI_BRIDGE_SUBCLASS_PCI_CARDBUS = 0x07,
	       PCI_BRIDGE_SUBCLASS_PCI_OTHER = 0x80 } pci_bridge_subclass_t;



/*
  struct pci_class_desc {
  uint16_t class;
  const char * desc;
  };
  
  static struct pci_class_desc pci_class_descriptions[] = {
  { 0x0100, "SCSI controller"},
  { 0x0101, "IDE controller"},
  { 0x0102, "Floppy controller"},
  { 0x0103, "IPI controller"},
  { 0x0104, "RAID controller"},
  { 0x0106, "SATA controller"},
  { 0x0107, "SAS controller"},
  { 0x0180, "Storage controller"},
  { 0x0200, "Ethernet controller"},
  { 0x0201, "Token Ring controller"},
  { 0x0202, "FDDI controller"},
  { 0x0203, "ATM controller"},
  { 0x0280, "Network controller"},
  { 0x0300, "VGA controller"},
  { 0x0301, "XGA controller"},
  { 0x0302, "3D controller"},
  { 0x0380, "Display controller"},
  { 0x0400, "Video controller"},
  { 0x0401, "Audio controller"},
  { 0x0402, "Phone"},
  { 0x0480, "Multimedia controller"},
  { 0x0500, "RAM controller"},
  { 0x0501, "Flash controller"},
  { 0x0580, "Memory controller"},
  { 0x0600, "Host bridge"},
  { 0x0601, "ISA bridge"},
  { 0x0602, "EISA bridge"},
  { 0x0603, "MC bridge"},
  { 0x0604, "PCI bridge"},
  { 0x0605, "PCMCIA bridge"},
  { 0x0606, "NUBUS bridge"},
  { 0x0607, "CARDBUS bridge"},
  { 0x0608, "RACEWAY bridge"},
  { 0x0680, "Bridge"},
  { 0x0c03, "USB controller"},
  { 0, NULL}
  };
*/



struct msi_addr {
    union {
	uint32_t val;
	struct {
	    uint32_t rsvd1        : 2;
	    uint32_t dst_mode     : 1;
	    uint32_t redir_hint   : 1;
	    uint32_t rsvd2        : 8;
	    uint32_t dst_id       : 8;
	    uint32_t fixaddr      : 12;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct msi_data {
    union {
	uint16_t val;
	struct {
	    uint16_t vector       : 8;
	    uint16_t del_mode     : 3;
	    uint16_t rsvd1        : 3;
	    uint16_t level        : 1;
	    uint16_t trig_mode    : 1;
	} __attribute__((packed));;
    } __attribute__((packed));
} __attribute__((packed));


struct msi_msg_ctrl {
    union {
	uint16_t val;
	struct {
	    uint16_t msi_enable       : 1;
	    uint16_t mult_msg_capable : 3;
	    uint16_t mult_msg_enable  : 3;
	    uint16_t cap_64bit        : 1;
	    uint16_t per_vect_mask    : 1;
	    uint16_t rsvd             : 7;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct msi32_msg_addr {
    struct msi_msg_ctrl msg_ctrl;
    struct msi_addr addr;
    struct msi_data data;
} __attribute__((packed));


struct msi64_msg_addr {
    struct msi_msg_ctrl msg_ctrl;
    struct msi_addr addr;
    uint32_t hi_addr;
    struct msi_data data;
} __attribute__((packed));


struct msi64_pervec_msg_addr {
    struct msi_msg_ctrl msg_ctrl;
    struct msi_addr addr;
    uint32_t hi_addr;
    struct msi_data data;
    uint16_t rsvd;
    uint32_t mask;
    uint32_t pending;
} __attribute__((packed));


struct msix_msg_ctrl {
    uint16_t table_size   : 11;
    uint16_t rsvd         : 4;
    uint16_t msix_enable  : 1;
} __attribute__((packed));

struct msix_cap {
    struct msix_msg_ctrl msg_ctrl;
    uint32_t hi_addr;
    uint32_t bir      : 3;
    uint32_t table_offset : 29;
} __attribute__((packed));


struct msix_table {
    struct {
	struct msi_data data;
	struct msi_addr addr;
    } __attribute__((packed)) entries[0];
} __attribute__((packed));



// See PCI power management specification (1.2)
struct pmc_cap {
    struct {
	uint16_t version        : 3;
	uint16_t pme_clock      : 1;
	uint16_t rsvd1          : 1;
	uint16_t dsi            : 1;
	uint16_t aux_current    : 3;
	uint16_t d1_support     : 1;
	uint16_t d2_support     : 1;
	uint16_t pme_support    : 5;
    } __attribute__((packed)) pmc;

    struct {
	uint16_t power_state    : 2;
	uint16_t rsvd1          : 1;
	uint16_t no_soft_reset  : 1;
	uint16_t rsvd2          : 4;
	uint16_t pme_en         : 1;
	uint16_t data_select    : 4;
	uint16_t data_scale     : 2;
	uint16_t pme_status     : 1;
    } __attribute__((packed)) pmcsr;

    struct {
	uint8_t rsvd1           : 6;
	uint8_t b2_b3           : 1;
	uint8_t bpcc_en         : 1;
    } __attribute__((packed)) pmcsr_bse;

    uint8_t data;
} __attribute__((packed));


struct pcie_cap_reg {
    uint16_t version     : 4;
    uint16_t type        : 4;
    uint16_t slot_impl   : 1;
    uint16_t irq_msg_num : 5;
    uint16_t rsvd        : 2;
} __attribute__((packed));

struct pcie_cap_v1 {
    struct pcie_cap_reg pcie_cap;

    union {
	uint32_t val;
	struct {
	    uint32_t max_payload          : 3;
	    uint32_t phantom_fns          : 2;
	    uint32_t ext_tag_support      : 1;
	    uint32_t endpt_L0_latency     : 3;
	    uint32_t endpt_L1_latency     : 3;
	    uint32_t maybe_rsvd1          : 3;
	    uint32_t role_err_reporting   : 1;
	    uint32_t rsvd2                : 2;
	    uint32_t slot_pwr_lim_val     : 8;
	    uint32_t slot_pwr_lim_scale   : 2;
	    uint32_t rsvd3                : 4;
	} __attribute__((packed));
    } __attribute__((packed)) dev_cap;

    union {
	uint16_t val;
	struct {
	    uint16_t correctable_err_enable  : 1;
	    uint16_t non_fatal_err_enable    : 1;
	    uint16_t fatal_err_enable        : 1;
	    uint16_t unsupp_req_enable       : 1;
	    uint16_t relaxed_order_enable    : 1;
	    uint16_t max_payload_size        : 3;
	    uint16_t ext_tag_field_enable    : 1;
	    uint16_t phantom_fn_enable       : 1;
	    uint16_t aux_pwr_enable          : 1;
	    uint16_t no_snoop_enable         : 1;
	    uint16_t max_read_req_size       : 3;
	    uint16_t bridge_cfg_retry_enable : 1;
	} __attribute__((packed));
    } __attribute__((packed)) dev_ctrl;

    union {
	uint16_t val;
	struct {
	    uint16_t correctable_err         : 1;
	    uint16_t non_fatal_err           : 1;
	    uint16_t fatal_err               : 1;
	    uint16_t unsupp_req              : 1;
	    uint16_t aux_pwr                 : 1;
	    uint16_t transaction_pending     : 1;
	    uint16_t rsvd                    : 10;
	} __attribute__((packed));
    } __attribute__((packed)) dev_status;

    union {
	uint32_t val;
	struct {
	    uint32_t max_link_speed            : 4;
	    uint32_t max_link_width            : 6;
	    uint32_t aspm_support              : 2; /* Active State Power Management Support */
	    uint32_t L0_exit_latency           : 3;
	    uint32_t L1_exit_latency           : 3;
	    uint32_t clk_pwr_mngmt             : 1;
	    uint32_t surprise_pwr_down_capable : 1;
	    uint32_t data_link_active_capable  : 1;
	    uint32_t rsvd1                     : 3;
	    uint32_t port_number               : 8;
	} __attribute__((packed));
    } __attribute__((packed)) link_cap;


    union {
	uint16_t val;
	struct {
	    uint16_t aspm_ctrl                 : 2;
	    uint16_t rsvd1                     : 1;
	    uint16_t rd_cmpl_bndry             : 1;
	    uint16_t link_disable              : 1;
	    uint16_t retrain_link              : 1;
	    uint16_t common_clk_cfg            : 1;
	    uint16_t ext_synch                 : 1;
	    uint16_t clk_pwr_mngmt_enable      : 1;
	    uint16_t rsvd2                     : 7;
	} __attribute__((packed));
    } __attribute__((packed)) link_ctrl;


    union {
	uint16_t val;
	struct {
	    uint16_t link_speed               : 4;
	    uint16_t negotiate_link_width     : 6;
	    uint16_t undef                    : 1;
	    uint16_t link_training            : 1;
	    uint16_t slot_clk_cfg             : 1;
	    uint16_t data_link_layer_active   : 1;
	    uint16_t rsvd                     : 2;
	} __attribute__((packed));
    } __attribute__((packed)) link_status;

} __attribute__((packed));




struct pcie_cap_v2 {
   struct pcie_cap_reg pcie_cap;

    union {
	uint32_t val;
	struct {
	    uint32_t max_payload          : 3;
	    uint32_t phantom_fns          : 2;
	    uint32_t ext_tag_support      : 1;
	    uint32_t endpt_L0_latency     : 3;
	    uint32_t endpt_L1_latency     : 3;
	    uint32_t maybe_rsvd1          : 3;
	    uint32_t role_err_reporting   : 1;
	    uint32_t rsvd2                : 2;
	    uint32_t slot_pwr_lim_val     : 8;
	    uint32_t slot_pwr_lim_scale   : 2;
	    uint32_t fn_level_reset       : 1;
	    uint32_t rsvd3                : 3;
	} __attribute__((packed));
    } __attribute__((packed)) dev_cap;

    union {
	uint16_t val;
	struct {
	    uint16_t correctable_err_enable  : 1;
	    uint16_t non_fatal_err_enable    : 1;
	    uint16_t fatal_err_enable        : 1;
	    uint16_t unsupp_req_enable       : 1;
	    uint16_t relaxed_order_enable    : 1;
	    uint16_t max_payload_size        : 3;
	    uint16_t ext_tag_field_enable    : 1;
	    uint16_t phantom_fn_enable       : 1;
	    uint16_t aux_pwr_enable          : 1;
	    uint16_t no_snoop_enable         : 1;
	    uint16_t max_read_req_size       : 3;
	    uint16_t bridge_cfg_retry_enable : 1;
	} __attribute__((packed));
    } __attribute__((packed)) dev_ctrl;

    union {
	uint16_t val;
	struct {
	    uint16_t correctable_err         : 1;
	    uint16_t non_fatal_err           : 1;
	    uint16_t fatal_err               : 1;
	    uint16_t unsupp_req              : 1;
	    uint16_t aux_pwr                 : 1;
	    uint16_t transaction_pending     : 1;
	    uint16_t rsvd                    : 10;
	} __attribute__((packed));
    } __attribute__((packed)) dev_status;

    union {
	uint32_t val;
	struct {
	    uint32_t max_link_speed            : 4;
	    uint32_t max_link_width            : 6;
	    uint32_t aspm_support              : 2; /* Active State Power Management Support */
	    uint32_t L0_exit_latency           : 3;
	    uint32_t L1_exit_latency           : 3;
	    uint32_t clk_pwr_mngmt             : 1;
	    uint32_t surprise_pwr_down_capable : 1;
	    uint32_t data_link_active_capable  : 1;
	    uint32_t rsvd1                     : 3;
	    uint32_t port_number               : 8;
	} __attribute__((packed));
    } __attribute__((packed)) link_cap;


    union {
	uint16_t val;
	struct {
	    uint16_t aspm_ctrl                 : 2;
	    uint16_t rsvd1                     : 1;
	    uint16_t rd_cmpl_bndry             : 1;
	    uint16_t link_disable              : 1;
	    uint16_t retrain_link              : 1;
	    uint16_t common_clk_cfg            : 1;
	    uint16_t ext_synch                 : 1;
	    uint16_t clk_pwr_mngmt_enable      : 1;
	    uint16_t rsvd2                     : 7;
	} __attribute__((packed));
    } __attribute__((packed)) link_ctrl;


    union {
	uint16_t val;
	struct {
	    uint16_t link_speed               : 4;
	    uint16_t negotiate_link_width     : 6;
	    uint16_t undef                    : 1;
	    uint16_t link_training            : 1;
	    uint16_t slot_clk_cfg             : 1;
	    uint16_t data_link_layer_active   : 1;
	    uint16_t rsvd                     : 2;
	} __attribute__((packed));
    } __attribute__((packed)) link_status;


    /* Some crap whose format we don't know because the PCI-SIG sucks */
    
    uint32_t slot_cap;
    uint16_t slot_ctrl;
    uint16_t slot_status;

    uint16_t root_ctrl;
    uint16_t root_cap;
    uint32_t root_status;

} __attribute__((packed));



#endif

