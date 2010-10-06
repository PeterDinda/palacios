#include "ezxml.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>


#define MAX_FILES 256

struct file_info {
    int size;
    char filename[2048];
    char id[256];
};

int num_files = 0;
struct file_info files[MAX_FILES];

#define STDIO_FD 1


int parse_config_input(ezxml_t cfg_input);
int write_output(char * filename, ezxml_t cfg_output);
int copy_file(int file_index, FILE * data_file);

void usage() {
    printf("Usage: build_vm <infile> [-o outfile]\n");
}

int main(int argc, char ** argv) {
    int c;
    char * outfile = NULL;
    char * infile = NULL;


    memset((char *)files, 0, sizeof(files));

    opterr = 0;

    while ((c = getopt(argc, argv, "ho:")) != -1) {
	switch (c) {
	    case 'o':
		outfile = optarg;
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


int write_output(char * filename, ezxml_t cfg_output) {
    FILE * data_file = fopen(filename, "w+");
    char * new_cfg_str = ezxml_toxml(cfg_output);
    unsigned int cfg_len = strlen(new_cfg_str);
    unsigned long long zero = 0;
    int i = 0;
    unsigned long long offset = 0;
    unsigned long long file_cnt = num_files;

    fwrite("v3vee\0\0\0", 8, 1, data_file);
    offset += 8;

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

    // each index entry is 16 bytes long plus end padding
    offset += (16 * num_files) + 8;

    for (i = 0; i < num_files; i++) {
	fwrite(&i, 4, 1, data_file);
	fwrite(&(files[i].size), 4, 1, data_file);
	fwrite(&offset, 8, 1, data_file);

	offset += files[i].size;
    }

    fwrite(&zero, 1, 8, data_file);

    for (i = 0; i < num_files; i++) {

	copy_file(i, data_file);
	
    }
    


    fclose(data_file);

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
