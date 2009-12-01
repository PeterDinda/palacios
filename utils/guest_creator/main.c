#include "ezxml.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_FILES 256

struct file_info {
    int size;
    char id[256];
    char * data;
    unsigned long long * offset_ptr;
};

int num_files = 0;
struct file_info files[MAX_FILES];


int parse_config_input(ezxml_t cfg_input);
int write_output(char * filename, ezxml_t cfg_output);

void usage() {
    printf("Usage: builder <infile> [-o outfile]\n");
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


    printf("Reading Input file: %s\n", infile);

    ezxml_t cfg_input = ezxml_parse_file(infile);

    if (cfg_input == NULL) {
	printf("Could not open configuration input file: %s\n", infile);
	return 1;
    }




    // parse input
    if (parse_config_input(cfg_input) == -1) {
	printf("Error parsing configuration input\n");
	return 1;
    }

    printf("xml : %s\n", ezxml_toxml(cfg_input));



    // write output

    if (outfile == NULL) {
	char * endptr = rindex(infile, '.');

	outfile = malloc(strlen(infile) + strlen(".dat") + 1);

	if (endptr) {
	    *endptr = '\0';
	}

	sprintf(outfile, "%s.dat", infile);
    }


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

    printf("setting number of files to %llu\n", file_cnt);
    printf("sizeof(file_cnt)=%d \n", sizeof(file_cnt));

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
	if (fwrite(files[i].data, 1, files[i].size, data_file) != files[i].size) {
	    printf("Error writing data file contents");
	    exit(-1);
	}	
    }
    


    fclose(data_file);

    return 0;
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
	FILE * data_file = NULL;
	char * id = get_val(tmp_file_tag, "id");
	char index_buf[256];

	snprintf(index_buf, 256, "%d", num_files);


	strncpy(files[num_files].id, ezxml_attr(tmp_file_tag, "id"), 256);

	printf("file id=%s, name=%s\n", 
	       id,
	       filename);
	

	if (stat(filename, &file_stats) != 0) {
	    perror(filename);
	    exit(-1);
	}

	files[num_files].size = (unsigned int)file_stats.st_size;
	printf("file size = %d\n", files[num_files].size);

	files[num_files].data = malloc(files[num_files].size);

	data_file = fopen(filename, "r");

	if (fread(files[num_files].data, files[num_files].size, 1, data_file) == 0) {
	    printf("Error reading data file %s\n", filename);
	    exit(-1);
	}

	fclose(data_file);


	ezxml_set_attr_d(tmp_file_tag, "index", index_buf);

	num_files++;
	tmp_file_tag = ezxml_next(tmp_file_tag);
    }


    return 0;
}
