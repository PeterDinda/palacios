/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National
 * Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org>
 * All rights reserved.
 *
 * Author: Alexander Kudryavtsev <alexk@ispras.ru>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_FW_CFG_H__
#define __VMM_FW_CFG_H__

#ifdef __V3VEE__

#include <palacios/vmm_types.h>

#define FW_CFG_FILE_FIRST       0x20
#define FW_CFG_FILE_SLOTS       0x10
#define FW_CFG_MAX_ENTRY        (FW_CFG_FILE_FIRST + FW_CFG_FILE_SLOTS)

typedef void (*v3_fw_cfg_cb)(void * opaque, uint8_t * data);

struct v3_fw_cfg_entry {
    uint32_t len;
    uint8_t * data;
    void * callback_opaque;
    v3_fw_cfg_cb callback;
};


struct v3_fw_cfg_state {
    struct v3_fw_cfg_entry entries[2][FW_CFG_MAX_ENTRY];
    uint16_t cur_entry;
    uint32_t cur_offset;
};

struct v3_vm_info;


int v3_fw_cfg_init(struct v3_vm_info * vm);
void v3_fw_cfg_deinit(struct v3_vm_info * vm);

#endif

#endif
