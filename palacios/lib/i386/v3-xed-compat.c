#include <xed/v3-xed-compat.h>


/* Standard I/O predefined streams
*/
static FILE   _streams = {0, 0, 0, 0, 0, NULL, NULL, 0, 0};
FILE  *stdin = (&_streams);
FILE  *stdout = (&_streams);
FILE  *stderr = (&_streams);

int fprintf(FILE *file, char *fmt, ...)
{
   // PrintDebug("In fprintf!!\n");

   return 0;

}

int printf(char *fmt, ...)
{
   // PrintDebug("In fprintf!!\n");

   return 0;

}

int fflush(FILE *stream)
{
    //PrintDebug("In fflush!!\n");

    return 0;
}

void abort(void)
{
   //PrintDebug("Abort!!\n");

   //__asm__ __volatile__("trap"); 
   //__builtin_unreached();


   while(1);
   
}
