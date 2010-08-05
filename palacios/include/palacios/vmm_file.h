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

#define V3_FileOpen(path, mode)						\
    ({									\
	extern struct v3_file_hooks *file_hooks;				\
	((file_hooks) && (file_hooks)->file_open) ?				\
	    (file_hooks)->file_open((path), (mode)) : -1 ;		\
    })

#define V3_FileClose(fd)						\
    ({									\
	extern struct v3_file_hooks *file_hooks;				\
	((file_hooks) && (file_hooks)->file_close) ?				\
	    (file_hooks)->file_close((fd))  :  -1 ;	\
    })

#define V3_FileSize(fd)						\
    ({									\
	extern struct v3_file_hooks *file_hooks;				\
	((file_hooks) && (file_hooks)->file_size) ?				\
	    (file_hooks)->file_size((fd))  : -1 ;	\
    })

#define V3_FileRead(fd,start,buf,len)					\
    ({									\
	extern struct v3_file_hooks *file_hooks;				\
	((file_hooks) && (file_hooks)->file_read) ?				\
	    (file_hooks)->file_read((fd),(start),(buf),(len)) : -1 ;  \
    })

#define V3_FileWrite(fd,start,buf,len)					\
    ({									\
	extern struct v3_file_hooks *file_hooks;				\
	((file_hooks) && (file_hooks)->file_write) ?				\
	    (file_hooks)->file_write((fd),(start),(buf),(len)) : -1 ;  \
    })


#endif

#define FILE_OPEN_MODE_READ	(1 << 0)
#define FILE_OPEN_MODE_WRITE	(1 << 1)

struct v3_file_hooks {

    int (*file_open)(const char *path, int mode);
    int (*file_close)(int fd);

    long long (*file_size)(int fd);

    // blocking reads and writes
    long long  (*file_read)(int fd,  long long start, void *buffer, long long length);
    long long  (*file_write)(int fd, long long start, void *buffer, long long length);

};


extern void V3_Init_File(struct v3_file_hooks * hooks);

#endif
