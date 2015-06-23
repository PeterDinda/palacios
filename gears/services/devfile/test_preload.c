/* 
   Device File Virtualization Guest Preload Library 
   Test Program

   (c) Akhil Guliani and William Gross, 2015
     
   Adapted from MPI module (c) 2012 Peter Dinda

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <malloc.h>

#include <sys/mman.h>
#include "devfile_hc.h"


int read_all(int fd, char *data, int numbytes) 
{
    int i;
    int rc;

    for (i=0;i<numbytes;) {
	rc = read(fd,data+i,(numbytes-i));
	if (rc<=0) {
	    return -1;
	} else {
	    i+=rc;
	}
    }
    return numbytes;
}

int write_all(int fd, char *data, int numbytes) 
{
    int i;
    int rc;

    for (i=0;i<numbytes;) {
	rc = write(fd,data+i,(numbytes-i));
	if (rc<=0) {
	    return -1;
	} else {
	    i+=rc;
	}
    }
    return numbytes;
}


int main(int argc, char * argv[])
{
    int fd, bytes, count, flags, mode;
    char *buff;
    char *path;
    char *what;
	
    if (argc!=4) {
	fprintf(stderr,"test_preload r <count> <path-to-file> > output\n");
	fprintf(stderr,"test_preload w <count> <path-to-file> < input\n");
	return -1;
    }

    what = argv[1];
    count = atoi(argv[2]);
    path = argv[3];
    
    fprintf(stderr,"what:  %s\n",what);
    fprintf(stderr,"path:  %s\n",path);
    fprintf(stderr,"count: %d\n",count);
    
    if (*what=='w') {
	flags = O_RDWR;
	mode = 0; // we are not doing a file creation
    } else if (*what=='r') {
        flags = O_RDONLY;
        mode = 0; // we are not doing a file creation
    } else {
	fprintf(stderr,"Don't know how to %s\n",what);
	return -1;
    }
    
    fprintf(stderr,"flags: %d, mode: %d\n",flags,mode);
    
    if ((fd = open(path, flags, mode)) < 0) {
        fprintf(stderr,"Failed to open file %s\n",argv[3]);
        return -1;
    }
    
    fprintf(stderr,"Open Done, fd : %d\n",fd);

    buff = (char*)malloc(count);
    
    if (!buff) {
        perror("Can't allocate\n");
        return -1;
    }
    
    if (*what=='r') { 
	fprintf(stderr,"READ: fd: %d, buff: %p, bytes: %d \n", fd, buff, count);
    
	bytes = read_all(fd,buff,count);

	if (bytes < 0) {
	    fprintf(stderr,"Failed to read file %s\n",path);
	    free(buff);
	    close(fd);
	    return -1;
	}

	fprintf(stderr,"WRITE: fd: %d, buff: %p, bytes: %d \n", 1, buff, count);
    
	bytes = write_all(1,buff,count);

	if (bytes<0) { 
	    fprintf(stderr,"Failed to write stdout\n");
	    free(buff);
	    close(fd);
	    return -1;
	}

    } else if (*what=='w') {

	fprintf(stderr,"READ: fd: %d, buff: %p, bytes: %d \n", 0, buff, count);
    
	bytes = read_all(0,buff,count);

	if (bytes < 0) {
	    fprintf(stderr,"Failed to read stdin\n");
	    free(buff);
	    close(fd);
	    return -1;
	}

	fprintf(stderr,"WRITE: fd: %d, buff: %p, bytes: %d \n", fd, buff, count);
    
	bytes = write_all(fd,buff,count);
	
	if (bytes<0) { 
	    fprintf(stderr,"Failed to write stdout\n");
	    free(buff);
	    close(fd);
	    return -1;
	}

    }

    free(buff);
    close(fd);
    fprintf(stderr,"Close done\n");
    return 0;
}
