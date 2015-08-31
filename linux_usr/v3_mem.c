/* 
 * V3 Control utility
 * (c) Jack lange, 2010
 */


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <sys/ioctl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <unistd.h> 
#include <string.h>
#include <dirent.h>
#include <alloca.h> 

#include "v3_ctrl.h"

// set to zero to ignore, or set
// to a level likely given the largest contiguous
// page allocation outside of the base regions
// note that the seed pools provide 2-4 MB chunks
// to start
#define PALACIOS_MIN_ALLOC (64*4096)

#define SYS_PATH "/sys/devices/system/memory/"

#define BUF_SIZE 128

int verbose=0;

char offname[256];
FILE *off;

int num_offline;
unsigned long long *start_offline;
unsigned long long *len_offline;

unsigned long long kernel_max_order;
unsigned long long kernel_max_page_alloc_bytes;
unsigned long long kernel_num_nodes;
unsigned long long kernel_num_cpus;
unsigned long long palacios_compiled_mem_block_size;
unsigned long long palacios_runtime_mem_block_size;


#define VPRINTF(...) do { if (verbose) { printf(__VA_ARGS__); } } while (0)
#define EPRINTF(...) do { fprintf(stderr,__VA_ARGS__); } while (0)


static int read_offlined();
static int write_offlined();
static int find_offlined(unsigned long long base_addr);
static int clear_offlined();


static int offline_memory(unsigned long long mem_size_bytes,
			  unsigned long long mem_min_start,
			  int limit32, 
			  int node,
			  unsigned long long *num_bytes, 
			  unsigned long long *base_addr);

static int online_memory(unsigned long long num_bytes, 
			 unsigned long long base_addr);


static int get_kernel_setup();



int main(int argc, char * argv[]) {
    unsigned long long mem_size_bytes = 0;
    unsigned long long mem_min_start = 0;
    int v3_fd = -1;
    int request = 0;
    int limit32 = 0;
    int help=0;
    int alloffline=0;
    enum {NONE, ADD, REMOVE} op;
    int node = -1;
    int c;
    unsigned long long num_bytes, base_addr;
    struct v3_mem_region mem;

    while ((c=getopt(argc,argv,"hvarklm:n:"))!=-1) {
	switch (c) {
	    case 'h':
		help=1;
		break;
	    case 'v':
		verbose=1;
		break;
	    case 'a':
		op=ADD;
		break;
	    case 'r':
		op=REMOVE;
		break;
	    case 'k':
		request=1;
		break;
	    case 'l':
		limit32=1;
		break;
	    case 'm':
		mem_min_start = atoll(optarg) * (1024*1024);
		break;
	    case 'n':
		node = atoi(optarg);
		break;
	    case '?':
		if (optopt=='n') { 
		    EPRINTF("-n requires the numa node...\n");
            return -1;
		} else if (optopt=='m') { 
		    EPRINTF("-m requires the minimum starting address (in MB)...\n");
            return -1;
		} else {
		    EPRINTF("Unknown option %c\n",optopt);
            return -1;
		}
		break;
	    default:
		EPRINTF("Unknown option %c\n",optopt);
		break;
	}
    }


    if (op==NONE || optind==argc || help) {
	EPRINTF("usage: v3_mem [-h] [-v] [ [-k] [-l] [-n k] [-m n] -a <memory size (MB)>] | [-r <hexaddr> | offline]\n\n"
	       "Palacios Memory Management\n\nMemory Addition\n"
	       " -a <mem>      Allocate memory for use by Palacios (MB).\n\n"
	       " With    -k    this requests in-kernel allocation\n"
	       " Without -k    this attempts to offline memory via hot remove\n\n"
	       " With    -l    the request or offlining is limited to first 4 GB\n"
	       " Without -l    the request or offlining has no limits\n\n"
	       " With    -m n  the search for offlineable memory starts at n MB\n"
	       " Without -m n  the search for offlineable memory starts at 0 MB\n\n"
	       " With    -n i  the request is for numa node i\n"
	       " Without -n i  the request can be satified on any numa node\n\n"
	       "Memory Removal\n"
	       " -r <hexaddr>  Free Palacios memory containing hexaddr, online it if needed\n"
	       " -r offline    Free all offline Palacios memory and online it\n\n"
	       "Shared Options\n"
	       " -v            Verbose\n"
	       " -h            Help\n"
	       );
	
	return -1;
    }


    if (get_kernel_setup()) { 
	EPRINTF("Cannot read kernel setup\n");
	return -1;
    }

    if (op==ADD) {
	mem_size_bytes = (unsigned long long) (atof(argv[optind]) * (1024 * 1024));

	if (mem_size_bytes < palacios_runtime_mem_block_size ||
	    (PALACIOS_MIN_ALLOC!=0 && mem_size_bytes < PALACIOS_MIN_ALLOC)) {
	    EPRINTF("Trying to add a smaller single chunk of memory than Palacios needs\n"
		   "Your request:                        %llu bytes\n"
		   "Palacios run-time memory block size: %llu bytes\n",
		   "Palacios minimal contiguous alloc:   %llu bytes\n",
		    mem_size_bytes, palacios_runtime_mem_block_size,
		    PALACIOS_MIN_ALLOC);
	    return -1;
	}

	if (request && mem_size_bytes > kernel_max_page_alloc_bytes) { 
	    EPRINTF("Trying to request a larger single chunk of memory than the kernel can allocate\n"
		   "Your request:                        %llu bytes\n"
		   "Kernel largest page allocation:      %llu bytes\n"
		   "Kernel MAX_ORDER:                    %llu\n",
		   mem_size_bytes, kernel_max_page_alloc_bytes, kernel_max_order);
	    return -1;
	}

	if (node>=0 && node>=kernel_num_nodes) { 
	    EPRINTF("Trying to request or allocate memory for a nonexistent node\n"
		   "Your request:                        node %d\n"
		   "Kernel number of nodes:              %llu\n",
		   node, kernel_num_nodes);
	}
	    

    } else if (op==REMOVE) { 
	if (!strcasecmp(argv[optind],"offline")) {
	    alloffline=1;
	} else {
	    base_addr=strtoll(argv[optind],NULL,16);
	}
    }
    
    if (!getenv("PALACIOS_DIR")) { 
	EPRINTF("Please set the PALACIOS_DIR variable\n");
	return -1;
    }

    strcpy(offname,getenv("PALACIOS_DIR"));
    strcat(offname,"/.v3offlinedmem");

    if (!(off=fopen(offname,"a+"))) { 
	EPRINTF("Cannot open or create offline memory file %s",offname);
	return -1;
    }

    // removing all offlined memory we added is a special case
    if (op==REMOVE && alloffline) {
	int i;
	int rc=0;

	// we just need to reinvoke ourselves
	read_offlined();
	for (i=0;i<num_offline;i++) {
	    char cmd[256];
	    sprintf(cmd,"v3_mem -r %llx", start_offline[i]);
	    rc|=system(cmd);
	}
	clear_offlined();
	return rc;
    }

	
    v3_fd = open(v3_dev, O_RDONLY);
    
    if (v3_fd == -1) {
	EPRINTF("Error opening V3Vee control device\n");
	fclose(off);
	return -1;
    }


    if (op==ADD) { 
		   
	if (!request) { 
	    VPRINTF("Trying to offline memory (size=%llu, min_start=%llu, limit32=%d)\n",mem_size_bytes,mem_min_start,limit32);
	    if (offline_memory(mem_size_bytes,mem_min_start,limit32,node,&num_bytes, &base_addr)) { 
		EPRINTF("Could not offline memory\n");
		fclose(off);
		close(v3_fd);
		return -1;
	    }
	    
	    fprintf(off,"%llx\t%llx\n",base_addr, num_bytes);
	    
	    printf("Memory of size %llu at 0x%llx has been offlined\n",num_bytes,base_addr);

	    mem.type=PREALLOCATED;
	    mem.node=node;
	    mem.base_addr=base_addr;
	    mem.num_pages=num_bytes/4096;

	    
	} else {
	    VPRINTF("Generating memory allocation request (size=%llu, limit32=%d)\n", mem_size_bytes, limit32);
	    mem.type = limit32 ? REQUESTED32 : REQUESTED;
	    mem.node = node;
	    mem.base_addr = 0;
	    mem.num_pages = mem_size_bytes / 4096;
	}
	
	VPRINTF("Allocation request is: type=%d, node=%d, base_addr=0x%llx, num_pages=%llu\n",
	       mem.type, mem.node, mem.base_addr, mem.num_pages);
	
	if (ioctl(v3_fd, V3_ADD_MEMORY, &mem)<0) { 
	    EPRINTF("Request rejected by Palacios\n");
	    printf("Allocation of memory by Palacios has failed.  Check dmesg output for more information.\n");
	    close(v3_fd);
	    fclose(off);
	    return -1;
	} else {
	    VPRINTF("Request accepted by Palacios\n");
	    printf("%llu bytes of memory has been allocated by Palacios\n",mem.num_pages*4096);
	    close(v3_fd); 	
	    fclose(off);
	    return 0;
	}
	
    } else if (op==REMOVE) { 
	int entry;

	read_offlined();

	entry=find_offlined(base_addr);
	
	if (entry<0) { 
	    // no need to offline
	    mem.type=REQUESTED;
	} else {
	    mem.type=PREALLOCATED;
	}
	
	mem.base_addr=base_addr;

	// now remove it from palacios
	VPRINTF("Deallocation request is: type=%d, base_addr=0x%llx\n",
	       mem.type, mem.base_addr);
	
	if (ioctl(v3_fd, V3_REMOVE_MEMORY, &mem)<0) { 
	    EPRINTF("Request rejected by Palacios\n");
	    close(v3_fd);
	    fclose(off);
	    return -1;
	} 

	VPRINTF("Request accepted by Palacios\n");

	printf("Memory at 0x%llx has been deallocated by Palacios\n", mem.base_addr);

	if (entry>=0) { 
	    VPRINTF("Onlining the memory to make it available to the kernel\n");
	    online_memory(start_offline[entry],len_offline[entry]);
	
	    len_offline[entry] = 0;

	    write_offlined();

	    printf("Memory at 0x%llx has been onlined\n",mem.base_addr);
	    
	} else {
	    VPRINTF("Memory was deallocated in the kernel\n");
	    printf("Memory at 0x%llx has been onlined\n",mem.base_addr);
	}

	clear_offlined();
	close(v3_fd);
	fclose(off);

	return 0;
    }

} 


static int dir_filter(const struct dirent * dir) {
    if (strncmp("memory", dir->d_name, 6) == 0) {
	return 1;
    }

    return 0;
}


static int dir_cmp(const struct dirent **dir1, const struct dirent ** dir2) {
    int num1 = atoi((*dir1)->d_name + 6);
    int num2 = atoi((*dir2)->d_name + 6);
    
    return num1 - num2;
}



#define UNWIND(first,last)					\
do {								\
    int i;							\
    for (i = first; i <= last; i++) {				\
	FILE *f;						\
	char name[256];						\
	snprintf(name,256,"%smemory%d/state",SYS_PATH,i);	\
	f=fopen(name,"r+");					\
	if (!f) {						\
	    perror("Cannot open state file\n");			\
	    return -1;						\
	}							\
	VPRINTF("Re-onlining block %d (%s)\n",i,name);		\
	fprintf(f,"online\n");					\
	fclose(f);						\
    }								\
} while (0)


static int offline_memory(unsigned long long mem_size_bytes,
			  unsigned long long mem_min_start,
			  int limit32, 
			  int node,
			  unsigned long long *num_bytes, 
			  unsigned long long *base_addr)
{

    unsigned int block_size_bytes = 0;
    int bitmap_entries = 0;
    unsigned char * bitmap = NULL;
    int num_blocks = 0;    
    int reg_start = 0;
    int mem_ready = 0;
    
    

    VPRINTF("Trying to find %dMB (%d bytes) of memory above %llu with limit32=%d\n", mem_size_bytes/(1024*1024), mem_size_bytes, mem_min_start, limit32);
	
    /* Figure out the block size */
    {
	int tmp_fd = 0;
	char tmp_buf[BUF_SIZE];
	
	tmp_fd = open(SYS_PATH "block_size_bytes", O_RDONLY);
	
	if (tmp_fd == -1) {
	    perror("Could not open block size file: " SYS_PATH "block_size_bytes");
	    return -1;
	}
    
	if (read(tmp_fd, tmp_buf, BUF_SIZE) <= 0) {
	    perror("Could not read block size file: " SYS_PATH "block_size_bytes");
	    return -1;
	}
	
	close(tmp_fd);
	
	block_size_bytes = strtoll(tmp_buf, NULL, 16);
	
	VPRINTF("Memory block size is %dMB (%d bytes)\n", block_size_bytes / (1024 * 1024), block_size_bytes);
	
    }
    
    
    num_blocks =  mem_size_bytes / block_size_bytes;
    if (mem_size_bytes % block_size_bytes) num_blocks++;
    
    mem_min_start = block_size_bytes * 
	((mem_min_start / block_size_bytes) + (!!(mem_min_start % block_size_bytes)));
    
    VPRINTF("Looking for %d blocks of memory starting at %p (block %llu) with limit32=%d for node %d\n", num_blocks, (void*)mem_min_start, mem_min_start/block_size_bytes,limit32,node);
	
    
    // We now need to find <num_blocks> consecutive offlinable memory blocks
    
    /* Scan the memory directories */
    {
	struct dirent ** namelist = NULL;
	int size = 0;
	int i = 0;
	int j = 0;
	int last_block = 0;
	int first_block = mem_min_start/block_size_bytes;
	int limit_block = 0xffffffff / block_size_bytes; // for 32 bit limiting
	
	last_block = scandir(SYS_PATH, &namelist, dir_filter, dir_cmp);
	bitmap_entries = atoi(namelist[last_block - 1]->d_name + 6) + 1;
	    
	size = bitmap_entries / 8;
	if (bitmap_entries % 8) size++;
	    
	bitmap = alloca(size);

	if (!bitmap) {
	    VPRINTF("ERROR: could not allocate space for bitmap\n");
	    return -1;
	}
	
	memset(bitmap, 0, size);
	
	for (i = 0 ; j < bitmap_entries - 1; i++) {
	    struct dirent * tmp_dir = namelist[i];
	    int block_fd = 0;	    
	    char status_str[BUF_SIZE];
	    char fname[BUF_SIZE];
	    char nname[BUF_SIZE];
	    struct stat s;
		
	    memset(status_str, 0, BUF_SIZE);

	    memset(fname, 0, BUF_SIZE);
	    snprintf(fname, BUF_SIZE, "%s%s/removable", SYS_PATH, tmp_dir->d_name);

	    memset(nname, 0, BUF_SIZE);
	    snprintf(nname, BUF_SIZE, "%s%s/node%d", SYS_PATH, tmp_dir->d_name,node);
		
	    j = atoi(tmp_dir->d_name + 6);
	    int major = j / 8;
	    int minor = j % 8;
		
		
	    if (i<first_block) { 
		VPRINTF("Skipping %s due to minimum start constraint\n",fname);
		continue;
	    } 

	    if (limit32 && i>limit_block) { 
		VPRINTF("Skipping %s due to 32 bit constraint\n",fname);
		continue;
	    } 
		
	    // The prospective block must be (a) removable, and (b) currently online
	    // and for the needed node
		
	    VPRINTF("Checking %s...", fname);
		
	    if (node>=0) { 
		if (stat(nname,&s)) { 
		    VPRINTF("Skipping %s due to it being in the wrong node\n", fname);
		    continue;
		}
	    }


	    block_fd = open(fname, O_RDONLY);
		
	    if (block_fd == -1) {
		EPRINTF("Hotpluggable memory not supported or could not determine if block is removable...\n");
		return -1;
	    }
		
	    if (read(block_fd, status_str, BUF_SIZE) <= 0) {
		perror("Could not read block removability information\n");
		return -1;
	    }
	    
	    status_str[BUF_SIZE-1]=0;
		
	    close(block_fd);
		
	    if (atoi(status_str) == 1) {
		VPRINTF("Removable ");
	    } else {
		VPRINTF("Not removable\n");
		continue;
	    }
	    
	    snprintf(fname, BUF_SIZE, "%s%s/state", SYS_PATH, tmp_dir->d_name);
	    
	    block_fd = open(fname, O_RDONLY);
	    
	    if (block_fd<0) { 
		perror("Could not open block state\n");
		return -1;
	    }

	    if (read(block_fd, status_str, BUF_SIZE) <=0) { 
		perror("Could not read block state information\n");
		return -1;
	    }

	    status_str[BUF_SIZE-1]=0;

	    close(block_fd);

	    if (!strncasecmp(status_str,"offline",7)) {
		VPRINTF("and Already Offline (unusable)\n");
	    } else if (!strncasecmp(status_str,"online",6)) { 
		VPRINTF("and Online (usable)\n");
		bitmap[major] |= (0x1 << minor);
	    } else {
		VPRINTF("and in Unknown State '%s' (unusable)\n",status_str);
	    }
	    
	}
	
    }
    
    while (!mem_ready) {

	/* Scan bitmap for enough consecutive space */
	{
	    // num_blocks: The number of blocks we need to find
	    // bitmap: bitmap of blocks (1 == allocatable)
	    // bitmap_entries: number of blocks in the system/number of bits in bitmap
	    // reg_start: The block index where our allocation will start
		
	    int i = 0;
	    int run_len = 0;
	    
	    for (i = 0; i < bitmap_entries; i++) {
		int i_major = i / 8;
		int i_minor = i % 8;
		
		if (!(bitmap[i_major] & (0x1 << i_minor))) {
		    reg_start = i + 1; // skip the region start to next entry
		    run_len = 0;
		    continue;
		}
		
		run_len++;
		    
		if (run_len >= num_blocks) {
		    break;
		}
	    }
	    
	    
	    if (run_len < num_blocks) {
		EPRINTF("Could not find enough consecutive memory blocks... (found %d)\n", run_len);
		// no offlining yet, so no need to unwind here
		return -1;
	    }
	}
	
	
	/* Offline memory blocks starting at reg_start */
	{
	    int i = 0;
	    
	    for (i = 0; i < num_blocks; i++) {	
		FILE * block_file = NULL;
		char fname[256];
		
		memset(fname, 0, 256);
		
		snprintf(fname, 256, "%smemory%d/state", SYS_PATH, i + reg_start);
		
		block_file = fopen(fname, "r+");
		
		if (block_file == NULL) {
		    perror("Could not open block file");
		    UNWIND(reg_start, i+reg_start-1);
		    return -1;
		}
		
		    
		VPRINTF("Offlining block %d (%s)\n", i + reg_start, fname);
		fprintf(block_file, "offline\n");
		
		fclose(block_file);
		
	    }
	}
	
	
	/*  We asked to offline set of blocks, but Linux could have lied. 
	 *  To be safe, check whether blocks were offlined and start again if not 
	 */
	
	{
	    int i = 0;
	    
	    mem_ready = 1; // Hopefully we are ok...
	    
	    
	    for (i = 0; i < num_blocks; i++) {
		int block_fd = 0;
		char fname[BUF_SIZE];
		char status_buf[BUF_SIZE];
		
		
		memset(fname, 0, BUF_SIZE);
		memset(status_buf, 0, BUF_SIZE);
		
		snprintf(fname, BUF_SIZE, "%smemory%d/state", SYS_PATH, i + reg_start);
		
		
		block_fd = open(fname, O_RDONLY);
		
		if (block_fd == -1) {
		    perror("Could not open block state file");
		    return -1;
		}
		
		if (read(block_fd, status_buf, BUF_SIZE) <= 0) {
		    perror("Could not read block state");
		    return -1;
		}

		status_buf[BUF_SIZE-1]=0;
		
		VPRINTF("Checking offlined block %d (%s)...", i + reg_start, fname);
		
		int ret = strncmp(status_buf, "offline", strlen("offline"));
		
		if (ret != 0) {  // uh oh
		    int j = 0;
		    int major = (i + reg_start) / 8;
		    int minor = (i + reg_start) % 8;
            char * pos;

		    bitmap[major] &= ~(0x1 << minor); // mark the block as not removable in bitmap
		    
		    mem_ready = 0; // Keep searching
		    
            // remove trailing newline
            if ((pos=strchr(status_buf, '\n')) != NULL) {
                *pos = '\0';
            }

		    EPRINTF("ERROR - block status is '%s'\n", status_buf);

		    // Unwind space
		    UNWIND(reg_start,reg_start+num_blocks-1);
		    
		    break;
		} 
	    }
	    
	    VPRINTF("Offlined Memory OK\n");
		
	}
    }
    
    /* Memory is offlined. Calculate size and phys start addr to send to Palacios */
    *num_bytes = (unsigned long long)(num_blocks) * (unsigned long long)(block_size_bytes);
    *base_addr = (unsigned long long)(reg_start) * (unsigned long long)(block_size_bytes);
    
    return 0;
}


static int online_memory(unsigned long long base_addr,
			 unsigned long long num_bytes)
{
    
    unsigned int block_size_bytes = 0;
    int bitmap_entries = 0;
    unsigned char * bitmap = NULL;
    int num_blocks = 0;    
    int reg_start = 0;
    int mem_ready = 0;
    
    

    VPRINTF("Trying to online memory from %llu to %llu\n",base_addr,base_addr+num_bytes-1);
	
    /* Figure out the block size */
    {
	int tmp_fd = 0;
	char tmp_buf[BUF_SIZE];
	
	tmp_fd = open(SYS_PATH "block_size_bytes", O_RDONLY);
	
	if (tmp_fd == -1) {
	    perror("Could not open block size file: " SYS_PATH "block_size_bytes");
	    return -1;
	}
    
	if (read(tmp_fd, tmp_buf, BUF_SIZE) <= 0) {
	    perror("Could not read block size file: " SYS_PATH "block_size_bytes");
	    return -1;
	}
	
	close(tmp_fd);
	
	block_size_bytes = strtoll(tmp_buf, NULL, 16);
	
	VPRINTF("Memory block size is %dMB (%d bytes)\n", block_size_bytes / (1024 * 1024), block_size_bytes);
	
    }
    
    num_blocks =  num_bytes / block_size_bytes;
    if (num_bytes % block_size_bytes) num_blocks++;

    reg_start = base_addr / block_size_bytes;

    VPRINTF("That is %lu blocks of size %llu starting at block %d\n", num_blocks, block_size_bytes, reg_start);
   
    
	
    /* Online memory blocks starting at reg_start */
    {
	int i = 0;
	    
	for (i = 0; i < num_blocks; i++) {	
	    FILE * block_file = NULL;
	    char fname[256];
		
	    memset(fname, 0, 256);
	    
	    snprintf(fname, 256, "%smemory%d/state", SYS_PATH, i + reg_start);
	    
	    block_file = fopen(fname, "r+");
	    
	    if (block_file == NULL) {
		perror("Could not open block file");
		return -1;
	    }
		
	    
	    VPRINTF("Onlining block %d (%s)\n", i + reg_start, fname);
	    fprintf(block_file, "online\n");
	    
	    fclose(block_file);
	    
	}
    }
    
    return 0;
    
}



static int read_offlined()
{
    rewind(off);
    unsigned long long base, len;
    int i;
    
    num_offline=0;
    while (fscanf(off,"%llx\t%llx\n",&base,&len)==2) { num_offline++; }


    start_offline=(unsigned long long *)calloc(num_offline, sizeof(unsigned long long));
    len_offline=(unsigned long long *)calloc(num_offline, sizeof(unsigned long long));

    if (!start_offline || !len_offline) { 
	EPRINTF("Cannot allocate space to load offline map\n");
	return -1;
    }

    rewind(off);
    for (i=0;i<num_offline;i++) { 
	fscanf(off,"%llx\t%llx",&(start_offline[i]),&(len_offline[i]));
    }
    // we are now back to the end, and can keep appending
    return 0;
}


static int write_offlined()
{
    int i;

    fclose(off);
    if (!(off=fopen(offname,"w+"))) {  // truncate
	EPRINTF("Cannot open %s for writing!\n",offname);
	return -1;
    }

    for (i=0;i<num_offline;i++) { 
	if (len_offline[i]) { 
	    fprintf(off,"%llx\t%llx\n",start_offline[i],len_offline[i]);
	}
    }
    // we are now back to the end, and can keep appending
    return 0;
}


static int clear_offlined()
{
    free(start_offline);
    free(len_offline);
    return 0;
}

static int find_offlined(unsigned long long base_addr)
{
    int i;

    for (i=0;i<num_offline;i++) { 
	if (base_addr>=start_offline[i] &&
	    base_addr<(start_offline[i]+len_offline[i])) { 
	    return i;
	}
    }

    return -1;

}



static int get_kernel_setup()
{
    FILE *f;
    
    f = fopen("/proc/v3vee/v3-info", "r");
    
    if (!f) { 
	EPRINTF("Cannot open /proc/v3vee/v3-info\n");
	return -1;
    }

    if (fscanf(f,"kernel MAX_ORDER:\t%llu\n",&kernel_max_order)!=1) { 
	EPRINTF("Cannot read kernel MAX_ORDER\n");
	return -1;
    }

    kernel_max_page_alloc_bytes =  4096ULL * (0x1ULL << kernel_max_order);

    if (fscanf(f,"number of nodes:\t%llu\n",&kernel_num_nodes)!=1) { 
	EPRINTF("Cannot read kernel number of numa nodes\n");
	return -1;
    }

    if (fscanf(f,"number of cpus:\t%llu\n",&kernel_num_cpus)!=1) { 
	EPRINTF("Cannot read kernel number of cpus\n");
	return -1;
    }

    if (fscanf(f,"palacios compiled mem_block_size:\t%llu\n",&palacios_compiled_mem_block_size)!=1) { 
	EPRINTF("Cannot read palacios compiled mem_block_size\n");
	return -1;
    }

    if (fscanf(f,"palacios run-time mem_block_size:\t%llu\n",&palacios_runtime_mem_block_size)!=1) { 
	EPRINTF("Cannot read palacios run-time mem_block_size\n");
	return -1;
    }

    return 0;
}

    
