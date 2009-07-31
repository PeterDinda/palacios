
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

#ifndef __DEVICES_BLOCK_DEV_H__
#define __DEVICES_BLOCK_DEV_H__

#ifdef __V3VEE__



#define ATAPI_BLOCK_SIZE 2048
#define HD_SECTOR_SIZE 512


struct v3_hd_ops {
    uint64_t (*get_capacity)(void * private_data);
    // Reads always operate on 2048 byte blocks
    int (*read)(uint8_t * buf, int sector_count, uint64_t lba, void * private_data);
    int (*write)(uint8_t * buf, int sector_count, uint64_t lba, void * private_data);
};



struct v3_cd_ops {
    uint32_t (*get_capacity)(void * private_data);
    // Reads always operate on 2048 byte blocks
    int (*read)(uint8_t * buf, int block_count, uint64_t lba, void * private_data);
};


typedef enum {BLOCK_NONE, BLOCK_DISK, BLOCK_CDROM} v3_block_type_t;



static const char * block_dev_type_strs[] = {"NONE", "HARDDISK", "CDROM" };

static inline const char * v3_block_type_to_str(v3_block_type_t type) {
    if (type > BLOCK_CDROM) {
	return NULL;
    }
    return block_dev_type_strs[type];
}



#endif


#endif
