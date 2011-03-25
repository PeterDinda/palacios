/* 
 * V3 Console utility
 * (c) Jack lange & Lei Xia, 2010
 */


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <sys/ioctl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include<linux/unistd.h>
#include <curses.h>


#include "v3_ctrl.h"

static int cons_fd = -1;
static pthread_t input_handler;

void *write_handler(void *val){
    char read;
    printf("Write handler active\n");
    fflush(stdout);
    while(1){
	read = getchar();
	if(write(cons_fd, &read, sizeof(char)) < 0){
	    printf("WRITE ERROR");
	}
    }
}


int main(int argc, char* argv[]) {
    int vm_fd;
    fd_set rset;
    char *vm_dev = NULL;
    char *stream; 

    if (argc < 2) {
	printf("Usage: ./v3_cons vm_device serial_number\n");
	return -1;
    }

    vm_dev = argv[1];
    stream = argv[2];

    vm_fd = open(vm_dev, O_RDONLY);
    if (vm_fd == -1) {
	printf("Error opening VM device: %s\n", vm_dev);
	return -1;
    }

    cons_fd = ioctl(vm_fd, V3_VM_SERIAL_CONNECT, stream); 

    /* Close the file descriptor.  */ 
    close(vm_fd); 
    if (cons_fd < 0) {
	printf("Error opening stream Console\n");
	return -1;
    }


    if(pthread_create(&input_handler,0,write_handler,0)){
	perror("pthread_create");
	exit(-1);
    }


    while (1) {
	int ret; 
	char cons_buf[1024];
	memset(cons_buf, 0, sizeof(cons_buf));
	int bytes_read = 0;

	FD_ZERO(&rset);
	FD_SET(cons_fd, &rset);
	
	ret = select(cons_fd + 1, &rset, NULL, NULL, NULL);
	
	if (ret == 1) {
	    bytes_read = read(cons_fd, cons_buf, 1024);
	    cons_buf[bytes_read]='\0';
	    printf("%s", cons_buf);
	} else {
	    printf("v3_cons ERROR: select returned %d\n", ret);
	    return -1;
	}
    } 


    return 0; 
}


