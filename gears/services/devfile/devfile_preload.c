/* 
   Device File Virtualization Guest Preload Library

   (c) Akhil Guliani and William Gross, 2015
     
   Adapted from MPI module (c) 2012 Peter Dinda

*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>



#include "devfile_hc.h"
#include "devfile_guest_fd_tracker.h"

static int __LocalFD = 0;
static int __last_open_index = -1;


unsigned long long syscall();

int touch_ptr(volatile char* ptr, int size)
{
    int i;
    for(i=0;i<size;i+=4096){
        ptr[i] = ptr[i];
    }
    return 0;
}

int open(const char *path, int flags, mode_t mode)
{

    DEBUG_PRINT("path %s %d %d\n", path, flags, mode);
 
    __last_open_index = check_name(path,dtrack);

    if (__last_open_index>=0) {
        DEBUG_PRINT("In our file, \n");
        __LocalFD = dtrack[__last_open_index].devFD; 
        if(__LocalFD >= 0) {
            return __LocalFD;
        } else {
            // Execute open hypercall here
            char buf[8192];
            char* my_path;
            int path_length = strlen(path)+1; 
            //check if path is across a page boundary
            //if so try to copy into a local buffer, that doesn't cross a page boundary
            //if its greater than 4k, fail 
            if (path_length > 4096){
                DEBUG_PRINT("Path greater than 4k, failing open\n");
                return -1;
            }
            //forcing path to be in a single page
            my_path = (char*)((((unsigned long long)buf+4096)/4096)*4096);
            strcpy(my_path,path); // my_path is touched via this
	    long long sys_errno;
            int rcFD =  dev_file_syscall_hcall(SYS64_OPEN,(long long)my_path,flags,0 ,0,0,0,&sys_errno);
            DEBUG_PRINT(" ReturnFD : %d; \n",  rcFD);
            __LocalFD = rcFD;
	    // set global errno so caller can see
	    errno=sys_errno;
            return rcFD;
        }
    } else {
        DEBUG_PRINT("performing original open\n");
        
        return syscall(SYS64_OPEN, path, flags, mode);
    }
}

size_t read(int fd, void *buf, size_t count)
{
    //does buf+count extend pass page boundary?
    //if so, only do up to page boundary
    // if( (buf%4096)+count <= 4096)
    //     normal

    if (fd == __LocalFD || (check_fd(fd,dtrack)>=0)){
        long long sys_errno;
        //Need to handle only up to a page boundary, can't handle contiguous pages right now
        count = MIN(count,4096-((unsigned long long)buf%4096));
	touch_ptr(buf,count);
        int ret =  dev_file_syscall_hcall(SYS64_READ,fd,(long long)buf,count,0,0,0,&sys_errno);
	DEBUG_PRINT("Read of %lu bytes returned %d (errno %lld)\n",count,ret,sys_errno);
	errno = sys_errno;
        return ret;
    } else {
        DEBUG_PRINT("performing original read\n");
        return syscall(SYS64_READ,fd, buf, count);
    }
}

ssize_t write(int fd, const void *buf, size_t count)
{
    if (fd == __LocalFD || (check_fd(fd,dtrack)>=0)) {
        long long sys_errno;
        //Need to handle only up to a page boundary, can't handle contiguous pages right now
        count = MIN(count,4096-((unsigned long long)buf%4096));
	touch_ptr((char*)buf,count);
        int ret =  dev_file_syscall_hcall(SYS64_WRITE,fd, (long long)buf,count,0,0,0,&sys_errno);
	DEBUG_PRINT("Write of %lu bytes returned %d (errno %lld)\n",count,ret,sys_errno);
	errno=sys_errno;
        return ret;
    } else {
        DEBUG_PRINT("performing original write\n");
        return syscall(SYS64_WRITE,fd, buf, count);
    }
}

int close(int fd)
{
    int index = check_fd(fd,dtrack); 
    if (fd == __LocalFD || (index>=0)) {
	long long sys_errno;
        int ret =  dev_file_syscall_hcall(SYS64_CLOSE,fd,0,0,0,0,0,&sys_errno);
        if (ret >=0) {
            dtrack[index].devFD = -1;
            __LocalFD = -1;
        }
	DEBUG_PRINT("Close returned %d (errno %lld)\n",ret, sys_errno);
	errno=sys_errno;
        return ret;
    } else {
        DEBUG_PRINT("performing original close\n");
        return syscall(SYS64_CLOSE,fd);
    }
}


int ioctl(int fd, int request, void* data) {
    //int sign_post;
    int index = check_fd(fd,dtrack);
    
    // this is where a careful, device-specific decode is needed
    // to figure out whether the argument or anything it points
    // is a pointer.  
    if (fd == __LocalFD || (index>=0)) {
	long long sys_errno=0;
	int ret;
	// First cut for just the data pointer:
	//   if (data > &sign_post){
	//probably on stack
	//    }
	// if (data < brk()
	ret = dev_file_syscall_hcall(SYS64_IOCTL,fd,request,(unsigned long long)data,0,0,0,&sys_errno);

	DEBUG_PRINT("ioctl(%d) returned %d (errno %lld)\n",request,ret, sys_errno);
	errno=sys_errno;
	return ret;
    }
    else{
        return syscall(SYS64_IOCTL,fd,request,data);
    }
    return -1;

}

