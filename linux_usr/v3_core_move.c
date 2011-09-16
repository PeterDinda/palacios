/* 
 * V3 Virtual Core Migrate Control
 * (c) Lei Xia, 2011
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
    struct v3_core_move_cmd cmd; 

    if (argc < 4) {
	printf("Usage: v3_core_migrate <vm_device> <vcore id> <target physical CPU id>\n");
	return -1;
    }

    vm_dev = argv[1];
    cmd.vcore_id = atoi(argv[2]);
    cmd.pcore_id = atoi(argv[3]);

    printf("Migrate vcore %d to physical CPU %d\n", cmd.vcore_id, cmd.pcore_id);

    vm_fd = open(vm_dev, O_RDONLY);

    if (vm_fd == -1) {
	printf("Error opening VM device: %s\n", vm_dev);
	return -1;
    }

    int err = ioctl(vm_fd, V3_VM_MOVE_CORE, &cmd); 

    if (err < 0) {
	printf("Error write core migrating command to vm\n");
	return -1;
    }

    close(vm_fd); 

    return 0; 
}


