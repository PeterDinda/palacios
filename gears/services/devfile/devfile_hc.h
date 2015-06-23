/* 
   Device File Virtualization Guest Preload Library Helpers

   (c) Akhil Guliani and William Gross, 2015
     
   Adapted from MPI module (c) 2012 Peter Dinda

*/

#define DEV_FILE_HCALL 99993

#include "syscall_ref.h"

#ifndef __KERNEL__

#define DEBUG 1
#if DEBUG
#define DEBUG_PRINT(fmt,args...) fprintf(stderr,(fmt),##args)
#else
#define DEBUG_PRINT(fmt,args...)
#endif


int dev_file_syscall_hcall(long long syscode, 
			   long long a1, long long a2, long long a3, long long a4, 
			   long long a5, long long a6, long long *sys_errno);

#endif // KERNEL
