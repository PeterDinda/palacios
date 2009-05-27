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

#include "v3_nbd.h"



raw_disk::raw_disk(string &filename) {
    this->f = fopen(filename.c_str(), "w+");
}


unsigned long long raw_disk::get_capacity() {
    

}



int raw_disk::read(unsigned char * buf, unsigned long long offset, int length) {


}



int raw_disk::write(unsigned char * buf, unsigned long long offset, int length) {


}
