/* Simple hypercall utility for common convention */
/* Copyright (c) 2015 Peter Dinda                 */

#include <stdlib.h>
#include <stdio.h>
#include "hcall.h"

int main(int argc, char *argv[])
{
    unsigned long long num, a1, a2, a3, a4, a5, a6, a7, a8;
    unsigned long long rc;

    if (argc<2 || argc>10) { 
	printf("usage: hypercall <number> [args (up to 8)]\n");
	printf("The hypercall number and arguments are in hex\n");
	return -1;
    }

    num = strtoull(argv[1],0,16);

    if (argc>2) { 
	a1=strtoull(argv[2],0,16);
    } else {
	a1=0;
    }
    if (argc>3) { 
	a2=strtoull(argv[3],0,16);
    } else {
	a2=0;
    }
    if (argc>5) { 
	a3=strtoull(argv[4],0,16);
    } else {
	a3=0;
    }
    if (argc>6) { 
	a4=strtoull(argv[5],0,16);
    } else {
	a4=0;
    }
    if (argc>7) { 
	a5=strtoull(argv[6],0,16);
    } else {
	a5=0;
    }
    if (argc>8) { 
	a6=strtoull(argv[7],0,16);
    } else {
	a6=0;
    }
    if (argc>9) { 
	a7=strtoull(argv[8],0,16);
    } else {
	a7=0;
    }
    if (argc>10) { 
	a8=strtoull(argv[9],0,16);
    } else {
	a8=0;
    }
    
    printf("Executing hcall 0x%llx with arguments a1=0x%llx, a2=0x%llx, a3=0x%llx, a4=0x%llx, a5=0x%llx, a6=0x%llx, a7=0x%llx, a8=0x%llx\n", num, a1, a2, a3, a4, a5, a6, a7, a8);

    HCALL(rc,num,a1,a2,a3,a4,a5,a6,a7,a8);
    
    printf("Return from hypercall was 0x%llx\n",rc);

    return 0;
}
