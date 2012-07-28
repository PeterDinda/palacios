/* 
 * V3 Control utility
 * (c) Jack lange, 2011
 */

#include <string.h>
 
#include "v3_ctrl.h"

int main(int argc, char* argv[]) {
    unsigned long vm_idx = 0;
    int ret;

    if (argc <= 1) 
	v3_usage("<vm-dev-idx>\n");


    vm_idx = strtol(argv[1], NULL, 0);

    printf("Freeing VM %d\n", vm_idx);
    
    if (v3_dev_ioctl(V3_FREE_GUEST, vm_idx) < 0) {
        fprintf(stderr, "Error freeing VM %d\n", vm_idx);
        return -1;
    }

    return 0; 
} 


