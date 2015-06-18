/* 
 * V3 VM reset
 * (c) Peter Dinda 2015
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

void usage()
{
    printf("usage: v3_reset <vm_device> all|hrt|ros|range [first_core num_cores]\n\n");
    printf("Resets the VM or a part of it.\n");
    printf("  all   : full reset of entire VM\n");
    printf("  hrt   : reset of all HRT cores for an HVM VM\n");
    printf("  ros   : reset of ROS cores for an HVM VM\n");
    printf("  range : reset of cores first_core to first_core+num_cores-1\n\n");
}

int main(int argc, char* argv[]) {
    int vm_fd;
    char * vm_dev = NULL;
    struct v3_reset_cmd cmd; 

    if (argc < 3) {
	usage();
   	return -1;
    }

    vm_dev = argv[1];

    if (!strcasecmp(argv[2],"all")) { 
	cmd.type=V3_RESET_VM_ALL;
    } else if (!strcasecmp(argv[2],"hrt")) { 
	cmd.type=V3_RESET_VM_HRT;
    } else if (!strcasecmp(argv[2],"ros")) { 
	cmd.type=V3_RESET_VM_ROS;
    } else if (!strcasecmp(argv[2],"range")) { 
	cmd.type=V3_RESET_VM_CORE_RANGE;
	if (argc!=5) { 
	    usage();
	    return -1;
	} else {
	    cmd.first_core = atoi(argv[3]);
	    cmd.num_cores = atoi(argv[4]);
	}
    }

    printf("Doing VM reset:  %s ",
	   cmd.type==V3_RESET_VM_ALL ? "ALL" :
	   cmd.type==V3_RESET_VM_HRT ? "HRT" :
	   cmd.type==V3_RESET_VM_ROS ? "ROS" :
	   cmd.type==V3_RESET_VM_CORE_RANGE ? "RANGE" : "UNKNOWN");
    if (cmd.type==V3_RESET_VM_CORE_RANGE) { 
	printf("cores %u to %u\n", cmd.first_core, cmd.first_core+cmd.num_cores-1);
    } else {
	printf("\n");
    }

    vm_fd = open(vm_dev, O_RDONLY);

    if (vm_fd == -1) {
	printf("Error opening VM device: %s\n", vm_dev);
	return -1;
    }

    int err = ioctl(vm_fd, V3_VM_RESET, &cmd); 

    if (err < 0) {
	printf("Error sending reset commad to vm\n");
	return -1;
    }

    close(vm_fd); 

    return 0; 
}


