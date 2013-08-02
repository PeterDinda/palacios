#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "v3_guest_mem.h"


#warning FIX THE PARSER TO CONFORM TO NEW /proc/v3 VM output format


#define GUEST_FILE "/proc/v3vee/v3-guests"
//#define GUEST_FILE "/441/pdinda/test.proc"
#define MAXLINE 65536

struct v3_guest_mem_map * v3_guest_mem_get_map(char *vmdev)
{
  FILE *f;
  char buf[MAXLINE];
  char name[MAXLINE];
  char dev[MAXLINE];
  char state[MAXLINE];
  uint64_t start, end;
  

  if (!(f=fopen(GUEST_FILE,"r"))) { 
    fprintf(stderr,"Cannot open %s - is Palacios active?\n",GUEST_FILE);
    return 0;
  }
  
  // This is using the current "single memory region" model
  // and will change when /proc convention changes to conform to
  // multiple region model
  while (fgets(buf,MAXLINE,f)) {
    if (sscanf(buf,
	       "%s %s %s [0x%llx-0x%llx]",
	       name,
	       dev,
	       state,
	       &start,
	       &end)!=5) { 
      fprintf(stderr, "Cannot parse following line\n%s\n",buf);
      fclose(f);
      return 0;
    }
    if (!strcmp(dev,vmdev)) { 
      struct v3_guest_mem_map *m = 
	(struct v3_guest_mem_map *) malloc(sizeof(struct v3_guest_mem_map)+1*sizeof(struct v3_guest_mem_block));
      if (!m) { 
	fprintf(stderr, "Cannot allocate space\n");
	fclose(f);
	return 0;
      }

      memset(m,0,sizeof(struct v3_guest_mem_map)+1*sizeof(struct v3_guest_mem_block));
      
      m->numblocks=1;
      m->block[0].gpa=0;
      m->block[0].hpa=(void*)start;
      m->block[0].numpages = (end-start+1) / 4096;
      fclose(f);
      return m;
    }
  }
  
  fprintf(stderr,"%s not found\n",vmdev);
  fclose(f);
  return 0;

}

int v3_map_guest_mem(struct v3_guest_mem_map *map)
{
  uint64_t i;
  
  if (map->fd) {
    fprintf(stderr, "Memory appears to already be mapped\n");
    return -1;
  }

  map->fd = open("/dev/mem", O_RDWR | O_SYNC);

  if (map->fd<0) { 
    fprintf(stderr, "Cannot open /dev/mem - are you root?\n");
    map->fd=0;
    return -1;
  }

  for (i=0; i<map->numblocks; i++) { 
    fprintf(stderr,"Mapping %llu bytes of /dev/mem offset 0x%llx\n",
	    map->block[i].numpages*4096, (off_t)(map->block[i].hpa));
    map->block[i].uva = mmap(NULL, 
			     map->block[i].numpages*4096,
			     PROT_READ | PROT_WRITE, 
			     MAP_SHARED, 
			     map->fd, 
			     (off_t) (map->block[i].hpa));

    if (map->block[i].uva == MAP_FAILED) { 
      fprintf(stderr, "Failed to map block %llu\n",i);
      map->block[i].uva=0;
      v3_unmap_guest_mem(map);
      return -1;
    }
  }
   
  return 0;
    
}

int v3_unmap_guest_mem(struct v3_guest_mem_map *map)
{
  uint64_t i;

  for (i=0; i<map->numblocks; i++) { 
    if (map->block[i].uva) { 
      munmap(map->block[i].uva, map->block[i].numpages*4096);
      map->block[i].uva=0;
    }
  }
  if (map->fd) { 
    close(map->fd);
    map->fd=0;
  }
  return 0;
}
