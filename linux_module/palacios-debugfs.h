/* 
 * DebugFS interface
 * (c) Jack Lange, 2011
 */

#include "palacios.h"

int palacios_init_debugfs( void );
int palacios_deinit_debugfs( void );



int dfs_register_vm(struct v3_guest * guest);

