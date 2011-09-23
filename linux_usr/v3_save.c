/* 
 * V3 checkpoint save utility
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


#define MAX_STORE_LEN 128
#define MAX_URL_LEN 256


struct v3_chkpt_info chkpt;

int main(int argc, char* argv[]) {
    int vm_fd;
    char * vm_dev = NULL;

    if (argc < 4) {
	printf("Usage: ./v3_save <vm_device> <store> <url>\n");
	return -1;
    }

    vm_dev = argv[1];

    if (strlen(argv[2]) >= MAX_STORE_LEN) {
	printf("ERROR: Checkpoint store name longer than maximum size (%d)\n", MAX_STORE_LEN);
	return -1;
    }

    strncpy(chkpt.store, argv[2], MAX_STORE_LEN);


    if (strlen(argv[3]) >= MAX_URL_LEN) {
	printf("ERROR: Checkpoint URL longer than maximum size (%d)\n", MAX_URL_LEN);
	return -1;
    }

    strncpy(chkpt.url, argv[3], MAX_URL_LEN);

    vm_fd = open(vm_dev, O_RDONLY);
    if (vm_fd == -1) {
	printf("Error opening VM device: %s\n", vm_dev);
	return -1;
    }

    ioctl(vm_fd, V3_VM_SAVE, &chkpt); 

    /* Close the file descriptor.  */ 
    close(vm_fd);
 
    return 0; 
}


