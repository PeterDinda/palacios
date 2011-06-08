#include "palacios.h"


int add_mod_cmd(struct v3_guest * guest, unsigned int cmd, 
		int (*handler)(struct v3_guest * guest, 
			       unsigned int cmd, unsigned long arg));


struct linux_ext {
    char * name;
    int (*init)( void );
    int (*deinit)( void );
    int (*guest_init)(struct v3_guest * guest, void ** priv_data);
    int (*guest_deinit)(struct v3_guest * guest, void * priv_data);
} __attribute__((packed));



int init_lnx_extensions( void );
int deinit_lnx_extensions( void );



#define register_extension(ext)					\
    static struct linux_ext * _lnx_ext				\
    __attribute__((used))					\
	__attribute__((unused, __section("_lnx_exts"),	\
		       aligned(sizeof(void *))))		\
	= ext;
