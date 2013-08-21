#include <stdint.h>
#include <stdio.h>

#include "v3_guest_mem.h"

void usage() 
{
  fprintf(stderr,"usage: v3_guest_mem_access /dev/v3-vmN read|write|hash gpa_hex numbytes [<data]\n");
}

int main(int argc, char *argv[])
{
  char *vmdev;
  enum {READ, WRITE, HASH} mode;
  uint64_t gpa;
  uint64_t numbytes;
  struct v3_guest_mem_map *map;
  uint64_t i;
  int rc;
  uint64_t hash;
  uint8_t *data=0;
    
  if (argc!=5) { 
    usage();
    return -1;
  }

  vmdev=argv[1];
  
  if (toupper(argv[2][0])=='R') { 
      mode=READ;
  } else if (toupper(argv[2][0])=='W') {
      mode=WRITE;
  } else if (toupper(argv[2][0])=='H') {
      mode=HASH;
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
  
  //for (i=0; i< map->numblocks; i++) { 
  //  fprintf(stderr,"Region %llu: gpa=%p, hpa=%p, numpages=%llu\n", 
  //    i, map->block[i].gpa, map->block[i].hpa, map->block[i].numpages);
  //}  

  if (v3_map_guest_mem(map)) { 
    fprintf(stderr, "Cannot map guest memory\n");
    free(map);
    return -1;
  }

  if (mode==READ || mode==WRITE) { 
      data = malloc(numbytes);
      if (!data) { 
	  fprintf(stderr, "Cannot allocate memory\n");
	  v3_unmap_guest_mem(map);
	  return -1;
      }
  }

  switch (mode) { 
      case WRITE:
	  for (i=0;i<numbytes;) {
	      rc = read(0,data+i,(numbytes-i));
	      if (rc<=0) {
		  fprintf(stderr, "Cannot read from stdin\n");
		  free(data);
		  v3_unmap_guest_mem(map);
		  return -1;
	      } else {
		  i+=rc;
	      }
	  }
	  if (v3_guest_mem_write(map,(void*)gpa,numbytes,data)) { 
	      fprintf(stderr, "Failed to write all of guest memory\n");
	      free(data);
	      v3_unmap_guest_mem(map);
	      return -1;
	  }

	  fprintf(stderr, "Write complete (%llu bytes)\n", numbytes);

	  free(data);

	  break;
	  
      case READ:
	  if (v3_guest_mem_read(map,(void*)gpa,numbytes,data)) { 
	      fprintf(stderr, "Failed to read all of guest memory\n");
	      free(data);
	      v3_unmap_guest_mem(map);
	      return -1;
	  }
	  for (i=0;i<numbytes;) {
	      rc = write(1,data+i,(numbytes-i));
	      if (rc<=0) {
		  fprintf(stderr, "Cannot write to stdout\n");
		  free(data);
		  v3_unmap_guest_mem(map);
		  return -1;
	      } else {
		  i+=rc;
	      }
	  }
	  
	  fprintf(stderr, "Read complete (%llu bytes)\n", numbytes);
	  
	  free(data);

	  break;
	  
      case HASH:
	  if (v3_guest_mem_hash(map,(void*)gpa,numbytes,&hash)) { 
	      fprintf(stderr, "Failed to hash all of guest memory\n");
	      v3_unmap_guest_mem(map);
	      return -1;
	  }
	  
	  fprintf(stderr, "Hash complete (%llu bytes), result is 0x%llx\n", numbytes, hash);
	  
	  break;
	  
  }
  
  
  if (v3_unmap_guest_mem(map)) { 
      fprintf(stderr, "Cannot unmap guest memory\n");
      free(map);
      return -1;
  }
  
  return 0;
}
  
