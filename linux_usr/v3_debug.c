/* 
 * V3 debug interface
 * (c) Jack Lange, 2012
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h> 
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "v3_ctrl.h"


int main(int argc, char* argv[]) {
    int vm_fd;
    char * vm_dev = NULL;
    struct v3_debug_cmd cmd; 

    if (argc < 4) {
	printf("usage: v3_debug <vm_device> <vm core> <cmd>\n");
	printf("This will cause debugging output to be sent to the log\n\n");
	printf("<cmds>: \n");
	printf(" 1   telemetry\n");
	printf(" 2   core state\n");
	printf(" 3   arch state\n");
	printf(" 4   stack\n");
	printf(" 5   backtrace\n");
	printf(" 100 everything\n");
	printf(" 101 telemetry+core state+arch state\n");
	return -1;
    }

    vm_dev = argv[1];
    cmd.core = atoi(argv[2]);
    cmd.cmd = atoi(argv[3]);

    printf("Debug Virtual Core %d with Command %d\n", cmd.core, cmd.cmd);

    vm_fd = open(vm_dev, O_RDONLY);

    if (vm_fd == -1) {
	printf("Error opening VM device: %s\n", vm_dev);
	return -1;
    }

    int err = ioctl(vm_fd, V3_VM_DEBUG, &cmd); 

    if (err < 0) {
	printf("Error write core migrating command to vm\n");
	return -1;
    }

    close(vm_fd); 

    return 0; 
}


