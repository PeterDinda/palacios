#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "v3_guest_mem.h"



#define GUEST_FILE "/proc/v3vee/v3-guests-details"
//#define GUEST_FILE "/v-test/numa/palacios-devel/test.proc"
#define MAXLINE 65536

struct v3_guest_mem_map * v3_guest_mem_get_map(char *vmdev)
{
  FILE *f;
  int rc;
  int i;
  char buf[MAXLINE];
  char dev[MAXLINE];
  uint64_t start, end, num;
  uint64_t guest_cur;
  uint64_t num_regions;
  uint64_t num_regions_shown;
  

  if (!(f=fopen(GUEST_FILE,"r"))) { 
      fprintf(stderr,"Cannot open %s - is Palacios active?\n",GUEST_FILE);
      return 0;
  }

  while (1) {
      if (!fgets(buf,MAXLINE,f)) {
	  fprintf(stderr,"Could not find info for %s\n",vmdev);
	  return 0;
      }
      if (sscanf(buf,"Device: %s",dev)==1) { 
	  if (!strcmp(dev,vmdev)) {
	      // found our VM
	      break;
	  } 
      }
  }
	
  // Now we need the number of regions
  while (1) {
      if (!fgets(buf,MAXLINE,f)) {
	  fprintf(stderr,"Could not find number of regions for %s\n",vmdev);
	  return 0;
      }
      if (sscanf(buf,"Regions: %llu (%llu shown)",&num_regions,&num_regions_shown)==2) {
	  break;
      }
  }

  if (num_regions != num_regions_shown) { 
      fprintf(stderr,"Cannot see all regions for %s\n",vmdev);
      return 0;
  }
 
  struct v3_guest_mem_map *m = 
      (struct v3_guest_mem_map *) malloc(sizeof(struct v3_guest_mem_map)+num_regions*sizeof(struct v3_guest_mem_block));
  if (!m) { 
      fprintf(stderr, "Cannot allocate space\n");
      fclose(f);
      return 0;
  }
  
  memset(m,0,sizeof(struct v3_guest_mem_map)+num_regions*sizeof(struct v3_guest_mem_block));

  m->numblocks=num_regions;

  // Now collect the region info
  guest_cur=0;
  i=0;
  while (i<num_regions) { 
      if (!fgets(buf,MAXLINE,f)) {
	  fprintf(stderr,"Did not find all regions...\n");
	  free(m);
	  return 0;
      }
      if (sscanf(buf," region %d has HPAs %llx-%llx",&num,&start,&end)==3) { 
	  m->block[i].gpa = (void*)guest_cur;
	  m->block[i].hpa = (void*)start;
	  m->block[i].numpages = (end-start) / 4096 + !!((end-start) % 4096);
	  if ((end-start)%4096) { 
	      fprintf(stderr,"Odd, region %d is a non-integral number of pages",i);
	  }
	  guest_cur+=end-start;
	  m->block[i].cumgpa=(void*)(guest_cur-1);
	  i++;
      }
  }

  fclose(f);

  return m;

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
      //fprintf(stderr,"Mapping %llu bytes of /dev/mem offset 0x%llx\n",
      //    map->block[i].numpages*4096, (off_t)(map->block[i].hpa));
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


void *v3_gpa_start(struct v3_guest_mem_map *map)
{
    return 0; // all guests start at zero for now
}

void *v3_gpa_end(struct v3_guest_mem_map *map)
{
    struct v3_guest_mem_block *l = &(map->block[map->numblocks-1]); 

    // currently, the regions are consecutive, so we just need the last block
    return l->gpa+l->numpages*4096-1; 
}


int v3_guest_mem_apply(void (*func)(void *data, uint64_t num_bytes, void *priv),
		       struct v3_guest_mem_map *map, void *gpa, uint64_t num_bytes, void *priv)
{
    void *cur_gpa;
    void *cur_uva;
    uint64_t left_bytes;
    uint64_t block_bytes;
    
    if (!(map->fd)) {
	return -1;
    }
    
    if (gpa < v3_gpa_start(map) || gpa+num_bytes-1 > v3_gpa_end(map)) { 
	return -1;
    }

    cur_gpa = gpa;
    left_bytes = num_bytes;

    while (left_bytes) { 
	cur_uva = v3_gpa_to_uva(map, cur_gpa, &block_bytes);
	if (!cur_uva) { 
	    return -1;
	}
	if (block_bytes>left_bytes) { 
	    block_bytes = left_bytes;
	}
	func(cur_uva,block_bytes,priv);
	left_bytes-=block_bytes;
	cur_gpa+=block_bytes;
    }
    
    return 0;
}



static void copy_out(void *uva, uint64_t num_bytes, void *curoff)
{
    memcpy(*((void**)(curoff)), uva, num_bytes);
    *(void**)curoff += num_bytes;
}

static void copy_in(void *uva, uint64_t num_bytes, void *curoff)
{
    memcpy(uva, *((void**)(curoff)), num_bytes);
    *(void**)curoff += num_bytes;
}

static void do_hash(void *uva, uint64_t num_bytes, void *priv)
{
    uint64_t i;
    uint64_t *curhash = (uint64_t *)priv;

    for (i=0;i<num_bytes;i++) { 
	*curhash += ((uint8_t*)uva)[i];
    }
}

int v3_guest_mem_read(struct v3_guest_mem_map *map, void *gpa, uint64_t num_bytes, char *data)
{
    void *cpy_ptr=data;

    return v3_guest_mem_apply(copy_out,map,gpa,num_bytes,&cpy_ptr);
}

int v3_guest_mem_write(struct v3_guest_mem_map *map, void *gpa, uint64_t num_bytes, char *data)
{
    void *cpy_ptr=data;

    return v3_guest_mem_apply(copy_in,map,gpa,num_bytes,&cpy_ptr);
}

int v3_guest_mem_hash(struct v3_guest_mem_map *map, void *gpa, uint64_t num_bytes, uint64_t *hash)
{
    *hash = 0;

    return v3_guest_mem_apply(do_hash,map,gpa,num_bytes,hash);
}



int v3_guest_mem_track_start(char *vmdev, 
			     v3_mem_track_access_t access, 
			     v3_mem_track_reset_t reset, 
			     uint64_t period)
{
    struct v3_mem_track_cmd cmd;

    cmd.request=V3_MEM_TRACK_START;
    cmd.config.access_type=access;
    cmd.config.reset_type=reset;
    cmd.config.period=period;

    return v3_vm_ioctl(vmdev,V3_VM_MEM_TRACK_CMD,&cmd);

}

int v3_guest_mem_track_stop(char *vmdev)
{
    struct v3_mem_track_cmd cmd;

    cmd.request=V3_MEM_TRACK_STOP;

    return v3_vm_ioctl(vmdev,V3_VM_MEM_TRACK_CMD,&cmd);

}


#define CEIL_DIV(x,y) (((x)/(y)) + !!((x)%(y)))

static uint8_t *alloc_bitmap(uint64_t num_pages) 
{
    uint8_t *b;
    
    if (!(b =  malloc(CEIL_DIV(num_pages,8)))) {
	return NULL;
    }
    
    memset(b,0,CEIL_DIV(num_pages,8));
    
    return b;
}


static void free_bitmap(uint8_t *b)
{
    if (b) { 
	free(b);
    }

}


void v3_guest_mem_track_free_snapshot(v3_mem_track_snapshot *s)
{
    int i;

    if (s) {
	for (i=0;i<s->num_cores;i++) {
	    free_bitmap(s->core[i].access_bitmap);
	}
	free(s);
    }
}


static v3_mem_track_snapshot *alloc_snapshot(uint64_t num_cores, uint64_t num_pages) 
{
    int i;
    v3_mem_track_snapshot *s;

    s = malloc(sizeof(v3_mem_track_snapshot) + sizeof(struct v3_core_mem_track) * num_cores);
    
    if (!s) { 
	return NULL;
    }

    memset(s,0,sizeof(v3_mem_track_snapshot) + sizeof(struct v3_core_mem_track) * num_cores);
    
    s->num_cores=num_cores;

    for (i=0;i<num_cores;i++) {
	if (!(s->core[i].access_bitmap = alloc_bitmap(num_pages))) { 
	    v3_guest_mem_track_free_snapshot(s);
	    return NULL;
	}
	s->core[i].num_pages=num_pages;
    }

    return s;
}


v3_mem_track_snapshot *v3_guest_mem_track_snapshot(char *vmdev)
{
    struct v3_mem_track_sizes size;
    v3_mem_track_snapshot *s;
    int rc;

    rc = v3_vm_ioctl(vmdev,V3_VM_MEM_TRACK_SIZE,&size);

    if (rc) { 
	return 0;
    }

    //printf("returned size num_cores=%u, num_pages=%llu",size.num_cores,size.num_pages);

    // allocate a snapshot;
    if (!(s=alloc_snapshot(size.num_cores,size.num_pages))) { 
	return 0;
    }

    

    if (v3_vm_ioctl(vmdev,V3_VM_MEM_TRACK_SNAP,s)) { 
	v3_guest_mem_track_free_snapshot(s);
	return 0;
    }

    return s;
}
    


