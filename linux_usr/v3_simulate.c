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


int main(int argc, char* argv[]) {
    char * filename = argv[1];
    unsigned int msecs = atoi(argv[2]);
    int vm_fd = 0;
    

    if (argc <= 2) {
	printf("Usage: ./v3_simulate <vm-dev> <msecs>\n");
	return -1;
    }

    printf("Simulating VM for %lu msecs\n", msecs);
    
    vm_fd = open(filename, O_RDONLY);

    if (vm_fd == -1) {
	printf("Error opening V3Vee VM device\n");
	return -1;
    }

    ioctl(vm_fd, 29, msecs); 

    /* Close the file descriptor.  */ 
    close(vm_fd); 
 


    return 0; 
} 


