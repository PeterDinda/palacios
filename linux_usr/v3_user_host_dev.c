#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>

#include "v3_user_host_dev.h"


int v3_user_host_dev_rendezvous(char *vmdev, char *url)
{
    int vmfd;
    int devfd;
    char buf[256];


    strcpy(buf,url);
    buf[255]=0;

    if ((vmfd=open(vmdev,O_RDWR))<0) { 
	return -1;
    }

    devfd = ioctl(vmfd,V3_VM_HOST_DEV_CONNECT,buf);
    
    close(vmfd);

    return devfd;

}
int v3_user_host_dev_depart(int devfd)
{
    return close(devfd);
}


int v3_user_host_dev_have_request(int devfd)
{
    uint64_t len;

    int rc=ioctl(devfd,V3_HOST_DEV_HOST_REQUEST_SIZE_IOCTL,&len);

    return rc==1;
}

int v3_user_host_dev_pull_request(int devfd, struct palacios_host_dev_host_request_response **req)
{
    uint64_t len;
    int rc;

    rc=ioctl(devfd,V3_HOST_DEV_HOST_REQUEST_SIZE_IOCTL,&len);

    if (rc<=0) { 
	return -1;
    } else {
	struct palacios_host_dev_host_request_response *r = malloc(len);

	if (!r) { 
	    return -1;
	}

	rc=ioctl(devfd, V3_HOST_DEV_HOST_REQUEST_PULL_IOCTL,r);
	
	if (rc<=0) { 
	    free(r);
	    return -1;
	} else {
	    *req=r;
	    return 0;
	}
    }
}
		

int v3_user_host_dev_push_response(int devfd, struct palacios_host_dev_host_request_response *resp)
{
    int rc;

    rc=ioctl(devfd, V3_HOST_DEV_USER_RESPONSE_PUSH_IOCTL,resp);
	
    if (rc<=0) { 
	return -1;
    } else {
	return 0;
    }
}
		


static uint64_t do_user(int devfd, struct palacios_host_dev_user_op *op)
{
    return ioctl(devfd, V3_HOST_DEV_USER_REQUEST_PUSH_IOCTL,op);
}

uint64_t v3_user_host_dev_read_guest_mem(int devfd, void *gpa, void *dest, uint64_t len)
{
    struct palacios_host_dev_user_op op;

    op.type= PALACIOS_HOST_DEV_USER_REQUEST_READ_GUEST;
    op.gpa=gpa;
    op.data=dest;
    op.len=len;
    op.irq=0;
    
    return do_user(devfd,&op);
}

uint64_t v3_user_host_dev_write_guest_mem(int devfd, void *gpa, void *src, uint64_t len)
{
    struct palacios_host_dev_user_op op;

    op.type= PALACIOS_HOST_DEV_USER_REQUEST_WRITE_GUEST;
    op.gpa=gpa;
    op.data=src;
    op.len=len;
    op.irq=0;
    
    return do_user(devfd,&op);
}

int      v3_user_host_dev_inject_irq(int devfd, uint8_t irq)
{
    struct palacios_host_dev_user_op op;

    op.type= PALACIOS_HOST_DEV_USER_REQUEST_IRQ_GUEST;
    op.gpa=0;
    op.data=0;
    op.len=0;
    op.irq=irq;
    
    return do_user(devfd,&op);
}



