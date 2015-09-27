#ifndef __v3_hvm_ros_user
#define __v3_hvm_ros_user

/*
  Copyright (c) 2015 Peter Dinda
*/



// setup and teardown
// note that there is ONE HRT hence  no naming
int v3_hvm_ros_user_init();
int v3_hvm_ros_user_deinit();

// Establish function to be invoked by the VMM
// to signal activity (basically an interrupt handler)
// The handler can use the GPRs, but must save/restore
// any other registers it needs itself.  If it goes
// out of its stack, it's out of luck
int v3_hvm_ros_register_signal(void (*handler)(uint64_t), void *stack, uint64_t stack_size); 
int v3_hvm_ros_unregister_signal();

// Replace the existing HRT with a new one
//  - this does not boot the new HRT
//  - the intial image is the one given in the .pal config
int v3_hvm_ros_install_hrt_image(void *image, uint64_t size);

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

// Signal the HRT from the ROS
// The ROS can call this too, but it shouldn't be necesary
int  v3_hvm_hrt_signal_ros(uint64_t code);

#endif
