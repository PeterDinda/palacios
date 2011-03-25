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
    unsigned long long base_addr = atoll(argv[1]);
    unsigned long long num_bytes = atoll(argv[2]);
    int v3_fd = 0;
    struct v3_mem_region mem;

    if (argc <= 2) {
	printf("Usage: ./v3_mem <base_addr> <num_bytes>\n");
	return -1;
    }

    printf("Giving Palacios %dMB of memory: \n", num_bytes / (1024 * 1024));

    mem.base_addr = base_addr;
    mem.num_pages = num_bytes / 4096;

    v3_fd = open(v3_dev, O_RDONLY);

    if (v3_fd == -1) {
	printf("Error opening V3Vee control device\n");
	return -1;
    }

    ioctl(v3_fd, V3_ADD_MEMORY, &mem); 



    /* Close the file descriptor.  */ 
    close(v3_fd); 
 


    return 0; 
} 



int read_file(int fd, int size, unsigned char * buf) {
    int left_to_read = size;
    int have_read = 0;

    while (left_to_read != 0) {
	int bytes_read = read(fd, buf + have_read, left_to_read);

	if (bytes_read <= 0) {
	    break;
	}

	have_read += bytes_read;
	left_to_read -= bytes_read;
    }

    if (left_to_read != 0) {
	printf("Error could not finish reading file\n");
	return -1;
    }
    
    return 0;
}
