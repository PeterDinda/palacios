/* 
 * Device File Virtualization Userland Shadow 
 * (c) Akhil Guliani and William Gross, 2015
 */



#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <sys/ioctl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <unistd.h> 
#include <string.h>
#include <getopt.h> 
#include <errno.h>

//DEV File include
#include <sys/mman.h>
#include <stdint.h>
#include <sys/socket.h>
#include <signal.h>
#include <linux/wait.h>
#include <sys/select.h>

#include "v3_guest_mem.h"

#define DEV_FILE_IOCTL_PATH "/dev/v3-devfile"

// IOCTL codes for signalling
#define MY_MACIG 'G'
#define INIT_IOCTL _IOR(MY_MACIG, 0, int)
#define WRITE_IOCTL _IOW(MY_MACIG, 1, int)
#define SHADOW_SYSCALL_DONE _IOW(MY_MACIG, 2, int)


#if 1
#define DEBUG_PRINT(fmt, args...) printf(fmt, ##args)
#else
#define DEBUG_PRINT(fmt, args...) 
#endif


//Helper function to generate user virtual address for a given guest physical address
static inline uint64_t get_uva_from_gpa(struct v3_guest_mem_map *map, uint64_t gpa)
{
    int i;
    uint64_t prev_gpa, prev_uva, offset;
    prev_gpa = 0;
    prev_uva = 0;
    for (i=0; i< map->numblocks; i++) { 
        if(gpa < (uint64_t)map->block[i].gpa){
            offset = gpa - prev_gpa;
            DEBUG_PRINT("gpa %llx, prev_gpa %llx, offset %llx, res %llx\n",gpa,prev_gpa,offset,prev_uva+offset);
            return prev_uva+offset;
        }
        prev_uva = (uint64_t)map->block[i].uva;
        if(i==0){
            prev_gpa = 0;
        }
        else{
            prev_gpa = (uint64_t)map->block[i].gpa;
        }
    } 
}

static inline int deref_args(struct v3_guest_mem_map *map,
			     uint64_t * a1, uint64_t * a2,
			     uint64_t * a3, uint64_t * a4,
			     uint64_t * a5, uint64_t * a6, 
			     uint64_t bvec)
{
    if (bvec & 1){
        DEBUG_PRINT("bvec 1\n");
        uint64_t a1tmp = *a1;
        *a1 = get_uva_from_gpa(map,a1tmp);
    }
    if (bvec & 2){
        DEBUG_PRINT("bvec 2\n");
        uint64_t a2tmp = *a2;
        *a2 = get_uva_from_gpa(map,a2tmp);
    }
    if (bvec & 4){
        DEBUG_PRINT("bvec 4\n");
        uint64_t a3tmp = *a3;
        *a3 = get_uva_from_gpa(map,a3tmp);
    }
    if (bvec & 8){
        DEBUG_PRINT("bvec 8\n");
        uint64_t a4tmp = *a4;
        *a4 = get_uva_from_gpa(map,a4tmp);
    }
    if (bvec & 16){
        DEBUG_PRINT("bvec 16\n");
        uint64_t a5tmp = *a5;
        *a5 = get_uva_from_gpa(map,a5tmp);
    }
    if (bvec & 32){
        DEBUG_PRINT("bvec 32\n");
        uint64_t a6tmp = *a6;
        *a6 = get_uva_from_gpa(map,a6tmp);
    }
    return 0; 
}

static inline int store_args_to_shared_page(uint64_t* shared_page, uint64_t* return_val, uint64_t *sys_errno)
{
    *(shared_page+8) = *return_val;
    *(shared_page+9) = *sys_errno;
    return 0;
}
    
static inline int load_args_from_shared_page(uint64_t* shared_page,
					     uint64_t* sys_code,
					     uint64_t* a1, uint64_t* a2, uint64_t* a3,
					     uint64_t* a4, uint64_t* a5, uint64_t* a6,
					     uint64_t* bit_vec)
{
    *sys_code = *shared_page;
    *a1 = *(shared_page+1);
    *a2 = *(shared_page+2);
    *a3 = *(shared_page+3);
    *a4 = *(shared_page+4);
    *a5 = *(shared_page+5);
    *a6 = *(shared_page+6);
    *bit_vec = *(shared_page+7);
    return 0;
};



// Get Physical address for given virtual address 
//
// This is horrific, but note that it is used only to locate
// the shared page - it is not used in the main execution loop
static uint64_t vtop(uint64_t vaddr) 
{
    FILE *pagemap;
    uint64_t paddr = 0;
    uint64_t offset = (vaddr / sysconf(_SC_PAGESIZE)) * sizeof(uint64_t);
    uint64_t e;
    char addr[80];
    sprintf(addr,"/proc/%d/pagemap", getpid());
    DEBUG_PRINT("Page size : %d, Offset: %d, In vtop",sysconf(_SC_PAGESIZE),offset);
    // https://www.kernel.org/doc/Documentation/vm/pagemap.txt
    if ((pagemap = fopen(addr, "r"))) {
        if (lseek(fileno(pagemap), offset, SEEK_SET) == offset) {
            if (fread(&e, sizeof(uint64_t), 1, pagemap)) {
                if (e & (1ULL << 63)) { // page present ?
                    paddr = e & ((1ULL << 54) - 1); // pfn mask
                    paddr = paddr * sysconf(_SC_PAGESIZE);
                    // add offset within page
                    paddr = paddr | (vaddr & (sysconf(_SC_PAGESIZE) - 1));
                }   
            }   
        }   
        fclose(pagemap);
    }   

    DEBUG_PRINT("The returned conversion is vaddr %p to paddr %p \n",(void*)vaddr,(void*)paddr);
    return paddr;
}




void usage()
{
    printf("v3_devfile_shadow <vm_device>\n\n");
    printf("Shadow process to support device file-level virtualization\n"
	   "in the spirit of Paradice.\n\n"
	   "This operates with the devfile_host.ko kernel module\n"
	   "in the host and devfile_preload.so preload library in\n"
	   "the guest.  These can be found, along with example scripts\n"
	   "in palacios/gears/services/devfile.\n\n"
	   "The general steps are:\n"
	   " 1. insmod kernel module into host\n"
	   " 2. copy preload library into guest\n"
	   " 3. instantiate guest\n"
	   " 4. use v3_hypercall to bind the devfile hypercall (99993)\n"
	   "    to the kernel module for your guest\n"
	   " 5. run v3_devfile_shadow\n"
	   " 6. run process in guest that uses preload library\n\n");
}

int main(int argc, char** argv)
{

     struct v3_guest_mem_map *map;
     uint64_t sys_rc, sys_errno, sys_code, a1,a2,a3,a4,a5,a6, bit_vec;
     int host_mod_fd, fd_shared, i, val;
     long long zero = 0;
     void* unbacked_region;
     fd_set readset;
     int select_rc;



     if (argc!=2) {
	 usage();
	 return 0;
     }

     if (!(map=v3_guest_mem_get_map(argv[1]))) { 
         DEBUG_PRINT("Cannot get guest memory map for %s\n",argv[1]);
         return -1;
     }

     if (v3_map_guest_mem(map)) { 
         DEBUG_PRINT("Cannot map guest memory\n");
         free(map);
         return -1;
     }

     for (i=0; i< map->numblocks; i++) { 
         DEBUG_PRINT("Region %llu: gpa=%p, hpa=%p, uva=%p, numpages=%llu\n", 
                 i, map->block[i].gpa, map->block[i].hpa,map->block[i].uva, map->block[i].numpages);

     }  


     if ((host_mod_fd = open(DEV_FILE_IOCTL_PATH, O_RDWR)) < 0) {
         perror("open");
         return -1;
     }

     unbacked_region = mmap(NULL,4096,PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED | MAP_ANONYMOUS, -1,0);

     if (unbacked_region == MAP_FAILED){
         perror("Anon Region: ");
         return -1;
     }

     sprintf(unbacked_region,"helloworldhelloworldhelloworld");

     DEBUG_PRINT("unbacked_region ptr: %p contents: %s\n",unbacked_region,(char*)unbacked_region);
     DEBUG_PRINT("Vaddr : %p ; Paddr : %p\n", unbacked_region, vtop((uint64_t)unbacked_region));
     DEBUG_PRINT("Persisting as a userspace for VM %s with address %p\n",argv[1], unbacked_region);

     if (ioctl(host_mod_fd,INIT_IOCTL,vtop((uint64_t)unbacked_region))) {
         perror("init ioctl");
	 return -1;
     }

     while (1) { 
         FD_ZERO(&readset);
         FD_SET(host_mod_fd,&readset);

         select_rc = select(host_mod_fd+1,&readset,0,0,0);

	 if (select_rc<0) { 
	     if (errno==EAGAIN) { 
		 continue;
	     } else {
		 perror("Select failed");
		 return -1;
	     }
	 }

         DEBUG_PRINT("Device File: Handling a forwarded system call\n");

         //Get syscall arguments from shared page
         load_args_from_shared_page((uint64_t*)unbacked_region,&sys_code,&a1,&a2,&a3,&a4,&a5,&a6,&bit_vec);

         //Extract Guest pointer arguments and map them to current userspace
         DEBUG_PRINT("About to deref args\nsys_code: %016llu, a1: %016llu, a2: %016llu,\na3: %016llu,a4: %016llu,a5: %016llu,a6: %016llu,bit_vec: %016llu\n", sys_code, a1, a2, a3,a4,a5,a6,bit_vec);

	 // swizzle pointers from their GPAs to their HVAs
         deref_args(map,&a1,&a2,&a3,&a4,&a5,&a6,bit_vec);

         DEBUG_PRINT("Derefed args\nsys_code: %016llu, a1: %016llu, a2: %016llu,\na3: %016llu,a4: %016llu,a5: %016llu,a6: %016llu,bit_vec: %016llu\n", sys_code, a1, a2, a3,a4,a5,a6,bit_vec);

         sys_rc = syscall(sys_code,a1,a2,a3,a4,a5,a6);            
	 sys_errno = errno;

         if (sys_rc < 0){
             perror("Failed Syscall: ");
         }

         DEBUG_PRINT("Device File: System call rc %d, errno %d\n",(int)sys_rc,sys_errno);

         //put return value into shared region
         store_args_to_shared_page((uint64_t*)unbacked_region, &sys_rc, &sys_errno); 

         // return to host module
         ioctl(host_mod_fd,SHADOW_SYSCALL_DONE,0);

     }

     return 0;

 }
