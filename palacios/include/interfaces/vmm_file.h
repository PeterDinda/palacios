/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2010, Peter Dinda (pdinda@cs.northwestern.edu> 
 * Copyright (c) 2010, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#ifndef __VMM_FILE_H__
#define __VMM_FILE_H__

#include <palacios/vmm.h>


#ifdef __V3VEE__
typedef void * v3_file_t;

int v3_mkdir(char * path, uint16_t permissions, uint8_t recursive);


v3_file_t v3_file_open(struct v3_vm_info * vm, char * path, uint8_t mode);
int v3_file_close(v3_file_t file);
uint64_t v3_file_size(v3_file_t file);

uint64_t v3_file_read(v3_file_t file, uint8_t * buf, uint64_t len, uint64_t off);
uint64_t v3_file_write(v3_file_t file, uint8_t * buf, uint64_t len, uint64_t off);

#endif

#define FILE_OPEN_MODE_READ	(1 << 0)
#define FILE_OPEN_MODE_WRITE	(1 << 1)
#define FILE_OPEN_MODE_CREATE        (1 << 2)

struct v3_file_hooks {
    int (*mkdir)(const char * path, unsigned short perms, int recursive);

    void * (*open)(const char * path, int mode, void * host_data);
    int (*close)(void * fd);

    long long (*size)(void * fd);

    // blocking reads and writes
    long long (*read)(void * fd, void * buffer, long long length, long long offset);
    long long (*write)(void * fd, void * buffer, long long length, long long offset);

};


extern void V3_Init_File(struct v3_file_hooks * hooks);

#endif
