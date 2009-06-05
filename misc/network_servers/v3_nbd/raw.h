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

#ifndef __RAW_H__
#define __RAW_H__

#include "v3_disk.h"
#include <stdio.h>

class raw_disk : public v3_disk {


 public:
    raw_disk(string & filename);

    off_t get_capacity();
    unsigned int read(unsigned char * buf, off_t offset, int length);
    unsigned int write(unsigned char * buf, off_t offset, int length);

    void attach();
    void detach();

 private:
    FILE * f;
};

#endif
