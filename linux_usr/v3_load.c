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
	printf("usage: v3_load <vm_device> <store> <url> [optionmask]\n");
	printf(" optionmask consists of the sum of any of the following\n");
	printf(" 0    none\n");
	printf(" 1    skip memory\n");
	printf(" 2    skip devices\n");
	printf(" 4    skip cores\n");
	printf(" 8    skip architecture-specific core state\n");
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

    if (argc>4) {
      chkpt.opts = atoll(argv[4]);
    } else {
      chkpt.opts = V3_CHKPT_OPT_NONE;
    }

    vm_fd = open(vm_dev, O_RDONLY);
    if (vm_fd == -1) {
	printf("Error opening VM device: %s\n", vm_dev);
	return -1;
    }

    ioctl(vm_fd, V3_VM_LOAD, &chkpt); 

    /* Close the file descriptor.  */ 
    close(vm_fd);
 
    return 0; 
}


