/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_socket.h>

#ifndef CONFIG_DEBUG_IDE
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


#define NBD_READ_CMD 0x1
#define NBD_WRITE_CMD 0x2
#define NBD_CAPACITY_CMD 0x3

#define NBD_STATUS_OK 0x00
#define NBD_STATUS_ERR 0xff


struct disk_state {
    uint64_t capacity; // in bytes

    v3_sock_t socket;

    uint32_t ip_addr;
    uint16_t port;

    struct v3_vm_info * vm;

    char disk_name[32];

};


static int send_all(v3_sock_t socket, char * buf, int length) {
    int bytes_sent = 0;
    
    PrintDebug("Sending %d bytes\n", length - bytes_sent);
    while (bytes_sent < length) {
	int tmp_bytes = v3_socket_send(socket, buf + bytes_sent, length - bytes_sent);
	PrintDebug("Sent %d bytes\n", tmp_bytes);
	
	if (tmp_bytes == 0) {
	    PrintError("Connection Closed unexpectedly\n");
	    return -1;
	}
	
	bytes_sent += tmp_bytes;
    }
    
    return 0;
}


static int recv_all(v3_sock_t socket, char * buf, int length) {
    int bytes_read = 0;
    
    PrintDebug("Reading %d bytes\n", length - bytes_read);
    while (bytes_read < length) {
	int tmp_bytes = v3_socket_recv(socket, buf + bytes_read, length - bytes_read);
	PrintDebug("Received %d bytes\n", tmp_bytes);
	
	if (tmp_bytes == 0) {
	    PrintError("Connection Closed unexpectedly\n");
	    return -1;
	}
	
	bytes_read += tmp_bytes;
    }
    
    return 0;
}


// HDs always read 512 byte blocks... ?
static int read(uint8_t * buf, uint64_t lba, uint64_t num_bytes, void * private_data) {
    struct disk_state * disk = (struct disk_state *)private_data ;
    uint8_t status;
    uint32_t ret_len = 0;
    char nbd_cmd[4] = {0,0,0,0};
    uint64_t offset = lba;
    uint64_t length = num_bytes;

    nbd_cmd[0] = NBD_READ_CMD;
    
    if (send_all(disk->socket, nbd_cmd, 4) == -1) {
	PrintError("Error sending read command\n");
	return -1;
    }
    
    if (send_all(disk->socket, (char *)&offset, 8) == -1) {
	PrintError("Error sending read offset\n");
	return -1;
    }

    if (send_all(disk->socket, (char *)&length, 4) == -1) {
	PrintError("Error sending read length\n");
	return -1;
    }

    if (recv_all(disk->socket, (char *)&status, 1) == -1) {
	PrintError("Error receiving status\n");
	return -1;
    }

    if (status != NBD_STATUS_OK) {
	PrintError("NBD Error....\n");
	return -1;
    }

    PrintDebug("Reading Data Ret Length\n");

    if (recv_all(disk->socket, (char *)&ret_len, 4) == -1) {
	PrintError("Error receiving Return read length\n");
	return -1;
    }

    if (ret_len != length) {
	PrintError("Read length mismatch (req=%llu) (result=%u)\n", length, ret_len);
	return -1;
    }

    PrintDebug("Reading Data (%d bytes)\n", ret_len);

    if (recv_all(disk->socket, (char *)buf, ret_len) == -1) {
	PrintError("Read Data Error\n");
	return -1;
    }

    return 0;
}


static int write(uint8_t * buf, uint64_t lba, uint64_t num_bytes, void * private_data) {
    struct disk_state * disk = (struct disk_state *)private_data ;
    uint64_t offset = lba;
    int length = num_bytes;
    uint8_t status;
    char nbd_cmd[4] = {0,0,0,0};

    nbd_cmd[0] = NBD_WRITE_CMD;

    if (send_all(disk->socket, nbd_cmd, 4) == -1) {
	PrintError("Error sending write command\n");
	return -1;
    }

    if (send_all(disk->socket, (char *)&offset, 8) == -1) {
	PrintError("Error sending write offset\n");
	return -1;
    }

    if (send_all(disk->socket, (char *)&length, 4) == -1) {
	PrintError("Error sending write length\n");
	return -1;
    }

    PrintDebug("Writing Data (%d bytes)\n", length);

    if (send_all(disk->socket, (char *)buf, length) == -1) {
	PrintError("Write Data Error\n");
	return -1;
    }

    if (recv_all(disk->socket, (char *)&status, 1) == -1) {
	PrintError("Error receiving status\n");
	return -1;
    }

    if (status != NBD_STATUS_OK) {
	PrintError("NBD Error....\n");
	return -1;
    }

    return 0;
}


static uint64_t get_capacity(void * private_data) {
    struct disk_state * disk = (struct disk_state *)private_data;

    return disk->capacity;
}

static struct v3_dev_blk_ops blk_ops = {
    .read = read, 
    .write = write,
    .get_capacity = get_capacity,
};





static int disk_free(struct disk_state * disk) {

    v3_socket_close(disk->socket);

    V3_Free(disk);
    return 0;
}

static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))disk_free,
};


static int socket_init(struct disk_state * disk) {
    char header[64];
    
    PrintDebug("Intializing Net Disk\n");

    disk->socket = v3_create_tcp_socket(disk->vm);

    PrintDebug("DISK socket: %d\n", disk->socket);
    PrintDebug("Connecting to: %s:%d\n", v3_inet_ntoa(disk->ip_addr), disk->port);

    v3_connect_to_ip(disk->socket, v3_ntohl(disk->ip_addr), disk->port);

    PrintDebug("Connected to NBD server\n");

    //snprintf(header, 64, "V3_NBD_1 %s\n", cd->disk_name);
    strcpy(header, "V3_NBD_1 ");
    strncat(header, disk->disk_name, 32);
    strncat(header, "\n", 1);


    if (send_all(disk->socket, header, strlen(header)) == -1) {
	PrintError("Error connecting to Network Block Device: %s\n", disk->disk_name);
	return -1;
    }

    // store local copy of capacity
    {
	char nbd_cmd[4] = {0,0,0,0};

	nbd_cmd[0] = NBD_CAPACITY_CMD;
	
	if (send_all(disk->socket, nbd_cmd, 4) == -1) {
	    PrintError("Error sending capacity command\n");
	    return -1;
	}

	if (recv_all(disk->socket, (char *)&(disk->capacity), 8) == -1) {
	    PrintError("Error Receiving Capacity\n");
	    return -1;
	}	

	PrintDebug("Capacity: %p\n", (void *)(addr_t)disk->capacity);
    }



    return 0;
}


static int disk_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct disk_state * disk = (struct disk_state *)V3_Malloc(sizeof(struct disk_state));

    char * ip_str = v3_cfg_val(cfg, "IP");
    char * port_str = v3_cfg_val(cfg, "port");
    char * disk_tag = v3_cfg_val(cfg, "tag");
    char * dev_id = v3_cfg_val(cfg, "ID");

    v3_cfg_tree_t * frontend_cfg = v3_cfg_subtree(cfg, "frontend");

    PrintDebug("Registering Net disk at %s:%s disk=%s\n", ip_str, port_str, disk_tag);

    strncpy(disk->disk_name, disk_tag, sizeof(disk->disk_name));
    disk->ip_addr = v3_inet_addr(ip_str);
    disk->port = atoi(port_str);
    disk->vm = vm;

    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, disk);

    if (dev == NULL) {
	PrintError("Could not attach device %s\n", dev_id);
	V3_Free(disk);
	return -1;
    }

    if (socket_init(disk) == -1) {
	PrintError("could not initialize network connection\n");
	v3_remove_device(dev);
	return -1;
    }

    PrintDebug("Registering Disk\n");

    if (v3_dev_connect_blk(vm, v3_cfg_val(frontend_cfg, "tag"), 
			   &blk_ops, frontend_cfg, disk) == -1) {
	PrintError("Could not connect %s to frontend\n", dev_id);
	v3_remove_device(dev);
	return -1;
    }

    PrintDebug("intialization done\n");

    return 0;
}


device_register("NETDISK", disk_init)
