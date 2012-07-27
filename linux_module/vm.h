/* 
 * Palacios VM interface
 * (c) Jack Lange, 2010
 */

#ifndef __PALACIOS_VM_H__
#define __PALACIOS_VM_H__

#include "palacios.h"

int create_palacios_vm(struct v3_guest * guest);
int free_palacios_vm(struct v3_guest * guest);


int add_guest_ctrl(struct v3_guest * guest,  unsigned int cmd, 
		  int (*handler)(struct v3_guest * guest, 
				 unsigned int cmd, unsigned long arg,
				 void * priv_data),
		  void * priv_data);

int remove_guest_ctrl(struct v3_guest * guest, unsigned int cmd);



#endif
