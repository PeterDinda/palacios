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


#ifndef __DEVICES_ATAPI_TYPES_H__
#define __DEVICES_ATAPI_TYPES_H__

#ifdef __V3VEE__

#include <palacios/vmm_types.h>

typedef enum {
    ATAPI_SEN_NONE = 0, 
    ATAPI_SEN_NOT_RDY = 2, 
    ATAPI_SEN_ILL_REQ = 5,
    ATAPI_SEN_UNIT_ATTNT = 6
} atapi_sense_key_t ;

typedef enum  {
    ASC_INV_CMD_FIELD = 0x24,
    ASC_MEDIA_NOT_PRESENT = 0x3a,
    ASC_SAVE_PARAM_NOT_SUPPORTED = 0x39,    
    ASC_LOG_BLK_OOR = 0x21                  /* LOGICAL BLOCK OUT OF RANGE */
} atapi_add_sense_code_t ; 


struct atapi_sense_data {
    union {
	uint8_t buf[18];
	struct {
	    uint8_t header;
	    uint8_t rsvd1;
	    uint8_t sense_key; // atapi_sense_key_t
	    uint8_t info[4];
	    uint8_t read_len; // num bytes past this point
	    uint8_t spec_info[4];
	    uint8_t asc;   // atapi_add_sense_code_t
	    uint8_t ascq; // ??
	    uint8_t fruc; // ??
	    uint8_t key_spec[3];
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));




struct atapi_read10_cmd {
    uint8_t atapi_cmd;
    uint8_t rel_addr       : 1;
    uint8_t rsvd1          : 2;
    uint8_t force_access   : 1; // can't use cache for data
    uint8_t disable_pg_out : 1;
    uint8_t lun            : 3;
    uint32_t lba;
    uint8_t rsvd2;
    uint16_t xfer_len;
    uint8_t ctrl;
} __attribute__((packed));


#endif

#endif
