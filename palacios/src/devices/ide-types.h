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


struct ide_drive_ctrl_reg {
    union {
	uint8_t val;
	struct {
	    uint_t rsvd0        : 1;
	    uint_t irq_enable   : 1;
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


typedef enum { 
    READ_SECT_W_RETRY = 0x20,
    READ_SECT = 0x21,
    READ_LONG_W_RETRY = 0x22,
    READ_LONG = 0x23,
    READ_VRFY_SECT_W_RETRY = 0x40,
    READ_VRFY_SECT = 0x41,
    FORMAT_TRACK = 0x50,
    EXEC_DRV_DIAG = 0x90,
    INIT_DRIVE_PARAM = 0x91,

} ide_cmd_t;




#endif // ! __V3VEE__

#endif
