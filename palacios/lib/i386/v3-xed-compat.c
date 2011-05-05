#include <xed/v3-xed-compat.h>


/* Standard I/O predefined streams
*/
static FILE   _streams = {0, 0, 0, 0, 0, NULL, NULL, 0, 0};
#ifdef V3_CONFIG_BUILT_IN_STDIN
FILE  *stdin = (&_streams);
#endif

#ifdef V3_CONFIG_BUILT_IN_STDOUT
FILE  *stdout = (&_streams);
#endif

#ifdef V3_CONFIG_BUILT_IN_STDERR
FILE  *stderr = (&_streams);
#endif

#ifdef V3_CONFIG_BUILT_IN_FPRINTF
int fprintf(FILE *file, char *fmt, ...) {
   // PrintDebug("In fprintf!!\n");
   return 0;

}
#endif

#ifdef V3_CONFIG_BUILT_IN_PRINTF
int printf(char *fmt, ...) {
   // PrintDebug("In fprintf!!\n");
   return 0;
}
#endif

#ifdef V3_CONFIG_BUILT_IN_FFLUSH
int fflush(FILE *stream) {
    //PrintDebug("In fflush!!\n");
    return 0;
}
#endif

#ifdef V3_CONFIG_BUILT_IN_ABORT
void abort(void)
{
   //PrintDebug("Abort!!\n");

   //__asm__ __volatile__("trap"); 
   //__builtin_unreached();

   while(1);   
}
#endif
