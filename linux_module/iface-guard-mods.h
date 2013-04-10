#ifndef __IFACE_GUARD_MODS_H__
#define __IFACE_GUARD_MODS_H__

#define V3_VM_REGISTER_MOD 14124

#define MAX_MOD_NAME_LEN 256
#define MAX_HASH_LEN     128

struct v3_entry_point {
    char * name;
    unsigned long offset;
    char is_ret;
    
   /* TODO: Fix this HACKERY! These last two are placeholders */
    void * hack[3];
};


struct v3_guard_mod {
    char * name;                     /* GM name */
    char * content_hash;    /* hash of the module .text segment */
    unsigned int hcall_offset;    /* offset from .text to hypercall */
    unsigned int text_size;             /* size of module .text segment */
    unsigned int num_entries;           /* number of entry points */
    unsigned int num_privs;
    char ** priv_array;           /* each bit represent a requested privilege */
    unsigned long long id;              /* GM ID */
    struct v3_entry_point * entry_points;        /* entry point array (offsets) */
};

#endif
