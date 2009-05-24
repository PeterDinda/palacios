#ifndef __DEBUG_H
#define __DEBUG_H 1




#ifdef DEBUG

#define ASSERT(exp) assert(exp)

/*
 *
 *
 */

#ifdef linux

extern FILE * logfile;
extern time_t dbgt;
extern char dmsg[1024];

#define DEBUG_DECLARE() FILE * logfile; time_t dbgt; char dmsg[1024];
#define JRLDBG( ...) time(&dbgt); sprintf(dmsg,"%s: ",ctime(&dbgt)); *(dmsg + strlen(dmsg) -3) = ' '; fprintf(logfile, dmsg); sprintf(dmsg,__VA_ARGS__); fprintf(logfile,dmsg); fflush(logfile);
#define debug_init(logfilename) logfile = fopen(logfilename,"w+")

#elif defined(WIN32)

#define DEBUG_DECLARE() 
#define JRLDBG printf
#define debug_init(logfilename) 

#endif

/*
 *
 *
 */

#else //!DEBUG

#ifdef WIN32

#define ASSERT(exp)
#define DEBUG_DECLARE()
#define JRLDBG()
#define debug_init(logfilename)

#elif defined(linux)

#define ASSERT(exp)
#define DEBUG_DECLARE(...)
#define JRLDBG(...)
#define debug_init(logfilename)

#endif

#endif



#endif
