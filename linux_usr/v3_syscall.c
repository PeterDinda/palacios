/*
 * V3 Selective System Call Exiting Control
 * (c) Kyle C. Hale, 2012
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "iface-syscall.h"

#define CMD_MAX 10
#define SYSCALL_MAX 256

static void usage () {
	fprintf(stderr, "\nusage: v3_syscall <vm device> <syscall_nr> <on|off|status>\n");
	exit(0);
}


int main (int argc, char * argv[]) {

	int vm_fd, ret, syscall_nr;
	char * vm_dev = NULL;
	struct v3_syscall_cmd syscall_cmd;

	if (argc < 4 || argc > 4) {
		fprintf(stderr, "Invalid number of arguments.\n");
		usage();
	}

	vm_dev = argv[1];
	syscall_nr = strtol(argv[2], NULL, 0);

	if (strncmp(argv[3], "on", CMD_MAX) == 0) {
		syscall_cmd.cmd = SYSCALL_ON;
	} else if (strncmp(argv[3], "off", CMD_MAX) == 0) {
		syscall_cmd.cmd = SYSCALL_OFF;
	} else if (strncmp(argv[3], "status", CMD_MAX) == 0) {
		syscall_cmd.cmd = SYSCALL_STAT;
	} else {
		fprintf(stderr, "Invalid command.\n");
		usage();
	}

    if (syscall_nr < 0 || syscall_nr > SYSCALL_MAX) {
        fprintf(stderr, "Invalid syscall number.\n");
        return -1;
    }

    syscall_cmd.syscall_nr = syscall_nr;

	vm_fd = open(vm_dev, O_RDONLY);

	if (vm_fd < 0) { 
		fprintf(stderr, "Error opening VM device: %s\n", vm_dev);
		return -1;
	}

	ret = ioctl(vm_fd, V3_VM_SYSCALL_CTRL, &syscall_cmd);

    if (ret < 0) {
        fprintf(stderr, "Error with syscall control\n");
        return -1;
    }

    if (syscall_cmd.cmd == SYSCALL_STAT) {
        if (ret == SYSCALL_ON) 
            printf("Selective exiting for syscall #%d is currently ON\n", syscall_cmd.syscall_nr);
        else if (ret == SYSCALL_OFF) 
            printf("Selective exiting for syscall #%d is currently OFF\n", syscall_cmd.syscall_nr);
    } else if (syscall_cmd.cmd == SYSCALL_ON) {
       printf("Selective exiting for syscall #%d ACTIVATED\n", syscall_cmd.syscall_nr);
    } else if (syscall_cmd.cmd == SYSCALL_OFF) {
        printf("Selective exiting for syscall #%d DEACTIVATED\n", syscall_cmd.syscall_nr);
    }

	return 0;
}
