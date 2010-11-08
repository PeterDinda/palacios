/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __DEVICES_SERIAL_H__
#define __DEVICES_SERIAL_H__

#ifdef __V3VEE__

/* Really need to find clean way to allow a backend stream device to be attachable
   to different kinds of frontend devices that can act as a stream */

struct v3_stream_ops  {
    /* called by serial device to the backend stream device */
    int (*read)(char *buf, uint_t len, void *private_data);
    int (*write)(char *buf, uint_t len, void *private_data);

    /* called by backend device to frontend serial device */
    int (*input)(char *buf, uint_t len, void *front_data);
    void *front_data;
};


int v3_stream_register_serial(struct vm_device * serial_dev, struct v3_stream_ops * ops, void * private_data);

#endif // ! __V3VEE__


#endif
