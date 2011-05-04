/* 
 * DebugFS interface
 * (c) Jack Lange, 2011
 */

#include "palacios.h"

int palacios_init_inspector( void );
int palacios_deinit_inspector( void );



int inspect_vm(struct v3_guest * guest);

