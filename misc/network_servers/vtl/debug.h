#ifndef __DEBUG_H
#define __DEBUG_H

#include <string>

using namespace std;

int vtl_debug_init(string logfilename, int debug_enable);
void vtl_debug(const char * fmt, ...);


#endif
