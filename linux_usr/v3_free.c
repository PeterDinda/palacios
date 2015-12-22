/* 
 * V3 Control utility
 * (c) Jack lange, 2011
 */

#include <string.h>
 
#include "v3_ctrl.h"

int main(int argc, char* argv[]) {
    unsigned long vm_idx = 0;
    char *idx;
    int ret;

    if (argc <= 1) 
	v3_usage("<vm-dev-idx>|<vm-dev>\n");

    if (!(idx=strstr(argv[1],"v3-vm"))) { 
	idx=argv[1];
    } else {
        idx+=5;
    }

    vm_idx = strtol(idx, NULL, 0);

    printf("Freeing VM %lu\n", vm_idx);
    
    if (v3_dev_ioctl(V3_FREE_GUEST, (void*)vm_idx) < 0) {
        fprintf(stderr, "Error freeing VM %lu (%s)\n", vm_idx,argv[1]);
        return -1;
    }

    return 0; 
} 


