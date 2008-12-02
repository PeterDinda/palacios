#ifndef __TEST_H__
#define __TEST_H__
#include <stdlib.h>
#include <stdio.h>
#include "ktypes.h"

#define PrintDebug printf
#define PrintError printf

#define V3_ASSERT(x)                                                    \
  do {                                                                  \
    if (!(x)) {                                                         \
      PrintDebug("Failed assertion in %s: %s at %s, line %d, RA=%lx\n", \
                 __func__, #x, __FILE__, __LINE__,                      \
                 (ulong_t) __builtin_return_address(0));                \
      while(1);                                                         \
    }                                                                   \
  } while(0)                                                            \
    


#endif

