/* 
   Device File Virtualization Guest Preload Library Helpers

   (c) Akhil Guliani and William Gross, 2015
     
   Adapted from MPI module (c) 2012 Peter Dinda

*/

#ifndef __x86_64__
#define __x86_64__
#endif 

#include <stdio.h>
#include "hcall.h"
#include "devfile_hc.h"


#include "sys_point_arr.h"

int dev_file_syscall_hcall(long long sys_code,
			   long long a1, long long a2, long long a3, long long a4, 
			   long long a5, long long a6, long long *sys_errno)
{
    long long rc;
    long long cmd = DEV_FILE_HCALL;
    long long bit_vec = sys_pointer_arr[sys_code];
    // Here, IOCTL needs to be handled specially because what 
    // arguments are pointes, etc, depends on the device
    DEBUG_PRINT("Initiate syscall hypercall, code: %llu\n",sys_code);
    HCALL(rc,cmd,sys_code,a1,a2,a3,a4,a5,a6,bit_vec);
    *sys_errno = cmd;
    DEBUG_PRINT("Syscall Hypercall done: rc = %llx errno = %llx\n",rc, *sys_errno);
    return rc;
}
