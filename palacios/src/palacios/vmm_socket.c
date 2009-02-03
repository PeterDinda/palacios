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


#include <palacios/vmm_socket.h>
#include <palacios/vmm.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_stddef.h>


struct v3_socket_hooks * sock_hooks = 0;


//static int v3_socket_api_test(void);


void V3_Init_Sockets(struct v3_socket_hooks * hooks) {
  PrintInfo("Initializing Socket Interface\n");
  sock_hooks = hooks;

  PrintDebug("V3 sockets inited\n");

  //v3_socket_api_test();
  
  return;
}



void v3_init_sock_set(struct v3_sock_set * sock_set) {
  sock_set->num_socks = 0;
  sock_set->socks = NULL;

  return;
}





/* This should probably check if the socket is already added */
// adds socket to the sockset
void v3_set_sock(struct v3_sock_set * sock_set, V3_SOCK sock) {
  struct v3_sock_entry * new_entry = V3_Malloc(sizeof(struct v3_sock_entry));

  new_entry->sock = sock;
  new_entry->is_set = 0;

  if (sock_set->socks) {
    new_entry->next = sock_set->socks;
  }

  sock_set->socks = new_entry;

  sock_set->num_socks++;
}


// deletes socket from sockset
void v3_clr_sock(struct v3_sock_set * sock_set, V3_SOCK sock) {
  struct v3_sock_entry * iter, * back_ptr;

  iter = sock_set->socks;
  back_ptr = NULL;

  v3_foreach_sock(sock_set, iter) {
    if (iter->sock == sock) {
      if (back_ptr == NULL) {
	sock_set->socks = iter->next;
      } else {
	back_ptr->next = iter->next;
      }

      V3_Free(iter);

      sock_set->num_socks--;
      break;
    }

    back_ptr = iter;
  }
}

// checks is_set vairable 
int v3_isset_sock(struct v3_sock_set * sock_set, V3_SOCK sock) {
  struct v3_sock_entry * iter;

  v3_foreach_sock(sock_set, iter) {
    if (iter->sock == sock) {
      return iter->is_set;
    }
  }
  return -1;
}


// clears all is_set variables.
void v3_zero_sockset(struct v3_sock_set * sock_set) {
  struct v3_sock_entry * iter;
  v3_foreach_sock(sock_set, iter) {
    iter->is_set = 0;
  }
}

#if 0
static int
v3_socket_api_test(void)
{
	unsigned int port;
	char buf[1024];
	int rc = 0;
	V3_SOCK sock; 
	V3_SOCK client;
	unsigned int remote_ip;
	
	PrintDebug("\nIn Palacios: TEST BEGIN: Sockets API\n");
	sock = sock_hooks->tcp_socket(0, 0, 0);
	if( sock == NULL ){
		PrintDebug( "ERROR: tcp_socket() failed!\n");
		return -1;
	}

	port = 80;

	if( sock_hooks->bind_socket(sock, port) < 0){
		PrintDebug("bind error\n");
		return -1;
	}

	if( sock_hooks->listen(sock, 1) < 0) {
		PrintDebug("listen error\n" );
		return -1;
	}

	PrintDebug( "Going into mainloop: server listening on port %d\n", port);

	client = sock_hooks->accept(sock, &remote_ip , &port);

	PrintDebug(" New connection from %d port: %d\n", remote_ip, port);
	     
	sock_hooks->send(client, "Welcome!\n", 9);

	while(1)
	{		
	     sock_hooks->send(client, buf, rc);
            rc = sock_hooks->recv(client, buf, sizeof(buf)-1);
	     if( rc <= 0 ){
				PrintDebug( "Closed connection\n");
				sock_hooks->close(client);
				break;
	     }

	     buf[rc] = '\0';

	     PrintDebug( "Read %d bytes: '%s'\n", rc, buf);
	 }

	PrintDebug("TEST END: Sockets API\n");
	return 0;
}

#endif
