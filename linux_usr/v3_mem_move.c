/* 
 * V3 memory movement control
 * (c) Peter Dinda 2013
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
    struct v3_mem_move_cmd cmd; 

    if (argc < 4) {
	printf("usage: v3_mem_move <vm_device> <guest_physical_addr> <target_physical_cpu>\n\n");
	printf("Moves the memory region into which the guest_physical_addr is mapped\n");
	printf("to host physical memory that has highest affinity for the target_physical_cpu.\n");
	printf("you can find the current memory mapping via /proc/v3vee/v3-guests-details\n\n");
	printf(" guest_physical_addr  - hex address\n");
	printf(" target_physical_cpu  - base 10 cpuid (0..numcpus-1)\n\n");
	return -1;
    }

    vm_dev = argv[1];
    cmd.gpa = strtoll(argv[2],0,16);
    cmd.pcore_id = atoi(argv[3]);

    printf("Migrating memory region of %p to memory with affinity for physical CPU %d\n", cmd.gpa, cmd.pcore_id);

    vm_fd = open(vm_dev, O_RDONLY);

    if (vm_fd == -1) {
	printf("Error opening VM device: %s\n", vm_dev);
	return -1;
    }

    int err = ioctl(vm_fd, V3_VM_MOVE_MEM, &cmd); 

    if (err < 0) {
	printf("Error write memory migrating command to vm\n");
	return -1;
    }

    close(vm_fd); 

    return 0; 
}


