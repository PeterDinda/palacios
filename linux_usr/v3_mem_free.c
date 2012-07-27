#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include "v3_ctrl.h"

int main(void) 
{
    FILE * fp;
    char * filepath = "/proc/v3vee/v3-mem";
    char line[255];
    unsigned long base_addr = 0;
    unsigned long long num_pages = 0;
    unsigned long amount_allocated = 0;
    unsigned long long block_size = 0;
    int i = 0;
    int v3_fd;
    
    fp = fopen(filepath, "r");
    if(fp == NULL) {
	fprintf(stderr, "Could not open %s\n", filepath);
	return -1;
    }
    memset(line, 0, 255);
    fgets(line, 255, fp);
    base_addr = strtoul(line, NULL, 16);
    base_addr = base_addr / (1024*1024);
    memset(line, 0, 255);
    fgets(line, 255, fp);
    num_pages = strtoull(line, NULL, 10);
    amount_allocated = (num_pages*4096)/(1024*1024);
    
    fclose(fp);
    
    /* now get the block size */
    fp = fopen("/sys/devices/system/memory/block_size_bytes", "r");
    if(fp == NULL) {
	fprintf(stderr, "Cannot lookup bytes per block size\n");
	return -1;
    }
    memset(line, 0, 255);
    fgets(line, 255, fp);
    block_size = strtoull(line, NULL, 16);
    block_size = block_size / (1024*1024); //convert to MB
    fclose(fp);
    
    /* turn base_addr into the region number */
    base_addr = base_addr / block_size;
    
    for(i=0;i< (amount_allocated / block_size); i++) {
	char path[50];
	
	memset(path, 0, 50);
	sprintf(path,"/sys/devices/system/memory/memory%d/state", 
		base_addr);
	fp = fopen(path, "w+");
	if(fp == NULL) {
	    fprintf(stderr, "Could not open %s\n", path);
	    return -1;
	}
	printf("Sending \"online\" to memory%d\n", base_addr);
	fprintf(fp, "online");
	fclose(fp);
	base_addr++;
    }
    
    v3_fd = open(v3_dev, O_RDONLY);
    if (v3_fd == -1) {
        printf("Error opening V3Vee control device\n");
        return -1;
    }

    ioctl(v3_fd, V3_RESET_MEMORY, NULL);
    close(v3_fd);

    return 0;
}
