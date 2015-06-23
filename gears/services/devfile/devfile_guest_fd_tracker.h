/* 
   Device File Virtualization Guest Preload Library Helpers

   (c) Akhil Guliani and William Gross, 2015
     
   Adapted from MPI module (c) 2012 Peter Dinda

*/

#define MAX_DEV_NAME_LENGTH 80
#define MAX_DEVICES 100

//PYTHONSCRIPTBREAK1
#define DEV_COUNT 9
//PYTHONSCRIPTBREAK2

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>



#define MIN(a,b) \
    ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })


char *strcpy(char *dest, const char *src)
{
    unsigned i;
    for (i=0; src[i] != '\0'; ++i)
        dest[i] = src[i];
    dest[i] = '\0';
    return dest;
}

size_t strlen(const char * str)
{
    const char *s;
    for (s = str; *s; ++s) {}
    return(s - str);
}


//taken from: https://stuff.mit.edu/afs/sipb/project/tcl80/src/tcl8.0/compat/strstr.c
char* strstr(const char* string,char* substring)
    //    register char *string;  /* String to search. */
    //    char *substring;        /* Substring to try to find in string. */
{
    register char *a, *b;

    /* First scan quickly through the two strings looking for a
     *      * single-character match.  When it's found, then compare the
     *           * rest of the substring.
     *                */

    b = substring;
    if (*b == 0) {
        return (char*)string;
    }
    for ( ; *string != 0; string += 1) {
        if (*string != *b) {
            continue;
        }
        a = (char*)string;
        while (1) {
            if (*b == 0) {
                return (char*)string;
            }
            if (*a++ != *b++) {
                break;
            }
        }
        b = substring;
    }
    return (char *) 0;
}


//Used to keep track of the active file descriptor for the supported devices
//Initially the fd is -1 to indicate that the device has not yet been opened
typedef struct dev_file_fd_tracker {
    char devName[MAX_DEV_NAME_LENGTH];
    int devFD;
} dev_tracker;


dev_tracker dtrack[] = {
//PYTHONSCRIPTBREAK3
{"/dev/urandom",-1},
{"/dev/input/mouse0",-1},
{"/dev/input/event0",-1},
{"/dev/input/event1",-1},
{"/dev/ttyS0",-1},
{"/dev/ttyS1",-1},
{"/dev/ttyS2",-1},
{"/dev/ttyS3",-1},
{"/dev/newDevice",-1}
//PYTHONSCRIPTBREAK4

};

//returns -1, means no match is found
//other than -1, is the index in the dtracker array of the match
int check_name(const char* path, dev_tracker tracker[]){
    int i;
    for(i=0; i<DEV_COUNT; i++){
        if (strstr(path,tracker[i].devName)!=NULL){
            return i;
        }
    }
    return -1;
}
//returns -1, means no match found
//other than -1, is the index in the dtracker array of the match
int check_fd(int fd, dev_tracker tracker[]){
    int i;
    for(i=0; i<DEV_COUNT; i++){
        if(tracker[i].devFD == fd){
            return i;
        }
    }
    return -1;
}



