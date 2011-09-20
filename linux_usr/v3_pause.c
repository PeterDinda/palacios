/* 
 * V3 Control utility
 * (c) Jack lange, 2010
 */


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <sys/ioctl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <unistd.h> 
#include <string.h>
 
#include "v3_ctrl.h"

int read_file(int fd, int size, unsigned char * buf);

int main(int argc, char* argv[]) {
    char * filename = argv[1];
    int vm_fd = 0;
 

    if (argc <= 1) {
	printf("Usage: ./v3_stop <vm-dev>\n");
	return -1;
    }

    printf("Stopping VM\n");
    
    vm_fd = open(filename, O_RDONLY);

    if (vm_fd == -1) {
	printf("Error opening V3Vee VM device\n");
	return -1;
    }

    ioctl(vm_fd, 23, NULL); 



    /* Close the file descriptor.  */ 
    close(vm_fd); 
 


    return 0; 
} 


