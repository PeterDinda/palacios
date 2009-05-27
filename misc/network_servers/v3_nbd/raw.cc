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

#include "v3_disk.h"
#include "raw.h"


raw_disk::raw_disk(string & filename) : v3_disk(filename){
    this->f = fopen(filename.c_str(), "w+");
}


off_t raw_disk::get_capacity() {
    
    return 0;
}



int raw_disk::read(unsigned char * buf, unsigned long long offset, int length) {

    return -1;
}



int raw_disk::write(unsigned char * buf, unsigned long long offset, int length) {
    
    return -1;
}


void raw_disk::attach() {
    this->locked = 1;
}


void raw_disk::detach() {
    this->locked = 0;
}
