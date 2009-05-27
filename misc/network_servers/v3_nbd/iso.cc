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
#include "iso.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


iso_image::iso_image(string & filename) : v3_disk(filename){
    this->f = fopen(filename.c_str(), "w+");
}

iso_image::~iso_image() {
}

off_t iso_image::get_capacity() {
    struct stat f_stats;

    stat(this->filename.c_str(), &f_stats);

    return f_stats.st_size;
}



int iso_image::read(unsigned char * buf, unsigned long long offset, int length) {

    return -1;
}



int iso_image::write(unsigned char * buf, unsigned long long offset, int length) {
    
    return -1;
}

void iso_image::attach() {
}


void iso_image::detach() {
}
