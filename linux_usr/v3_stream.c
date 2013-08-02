/* 
 * V3 Stream Utility
 * (c) Jack lange, Lei Xia, Peter Dinda 2010, 2013
 */


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <sys/ioctl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <linux/unistd.h>
#include <termios.h>
#include <signal.h>

#include "v3_ctrl.h"

#define BUF_LEN  512
#define STREAM_NAME_LEN 128
/* ^\ is the escape key we're going to use */
#define ESC_KEY 0x1c

int interactive=0;
int stream_fd=0;
struct termios ourterm, oldterm;

int setup_term()
{
  if (interactive) { 
    if (tcgetattr(0,&oldterm)) { 
      fprintf(stderr,"Cannot get terminal attributes\n");
      return -1;
    }
    
    ourterm = oldterm;            // clone existing terminal behavior
    ourterm.c_lflag &= ~ICANON;   // but without buffering
    ourterm.c_lflag &= ~ECHO;     // or echoing
    ourterm.c_lflag &= ~ISIG;     // or interrupt keys
    ourterm.c_cc[VMIN] = 0;       // or weird delays to compose
    ourterm.c_cc[VTIME] = 0;      // function keys (e.g., set raw)
    //fprintf(stderr, "Calling tcsetattr.\n");

    if (tcsetattr(0,TCSANOW,&ourterm)) { 
      fprintf(stderr,"Cannot set terminal attributes\n");
      return -1;
    }  
    //fprintf(stderr,"term setup interactive\n");
  } else {
    //fprintf(stderr, "term setup noninteractive\n");
  }
  
  return 0;
}

int restore_term()
{
  if (interactive) { 
    return -!!tcsetattr(0,TCSANOW,&oldterm);
  } else {
    return 0;
  }
}

int process_escapes(char in)
{
  static int found_esc = 0;
  if (found_esc) {
    switch(in) {
      case 'q':
        close(stream_fd);
        restore_term();
        fprintf(stderr,"Bye\n");
		exit(0);
      case 'h':
        fprintf(stderr, 
                "Control codes: ^-\\ q: quit\n"
                "               ^-\\ h: help\n"
                "               ^-\\ ^-\\: send ^-\\;\n");
        return 1;
      case ESC_KEY:
        /* Pass the second escape! */
	    found_esc = 0;
        return 0;
	  default:
        fprintf(stderr, "Unknown ESC sequence.\n");
	    found_esc = 0;
        return 1;
        }
  } else if (in == ESC_KEY)  {
	found_esc = 1;
    return 1;
  }
}

int pump(int in, int out)
{
    int bytes_read;
    int bytes_written;
    int thiswrite;
    int check_esc;
    int ret;
    char buf[BUF_LEN];
    
    // data from the stream
    check_esc = interactive && (in == STDIN_FILENO);
    bytes_read = read(in, buf, check_esc ? 1 : BUF_LEN);
    
    if (bytes_read<0) { 
        return -1;
    }
 
    if (check_esc) {   
      ret = process_escapes(buf[0]);
      if (ret) return 0;
    }

    //fprintf(stderr,"read %d bytes\n", bytes_read);

    bytes_written=0;

    while (bytes_written<bytes_read) { 
    thiswrite = write(out,&(buf[bytes_written]),bytes_read-bytes_written);
    if (thiswrite<0) {
        if (errno==EWOULDBLOCK) {
                continue;
            } else {
      perror("Cannot write");
      return -1;
    }}
    if (thiswrite==0) {
      fprintf(stderr,"Hmm, surprise end-of-file on write\n");
      return -1;
    }
    bytes_written+=thiswrite;
  }

  //fprintf(stderr,"wrote %d bytes\n",bytes_written);
  
  return 0;
}



void signal_handler(int num)
{
  switch (num) { 
  case SIGINT:
    close(stream_fd);
    restore_term();
    fprintf(stderr,"Bye\n");
    exit(0);
    break;
  default:
    fprintf(stderr,"Unknown signal %d ignored\n",num);
  }   
}

int usage(char *argv0)
{
    fprintf(stderr, 
	    "usage: %s [-i] <vm_device> <stream_name>\n\n"
	    "Connects stdin/stdout to a bidirectional stream on the VM,\n"
	    "for example a serial port.\n\n"
	    "If the [-i] option is given, a terminal is assumed\n"
	    "and it is placed in non-echoing, interactive mode, \n"
	    "which is what you probably want for use as a console.\n",
        argv0);
}

int main(int argc, char* argv[]) 
{
  int ret; 
  int vm_fd;
  fd_set rset;
  char * vm_dev = NULL;
  char stream[STREAM_NAME_LEN];
  int argstart;
  
  if (argc < 2) {
    usage(argv[0]);
    exit(0);
  }
  
  if (!strcasecmp(argv[1],"-i")) { 
    interactive=1;
    argstart=2;
    if (argc < 3) {
        usage(argv[0]);
        exit(0);
    } 
  } else {
    // noninteractive mode
    interactive=0;
    argstart=1;
  }
  
  vm_dev = argv[argstart];
  
  if (strlen(argv[argstart+1]) >= STREAM_NAME_LEN) {
    fprintf(stderr, "ERROR: Stream name longer than maximum size (%d)\n", STREAM_NAME_LEN);
    exit(-1);
  }

  memcpy(stream, argv[argstart+1], strlen(argv[argstart+1]));
  
  vm_fd = open(vm_dev, O_RDONLY);
  
  if (vm_fd == -1) {
    fprintf(stderr,"Error opening VM device: %s\n", vm_dev);
    exit(-1);
  }
  
  stream_fd = ioctl(vm_fd, V3_VM_SERIAL_CONNECT, stream); 
  
  close(vm_fd);
  
  if (stream_fd<0) { 
      fprintf(stderr,"Error opening VM device: %s\n", vm_dev);
      exit(-1);
  }
  
  if (setup_term()) { 
    fprintf(stderr,"Cannot setup terminal for %s mode\n", interactive ? "interactive" : "noninteraInteractive");
    close(stream_fd);
    exit(-1);
  }
  
  signal(SIGINT,signal_handler);
  
  while (1) {
    
    FD_ZERO(&rset);
    FD_SET(stream_fd, &rset);
    FD_SET(STDIN_FILENO, &rset);
    
    // wait for data from stdin or from the stream
    ret = select(stream_fd + 1, &rset, NULL, NULL, NULL);
  
    //fprintf(stderr,"select ret=%d\n", ret);

    if (ret==0) {
      perror("select returned zero without a timer!");
      goto error;
    } else if (ret<0) {
      if (errno==EAGAIN) { 
	continue;
      } else {
	perror("select failed");
	goto error;
      }
    }
    
    // positive return - we have data on one or both
    
    // check data from stream
    if (FD_ISSET(stream_fd, &rset)) {
      //fprintf(stderr,"stream is readable\n");
      if (pump(stream_fd, STDOUT_FILENO)) { 
	//fprintf(stderr,"Cannot transfer all data from stream to stdout...\n");
	goto error;
      }
    }
    
    // check data from stdin
    
    if (FD_ISSET(STDIN_FILENO, &rset)) {
      //fprintf(stderr,"stdin is readable\n");
      if (pump(STDIN_FILENO,stream_fd)) { 
	//fprintf(stderr,"Cannot transfer all data from stdin to stream...\n");
	goto error;
      }
    }
  } 
  
 error:
  ret = -1;
  goto out;
  
 done:
  ret = 0;
  goto out;
  
 out:
  
  close(stream_fd);
  restore_term();
  
  return ret; 
  

}


