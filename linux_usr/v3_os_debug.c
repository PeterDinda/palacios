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
    fprintf(stderr,"v3_user_host_dev_example /dev/v3-vm0 user:mydev busywait|select\n");
}


int do_work(struct palacios_host_dev_host_request_response *req, 
	    struct palacios_host_dev_host_request_response **resp)
{
    uint64_t datasize;

    switch (req->type) {
    case PALACIOS_HOST_DEV_HOST_REQUEST_WRITE_IO: {
      if (req->port==0xc0c0) {
	uint64_t i;
	uint64_t numchars;
	numchars = req->data_len - sizeof(struct palacios_host_dev_host_request_response);
	for (i=0;i<numchars;i++) { 
	  putchar(req->data[i]);
	}
	*resp = malloc(sizeof(struct palacios_host_dev_host_request_response));
	if (!*resp) { 
	    printf("Cannot allocate response\n");
	    return -1;
	}
	**resp=*req;
	(*resp)->len = (*resp)->data_len = sizeof(struct palacios_host_dev_host_request_response);
	(*resp)->op_len = numchars;
      } else {
	printf("Huh?  Unknown port %d\n",req->port);
      }
    }
      break;

      default:
	printf("Huh?  Unknown request %d\n", req->type);
    }
    
    
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
