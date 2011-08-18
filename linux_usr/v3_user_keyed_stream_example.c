#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include <malloc.h>

#define sint64_t int64_t

#include "v3_user_keyed_stream.h"

void usage()
{
    fprintf(stderr,"v3_user_keyed_stream_example /dev/v3-vm0 user:mystreamtype:mystream busywait|select\n");
}


int do_work(struct palacios_user_keyed_stream_op *req, 
	    struct palacios_user_keyed_stream_op **resp)
{
    uint64_t datasize;
    
    //
    //
    // Process request here
    // 
    // req->len      : total structure length
    // req->type     : request type (currently open/close key and read/write key
    // req->xfer     : unused
    // req->user_key : the opaque key previously provided by you by an open key
    // req->buf_len  : length of data
    // req->buf      : buffer (contains key name (open key) or value (write key))
    //

    // now built a response
    *resp = malloc(sizeof(struct palacios_user_keyed_stream_op) + datasize);
    (*resp)->len = sizeof(struct palacios_user_keyed_stream_op) + datasize;
    (*resp)->buf_len = datasize;
    (*resp)->type = req->type;
    (*resp)->user_key = req->user_key;

    //
    // The response 
    // 
    // resp->len      : total structure length 
    // resp->type     : response type - must match the request
    // resp->xfer     : contains the size of data read or written (in read key or write key)
    // resp->user_key : unused
    // resp->buf_len  : length of data following
    // resp->buf      : buffer (contains the data (read key))
    

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

    // The URL should begin with user:
    // the remainder can be used to demultiplex internally
    // for example user:file:foo might refer to a user-side file-based implementation
    //

    if (strncmp(url,"user:",5)) { 
	fprintf(stderr, "URL %s is not a user: url\n");
	exit(-1);
    }

    fprintf(stderr,"Attempting to attach to vm %s as url %s\n", vm, url);
    
    if ((devfd = v3_user_keyed_stream_attach(vm,url))<0) { 
	perror("failed to attach");
	exit(-1);
    }
    
    fprintf(stderr,"Attachment succeeded, I will now operate in %s mode\n", mode==0 ? "busywait" : "select");
    
    if (mode==0) { 
	//busywait

	struct palacios_user_keyed_stream_op *req;
	struct palacios_user_keyed_stream_op *resp;
	uint64_t datasize;

	while (1) { 
	    while (!(v3_user_keyed_stream_have_request(devfd))) { 
	    }
	    v3_user_keyed_stream_pull_request(devfd, &req);

	    do_work(req, &resp);
	    
	    v3_user_keyed_stream_push_response(devfd, resp);

	    free(resp);
	    free(req);
	}
    } else {

	struct palacios_user_keyed_stream_op *req;
	struct palacios_user_keyed_stream_op *resp;
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
		    v3_user_keyed_stream_pull_request(devfd, &req);

		    do_work(req, &resp);
		    
		    v3_user_keyed_stream_push_response(devfd, resp);
		    
		    free(resp);
		    free(req);
		}
	    }
	}
    }

    v3_user_keyed_stream_detatch(devfd);

    return 0;
		    
}
