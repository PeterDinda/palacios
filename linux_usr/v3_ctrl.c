/* 
 * V3 Control Library
 * (c) Jack lange, 2010
 *     Kyle Hale,  2012
 */


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <sys/ioctl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <sys/mman.h>
#include <unistd.h> 
#include <string.h>
 
#include "v3_ctrl.h"


/*
 * create a file-backed memory region 
 */
void * v3_mmap_file (const char * filename, int prot, int flags) {
    int fd;
    struct stat st;
    void * m;

    fd = open(filename, O_RDONLY);
    if (!fd) {
        fprintf(stderr, "Error opening file for mapping: %s\n", filename);
        return NULL;
    }

    fstat(fd, &st);

    if ((m = mmap(NULL, st.st_size, prot, flags, fd, 0)) == MAP_FAILED) {
        fprintf(stderr, "Error mapping file (%s)\n", filename);
        close(fd);
        return NULL;
    }

    close(fd);
    return m;
}


/*
 * read a file into a buffer
 */
int v3_read_file (int fd, int size, unsigned char * buf) {
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
	fprintf(stderr, "Error could not finish reading file\n");
	return -1;
    }
    
    return 0;
}


/*
 * perform an ioctl on v3vee device
 */
int v3_dev_ioctl (int request, void * arg) {
    int fd, ret;

    fd = open(v3_dev, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Error opening V3Vee control device\n");
        return -1;
    }

    ret = ioctl(fd, request, arg);

    if (ret < 0) 
        fprintf(stderr, "IOCTL error on V3Vee control device (%d)\n", ret);


    close(fd);
    return ret;
}


/*
 * perform an ioctl on arbitrary VM device
 */
int v3_vm_ioctl (const char * filename, 
                 int request, 
                 void * arg) { 
    int fd, ret;

    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Error opening V3Vee VM device: %s\n", filename);
        return -1;
    }

    ret = ioctl(fd, request, arg);

    if (ret < 0) 
        fprintf(stderr, "IOCTL error on device %s (%d)\n", filename, ret);

    close(fd);
    return ret;
}


/* 
 * launch a VM with VM device path
 */
int launch_vm (const char * filename) {
    int err;

    printf("Launching VM (%s)\n", filename);
    err = v3_vm_ioctl(filename, V3_VM_LAUNCH, NULL);

    if (err < 0) {
        fprintf(stderr, "Error launching VM (%s)\n", filename);
        return -1;
    }

    return 0;
}


/*
 * stop a VM with VM device path
 */
int stop_vm (const char * filename) {
    int err;

    printf("Stopping VM (%s)\n", filename);

    if (v3_vm_ioctl(filename, V3_VM_STOP, NULL) < 0) 
        return -1;

    return 0;
}


/*
 * generic ELF header buffer hash function. 
 * Mirrors internal Palacios implementation
 */
unsigned long v3_hash_buffer (unsigned char * msg, unsigned int len) {
    unsigned long hash = 0;
    unsigned long temp = 0;
    unsigned int i;

    for (i = 0; i < len; i++) {
        hash = (hash << 4) + *(msg + i) + i;
        if ((temp = (hash & 0xF0000000))) {
            hash ^= (temp >> 24);
        }
        hash &= ~temp;
    }
    return hash;
}

