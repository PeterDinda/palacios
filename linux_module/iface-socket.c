/* 
 * Palacios Socket Interface Implementation
 * (c) Lei Xia  2010
 */
 

#include <interfaces/vmm_socket.h>

#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <linux/inet.h>
#include <linux/kthread.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/string.h>
#include <linux/preempt.h>
#include <linux/sched.h>
#include <linux/list.h>

#include "palacios.h"
#include "linux-exts.h"


struct palacios_socket {
    struct socket * sock;
    
    struct v3_guest * guest;
    struct list_head sock_node;
};

static struct list_head global_sockets;


// currently just the list of created sockets
struct vm_socket_state {
    struct list_head socket_list;
};


//ignore the arguments given here currently
static void * 
palacios_tcp_socket(const int bufsize, const int nodelay, 
		    const int nonblocking, void * private_data) {
    struct v3_guest * guest = (struct v3_guest *)private_data;
    struct palacios_socket * sock = NULL;
    struct vm_socket_state * vm_state = NULL;
    int err;

    if (guest != NULL) {
        vm_state = get_vm_ext_data(guest, "SOCKET_INTERFACE");
        
        if (vm_state == NULL) {
            printk("ERROR: Could not locate vm socket state for extension SOCKET_INTERFACE\n");
            return NULL;
        }
    }


    sock = kmalloc(sizeof(struct palacios_socket), GFP_KERNEL);
    memset(sock, 0, sizeof(struct palacios_socket));

    err = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &(sock->sock));

    if (err < 0) {
	kfree(sock);
	return NULL;
    }
       
    sock->guest = guest;
    
    if (guest == NULL) {
	list_add(&(sock->sock_node), &global_sockets);
    } else {
	list_add(&(sock->sock_node), &(vm_state->socket_list));
    }
    
    return sock;
}

//ignore the arguments given here currently
static void *
palacios_udp_socket(
	const int bufsize,
	const int nonblocking,
 	void * private_data
)
{
    struct v3_guest * guest = (struct v3_guest *)private_data;
    struct palacios_socket * sock = NULL;
    struct vm_socket_state * vm_state = NULL;
    int err;

    if (guest != NULL) {
        vm_state = get_vm_ext_data(guest, "SOCKET_INTERFACE");
        
        if (vm_state == NULL) {
            printk("ERROR: Could not locate vm socket state for extension SOCKET_INTERFACE\n");
            return NULL;
        }
    }


    sock = kmalloc(sizeof(struct palacios_socket), GFP_KERNEL);
    memset(sock, 0, sizeof(struct palacios_socket));

    err = sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &(sock->sock)) ;
	
    if (err < 0){
	kfree(sock);
	return NULL;
    }
    
    
    sock->guest = guest;
    
    if (guest == NULL) {
	list_add(&(sock->sock_node), &global_sockets);
    } else {
	list_add(&(sock->sock_node), &(vm_state->socket_list));
    }

    return sock;
}


static void 
palacios_close(void * sock_ptr)
{
    struct palacios_socket * sock = (struct palacios_socket *)sock_ptr;

    if (sock != NULL) {
	sock->sock->ops->release(sock->sock);
	
	list_del(&(sock->sock_node));
	kfree(sock);
    }
}

static int 
palacios_bind_socket(
	const void * sock_ptr,
	const int port
)
{
	struct sockaddr_in addr;
	struct palacios_socket * sock = (struct palacios_socket *)sock_ptr;

	if (sock == NULL) {
	    return -1;
	}

	addr.sin_family = AF_INET;
  	addr.sin_port = htons(port);
  	addr.sin_addr.s_addr = INADDR_ANY;

	return sock->sock->ops->bind(sock->sock, (struct sockaddr *)&addr, sizeof(addr));
}

static int 
palacios_listen(
	const void * sock_ptr,
	int backlog
)
{
	struct palacios_socket * sock = (struct palacios_socket *)sock_ptr;

	if (sock == NULL) {
	    return -1;
	}

	return sock->sock->ops->listen(sock->sock, backlog);
}

static void * palacios_accept(const void * sock_ptr, unsigned int * remote_ip, unsigned int * port) {

    struct palacios_socket * sock = (struct palacios_socket *)sock_ptr;
    struct palacios_socket * newsock = NULL;
    struct vm_socket_state * vm_state = NULL;
    int err;

    if (sock == NULL) {
	return NULL;
    }
    
    if (sock->guest != NULL) {
        vm_state = get_vm_ext_data(sock->guest, "SOCKET_INTERFACE");
        
        if (vm_state == NULL) {
            printk("ERROR: Could not locate vm socket state for extension SOCKET_INTERFACE\n");
            return NULL;
        }
    }


    newsock = kmalloc(sizeof(struct palacios_socket), GFP_KERNEL);

    err = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &(newsock->sock));

    if (err < 0) {
	kfree(newsock);
	return NULL;
    }

    newsock->sock->type = sock->sock->type;
    newsock->sock->ops = sock->sock->ops;

    err = newsock->sock->ops->accept(sock->sock, newsock->sock, 0);

    if (err < 0){
	kfree(newsock);
	return NULL;
    }

    //TODO: How do we get ip & port?

        
    newsock->guest = sock->guest;
    
    if (sock->guest == NULL) {
	list_add(&(newsock->sock_node), &global_sockets);
    } else {
	list_add(&(newsock->sock_node), &(vm_state->socket_list));
    }

    return newsock;
}

static int 
palacios_select(
	struct v3_sock_set * rset,
	struct v3_sock_set * wset,
	struct v3_sock_set * eset,
	struct v3_timeval tv)
{
  	//TODO:

	return 0;
}

static int 
palacios_connect_to_ip(
	const void * sock_ptr,
	const int hostip,
	const int port
)
{
  	struct sockaddr_in client;
	struct palacios_socket * sock = (struct palacios_socket *)sock_ptr;

	if (sock == NULL) {
	    return -1;
	}

  	client.sin_family = AF_INET;
  	client.sin_port = htons(port);
  	client.sin_addr.s_addr = htonl(hostip);

  	return sock->sock->ops->connect(sock->sock, (struct sockaddr *)&client, sizeof(client), 0);
}

static int 
palacios_send(
	const void * sock_ptr,
	const char * buf,
	const int len
)
{
	struct palacios_socket * sock = (struct palacios_socket *)sock_ptr;
	struct msghdr msg;
	mm_segment_t oldfs;
	struct iovec iov;
  	int err = 0;

	if (sock == NULL) {
	    return -1;
	}

	msg.msg_flags = MSG_NOSIGNAL;//0/*MSG_DONTWAIT*/;;
	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	iov.iov_base = (char *)buf;
	iov.iov_len = (size_t)len;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	err = sock_sendmsg(sock->sock, &msg, (size_t)len);

	set_fs(oldfs);

	return err;
}

static int 
palacios_recv(
	const void * sock_ptr,
	char * buf,
	const int len
)
{

	struct palacios_socket * sock = (struct palacios_socket *)sock_ptr;
	struct msghdr msg;
	mm_segment_t oldfs;
	struct iovec iov;
	int err;

	if (sock == NULL) {
	    return -1;
	}

	msg.msg_flags = 0;
	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	iov.iov_base = (void *)&buf[0];
	iov.iov_len = (size_t)len;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	err = sock_recvmsg(sock->sock, &msg, (size_t)len, 0/*MSG_DONTWAIT*/);

	set_fs(oldfs);

	return err;
}

static int 
palacios_sendto_ip(
	const void * sock_ptr,
	const int ip_addr,
	const int port,
	const char * buf,
	const int len
)
{
	struct palacios_socket * sock = (struct palacios_socket *)sock_ptr;
	struct msghdr msg;
	mm_segment_t oldfs;
	struct iovec iov;
	struct sockaddr_in dst;
 	int err = 0;

	if (sock == NULL) {
	    return -1;
	}

	dst.sin_family = AF_INET;
	dst.sin_port = htons(port);
	dst.sin_addr.s_addr = htonl(ip_addr);

	msg.msg_flags = MSG_NOSIGNAL;//0/*MSG_DONTWAIT*/;;
	msg.msg_name = &dst;
	msg.msg_namelen = sizeof(struct sockaddr_in);
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	iov.iov_base = (char *)buf;
	iov.iov_len = (size_t)len;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	err = sock_sendmsg(sock->sock, &msg, (size_t)len);

	set_fs(oldfs);

	return err;
}


// TODO:
static int 
palacios_recvfrom_ip(
	const void * sock_ptr,
	const int ip_addr,
	const int port,
	char * buf,
	const int len
)
{
	struct palacios_socket * sock = (struct palacios_socket *)sock_ptr;
  	struct sockaddr_in src;
  	int alen;
	struct msghdr msg;
	mm_segment_t oldfs;
	struct iovec iov;
	int err;

	if (sock == NULL) {
	    return -1;
	}

  	src.sin_family = AF_INET;
  	src.sin_port = htons(port);
  	src.sin_addr.s_addr = htonl(ip_addr);
  	alen = sizeof(src);


	msg.msg_flags = 0;
	msg.msg_name = &src;
	msg.msg_namelen = sizeof(struct sockaddr_in);
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	iov.iov_base = (void *)&buf[0];
	iov.iov_len = (size_t)len;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	err = sock_recvmsg(sock->sock, &msg, (size_t)len, 0/*MSG_DONTWAIT*/);

	set_fs(oldfs);

	return err;
}

static struct v3_socket_hooks palacios_sock_hooks = {
  	.tcp_socket = palacios_tcp_socket,
  	.udp_socket = palacios_udp_socket,
  	.close = palacios_close,
  	.bind = palacios_bind_socket,
  	.listen = palacios_listen,
  	.accept = palacios_accept,
  	.select = palacios_select,
  	.connect_to_ip = palacios_connect_to_ip,
  	.connect_to_host = NULL,
  	.send = palacios_send,
  	.recv = palacios_recv,
  	.sendto_host = NULL,
  	.sendto_ip = palacios_sendto_ip,
  	.recvfrom_host = NULL,
  	.recvfrom_ip = palacios_recvfrom_ip,
};

static int socket_init( void ) {
  	V3_Init_Sockets(&palacios_sock_hooks);
	INIT_LIST_HEAD(&global_sockets);
	return 0;
}

static int socket_deinit( void ) {
    if (!list_empty(&(global_sockets))) {
	printk("Error removing module with open sockets\n");
    }

    return 0;
}


static struct linux_ext socket_ext = {
    .name = "SOCKET_INTERFACE",
    .init = socket_init,
    .deinit = socket_deinit,
    .guest_init = NULL,
    .guest_deinit = NULL
};

register_extension(&socket_ext);
