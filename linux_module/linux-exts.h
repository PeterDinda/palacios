#include "palacios.h"





struct linux_ext {
    char * name;
    int (*init)( void );
    int (*deinit)( void );
    int (*guest_init)(struct v3_guest * guest, void ** priv_data);
    int (*guest_deinit)(struct v3_guest * guest, void * priv_data);
};



int init_lnx_extensions( void );
int deinit_lnx_extensions( void );

int init_vm_extensions(struct v3_guest * guest);
int deinit_vm_extensions(struct v3_guest * guest);

void * get_vm_ext_data(struct v3_guest * guest, char * ext_name);



struct global_ctrl {
    unsigned int cmd;

    int (*handler)(unsigned int cmd, unsigned long arg);

    struct rb_node tree_node;
};

int add_global_ctrl(unsigned int cmd, 
		    int (*handler)(unsigned int cmd, unsigned long arg));

struct global_ctrl * get_global_ctrl(unsigned int cmd);



#define register_extension(ext)					\
    static struct linux_ext * _lnx_ext				\
    __attribute__((used))					\
	__attribute__((unused, __section__("_lnx_exts"),		\
		       aligned(sizeof(void *))))		\
	= ext;
