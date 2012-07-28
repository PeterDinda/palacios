/* 
 * V3 Control utility
 * (c) Jack lange, 2010
 */

#include "v3_ctrl.h"

int main(int argc, char* argv[]) {
    char * filename = argv[1];

    if (argc <= 1) 
	v3_usage("<vm-device>\n");


    launch_vm(filename);

    return 0; 
} 
