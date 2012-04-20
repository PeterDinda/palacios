/* 
 * V3 Hypercall Add Utility
 * Allows hypercalls to be added to Palacios at run-time
 *
 * (c) Kyle C. Hale, 2011
 */

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "../linux_module/iface-host-hypercall.h"

static void usage (char * bin) {
	fprintf(stderr, "%s /dev/v3-vm<N> add|remove <nr> [function]\n", bin);
	fprintf(stderr, "<nr> = hypercall number\n"
		"[function] = kernel symbol to bind to\n"
		"             (defaults to a nop if not given)\n");
}

int main (int argc, char ** argv) {
  char * vm_dev = NULL;
  int vm_fd, err;
  struct hcall_data hd;
  enum {ADD,REMOVE} task;

  
  if (argc < 4 || argc>5) {
    usage(argv[0]);
    return -1;
  }

  vm_dev = argv[1];

  hd.hcall_nr = strtol(argv[3], NULL, 0);
  
 
  if (!strcasecmp(argv[2],"add")) { 
    task=ADD;
    if (argc==4) { 
      hd.fn[0]=0;  // blank
    } else {
      strcpy(hd.fn,argv[4]);
    }
  } else if (!strcasecmp(argv[2],"remove")) { 
    task=REMOVE;
  } else {
    usage(argv[0]);
    return -1;
  }

  printf("%s hypercall %d (0x%x) -> '%s' on %s\n",
	 task==ADD ? "Adding" : "Removing",
	 hd.hcall_nr, hd.hcall_nr,
	 task==REMOVE ? "(unimportant)" 
	 : strcmp(hd.fn,"") ? hd.fn : "(default nop)", vm_dev);

  vm_fd = open(vm_dev, O_RDONLY);
  if (vm_fd == -1) {
    perror("Cannot open VM device");
    return -1;
  }

  if (ioctl(vm_fd, 
	    task==ADD ? V3_VM_HYPERCALL_ADD : V3_VM_HYPERCALL_REMOVE,
	    &hd) < 0) { 
    perror("Cannot complete task due ioctl failure");
    close(vm_fd);
    return -1;
  }
  
  close(vm_fd);

  printf("Done.\n");

  return 0;
}


