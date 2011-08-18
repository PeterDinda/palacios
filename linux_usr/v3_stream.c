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

#define BUF_LEN 1025
#define STREAM_NAME_LEN 128

int main(int argc, char* argv[]) {
    int vm_fd;
    fd_set rset;
    char * vm_dev = NULL;
    char stream[STREAM_NAME_LEN];
    char cons_buf[BUF_LEN];
    int stream_fd = 0;

    if (argc < 2) {
	printf("Usage: ./v3_cons vm_device serial_number\n");
	return -1;
    }

    vm_dev = argv[1];

    if (strlen(argv[2]) >= STREAM_NAME_LEN) {
	printf("ERROR: Stream name longer than maximum size (%d)\n", STREAM_NAME_LEN);
	return -1;
    }

    memcpy(stream, argv[2], strlen(argv[2]));

    vm_fd = open(vm_dev, O_RDONLY);
    if (vm_fd == -1) {
	printf("Error opening VM device: %s\n", vm_dev);
	return -1;
    }

    stream_fd = ioctl(vm_fd, V3_VM_SERIAL_CONNECT, stream); 

    /* Close the file descriptor.  */ 
    close(vm_fd);
 
    if (stream_fd < 0) {
	printf("Error opening stream Console\n");
	return -1;
    }

    while (1) {
	int ret; 
	int bytes_read = 0;
	char in_buf[512];

	memset(cons_buf, 0, BUF_LEN);


	FD_ZERO(&rset);
	FD_SET(stream_fd, &rset);
	FD_SET(STDIN_FILENO, &rset);

	ret = select(stream_fd + 1, &rset, NULL, NULL, NULL);
	
	if (ret == 0) {
	    continue;
	} else if (ret == -1) {
	    perror("Select returned error\n");
	    return -1;
	}

	if (FD_ISSET(stream_fd, &rset)) {

	    bytes_read = read(stream_fd, cons_buf, BUF_LEN - 1);

	    cons_buf[bytes_read]='\0';
	    printf("%s", cons_buf);
	    fflush(stdout);

	} else if (FD_ISSET(STDIN_FILENO, &rset)) {
	    fgets(in_buf, 512, stdin);

	    if (write(stream_fd, in_buf, strlen(in_buf)) != strlen(in_buf)) {
		fprintf(stderr, "Error sending input bufer\n");
		return -1;
	    }
	} else {
	    printf("v3_cons ERROR: select returned %d\n", ret);
	    return -1;
	}
	

    } 


    return 0; 
}


