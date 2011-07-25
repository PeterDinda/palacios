#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include <malloc.h>
#include <alloca.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#define sint64_t int64_t

#include "v3_user_keyed_stream.h"

void usage()
{
    fprintf(stderr,"v3_user_keyed_stream_file /dev/v3-vm0 user:file:stream\n");
}

char *dir;

int dir_setup(char *dir)
{
    DIR *d;
    int created=0;
    int fd;
    
    char buf[strlen(dir)+strlen("/.palacios_keyed_stream_user_file")+1];


    strcpy(buf,dir);
    strcat(buf,"/.palacios_keyed_stream_user_file");

    d=opendir(dir);

    // does the directory exist or can we create it
    if (d) { 
	closedir(d);
    } else {
	if (mkdir(dir,0700)<0) { 
	    perror("cannot create directory");
	    return -1;
	} else {
	    created=1;
	}
    }

    // can we write to it?

    fd = open(buf,O_RDWR | O_CREAT,0600);

    if (fd<0) { 
	perror("cannot write directory");
	if (created) { 
	    rmdir(dir);
	}
	return -1;
    }

    // ok, we are done

    close(fd);

    return 0;
}
    


int handle_open_key(struct palacios_user_keyed_stream_op *req, 
		    struct palacios_user_keyed_stream_op **resp,
		    char *dir)
{
    int fd;
    char fn[strlen(dir)+req->buf_len+1];

    strcpy(fn,dir);
    strcat(fn,"/");
    strcat(fn,req->buf);


    fd = open(fn,O_RDWR | O_CREAT,0600);
    
    (*resp) = malloc(sizeof(struct palacios_user_keyed_stream_op)+0);
    
    if (!(*resp)) {
	return -1;
    }

    (*resp)->len=sizeof(struct palacios_user_keyed_stream_op);
    (*resp)->type=req->type;
    (*resp)->xfer=0;
    (*resp)->user_key=(void*)fd;
    (*resp)->buf_len=0;
    
    return 0;

}

int handle_close_key(struct palacios_user_keyed_stream_op *req, 
		     struct palacios_user_keyed_stream_op **resp,
		     char *dir)
{
    int fd;
    int rc;
 
    fd = (int) (req->user_key);

    rc = close(fd);

    (*resp) = malloc(sizeof(struct palacios_user_keyed_stream_op)+0);
    
    if (!(*resp)) {
	return -1;
    }

    (*resp)->len=sizeof(struct palacios_user_keyed_stream_op);
    (*resp)->type=req->type;
    (*resp)->xfer=rc;
    (*resp)->user_key=(void*)fd;
    (*resp)->buf_len=0;
    
    return 0;

}

int read_all(int fd, char *buf, int len)
{
    int xfer;
    int left;

    left=len;

    while (left>0) { 
	xfer=read(fd, buf+len-left,left);
	if (xfer<0) {
	    perror("cannot read file");
	    return -1;
	} else {
	    left-=xfer;
	}
    }
    return len;
}

int write_all(int fd, char *buf, int len)
{
    int xfer;
    int left;

    left=len;

    while (left>0) { 
	xfer=write(fd, buf+len-left,left);
	if (xfer<0) {
	    perror("cannot write file");
	    return -1;
	} else {
	    left-=xfer;
	}
    }
    return len;
}


int handle_write_key(struct palacios_user_keyed_stream_op *req, 
		     struct palacios_user_keyed_stream_op **resp,
		     char *dir)
{
    int fd;
    int rc;
 
    fd = (int) (req->user_key);

    rc = write_all(fd,req->buf,req->xfer);

    (*resp) = malloc(sizeof(struct palacios_user_keyed_stream_op)+0);
    
    if (!(*resp)) {
	return -1;
    }

    (*resp)->len=sizeof(struct palacios_user_keyed_stream_op);
    (*resp)->type=req->type;
    (*resp)->xfer=rc;
    (*resp)->user_key=(void*)fd;
    (*resp)->buf_len=0;
    

    return 0;

}

int handle_read_key(struct palacios_user_keyed_stream_op *req, 
		    struct palacios_user_keyed_stream_op **resp,
		    char *dir)
{
    int fd;
    int rc;
 
    fd = (int) (req->user_key);

    (*resp) = malloc(sizeof(struct palacios_user_keyed_stream_op)+req->xfer);
    
    if (!(*resp)) {
	return -1;
    }

    rc = read_all(fd,(*resp)->buf,req->xfer);

    (*resp)->len=sizeof(struct palacios_user_keyed_stream_op) + (rc>0 ? rc : 0);
    (*resp)->type=req->type;
    (*resp)->xfer=rc;
    (*resp)->user_key=(void*)fd;
    (*resp)->buf_len=rc>0 ? rc : 0;
    

    return 0;

}




int handle_request(struct palacios_user_keyed_stream_op *req, 
		   struct palacios_user_keyed_stream_op **resp,
		   char *dir)
{
    uint64_t datasize;
    
    switch (req->type) { 
	case PALACIOS_KSTREAM_OPEN:
	case PALACIOS_KSTREAM_CLOSE:
	    fprintf(stderr,"unsupported stream open or close\n");
	    return -1;
	    break;

	case PALACIOS_KSTREAM_OPEN_KEY:
	    return handle_open_key(req,resp,dir);
	    break;
	case PALACIOS_KSTREAM_CLOSE_KEY:
	    return handle_close_key(req,resp,dir);
	    break;
	case PALACIOS_KSTREAM_READ_KEY:
	    return handle_read_key(req,resp,dir);
	    break;
	case PALACIOS_KSTREAM_WRITE_KEY:
	    return handle_write_key(req,resp,dir);
	    break;
	default:
	    fprintf(stderr,"unknown request type\n");
	    return -1;
	    break;
    }

    return 0;
}


int run(int devfd, char *dir) 
{ 
    struct palacios_user_keyed_stream_op *req;
    struct palacios_user_keyed_stream_op *resp;
    fd_set   readset;
    int rc;
    
    while (1) { 
	FD_ZERO(&readset);
	FD_SET(devfd,&readset);
	
	rc = select(devfd+1, &readset, 0, 0, 0);  
	
	if (rc>0) { 
	    if (FD_ISSET(devfd,&readset)) { 

		int err;

		if (v3_user_keyed_stream_pull_request(devfd, &req)) { 
		    fprintf(stderr, "could not get request\n");
		    free(req);
		    return -1;
		}
		
		err=handle_request(req, &resp, dir);

		if (v3_user_keyed_stream_push_response(devfd, resp)) { 
		    fprintf(stderr,"could not send response\n");
		    free(req);
		    free(resp);
		    return -1;
		}

		if (err) { 
		    fprintf(stderr, "request handling resulted in an error, continuing\n");
		}
		
		free(req);
		free(resp);
	    }
	}
    }

    return 0;
}



int main(int argc, char *argv[])
{
    int devfd;
    char *vm, *url;
    char *dir;

    if (argc!=3) { 
	usage();
	exit(-1);
    }
    
    vm=argv[1];
    url=argv[2];

    if (strncmp(url,"user:file:",10)) { 
	fprintf(stderr, "Url %s is not a user:file: url\n",url);
	exit(-1);
    }

    dir = url+10;

    if (dir_setup(dir)) { 
	fprintf(stderr,"Unable to open or create directory %s\n",dir);
	return -1;
    }

    fprintf(stderr,"Attempting to attach to vm %s as url %s\n", vm, url);
    
    if ((devfd = v3_user_keyed_stream_attach(vm,url))<0) { 
	perror("failed to attach");
	exit(-1);
    }
    
    fprintf(stderr,"Attached and running\n");

    run(devfd,dir);

    v3_user_keyed_stream_detach(devfd);
    
    return 0;

}

