/* Host PCI User space tool
 *  (c) Jack Lange, 2012
 *  jacklange@cs.pitt.edu 
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


int main(int argc, char ** argv) {
    int v3_fd = 0;
    struct v3_hw_pci_dev dev_info;
    unsigned int bus = 0;
    unsigned int dev = 0;
    unsigned int func = 0;
    int ret = 0;

    if (argc < 3) {
	printf("Usage: ./v3_pci <name> <bus> <dev> <func>\n");
	return -1;
    }

    bus = atoi(argv[2]);
    dev = atoi(argv[3]);
    func = atoi(argv[4]);

    strncpy(dev_info.url, argv[1], 128);
    dev_info.bus = bus;
    dev_info.dev = dev;
    dev_info.func = func;
    

    v3_fd = open("/dev/v3vee", O_RDONLY);

    if (v3_fd == -1) {
	printf("Error opening V3Vee device file\n");
	return -1;
    }


    ret = ioctl(v3_fd, V3_ADD_PCI_HW_DEV, &dev_info);
    

    if (ret < 0) {
	printf("Error registering PCI device\n");
	return -1;
    }

    close(v3_fd);
}
