#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include <malloc.h>

#include "v3_user_host_dev.h"

void usage()
{
    fprintf(stderr,"v3_host_dev_example /dev/v3-vm0 user:mydev busywait|select\n");
}


int do_work(struct palacios_host_dev_host_request_response *req, 
	    struct palacios_host_dev_host_request_response **resp)
{
    uint64_t datasize;
    
    //
    //
    // Process request here, perhaps calling these functions:
    //
    // uint64_t v3_user_host_dev_read_guest_mem(int devfd, void *gpa, void *dest, uint64_t len);
    // uint64_t v3_user_host_dev_write_guest_mem(int devfd, void *gpa, void *src, uint64_t len);
    // int      v3_user_host_dev_inject_guest_irq(int devfd, uint8_t irq);
    //
    // determine datasize - # bytes to include in response
    //
    // now built a response
    *resp = malloc(sizeof(struct palacios_host_dev_host_request_response) + datasize);
    (*resp)->data_len = sizeof(struct palacios_host_dev_host_request_response) + datasize;

    //
    // Fill out the fields of the response - notice that there are six kinds of things to response to:
    //   - read/write device port
    //   - read/write device mem
    //   - read/write device configuration space

    return 0;
}

int main(int argc, char *argv[])
{
    int devfd;
    int mode=0;
    char *vm, *url;

    if (argc!=4) { 
	usage();
	exit(-1);
    }
    
    vm=argv[1];
    url=argv[2];
    mode = argv[3][0]=='s';

    fprintf(stderr,"Attempting to rendezvous with host device %s on vm %s\n", url, vm);
    
    if ((devfd = v3_user_host_dev_rendezvous(vm,url))<0) { 
	perror("failed to rendezvous");
	exit(-1);
    }
    
    fprintf(stderr,"Rendezvous succeeded, I will now operate in %s mode\n", mode==0 ? "busywait" : "select");
    
    if (mode==0) { 
	//busywait

	struct palacios_host_dev_host_request_response *req;
	struct palacios_host_dev_host_request_response *resp;
	uint64_t datasize;

	while (1) { 
	    while (!(v3_user_host_dev_have_request(devfd))) { 
	    }
	    v3_user_host_dev_pull_request(devfd, &req);

	    do_work(req, &resp);
	    
	    v3_user_host_dev_push_response(devfd, resp);

	    free(resp);
	    free(req);
	}
    } else {

	struct palacios_host_dev_host_request_response *req;
	struct palacios_host_dev_host_request_response *resp;
	uint64_t datasize;
	fd_set   readset;
	int rc;

	// select-based operation so that you can wait for multiple things
	
	while (1) { 
	    FD_ZERO(&readset);
	    FD_SET(devfd,&readset);

	    rc = select(devfd+1, &readset, 0, 0, 0);  // pick whatever you want to select on, just include devfd

	    if (rc>0) { 
		if (FD_ISSET(devfd,&readset)) { 
		    // a request is read for us!
		    v3_user_host_dev_pull_request(devfd, &req);

		    do_work(req, &resp);
		    
		    v3_user_host_dev_push_response(devfd, resp);
		    
		    free(resp);
		    free(req);
		}
	    }
	}
    }

    v3_user_host_dev_depart(devfd);

    return 0;
		    
}
