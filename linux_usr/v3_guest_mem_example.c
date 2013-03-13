#include <stdint.h>
#include <stdio.h>

#include "v3_guest_mem.h"

void usage() 
{
  fprintf(stderr,"usage: v3_guest_mem_example /dev/v3-vmN read|write gpa_hex numbytes [<data]\n");
}

int main(int argc, char *argv[])
{
  char *vmdev;
  enum {READ, WRITE} mode;
  uint64_t gpa;
  uint64_t numbytes;
  struct v3_guest_mem_map *map;
  uint64_t i;
    
  if (argc!=5) { 
    usage();
    return -1;
  }

  vmdev=argv[1];
  
  if (toupper(argv[2][0])=='R') { 
    mode=READ;
  } else if (toupper(argv[2][0]=='W')) {
    mode=WRITE;
  } else {
    fprintf(stderr,"Unknown mode %s\n", argv[2]);
    return -1;
  }
  
  if (sscanf(argv[3],"%llx",&gpa)!=1) { 
    fprintf(stderr,"Don't understand address %s\n",argv[3]);
    return -1;
  }

  numbytes=atol(argv[4]);

  if (!(map=v3_guest_mem_get_map(vmdev))) { 
    fprintf(stderr,"Cannot get guest memory map for %s\n",vmdev);
    return -1;
  }
  
  for (i=0; i< map->numblocks; i++) { 
    fprintf(stderr,"Region %llu: gpa=%p, hpa=%p, numpages=%llu\n", 
	    i, map->block[i].gpa, map->block[i].hpa, map->block[i].numpages);
  }  

  if (map->numblocks!=1) { 
    fprintf(stderr,"Don't handle multiregion map yet\n");
    return -1;
  }
  
  if (!( ((void*)(gpa) >= map->block[0].gpa) &&
	 (numbytes <= map->block[0].numpages*4096))) { 
    fprintf(stderr,"request (%p to %p) is out of range\n",
	    gpa, gpa+numbytes-1);
    return -1;
  }

  if (v3_map_guest_mem(map)) { 
    fprintf(stderr, "Cannot map guest memory\n");
    free(map);
    return -1;
  }

  for (i=0; i<numbytes; i++) {
    uint8_t cur;
    if (mode==WRITE) {
      if (read(0,&cur,1)!=1) { 
	fprintf(stderr,"can't get data from stdin for byte %llu\n", i);
	break;
      }
      *((uint8_t*)(map->block[0].uva+gpa+i))=cur;
    } else {
      cur = *((uint8_t *)(map->block[0].uva+gpa+i));
      //      fprintf(stderr, "read %llu from uva=%p, ptr=%p\n",i,map->block[0].uva, map->block[0].uva+gpa+i);
      if (write(1,&cur,1)!=1) { 
	fprintf(stderr,"can't write data to stdout for byte %llu\n", i);
	break;
      }
    }
  }
      

  if (v3_unmap_guest_mem(map)) { 
    fprintf(stderr, "Cannot unmap guest memory\n");
    free(map);
    return -1;
  }
}
  
