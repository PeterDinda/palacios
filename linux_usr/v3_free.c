/* 
 * V3 Control utility
 * (c) Jack lange, 2011
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
    int vm_fd = 0;
    unsigned long vm_idx = 0;


    if (argc <= 1) {
	printf("Usage: ./v3_free <vm-dev-idx>\n");
	return -1;
    }


    vm_idx = atoi(argv[1]);

    printf("Freeing VM %d\n", vm_idx);
    
    vm_fd = open("/dev/v3vee", O_RDONLY);

    if (vm_fd == -1) {
	printf("Error opening V3Vee VM device\n");
	return -1;
    }

    ioctl(vm_fd, V3_FREE_GUEST, vm_idx); 



    /* Close the file descriptor.  */ 
    close(vm_fd); 
 


    return 0; 
} 


