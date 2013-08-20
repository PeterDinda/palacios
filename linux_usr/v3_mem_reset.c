#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include "v3_ctrl.h"

int main(int argc, char *argv[]) 
{
    int v3_fd;

    if (argc!=2 || strcmp(argv[1],"-f")) { 
	printf("usage: v3_mem_reset -f\n\n"
	       "This will deinit and then init the memory manager.\n\nYou probably do not want to do this.\n");
	return -1;
    }

   
    v3_fd = open("/dev/v3vee", O_RDONLY);
    if (v3_fd == -1) {
        printf("Error opening V3Vee control device\n");
        return -1;
    }

    ioctl(v3_fd, V3_RESET_MEMORY, NULL);
    close(v3_fd);

    return 0;
}
