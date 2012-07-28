#include "ezxml.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

#include <v3_ctrl.h>

#define MAX_FILES 256

/*
struct file_info {
    int size;
    char filename[2048];
    char id[256];
};
*/

/*

   The Palacios cookie encodes "v3vee" followed by a 
   3 byte version code.   There are currently two versions:

    \0\0\0 => original (no checksum)
    \0\0\1 => checksum
*/

#define COOKIE "v3vee\0\0\0"
#define COOKIE_LEN            8
#define COOKIE_VERSION_OFFSET 5
#define COOKIE_VERSION_LEN    3

int version = 1; // default version is now 1


int num_files = 0;
struct file_info files[MAX_FILES];

#define STDIO_FD 1


int parse_config_input(ezxml_t cfg_input);
int write_output(char * filename, ezxml_t cfg_output);
int copy_file(int file_index, FILE * data_file);

void usage() {
    printf("Usage: build_vm <infile> [-v version] [-o outfile]\n");
}

int main(int argc, char ** argv) {
    int c;
    char * outfile = NULL;
    char * infile = NULL;

    memset((char *)files, 0, sizeof(files));

    opterr = 0;

    while ((c = getopt(argc, argv, "ho:v:")) != -1) {
	switch (c) {
	    case 'o':
		outfile = optarg;
		break;
	    case 'v':
                version = atoi(optarg);
                break;
	    case 'h':
		usage();
		return 1;
	    default:
		abort();
	}
    }

    if (optind >= argc) {
	usage();
	return 1;
    }

    if (version != 0 && version != 1) { 
	printf("Only versions 0 and 1 are supported\n");
	return 1;
    }

    infile = argv[optind];


    if (outfile == NULL) {
	char * endptr = rindex(infile, '.');

	outfile = malloc(strlen(infile) + strlen(".dat") + 1);

	strncpy(outfile, infile, endptr - infile);

	sprintf(outfile, "%s.dat", outfile);
    }



    printf("Input: [%s] ==>>  Output: [%s]\n", infile, outfile);

    ezxml_t cfg_input = ezxml_parse_file(infile);
    if (strcmp("", ezxml_error(cfg_input)) != 0) {
	printf("%s\n", ezxml_error(cfg_input));
	return -1;
    }

    if (cfg_input == NULL) {
	printf("Could not open configuration input file: %s\n", infile);
	return 1;
    }




    // parse input
    if (parse_config_input(cfg_input) == -1) {
	printf("Error parsing configuration input\n");
	return 1;
    }

    //  printf("xml : %s\n", ezxml_toxml(cfg_input));



    // write output



    write_output(outfile, cfg_input);


    return 0;
}

char * get_val(ezxml_t cfg, char * tag) {
    char * attrib = (char *)ezxml_attr(cfg, tag);
    ezxml_t txt = ezxml_child(cfg, tag);

    if ((txt != NULL) && (attrib != NULL)) {
	printf("Invalid Cfg file: Duplicate value for %s (attr=%s, txt=%s)\n", 
	       tag, attrib, ezxml_txt(txt));
	exit(-1);
    }

    return (attrib == NULL) ? ezxml_txt(txt) : attrib;
}



int parse_config_input(ezxml_t cfg_input) {
    ezxml_t file_tags = NULL;
    ezxml_t tmp_file_tag = NULL;

    // files are transformed into blobs that are slapped to the end of the file
        
    file_tags = ezxml_child(cfg_input, "files");

    tmp_file_tag = ezxml_child(file_tags, "file");

    while (tmp_file_tag) {
	char * filename = get_val(tmp_file_tag, "filename");
	struct stat file_stats;
	char * id = get_val(tmp_file_tag, "id");
	char index_buf[256];


	if (stat(filename, &file_stats) != 0) {
	    perror(filename);
	    exit(-1);
	}

	files[num_files].size = (unsigned int)file_stats.st_size;
	strncpy(files[num_files].id, id, 256);
	strncpy(files[num_files].filename, filename, 2048);

	snprintf(index_buf, 256, "%d", num_files);
	ezxml_set_attr_d(tmp_file_tag, "index", index_buf);

	num_files++;
	tmp_file_tag = ezxml_next(tmp_file_tag);
    }


    return 0;
}


static int generate_file_hashes (char * filename,
                                 unsigned long long hdr_offset,
                                 int num_files) {
    unsigned char * file_data;
    unsigned char * out_data;
    struct mem_file_hdr * hdrs = NULL;
    int i, fd, out_fd;
    struct stat st;

    out_fd = open(filename, O_RDWR);
    if (!out_fd) {
        fprintf(stderr, "Couldn't open output file %s\n", filename);
        return -1;
    }
    fstat(out_fd, &st);

    /* mmap the out file, easy access to file headers */
    if ((out_data = mmap(0,
                         st.st_size,
                         PROT_READ|PROT_WRITE,
                         MAP_SHARED,
                         out_fd,
                         0)) == MAP_FAILED) {
        fprintf(stderr, "Error mapping output file (%d)\n", errno);
        return -1;
    }

    hdrs = (struct mem_file_hdr *)(out_data + hdr_offset);

    /* mmap each file, then update it's hash */
    for (i = 0; i < num_files; i++) {

        fd = open(files[i].filename, O_RDONLY);
        if (!fd) {
            fprintf(stderr, "Error opening file %s\n",
                    files[i].filename);
            return -1;
        }

        if ((file_data = mmap(0, 
                              hdrs[i].file_size, 
                              PROT_READ,
                              MAP_PRIVATE,
                              fd,
                              0)) == MAP_FAILED) {
            fprintf(stderr, "Could not mmap file for hashing\n");
            return -1;
         }

         /* generate the hash and save it */
        hdrs[i].file_hash = v3_hash_buffer(file_data, hdrs[i].file_size);
        printf("Generating hash for file %s (hash=0x%lx)\n", 
                files[i].filename, hdrs[i].file_hash);

        munmap(file_data, hdrs[i].file_size);
        close(fd);
    }

    munmap(out_data, st.st_size);
    return 0;
}

void gen_cookie(char *dest)
{
    memcpy(dest,COOKIE,COOKIE_LEN);
    dest[COOKIE_VERSION_OFFSET]   = (((unsigned)version) >> 16) & 0xff;
    dest[COOKIE_VERSION_OFFSET+1] = (((unsigned)version) >>  8) & 0xff;
    dest[COOKIE_VERSION_OFFSET+2] = (((unsigned)version) >>  0) & 0xff;
}


int write_output(char * filename, ezxml_t cfg_output) {
    FILE * data_file = fopen(filename, "w+");
    char * new_cfg_str = ezxml_toxml(cfg_output);
    unsigned int cfg_len = strlen(new_cfg_str);
    unsigned long long zero = 0;
    int i = 0;
    unsigned long long offset = 0;
    unsigned long long file_cnt = num_files;
    unsigned long long hdr_offset = 0;
    char cookie[COOKIE_LEN];

    gen_cookie(cookie);

    fwrite(cookie, COOKIE_LEN, 1, data_file);
    offset += COOKIE_LEN;

    //  printf("New config: \n%s\n", new_cfg_str);
    
    fwrite(&cfg_len, sizeof(unsigned int), 1, data_file);
    offset += sizeof(unsigned int);

    fwrite(new_cfg_str, cfg_len, 1, data_file);
    offset += cfg_len;

    fwrite(&zero, 1, 8, data_file);
    offset += 8;

    printf("Total number of files: %llu\n", file_cnt);

    fwrite(&file_cnt, 8, 1, data_file);
    offset += 8;


    if (version==0) { 
	// for version 0, we simply have the file num, offset, size list
	// each index entry is 16 bytes long plus end padding
	offset += (16 * num_files) + 8;
    } else if (version==1) { 
	// For a version 1, we have the file num, offset, size, and hash list
	// We need to remember where this begins in the file, though...
	hdr_offset = offset;

	// each index entry is (16+sizeof(unsigned long)) bytes long plus end padding
	offset += ((16 + sizeof(unsigned long)) * num_files) + 8;
    }
    
    for (i = 0; i < num_files; i++) {
	fwrite(&i, 4, 1, data_file);
	fwrite(&(files[i].size), 4, 1, data_file);
	fwrite(&offset, 8, 1, data_file);

	if (version==1) { 
	    /* we can't generate the hash yet, zero for now */
	    fwrite(&zero, sizeof(unsigned long), 1, data_file);
	}

	offset += files[i].size;
    }

    fwrite(&zero, 1, 8, data_file);

    for (i = 0; i < num_files; i++) {

	copy_file(i, data_file);
	
    }
    
    fclose(data_file);

    if (version==1) { 
	// We now will go back and place the hashes
	if (generate_file_hashes(filename,
				 hdr_offset,
				 num_files) < 0) {
	    fprintf(stderr, "Error generating file hashes\n");
	    return -1;
	}
    }


    return 0;
}


#define XFER_BLK_SIZE 4096

int copy_file(int file_index, FILE * data_file) {
    char xfer_buf[XFER_BLK_SIZE];
    int bytes_to_read = files[file_index].size;
    int bytes_read = 0;
    int xfer_len = XFER_BLK_SIZE;
    FILE * in_file = NULL;
    char * filename = files[file_index].filename;
    struct winsize wsz;
    char cons_line[256];
    int prog_len = 0;
    double ratio = 100;
    int num_dots = 256;

    printf("Copying [%d] -- %s \n",
	   file_index,	   
	   filename);


    if (ioctl(STDIO_FD, TIOCGWINSZ, &wsz) == -1) {
	printf("ioctl error on STDIO\n");
	return -1;
    }


    memset(cons_line, 0, 256);
    snprintf(cons_line, 256, "\r(%s) [", files[file_index].id);
    prog_len = wsz.ws_col - (strlen(cons_line) + 11);



    in_file = fopen(filename, "r");

    while (bytes_to_read > 0) {
	struct winsize tmp_wsz;
	int tmp_dots = 0;

	if (ioctl(STDIO_FD, TIOCGWINSZ, &tmp_wsz) == -1) {
	    printf("ioctl error on STDIO\n");
	    return -1;
	}

	ratio = (double)bytes_read / (double)(files[file_index].size); 
	tmp_dots = (int)(ratio * (double)prog_len);

	if ((tmp_dots != num_dots) || (tmp_wsz.ws_col != wsz.ws_col)) {
	    int i = 0;
	    int num_blanks = 0;
	    
	    wsz = tmp_wsz;
	    num_dots = tmp_dots;
	    
	    num_blanks = prog_len - num_dots;

	    memset(cons_line, 0, 256);
	    snprintf(cons_line, 256, "\r(%s) [", files[file_index].id);
	    
	    for (i = 0; i <= num_dots; i++) {
		strcat(cons_line, "=");
	    }
	    
	    for (i = 0; i < num_blanks - 1; i++) {
		strcat(cons_line, " ");
	    }
	    
	    strcat(cons_line, "] ");

	    //	printf("console width = %d\n", wsz.ws_col);
	    write(STDIO_FD, cons_line, wsz.ws_col);
	}
	
	
	
	if (xfer_len > bytes_to_read) {
	    xfer_len = bytes_to_read;
	}
	
	if (fread(xfer_buf, xfer_len, 1, in_file) == 0) {
	    printf("Error reading data file %s\n", filename);
	    exit(-1);
	}	
	
	if (fwrite(xfer_buf, xfer_len, 1, data_file) == 0) {
	    printf("Error writing data file contents");
	    exit(-1);
	}


	bytes_read += xfer_len;
	bytes_to_read -= xfer_len;
    }

    strcat(cons_line, "Done\n");
    write(STDIO_FD, cons_line, wsz.ws_col);


    fclose(in_file);

    return 0;
}
