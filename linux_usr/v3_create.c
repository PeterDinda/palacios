/* 
 * V3 Control utility
 * (c) Jack lange, 2010
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <sys/ioctl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <unistd.h> 
#include <string.h>
#include <getopt.h> 


#include "v3_ctrl.h"
#include "ezxml.h"

struct cfg_value {
    char * tag;
    char * value;
};

struct xml_option {
    char * tag;
    ezxml_t location;
    struct xml_option * next;
};


struct file_info {
    int size;
    char filename[2048];
    char id[256];
};

#define MAX_FILES 256
unsigned long long num_files = 0;
struct file_info files[MAX_FILES];


int read_file(int fd, int size, unsigned char * buf);



int create_vm(char * vm_name, void * img_data, unsigned int img_size) {
    struct v3_guest_img guest_img;
    int v3_fd = 0;
    int dev_idx = 0;

    memset(&guest_img, 0, sizeof(struct v3_guest_img));

    guest_img.size = img_size;
    guest_img.guest_data = img_data;
    strncpy(guest_img.name, vm_name, 127);


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

    printf("VM (%s) created at /dev/v3-vm%d\n", vm_name, dev_idx);

    /* Close the file descriptor.  */ 
    close(v3_fd); 

    return 0;
}


int load_image(char * vm_name, char * filename) {
    int guest_fd = 0;
    struct stat guest_stats;
    int img_size = 0;
    void * img_data = NULL;

    guest_fd = open(filename, O_RDONLY); 

    if (guest_fd == -1) {
	printf("Error Opening guest image: %s\n", filename);
	return -1;
    }

    if (fstat(guest_fd, &guest_stats) == -1) {
	printf("ERROR: Could not stat guest image file -- %s\n", filename);
	return -1;
    }
    
    img_size = guest_stats.st_size;

    // load guest image into user memory
    img_data = malloc(img_size);

    read_file(guest_fd, img_size, img_data);
    
    close(guest_fd);

    printf("Guest image Loaded (size=%u)\n", img_size);
    return create_vm(vm_name, img_data, img_size);

}


ezxml_t open_xml_file(char * filename) {

    ezxml_t xml_input = ezxml_parse_file(filename);
    
    if (xml_input == NULL) {
	printf("Error: Could not open XML input file (%s)\n", filename);
	return NULL;
    } else if (strcmp("", ezxml_error(xml_input)) != 0) {
	printf("%s\n", ezxml_error(xml_input));
	return NULL;
    }

    return xml_input;
}


int find_xml_options(ezxml_t xml,  struct xml_option ** opts) {
    int num_opts = 0;
    ezxml_t child = xml->child;
    struct xml_option * next_opt = NULL;

    char * opt = (char *)ezxml_attr(xml, "opt_tag");

    if (opt) {
	next_opt = malloc(sizeof(struct xml_option));

	memset(next_opt, 0, sizeof(struct xml_option));

	next_opt->tag = opt;
	next_opt->location = xml;
	next_opt->next = NULL;

//	printf("Option found: %s\n", opt);

	*opts = next_opt;
	num_opts++;
    }


    while (child) {

	fflush(stdout);

	if (next_opt != 0x0) {
	    num_opts += find_xml_options(child, &(next_opt->next));
	} else {
	    num_opts += find_xml_options(child, opts);

	    if (*opts) {
		next_opt = *opts;
	    }
	}

	if (next_opt) {
	    while (next_opt->next) {
		next_opt = next_opt->next;
	    }
	}

	child = child->ordered;
    }
    
    return num_opts;

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


int parse_aux_files(ezxml_t cfg_input) {
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

	snprintf(index_buf, 256, "%llu", num_files);
	ezxml_set_attr_d(tmp_file_tag, "index", index_buf);

	num_files++;
	tmp_file_tag = ezxml_next(tmp_file_tag);
    }


    return 0;
}

int build_image(char * vm_name, char * filename, struct cfg_value * cfg_vals, int num_options) {
    int i = 0;
    ezxml_t xml = NULL;
    struct xml_option * xml_opts = NULL;
    int num_xml_opts = 0;
    void * guest_img_data = NULL;
    int guest_img_size = 0;


    xml = open_xml_file(filename);
    
    // parse options
    num_xml_opts = find_xml_options(xml, &xml_opts);
    
    //  printf("%d options\n", num_xml_opts);

    // apply options
    for (i = 0; i < num_options; i++) {
	struct cfg_value * cfg_val = &cfg_vals[i];
	struct xml_option * xml_opt = xml_opts;

	while (xml_opt) {
	    if (strcasecmp(cfg_val->tag, xml_opt->tag) == 0) {
		break;
	    }
	    
	    xml_opt = xml_opt->next;
	}


	if (!xml_opt) {
	    printf("Could not find Config option (%s) in XML file\n", cfg_val->tag);
	    return -1;
	}

	ezxml_set_txt(xml_opt->location, cfg_val->value);
    }
    


    // parse files
    parse_aux_files(xml);
    
    // create image data blob
    {
	char * new_xml_str = ezxml_toxml(xml);
	int file_data_size = 0;
	int i = 0;
	int offset = 0;
	unsigned long long file_offset = 0;

	/* Image size is: 
	   8 byte header + 
	   4 byte xml length + 
	   xml strlen + 
	   8 bytes of zeros + 
	   8 bytes (number of files) + 
	   num_files * 16 byte file header + 
	   8 bytes of zeroes + 
	   file data
	*/
	for (i = 0; i < num_files; i++) {
	    file_data_size += files[i].size;
	}

	guest_img_size = 8 + 4 + strlen(new_xml_str) + 8 + 8 + 
	    (num_files * 16) + 8 + file_data_size;
	    

	guest_img_data = malloc(guest_img_size);
	memset(guest_img_data, 0, guest_img_size);

	memcpy(guest_img_data, "v3vee\0\0\0", 8);
	offset += 8;

	*(unsigned int *)(guest_img_data + offset) = strlen(new_xml_str);
	offset += 4;

	memcpy(guest_img_data + offset, new_xml_str, strlen(new_xml_str));
	offset += strlen(new_xml_str);

	memset(guest_img_data + offset, 0, 8);
	offset += 8;
	
	*(unsigned long long *)(guest_img_data + offset) = num_files;
	offset += 8;

	
	// The file offset starts at the end of the file list
	file_offset = offset + (16 * num_files) + 8;

	for (i = 0; i < num_files; i++) {
	    *(unsigned int *)(guest_img_data + offset) = i;
	    offset += 4;
	    *(unsigned int *)(guest_img_data + offset) = files[i].size;
	    offset += 4;
	    *(unsigned long long *)(guest_img_data + offset) = file_offset;
	    offset += 8;

	    file_offset += files[i].size;

	}

	memset(guest_img_data + offset, 0, 8);
	offset += 8;


	for (i = 0; i < num_files; i++) {
	    int fd = open(files[i].filename, O_RDONLY);

	    if (fd == -1) {
		printf("Error: Could not open aux file (%s)\n", files[i].filename);
		return -1;
	    }

	    read_file(fd, files[i].size, (unsigned char *)(guest_img_data + offset));

	    close(fd);

	    offset += files[i].size;

	}
	
	free(new_xml_str);

    }

    printf("Guest Image Created (size=%u)\n", guest_img_size);
    return create_vm(vm_name, guest_img_data, guest_img_size);
}






int main(int argc, char** argv) {
    char * filename = NULL;
    char * name = NULL;
    int build_flag  = 0;
    int c = 0;

    opterr = 0;

    while (( c = getopt(argc, argv, "b")) != -1) {
	switch (c) {
	case 'b':
	    build_flag = 1;
	    break;
	}
    }

    if (argc - optind + 1 < 3) {
	printf("usage: v3_create [-b] <guest_img> <vm name> [cfg options]\n");
	return -1;
    }

    filename = argv[optind];
    name = argv[optind + 1];


    if (build_flag == 1) {
	int cfg_idx = optind + 2;
	int i = 0;
	struct cfg_value * cfg_vals = NULL;
	
	printf("Building VM Image (cfg=%s) (name=%s) (%d config options)\n", filename, name, argc - cfg_idx);

	cfg_vals = malloc(sizeof(struct cfg_value) * argc - cfg_idx);
	
	

	while (i < argc - cfg_idx) {
	    char * tag = NULL;
	    char * value = NULL;

	    value = argv[cfg_idx + i];

	    tag = strsep(&value, "=");
	    // parse

	    if (value == NULL) {
		printf("Invalid Option format: %s\n", argv[cfg_idx + i]);
		return -1;
	    }

	    cfg_vals[i].tag = tag;
	    cfg_vals[i].value = value;

	    printf("Config option: %s: %s\n", tag, value);


	    i++;
	}

	return build_image(name, filename, cfg_vals, argc - cfg_idx);


    } else {
	printf("Loading VM Image (img=%s) (name=%s)\n", filename, name);
	return load_image(name, filename);
    }

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
