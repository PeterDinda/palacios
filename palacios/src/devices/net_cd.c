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
#include <devices/net_cd.h>
#include <devices/ide.h>
#include <palacios/vmm_socket.h>

/* #ifndef DEBUG_IDE */
/* #undef PrintDebug */
/* #define PrintDebug(fmt, args...) */
/* #endif */



#define NBD_READ_CMD 0x1
#define NBD_WRITE_CMD 0x2
#define NBD_CAPACITY_CMD 0x3



#define NBD_STATUS_OK 0x00
#define NBD_STATUS_ERR 0xff

struct cd_state {
    uint64_t capacity; // in bytes

    int socket;

    uint32_t ip_addr;
    uint16_t port;

    char disk_name[32];

    struct vm_device * ide;

    uint_t bus;
    uint_t drive;
};


static int send_all(int socket, char * buf, int length) {
    int bytes_sent = 0;
    
    PrintDebug("Sending %d bytes\n", length - bytes_sent);
    while (bytes_sent < length) {
	int tmp_bytes = V3_Send(socket, buf + bytes_sent, length - bytes_sent);
	PrintDebug("Sent %d bytes\n", tmp_bytes);
	
	if (tmp_bytes == 0) {
	    PrintError("Connection Closed unexpectedly\n");
	    return -1;
	}
	
	bytes_sent += tmp_bytes;
    }
    
    return 0;
}


static int recv_all(int socket, char * buf, int length) {
    int bytes_read = 0;
    
    PrintDebug("Reading %d bytes\n", length - bytes_read);
    while (bytes_read < length) {
	int tmp_bytes = V3_Recv(socket, buf + bytes_read, length - bytes_read);
	PrintDebug("Received %d bytes\n", tmp_bytes);
	
	if (tmp_bytes == 0) {
	    PrintError("Connection Closed unexpectedly\n");
	    return -1;
	}
	
	bytes_read += tmp_bytes;
    }
    
    return 0;
}

// CDs always read 2048 byte blocks... ?
static int cd_read(uint8_t * buf, int block_count, uint64_t lba,  void * private_data) {
    struct vm_device * cd_dev = (struct vm_device *)private_data;
    struct cd_state * cd = (struct cd_state *)(cd_dev->private_data);
    uint64_t offset = lba * ATAPI_BLOCK_SIZE;
    int length = block_count * ATAPI_BLOCK_SIZE;
    uint8_t status;
    uint32_t ret_len = 0;
    char nbd_cmd[4] = {0,0,0,0};

    nbd_cmd[0] = NBD_READ_CMD;
    
    if (send_all(cd->socket, nbd_cmd, 4) == -1) {
	PrintError("Error sending capacity command\n");
	return -1;
    }
    
    if (send_all(cd->socket, (char *)&offset, 8) == -1) {
	PrintError("Error sending read offset\n");
	return -1;
    }

    if (send_all(cd->socket, (char *)&length, 4) == -1) {
	PrintError("Error sending read length\n");
	return -1;
    }

    if (recv_all(cd->socket, (char *)&status, 1) == -1) {
	PrintError("Error receiving status\n");
	return -1;
    }

    if (status != NBD_STATUS_OK) {
	PrintError("NBD Error....\n");
	return -1;
    }

    PrintDebug("Reading Data Ret Length\n");

    if (recv_all(cd->socket, (char *)&ret_len, 4) == -1) {
	PrintError("Error receiving Return read length\n");
	return -1;
    }

    if (ret_len != length) {
	PrintError("Read length mismatch (req=%d) (result=%d)\n", length, ret_len);
	return -1;
    }

    PrintDebug("Reading Data (%d bytes)\n", ret_len);

    if (recv_all(cd->socket, (char *)buf, ret_len) == -1) {
	PrintError("Read Data Error\n");
	return -1;
    }
    
    return 0;
}


static uint32_t cd_get_capacity(void * private_data) {
    struct vm_device * cd_dev = (struct vm_device *)private_data;
    struct cd_state * cd = (struct cd_state *)(cd_dev->private_data);

    return cd->capacity / ATAPI_BLOCK_SIZE;
}

static struct v3_ide_cd_ops cd_ops = {
    .read = cd_read, 
    .get_capacity = cd_get_capacity,
};


static int cd_init(struct vm_device * dev) {
    struct cd_state * cd = (struct cd_state *)(dev->private_data);
    char header[64];
    
    PrintDebug("Intializing Net CD\n");

    cd->socket = V3_Create_TCP_Socket();

    PrintDebug("CD socket: %d\n", cd->socket);
    PrintDebug("Connecting to: %s:%d\n", v3_inet_ntoa(cd->ip_addr), cd->port);

    V3_Connect_To_IP(cd->socket, v3_ntohl(cd->ip_addr), cd->port);


    PrintDebug("Connected to NBD server\n");

    //snprintf(header, 64, "V3_NBD_1 %s\n", cd->disk_name);
    strcpy(header, "V3_NBD_1 ");
    strncat(header, cd->disk_name, 32);
    strncat(header, "\n", 1);


    if (send_all(cd->socket, header, strlen(header)) == -1) {
	PrintError("Error connecting to Network Block Device: %s\n", cd->disk_name);
	return -1;
    }

    // Cache Capacity
    {
	char nbd_cmd[4] = {0,0,0,0};

	nbd_cmd[0] = NBD_CAPACITY_CMD;
	
	if (send_all(cd->socket, nbd_cmd, 4) == -1) {
	    PrintError("Error sending capacity command\n");
	    return -1;
	}

	if (recv_all(cd->socket, (char *)&(cd->capacity), 8) == -1) {
	    PrintError("Error Receiving Capacity\n");
	    return -1;
	}	

	PrintDebug("Capacity: %p\n", (void *)(cd->capacity));
    }


    PrintDebug("Registering CD\n");

    if (v3_ide_register_cdrom(cd->ide, cd->bus, cd->drive, "NET-CD", &cd_ops, dev) == -1) {
	return -1;
    }

    PrintDebug("intialization done\n");
    
    return 0;
}


static int cd_deinit(struct vm_device * dev) {
    return 0;
}

static struct vm_device_ops dev_ops = {
    .init = cd_init, 
    .deinit = cd_deinit,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};

struct vm_device * v3_create_net_cd(struct vm_device * ide, 
				    uint_t bus, uint_t drive, 
				    const char * ip_str, uint16_t port, 
				    const char * disk_tag) {
    struct cd_state * cd = (struct cd_state *)V3_Malloc(sizeof(struct cd_state));

    PrintDebug("Registering Net CD at %s:%d disk=%s\n", ip_str, port, disk_tag);

    strncpy(cd->disk_name, disk_tag, sizeof(cd->disk_name));
    cd->ip_addr = v3_inet_addr(ip_str);
    cd->port = port;

    cd->ide = ide;
    cd->bus = bus;
    cd->drive = drive;
	
    struct vm_device * cd_dev = v3_create_device("NET-CD", &dev_ops, cd);

    return cd_dev;
}
