#ifndef __IFACE_ENV_INJECT_H__
#define __IFACE_ENV_INJECT_H__


#define MAX_NUM_STRINGS 10
#define MAX_STRING_LEN 128
#define MAX_ENV_INJECT 10

#define V3_VM_ENV_INJECT 13125

struct env_data {
    int num_strings;
    char ** strings;
    char bin_name[MAX_STRING_LEN];
};


#endif
