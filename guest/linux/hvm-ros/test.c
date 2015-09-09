#include <sys/syscall.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "v3_hvm_ros_user.h"

typedef unsigned char uchar_t;

#define rdtscll(val)					\
    do {						\
	uint64_t tsc;					\
	uint32_t a, d;					\
	asm volatile("rdtsc" : "=a" (a), "=d" (d));	\
	*(uint32_t *)&(tsc) = a;			\
	*(uint32_t *)(((uchar_t *)&tsc) + 4) = d;	\
	val = tsc;					\
    } while (0)					



int simple_test_sync()
{
    char buf[4096];  
    
    memset(buf,1,4096);
    memset(buf,0,4096);
    strcpy(buf,"hello world\n");

    
    printf("Merge\n");
    if (v3_hvm_ros_merge_address_spaces()) { 
	printf("failed to merge address spaces\n");
	return -1;
    }

    printf("Synchronize\n");
    if (v3_hvm_ros_synchronize()) { 
	printf("failed to synchronize\n");
	return -1;
    }

    printf("Invoke\n");
    if (v3_hvm_ros_invoke_hrt_sync(buf,0)) { 
	printf("failed to invoke HRT\n");
	return -1;
    }

    printf("Desynchonize\n");
    if (v3_hvm_ros_desynchronize()) { 
	printf("failed to desynchronize\n");
	return -1;
    }

    printf("Unmerge\n");
    if (v3_hvm_ros_unmerge_address_spaces()) { 
	printf("failed to merge address spaces\n");
	return -1;
    }

    printf("Done.\n");

    return 0;
}

int timing_test_sync(uint64_t num_merge, uint64_t num_call)
{
    char buf[4096];  
    unsigned long long start,end,i;
    
    memset(buf,1,4096);
    memset(buf,0,4096);
    strcpy(buf,"hello world\n");

    printf("Executing %lu address space merges\n",num_merge);
    rdtscll(start);
    for (i=0;i<num_merge;i++) {
	if (v3_hvm_ros_merge_address_spaces()) { 
	    return -1;
	} 
	//fprintf(stderr,"%llu\n",i+1);
    }
    rdtscll(end);
    printf("Took %llu cycles, %llu cycles/iter, or %lf seconds/iter\n",end-start,(end-start)/num_merge,(((double)end-start)/num_merge)/2.1e9);
    
    printf("Setting up synchronous invocation\n");
    
    if (v3_hvm_ros_synchronize()) {
	return -1;
    }

    printf("Executing %lu HRT calls synchronously\n",num_call);
    rdtscll(start);
    for (i=0;i<num_call;i++) {
	if (v3_hvm_ros_invoke_hrt_sync(buf,0)) {
	    return -1;
	} 
	//fprintf(stderr,"%llu\n",i+1);
    }
    rdtscll(end);
    printf("Took %llu cycles, %llu cycles/iter, or %lf seconds/iter\n",end-start,(end-start)/num_call,(((double)end-start)/num_call)/2.1e9);

    if (v3_hvm_ros_desynchronize()) { 
	return -1;
    }

    if (v3_hvm_ros_unmerge_address_spaces()) { 
	return -1;
    } 

    return 0;
}

int simple_test_async()
{
    char buf[4096];  
    
    memset(buf,1,4096);
    memset(buf,0,4096);
    strcpy(buf,"hello world\n");

   printf("Merge\n");

    if (v3_hvm_ros_merge_address_spaces()) { 
	printf("failed to merge address spaces\n");
	return -1;
    }

    printf("Invoke\n");

    if (v3_hvm_ros_invoke_hrt_async(buf,0)) { 
	printf("failed to invoke HRT\n");
	return -1;
    }

    printf("Unmerge\n");

    if (v3_hvm_ros_unmerge_address_spaces()) { 
	printf("failed to unmerge address spaces\n");
	return -1;
    }
    
    
    printf("Done.\n");
    

    return 0;
}

int timing_test_async(uint64_t num_merge, uint64_t num_call)
{
    char buf[4096];  
    unsigned long long start,end,i;
    
    memset(buf,1,4096);
    memset(buf,0,4096);
    strcpy(buf,"hello world\n");

    printf("Executing %lu address space merges\n",num_merge);
    rdtscll(start);
    for (i=0;i<num_merge;i++) {
	if (v3_hvm_ros_merge_address_spaces()) { 
	    return -1;
	} 
	//fprintf(stderr,"%llu\n",i+1);
    }
    rdtscll(end);
    printf("Took %llu cycles, %llu cycles/iter, or %lf seconds/iter\n",end-start,(end-start)/num_merge,(((double)end-start)/num_merge)/2.1e9);
    
    printf("Executing %lu HRT calls\n",num_call);
    rdtscll(start);
    for (i=0;i<num_call;i++) {
	if (v3_hvm_ros_invoke_hrt_async(buf,0)) {
	    return -1;
	} 
	//fprintf(stderr,"%llu\n",i+1);
    }
    rdtscll(end);
    printf("Took %llu cycles, %llu cycles/iter, or %lf seconds/iter\n",end-start,(end-start)/num_call,(((double)end-start)/num_call)/2.1e9);

    if (v3_hvm_ros_unmerge_address_spaces()) { 
	return -1;
    } 

    return 0;
}

int main(int argc, char *argv[]) 
{
    int rc;
    
    if (argc!=3 && argc!=5) { 
	printf("usage: test simple|time sync|async num_merges num_calls\n");
	return -1;
    }
    
    v3_hvm_ros_user_init();
    
    if (argv[1][0]=='s') {
	if (argv[2][0]=='s') { 
	    rc=simple_test_sync();
	} else if (argv[2][0]=='a') {
	    rc=simple_test_async();
	} else {
	    printf("Unknown type %s\n",argv[2]);
	    rc=-1;
	}
    } else if (argv[1][0]=='t') {
	if (argv[2][0]=='s') { 
	    rc=timing_test_sync(atoi(argv[3]),atoi(argv[4]));
	} else if (argv[2][0]=='a') {
	    rc=timing_test_async(atoi(argv[3]),atoi(argv[4]));
	} else {
	    printf("Unknown type %s\n",argv[2]);
	    rc=-1;
	}
    } else {
	printf("Unknown mode %s\n",argv[1]);
	rc=-1;
    }
    
    v3_hvm_ros_user_deinit();

    return rc;
}
