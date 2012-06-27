/* 
 * V3 Creation with Binding utility
 * (c) Jack lange, 2010
 * (c) Ferrol Aderholdt, 2012 
 */


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <sys/ioctl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <unistd.h> 
#include <string.h>
 
#include "v3_ctrl.h"

int read_file(int fd, int size, unsigned char * buf);

/* loop over the .dat/.img file that is in memory and look for the xml */
static int find_starting_point(unsigned char * buf) {
    int i = 0;
    for(;i<32;i++) {
	if(*(buf+i) == '<') 
	    /* Looking for <vm class="PC"> portion of xml file. */
	    if(*(buf+i+1) == 'v')
		return i;
    }
    
    return -1;
}

/* simple override option that just overwrites the characters in memory with
   a new value. Supports string lengths of equal size or lesser size, but not
   more than size. Kind of a rough hack to get it done, but should work for 
   it's goal of changing mac addresses on the fly. */
int override_option(unsigned char * buf, char * option, char * value) {
    int starting_point = find_starting_point(buf);
    unsigned char * ptr;
    unsigned char * end;
    int i = 0;
    
    if(starting_point == -1) {
	fprintf(stderr, "Could not find a starting point to make ");
	fprintf(stderr, "modifications to the xml\n");
	return -1;
    }
    
    ptr = strstr(buf+starting_point, option);
    /* essentially, return if the option isn't present */
    if(ptr == NULL)
	return -1;
    
    /* find end of xml tag */
    ptr = strchr(buf+(ptr-buf), '>');
    end = strchr(buf+(ptr-buf), '<');
    
    if(strlen(value) > (end-ptr-1)) {
	fprintf(stderr, "Override option %s length is longer than currentlyin VM image file. This is not currently supported.\n", option);
	return -1;
    }
    
    for(i=0;i<strlen(value);i++) {
	ptr[i+1] = value[i];
    }
    
    /* the new option is smaller than the old one */
    if(strlen(value) < (end-ptr-1)) {
	int j = 0;
	ptr[i+1] = '<';
	i++;
	ptr[i+1] = '/';
	i++;
	for(j;j<strlen(option);j++,i++) {
	    ptr[i+1] = option[j];
	}
	ptr[i+1] = '>';
	i++;
	for(j=0;j< (end-ptr-1)-strlen(value);j++,i++) {
	    ptr[i+1] = ' ';
	}
    }
    
    return 0;
}




int main(int argc, char* argv[]) {
    char * filename = argv[1];
    char * name = argv[2];
    int guest_fd = 0;
    int v3_fd = 0;
    struct v3_guest_img guest_img;
    struct stat guest_stats;
    int dev_idx = 0;

    memset(&guest_img, 0, sizeof(struct v3_guest_img));

    if (argc <= 2) {
	printf("usage: v3_create_bind <guest_img> <vm name> [override options]\n");
	printf(" override options are of the form  KEY=VALUE\n");
	return -1;
    }

    printf("Creating guest: %s\n", filename);

    guest_fd = open(filename, O_RDONLY); 

    if (guest_fd == -1) {
	printf("Error Opening guest image: %s\n", filename);
	return -1;
    }

    if (fstat(guest_fd, &guest_stats) == -1) {
	printf("ERROR: Could not stat guest image file -- %s\n", filename);
	return -1;
    }

    
    guest_img.size = guest_stats.st_size;
    
    // load guest image into user memory
    guest_img.guest_data = malloc(guest_img.size);
    if (!guest_img.guest_data) {
            printf("ERROR: could not allocate memory for guest image\n");
            return -1;
    }


    read_file(guest_fd, guest_img.size, guest_img.guest_data);
    
    close(guest_fd);

    printf("Loaded guest image. Binding now.\n");

    if(argc > 3) {
	/* these are options. parse them and override the xml */
	int i = 0;
	for(i=3;i<argc;i++) {
	    char * argv_copy = malloc(strlen(argv[i]));
	    char * option;
	    char * value;
	    
	    strcpy(argv_copy, argv[i]);
	    option = strtok(argv_copy, "=");
	    value = strtok(NULL, "=");
	    
	    printf("Binding \"%s\" to \"%s\"\n",
		   option, value);
	    override_option(guest_img.guest_data, option, value);
	    free(argv_copy);
	}
    }
    
    printf("Bound guest image.  Launching to V3Vee\n");
    
    strncpy(guest_img.name, name, 127);


    v3_fd = open(v3_dev, O_RDONLY);

    if (v3_fd == -1) {
	printf("Error opening V3Vee control device\n");
	return -1;
    }

    dev_idx = ioctl(v3_fd, V3_CREATE_GUEST, &guest_img); 


    if (dev_idx < 0) {
	printf("Error (%d) creating VM\n", dev_idx);
	return -1;
    }

    printf("VM (%s) created at /dev/v3-vm%d\n", name, dev_idx);

    /* Close the file descriptor.  */ 
    close(v3_fd); 
 


    return 0; 
} 



int read_file(int fd, int size, unsigned char * buf) {
    int left_to_read = size;
    int have_read = 0;

    while (left_to_read != 0) {
	int bytes_read = read(fd, buf + have_read, left_to_read);

	if (bytes_read <= 0) {
	    break;
	}

	have_read += bytes_read;
	left_to_read -= bytes_read;
    }

    if (left_to_read != 0) {
	printf("Error could not finish reading file\n");
	return -1;
    }
    
    return 0;
}
