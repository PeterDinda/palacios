/* Northwestern University */
/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */

#include <palacios/vmm_debug.h>
#include <palacios/vmm.h>


void PrintDebugHex(unsigned char x)
{
  unsigned char z;
  
  z = (x>>4) & 0xf ;
  PrintDebug("%x", z);
  z = x & 0xf;
  PrintDebug("%x", z);
}

void PrintDebugMemDump(unsigned char *start, int n)
{
  int i, j;

  for (i=0;i<n;i+=16) {
    PrintDebug("%8x", (unsigned)(start+i));
    for (j=i; j<i+16 && j<n; j+=2) {
      PrintDebug(" ");
      PrintDebugHex(*((unsigned char *)(start+j)));
      if ((j+1)<n) { 
	PrintDebugHex(*((unsigned char *)(start+j+1)));
      }
    }
    PrintDebug(" ");
    for (j=i; j<i+16 && j<n;j++) {
      PrintDebug("%c", ((start[j]>=32) && (start[j]<=126)) ? start[j] : '.');
    }
    PrintDebug("\n");
  }
}
