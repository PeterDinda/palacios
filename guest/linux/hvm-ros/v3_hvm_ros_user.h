#ifndef __v3_hvm_ros_user
#define __v3_hvm_ros_user

/*
  Copyright (c) 2015 Peter Dinda
*/



// setup and teardown
// note that there is ONE HRT hence  no naming
int v3_hvm_ros_user_init();
int v3_hvm_ros_user_deinit();


typedef enum {RESET_HRT, RESET_ROS, RESET_BOTH} reset_type;

int v3_hvm_ros_reset(reset_type what);

int v3_hvm_ros_merge_address_spaces();
int v3_hvm_ros_unmerge_address_spaces();


// Asynchronosus invocation of the HRT using an
// opaque pointer (typically this is a pointer
// to a structure containing a function pointer and
// arguments.  The parallel flag indicates that
// that it will be invoked simulatneously on all
// cores.  
int  v3_hvm_ros_invoke_hrt_async(void *p, int parallel);


// synchronize with HRT via shared location
// allow synchronous invokcations.  Note that
// any parallelism is done internal to the HRT. 
// Also the synchronous invocation always waits
int  v3_hvm_ros_synchronize();   
int  v3_hvm_ros_invoke_hrt_sync(void *p, int handle_ros_events);
int  v3_hvm_ros_desynchronize();


#endif
