/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Madhav Suresh <madhav@u.northwestern.edu> 
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Authors: Madhav Suresh <madhav@u.northwestern.edu>
 *          Mark Cartwright <mcartwright@gmail.com> (live migration)
 *          Peter Dinda <pdinda@northwestern.edu> (store interface changes)
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_CHECKPOINT_H__
#define __VMM_CHECKPOINT_H__

#ifdef __V3VEE__

#include <palacios/vmm.h>


/*
  This code implements both checkpointing and live migration.  The
  main difference between these two is how the memory of the VM is
  transfered.

  A checkpoint is written to/read from a checkpoint store.  A
  checkpoint store is conceptually an unseekable stream.  This allows
  us to reuse the same model for things like:

  - checkpoint to memory
  - checkpoint to directory of files
  - checkpoint to gem5
  - checkpoint to network
  - live migrate over network
  
  To save data to a checkpoint store, you first open a context on the
  checkpoint store, then you save data to the context. The data
  consists of blobs tagged with strings.

  Only a single context can be open at a time, and a context cannot be
  reopened once it has been closed.  These invariants are also essential
  to maximizing compatability with different implementations. 

  The result of these constraints is that the stream that the
  checkpoint store records will look like:

  [contextname1][tag1len][tag1]][datalen][data][tag2len][tag2][datalen][data]....
  [contextname2][tag1len][tag1]][datalen][data][tag2len][tag2][datalen][data]....
  ...

  The code implemented here assures that
  [tag?len][tag?][datalen][data] is written as shown above.  The
  checkpoint store handles [contextname?] internally. For example, it
  might use the context names as files for a checkpoint, or it might
  communicate the context names over a network stream.

  To add checkpointing to your code, the primary thing you need to do
  is implement save and load functions.   For a device, you then add them
  to your dev_ops structure.   The save function takes an context.  
  You then write on this context using one of the following functions/macros:

  1. v3_chkpt_save(context, tag, datalen, dataptr);
  2. V3_CHKPT_SAVE(context, tag, data, faillabel);
  3. V3_CHKPT_SAVE_AUTOTAG(context, data, faillabel)
  
  Here (2) is a macro that computes the data length from sizeof(data)
  while (3) does the same, and uses as the tag the name of the variable data.
  We strongly recommend the use of (2). 
  
  These functions and macros will return -1 if the save is unsuccessful.
  The faillabel argumnent is optional, if supplied and an error occurs,
  the macro will goto the label. 

  Some classes of devices, for example IDE HDs and CD-ROMs, and PCI
  devices have a class-specific component and a device-specific
  component.  For such devices, the class implementation (e.g., IDE, PCI) 
  will provide a class-specific save function that should be called first
  in the device's save function.  For example:

  #include <devices/pci.h>
  
  static int my_device_save(struct v3_chkpt_ctx *ctx, void *priv) 
  { 
     struct my_device *dev = priv;

     if (v3_pci_save(ctx, dev->pci)<0) { 
        goto fail;
     }

     V3_CHKPT_SAVE(ctx, "myfoo", dev->foo, failout),;
     V3_CHKPT_SAVE(ctx, "mybar", dev->bar, failout);

     // Success
     return 0;

failout:

     PrintError(info->vm_info, info, "Failed to save device\n");
     return -1;

  }     

  The load side is symmetric. 

*/

struct v3_chkpt;


struct v3_chkpt_ctx {
  struct v3_chkpt * chkpt;
  void *store_ctx;
};



/*
 * You do not need to look behind this curtain
 */
#define SELECT(x,A,FUNC, ...) FUNC
#define V3_CHKPT_SAVE_BASE(context, tag, data)  v3_chkpt_save(context,tag,sizeof(data),&(data))
#define V3_CHKPT_SAVE_LABELED(context, tag, data, faillabel)   (({if (V3_CHKPT_SAVE_BASE(context,tag,data)<0) { goto faillabel; } 0; }), 0)
#define V3_CHKPT_LOAD_BASE(context, tag, data)  v3_chkpt_load(context,tag,sizeof(data),&(data))
#define V3_CHKPT_LOAD_LABELED(context, tag, data, faillabel)   (({if (V3_CHKPT_LOAD_BASE(context,tag,data)<0) { goto faillabel; } 0; }), 0)
/*
 * Safe to open your eyes again
 */


//
// Functions and macros to save to a context
//
//
int     v3_chkpt_save(struct v3_chkpt_ctx * ctx, char * tag, uint64_t len, void * buf);
#define V3_CHKPT_SAVE(context, tag, data, ...) SELECT(,##__VA_ARGS__,V3_CHKPT_SAVE_LABELED(context,tag,data,__VA_ARGS__),V3_CHKPT_SAVE_BASE(context,tag,data))
#define V3_CHKPT_SAVE_AUTOTAG(context, data, ...) V3_CHKPT_SAVE(context, #data ,data,__VA_ARGS__)

//
// Funtions and macros to load from a context
// 
//
int     v3_chkpt_load(struct v3_chkpt_ctx * ctx, char * tag, uint64_t len, void * buf);
#define V3_CHKPT_LOAD(context, tag, data, ...) SELECT(,##__VA_ARGS__,V3_CHKPT_LOAD_LABELED(context,tag,data,__VA_ARGS__),V3_CHKPT_LOAD_BASE(context,tag,data))
#define V3_CHKPT_LOAD_AUTOTAG(context, data, ...) V3_CHKPT_LOAD(context, #data ,data,__VA_ARGS__)


struct v3_chkpt_ctx * v3_chkpt_open_ctx(struct v3_chkpt * chkpt, char * name);
int                   v3_chkpt_close_ctx(struct v3_chkpt_ctx * ctx);

int v3_chkpt_save_vm(struct v3_vm_info * vm, char * store, char * url);
int v3_chkpt_load_vm(struct v3_vm_info * vm, char * store, char * url);

#ifdef V3_CONFIG_LIVE_MIGRATION
int v3_chkpt_send_vm(struct v3_vm_info * vm, char * store, char * url);
int v3_chkpt_receive_vm(struct v3_vm_info * vm, char * store, char * url);
#endif

int V3_init_checkpoint();
int V3_deinit_checkpoint();

#endif

#endif
