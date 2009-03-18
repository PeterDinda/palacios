/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#ifndef __DEVICES_IDE_TYPES_H__
#define __DEVICES_IDE_TYPES_H__

#ifdef __V3VEE__

#include <palacios/vmm_types.h>


struct ide_error_reg {
    union {
	uint8_t val;
	struct {
	    uint_t addr_mark_nf     : 1;
	    uint_t track0_nf        : 1;
	    uint_t abort            : 1;
	    uint_t rsvd0            : 1;
	    uint_t ID_nf            : 1;
	    uint_t rsvd1            : 1;
	    uint_t data_error       : 1;
	    uint_t bad_block        : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct ide_drive_head_reg {
    union {
	uint8_t val;
	struct {
	    uint_t head_num      : 4;
	    uint_t drive_sel     : 1;
	    uint_t rsvd1         : 1;
	    uint_t lba_mode      : 1;
	    uint_t rsvd2         : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

struct ide_status_reg {
    union {
	uint8_t val;
	struct {
	    uint_t error         : 1;
	    uint_t index         : 1;
	    uint_t corrected     : 1;
	    uint_t data_req      : 1;
	    uint_t seek_complete : 1;
	    uint_t write_fault   : 1;
	    uint_t ready         : 1;
	    uint_t busy          : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct ide_ctrl_reg {
    union {
	uint8_t val;
	struct {
	    uint_t rsvd0        : 1;
	    uint_t irq_disable   : 1;
	    uint_t soft_reset   : 1;
	    uint_t rsvd1        : 5;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct ide_features_reg {
    union {
	uint8_t val;
    } __attribute__((packed));
} __attribute__((packed));

typedef enum {IDE_CTRL_NOT_SPECIFIED, 
	      IDE_CTRL_SINGLE_PORT, 
	      IDE_CTRL_DUAL_PORT, 
	      IDE_CTRL_DUAL_PORT_CACHE} ide_controller_type;

struct ide_drive_id {
    union {
	uint16_t buf[256];
	struct {
	    uint_t rsvd1           : 1;
	    uint_t hard_sectors    : 1;
	    uint_t no_soft_sectors : 1;
	    uint_t no_mfm_enc      : 1;
	    uint_t head_switch_time : 1;
	    uint_t spnd_mot_ctrl   : 1;
	    uint_t fixed_drive     : 1;
	    uint_t removable_media : 1;
	    uint_t disk_speed1     : 1;
	    uint_t disk_speed2     : 1;
	    uint_t disk_speed3     : 1;
	    uint_t rpm_tolerance   : 1;
	    uint_t data_strobe_offset : 1;
	    uint_t track_offset_option : 1;
	    uint_t fmt_speed_tol   : 1;
	    uint_t cdrom_flag      : 1;

	    uint16_t num_cylinders;
	    uint16_t rsvd2;
	    uint16_t num_heads;

	    uint16_t bytes_per_track;
	    uint16_t bytes_per_sector;
	    uint16_t sectors_per_track;

	    uint16_t sector_gap;
	    
	    uint8_t phase_lock_bytes;
	    uint8_t rsvd3;

	    uint16_t num_vendor_wds;
	    
	    uint8_t serial_num[20]; // right aligned, padded with 0x20
	    

	    uint16_t controller_type;

	    uint16_t buffer_size; // in 512 byte chunks

	    uint16_t num_ecc_bytes;

	    uint8_t firmware_rev[8]; // space padded
	    uint8_t model_num[40]; // space padded
	    
	    uint16_t rw_multiples;

	    uint16_t dword_io;

	    uint8_t rsvd4;
 	    uint8_t lba_enable;
	    

	    uint16_t rsvd6;

	    uint16_t min_PIO_cycle;
	    uint16_t min_DMA_cycle;

	    uint16_t rsvd7[503];

	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


#endif // ! __V3VEE__

#endif
