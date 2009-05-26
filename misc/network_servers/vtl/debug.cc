#include "debug.h"
#include <time.h>
#include <stdio.h>
#include <stdarg.h>

static FILE * logfile = NULL;
static int debug_on = 0;

int vtl_debug_init(string logfilename, int debug_enable) {
    debug_on = debug_enable; 
    logfile = fopen(logfilename.c_str(), "w+");
    return 0;
}



void vtl_debug(const char * fmt, ...) {
    if (debug_on) {
	va_list args;
	time_t dbgt;
	struct tm * time_data;
	char time_str[200];

	time(&dbgt); 
	time_data = localtime(&dbgt);


	strftime(time_str, sizeof(time_str), "%a %b %d %r %Y : ", time_data);

	fprintf(logfile, "%s", time_str); 

	va_start(args, fmt);
	vfprintf(logfile, fmt, args);
	va_end(args);

	fflush(logfile);
    }
}
