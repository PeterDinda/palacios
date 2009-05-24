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

#ifndef __V3_NBD_H__
#define __V3_NBD_H__

#include <string>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <sstream>

#ifdef linux 
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(WIN32) && !defined(__CYGWIN__)

#endif


using namespace std;



#define DEFAULT_LOG_FILE "./status.log"
#define VIDS_CONF_FILE "v3_nbd.ini"
#define MAX_STRING_SIZE 1024


typedef struct nbd_config {
  unsigned long server_addr;
  int server_port;


} nbd_config_t;



void usage();
int config_nbd(string conf_file_name);



#define VIDS_SERVER_TAG "vids_server"


#endif // !__VIDS_H
