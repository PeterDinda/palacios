#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>

#include "v3_user_keyed_stream.h"


int v3_user_keyed_stream_attach(char *vmdev, char *url)
{
    int vmfd;
    int devfd;

    struct palacios_user_keyed_stream_url *u;

    u=malloc(sizeof(struct palacios_user_keyed_stream_url)+strlen(url)+1);

    if (!u) { 
	return -1;
    }

    strcpy(u->url,url);
    u->len = strlen(url)+1;


    if ((vmfd=open(vmdev,O_RDWR))<0) { 
	free(u);
	return -1;
    }

    devfd = ioctl(vmfd,V3_VM_KSTREAM_USER_CONNECT,u);
    
    close(vmfd);

    free(u);

    return devfd;

}
int v3_user_keyed_stream_detach(int devfd)
{
    return close(devfd);
}


int v3_user_keyed_stream_have_request(int devfd)
{
    uint64_t len;

    int rc=ioctl(devfd,V3_KSTREAM_REQUEST_SIZE_IOCTL,&len);

    return rc==1;
}

int v3_user_keyed_stream_pull_request(int devfd, struct palacios_user_keyed_stream_op **req)
{
    uint64_t len;
    int rc;

    rc=ioctl(devfd,V3_KSTREAM_REQUEST_SIZE_IOCTL,&len);

    if (rc<=0) { 
	return -1;
    } else {
	struct palacios_user_keyed_stream_op *r = malloc(len);

	if (!r) { 
	    fprintf(stderr,"malloc failed\n");
	    return -1;
	}

	rc=ioctl(devfd, V3_KSTREAM_REQUEST_PULL_IOCTL,r);

	
	if (rc<=0) { 
	    free(r);
	    return -1;
	} else {
	    *req=r;
	    return 0;
	}
    }
}
		

int v3_user_keyed_stream_push_response(int devfd, struct palacios_user_keyed_stream_op *resp)
{
    int rc;

    rc=ioctl(devfd,V3_KSTREAM_RESPONSE_PUSH_IOCTL,resp);

    if (rc<=0) { 
	return -1;
    } else {
	return 0;
    }
}
		




