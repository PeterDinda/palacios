/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2010, Peter Dinda <pdinda@northwestern.edu> 
 * Copyright (c) 2010, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <interfaces/vmm_file.h>
#include <palacios/vmm.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_types.h>
#include <palacios/vm_guest.h>

static struct v3_file_hooks * file_hooks = NULL;

void V3_Init_File(struct v3_file_hooks * hooks) {
    file_hooks = hooks;
    PrintDebug("V3 file access inited\n");

    return;
}


int v3_mkdir(char * path, uint16_t permissions, uint8_t recursive) {
    V3_ASSERT(file_hooks);
    V3_ASSERT(file_hooks->mkdir);
    
    return file_hooks->mkdir(path, permissions, recursive);
}

v3_file_t v3_file_open(struct v3_vm_info * vm, char * path, uint8_t mode) {
    void * priv_data = NULL;
    V3_ASSERT(file_hooks);
    V3_ASSERT(file_hooks->open);
    
    if (vm) {
	priv_data = vm->host_priv_data;
    }

    return file_hooks->open(path, mode, priv_data);
}

int v3_file_close(v3_file_t file) {
    V3_ASSERT(file_hooks);
    V3_ASSERT(file_hooks->close);
    
    return file_hooks->close(file);
}

uint64_t v3_file_size(v3_file_t file) {
    V3_ASSERT(file_hooks);
    V3_ASSERT(file_hooks->size);
    
    return file_hooks->size(file);
}

uint64_t v3_file_read(v3_file_t file, uint8_t * buf, uint64_t len, uint64_t off) {
    V3_ASSERT(file_hooks);
    V3_ASSERT(file_hooks->read);
    
    return file_hooks->read(file, buf, len, off);
}


uint64_t v3_file_write(v3_file_t file, uint8_t * buf, uint64_t len, uint64_t off) {
    V3_ASSERT(file_hooks);
    V3_ASSERT(file_hooks->write);
    
    return file_hooks->write(file, buf, len, off);
}
