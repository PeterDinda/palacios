/* 
 * Palacios VM interface
 * (c) Jack Lange, 2010
 */

#ifndef __PALACIOS_VM_H__
#define __PALACIOS_VM_H__

#include "palacios.h"

int start_palacios_vm(void * arg);
int stop_palacios_vm(struct v3_guest * guest);


int add_guest_ctrl(struct v3_guest * guest,  unsigned int cmd, 
		  int (*handler)(struct v3_guest * guest, 
				 unsigned int cmd, unsigned long arg,
				 void * priv_data),
		  void * priv_data);



#endif
