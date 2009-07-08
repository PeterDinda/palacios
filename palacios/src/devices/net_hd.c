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
#include <devices/net_hd.h>
#include <devices/ide.h>
#include <palacios/vmm_socket.h>

#ifndef DEBUG_IDE
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


#define NBD_READ_CMD 0x1
#define NBD_WRITE_CMD 0x2
#define NBD_CAPACITY_CMD 0x3

#define NBD_STATUS_OK 0x00
#define NBD_STATUS_ERR 0xff


struct hd_state {
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


// HDs always read 512 byte blocks... ?
static int hd_read(uint8_t * buf, int sector_count, uint64_t lba,  void * private_data) {
    struct vm_device * hd_dev = (struct vm_device *)private_data;
    struct hd_state * hd = (struct hd_state *)(hd_dev->private_data);
    int offset = lba * IDE_SECTOR_SIZE;
    int length = sector_count * IDE_SECTOR_SIZE;
    uint8_t status;
    uint32_t ret_len = 0;
    char nbd_cmd[4] = {0,0,0,0};

    nbd_cmd[0] = NBD_READ_CMD;
    
    if (send_all(hd->socket, nbd_cmd, 4) == -1) {
	PrintError("Error sending read command\n");
	return -1;
    }
    
    if (send_all(hd->socket, (char *)&offset, 8) == -1) {
	PrintError("Error sending read offset\n");
	return -1;
    }

    if (send_all(hd->socket, (char *)&length, 4) == -1) {
	PrintError("Error sending read length\n");
	return -1;
    }

    if (recv_all(hd->socket, (char *)&status, 1) == -1) {
	PrintError("Error receiving status\n");
	return -1;
    }

    if (status != NBD_STATUS_OK) {
	PrintError("NBD Error....\n");
	return -1;
    }

    PrintDebug("Reading Data Ret Length\n");

    if (recv_all(hd->socket, (char *)&ret_len, 4) == -1) {
	PrintError("Error receiving Return read length\n");
	return -1;
    }

    if (ret_len != length) {
	PrintError("Read length mismatch (req=%d) (result=%d)\n", length, ret_len);
	return -1;
    }

    PrintDebug("Reading Data (%d bytes)\n", ret_len);

    if (recv_all(hd->socket, (char *)buf, ret_len) == -1) {
	PrintError("Read Data Error\n");
	return -1;
    }

    return 0;
}


static int hd_write(uint8_t * buf, int sector_count, uint64_t lba, void * private_data) {
    struct vm_device * hd_dev = (struct vm_device *)private_data;
    struct hd_state * hd = (struct hd_state *)(hd_dev->private_data);
    int offset = lba * IDE_SECTOR_SIZE;
    int length = sector_count * IDE_SECTOR_SIZE;
    uint8_t status;
    char nbd_cmd[4] = {0,0,0,0};

    nbd_cmd[0] = NBD_WRITE_CMD;

    if (send_all(hd->socket, nbd_cmd, 4) == -1) {
	PrintError("Error sending write command\n");
	return -1;
    }

    if (send_all(hd->socket, (char *)&offset, 8) == -1) {
	PrintError("Error sending write offset\n");
	return -1;
    }

    if (send_all(hd->socket, (char *)&length, 4) == -1) {
	PrintError("Error sending write length\n");
	return -1;
    }

    PrintDebug("Writing Data (%d bytes)\n", length);

    if (send_all(hd->socket, (char *)buf, length) == -1) {
	PrintError("Write Data Error\n");
	return -1;
    }

    if (recv_all(hd->socket, (char *)&status, 1) == -1) {
	PrintError("Error receiving status\n");
	return -1;
    }

    if (status != NBD_STATUS_OK) {
	PrintError("NBD Error....\n");
	return -1;
    }

    return 0;
}


static uint64_t hd_get_capacity(void * private_data) {
    struct vm_device * hd_dev = (struct vm_device *)private_data;
    struct hd_state * hd = (struct hd_state *)(hd_dev->private_data);

    return hd->capacity / IDE_SECTOR_SIZE;
}

static struct v3_ide_hd_ops hd_ops = {
    .read = hd_read, 
    .write = hd_write,
    .get_capacity = hd_get_capacity,
};


static int hd_init(struct vm_device * dev) {
    struct hd_state * hd = (struct hd_state *)(dev->private_data);
    char header[64];
    
    PrintDebug("Intializing Net HD\n");

    hd->socket = V3_Create_TCP_Socket();

    PrintDebug("HD socket: %d\n", hd->socket);
    PrintDebug("Connecting to: %s:%d\n", v3_inet_ntoa(hd->ip_addr), hd->port);

    V3_Connect_To_IP(hd->socket, v3_ntohl(hd->ip_addr), hd->port);

    PrintDebug("Connected to NBD server\n");

    //snprintf(header, 64, "V3_NBD_1 %s\n", cd->disk_name);
    strcpy(header, "V3_NBD_1 ");
    strncat(header, hd->disk_name, 32);
    strncat(header, "\n", 1);


    if (send_all(hd->socket, header, strlen(header)) == -1) {
	PrintError("Error connecting to Network Block Device: %s\n", hd->disk_name);
	return -1;
    }

    // Cache Capacity
    {
	char nbd_cmd[4] = {0,0,0,0};

	nbd_cmd[0] = NBD_CAPACITY_CMD;
	
	if (send_all(hd->socket, nbd_cmd, 4) == -1) {
	    PrintError("Error sending capacity command\n");
	    return -1;
	}

	if (recv_all(hd->socket, (char *)&(hd->capacity), 8) == -1) {
	    PrintError("Error Receiving Capacity\n");
	    return -1;
	}	

	PrintDebug("Capacity: %p\n", (void *)(hd->capacity));
    }

    PrintDebug("Registering HD\n");

    if (v3_ide_register_harddisk(hd->ide, hd->bus, hd->drive, "V3-NET-HD", &hd_ops, dev) == -1) {
	return -1;
    }
    
    PrintDebug("intialization done\n");

    return 0;
}


static int hd_deinit(struct vm_device * dev) {
    return 0;
}

static struct vm_device_ops dev_ops = {
    .init = hd_init, 
    .deinit = hd_deinit,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};

struct vm_device * v3_create_net_hd(struct vm_device * ide, 
				    uint_t bus, uint_t drive, 
				    const char * ip_str, uint16_t port, 
				    const char * disk_tag) {
    struct hd_state * hd = (struct hd_state *)V3_Malloc(sizeof(struct hd_state));

    PrintDebug("Registering Net HD at %s:%d disk=%s\n", ip_str, port, disk_tag);

    strncpy(hd->disk_name, disk_tag, sizeof(hd->disk_name));
    hd->ip_addr = v3_inet_addr(ip_str);
    hd->port = port;

    hd->ide = ide;
    hd->bus = bus;
    hd->drive = drive;
	
    struct vm_device * hd_dev = v3_create_device("NET-HD", &dev_ops, hd);

    return hd_dev;
}
