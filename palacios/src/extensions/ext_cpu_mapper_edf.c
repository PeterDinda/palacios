/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National
 * Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at
 * http://www.v3vee.org
 *
 * Copyright (c) 2013, Oscar Mondragon <omondrag@cs.unm.edu>
 * Copyright (c) 2013, The V3VEE Project <http://www.v3vee.org>
 * All rights reserved.
 *
 * Author: Oscar Mondragon <omondrag@cs.unm.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <palacios/vmm.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_extensions.h>
#include <palacios/vmm_cpu_mapper.h>

#ifndef V3_CONFIG_DEBUG_EXT_CPU_MAPPER_EDF
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


/* Overview
 *
 * cpu_mapper for EDF Scheduling
 *
 */

#define MAX_TDF 20
#define MIN_TDF 1
#define UTILIZATION 80

// Next Fit heuristic implementation

int nextFit(int save, int tdf,struct v3_vm_info *vm){

    PrintDebug(vm, VCORE_NONE,"nextFit for tdf %d \n", tdf);

    int V = vm->num_cores;          // Number of virtual cores
    int L = vm->avail_cores;        // Number of Logical cores
    int speedRatio[L];              // mapped virtual cores to logical core ratio. Must be less or equal than UTILIZATION X TDF
    uint64_t speedVCores[V];        // Virtual core speeds
    int mapping[V];                 // mapping array
    int vc=0;                       // virtual core id
    int lc=0;                       // logical core id
    uint_t cpu_khz = V3_CPU_KHZ();  // Physical core speed
    int i=0;

    for(i=0;i<L;i++){
        speedRatio[i]=0;
    }

    for(i=0;i<V;i++){
        mapping[i] = 0;
        speedVCores[i] = 0;
    }

    // Initialize Virtual cores utilization vector
    v3_cfg_tree_t * cfg_tree = vm->cfg_data->cfg;
    v3_cfg_tree_t * core = v3_cfg_subtree(v3_cfg_subtree(cfg_tree, "cores"), "core");

    while (core){

        uint_t vcSpeed = cpu_khz;
        char *speed = v3_cfg_val(core, "khz");
        if(speed){
            vcSpeed = atoi(speed);
        }

        speedVCores[vc] = vcSpeed;
        vc++;
        core = v3_cfg_next_branch(core);

	/*PrintDebug(VM_NONE, VCORE_NONE,"mapper. vc %d, speed %llu, vcores %d, lcores %d, cpu_khz %u\n",
               vc,
	       vcSpeed,
	       V,
	       L,
	       cpu_khz);*/

 }


    vc = 0;

    // TODO Control Target CPU case

    while(vc < V){

      //PrintDebug(VM_NONE, VCORE_NONE,"mapper. vc %d, ratio %llu, tdf %d\n",vc, (speedRatio[lc] + 100*speedVCores[vc]/cpu_khz), tdf);

      if( (speedRatio[lc] + 100*speedVCores[vc]/cpu_khz) <= (UTILIZATION*tdf)){
            mapping[vc] = lc;
            speedRatio[lc] += 100*speedVCores[vc]/cpu_khz;
        }
        else{
	  if (((lc+1)< L) &&  (100*speedVCores[vc]/cpu_khz <= (UTILIZATION*tdf))){
               lc = lc+1;
               mapping[vc] = lc;
               speedRatio[lc] += 100*speedVCores[vc]/cpu_khz;
            }
            else{
                return -1;  // Could not map
            }

        }

      vc = vc +1;
     }

    if(save ==0){

        vc=0;

        // Assing computed TDF
        struct v3_time *vm_ts = &(vm->time_state);
	vm_ts->td_denom = tdf;

        // mapping virtual cores in logical cores
        for (vc = 0; vc < vm->num_cores; vc++) {
            struct guest_info * core = &(vm->cores[vc]);
            core-> pcpu_id = mapping[vc];
        }


        int x = 0;
        PrintDebug(vm, VCORE_NONE,"mapper. Number of Logical cores: %d",L);
        PrintDebug(vm, VCORE_NONE, "mapper. Mapping Array:\n");
        for(x=0;x<V;x++){
	    PrintDebug(vm, VCORE_NONE,"mapper. vcore %d: %d ",x,mapping[x]);
        }
    }

    return 0;
}


int edf_mapper_vm_init(struct v3_vm_info *vm){

    PrintDebug(vm, VCORE_NONE,"mapper. Initializing edf cpu_mapper");
    return 0;
}


int edf_mapper_admit(struct v3_vm_info *vm, unsigned int cpu_mask){
    PrintDebug(vm, VCORE_NONE,"mapper. Edf cpu_mapper admit\n");

    int min_tdf = MIN_TDF;
    int max_tdf = MAX_TDF;
    int tdf = MAX_TDF; // Time dilation factor


     // Binary Search of TDF

    int mappable = 0;

    while( (max_tdf-min_tdf) > 0 ){

        mappable = nextFit(-1,tdf,vm);

        if(mappable != -1){
            max_tdf = tdf/2;
        }
        else{
            max_tdf = tdf * 2;
            min_tdf = tdf;
        }
        tdf = max_tdf;
    }

    mappable =  nextFit(-1,tdf,vm);
    if(mappable !=-1){
        nextFit(0,tdf,vm);
    }
    else{
        tdf = 2*tdf;
        nextFit(0,tdf,vm);
    }

    PrintDebug(vm, VCORE_NONE,"mapper. Calculated TDF denom %d\n",tdf);
   return 0;
}

int edf_mapper_admit_core(struct v3_vm_info * vm, int vcore_id, int target_cpu){
    PrintDebug(vm, VCORE_NONE,"mapper. Edf cpu_mapper admit core\n");
    return 0;
}


static struct vm_cpu_mapper_impl edf_mapper = {

    .name = "edf",
    .init = NULL,
    .deinit = NULL,
    .vm_init = edf_mapper_vm_init,
    .vm_deinit = NULL,
    .admit = edf_mapper_admit,
    .admit_core = edf_mapper_admit_core

};


static int
ext_mapper_edf_init() {
    PrintDebug(VM_NONE, VCORE_NONE,"mapper. Creating (%s) cpu_mapper\n",edf_mapper.name);
    return v3_register_cpu_mapper(&edf_mapper);
}

static int
ext_mapper_edf_vm_init() {
    return 0;
}

static struct v3_extension_impl mapper_edf_impl = {

    .name = "cpu_mapper for EDF Scheduler",
    .init = ext_mapper_edf_init,
    .vm_init = ext_mapper_edf_vm_init,
    .vm_deinit = NULL,
    .core_init = NULL,
    .core_deinit = NULL,
    .on_entry = NULL,
    .on_exit = NULL
};

register_extension(&mapper_edf_impl);
