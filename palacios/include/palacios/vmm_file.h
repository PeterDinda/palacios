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

#define V3_FileOpen(path, mode, host_data)				\
    ({									\
	void * fd = NULL;						\
	extern struct v3_file_hooks * file_hooks;			\
	if ((file_hooks) && (file_hooks)->file_open) {			\
	    fd = (file_hooks)->file_open((path), (mode), (host_data));	\
	}								\
	fd;								\
    })

#define V3_FileClose(fd)						\
    ({									\
	int ret = 0;							\
	extern struct v3_file_hooks * file_hooks;			\
	if ((file_hooks) && (file_hooks)->file_close) {			\
	    ret = (file_hooks)->file_close((fd));			\
	}								\
	ret;								\
    })

#define V3_FileSize(fd)							\
    ({									\
	long long size = -1;						\
	extern struct v3_file_hooks * file_hooks;			\
	if ((file_hooks) && (file_hooks)->file_size) {			\
	    size = (file_hooks)->file_size((fd));			\
	}								\
	size;								\
    })

#define V3_FileRead(fd, start, buf, len)				\
    ({									\
	long long ret = -1;						\
	extern struct v3_file_hooks * file_hooks;			\
	if ((file_hooks) && (file_hooks)->file_read) {			\
	    ret = (file_hooks)->file_read((fd), (buf), (len), (start));	\
	}								\
	ret;								\
    })

#define V3_FileWrite(fd,start,buf,len)					\
    ({									\
	long long ret = -1;						\
	extern struct v3_file_hooks * file_hooks;			\
	if ((file_hooks) && (file_hooks)->file_write) {			\
	    ret = (file_hooks)->file_write((fd), (buf), (len), (start)); \
	}								\
	ret;								\
    })


#endif

#define FILE_OPEN_MODE_READ	(1 << 0)
#define FILE_OPEN_MODE_WRITE	(1 << 1)

struct v3_file_hooks {

    void * (*file_open)(const char * path, int mode, void * host_data);
    int (*file_close)(void * fd);

    long long (*file_size)(void * fd);

    // blocking reads and writes
    long long (*file_read)(void * fd, void * buffer, long long length, long long offset);
    long long (*file_write)(void * fd, void * buffer, long long length, long long offset);

};


extern void V3_Init_File(struct v3_file_hooks * hooks);

#endif
