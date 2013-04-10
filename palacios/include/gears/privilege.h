#ifndef __PRIVILEGE_H__
#define __PRIVILEGE_H__


struct v3_privileges {

    struct list_head priv_list;
    struct hashtable * priv_table;

};

struct v3_priv {
    const char * name;
    

    int (*init)   (struct guest_info * core, void ** private_data);
    int (*lower)  (struct guest_info * core, void * private_data);
    int (*raise)  (struct guest_info * core, void * private_data);
    int (*deinit) (struct guest_info * core, void * private_data);

    void * private_data;

    struct list_head priv_node;
    
};

struct v3_priv * v3_lookup_priv (struct v3_vm_info * vm, const char * name);

int v3_core_raise_all_privs (struct guest_info * core);
int v3_core_lower_all_privs (struct guest_info * core);

int v3_bind_privilege (struct guest_info * core,
                       const char * priv_name,
                       int (*init)   (struct guest_info * core, void ** private_data),
                       int (*lower)  (struct guest_info * core, void * private_data),
                       int (*raise)  (struct guest_info * core, void * private_data),
                       int (*deinit) (struct guest_info * core, void * private_data),
                       void * priv_data);

                           
/*

#define register_privilege(priv)                         \
    static struct v3_priv * _v3_priv                     \
    __attribute__((used))                                \
    __attribute__((unused, __section__("_v3_privileges"),\
                    aligned(sizeof(addr_t))))            \
    = priv;

*/

#endif
