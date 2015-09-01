/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2009, Robert Deloatch <rtdeloatch@gmail.com>
 * Copyright (c) 2009, Steven Jaconette <stevenjaconette2007@u.northwestern.edu> 
 * Copyright (c) 2009, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Robdert Deloatch <rtdeloatch@gmail.com>
 *         Steven Jaconette <stevenjaconette2007@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

/* Interface between virtual video card and client apps */

#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vmm_host_events.h>
#include <palacios/vmm_lock.h>
#include <palacios/vmm_string.h>
#include <interfaces/vmm_socket.h>

#include <devices/console.h>


#define NUM_ROWS 25
#define NUM_COLS 80
#define BYTES_PER_ROW (NUM_COLS * 2)
#define BYTES_PER_COL 2


#define SCREEN_SIZE 4000

#define NO_KEY { 0, 0 }
#define ESC_CHAR  ((uint8_t)0x1b)
#define CR_CHAR   ((uint8_t)0x0d)

#define ASCII_CTRL_CODE 0x1d


struct cons_state {
    v3_sock_t server_fd;
    v3_sock_t client_fd;

    uint16_t port;

    int connected;

    v3_lock_t cons_lock;

    struct v3_vm_info * vm;

    struct vm_device * frontend_dev;
};

struct key_code {
    uint8_t scan_code;
    uint8_t capital;
};


static const struct key_code ascii_to_key_code[] = {             // ASCII Value Serves as Index
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x00 - 0x03
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x04 - 0x07
    { 0x0E, 0 },    { 0x0F, 0 },    { 0x1C, 0 },    NO_KEY,      // 0x08 - 0x0B
    NO_KEY,         { 0x1C, 0 },    NO_KEY,         NO_KEY,      // 0x0C - 0x0F
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x10 - 0x13
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x14 - 0x17
    NO_KEY,         NO_KEY,         NO_KEY,         { 0x01, 0 }, // 0x18 - 0x1B
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x1C - 0x1F
    { 0x39, 0 },    { 0x02, 1 },    { 0x28, 1 },    { 0x04, 1 }, // 0x20 - 0x23
    { 0x05, 1 },    { 0x06, 1 },    { 0x08, 1 },    { 0x28, 0 }, // 0x24 - 0x27
    { 0x0A, 1 },    { 0x0B, 1 },    { 0x09, 1 },    { 0x0D, 1 }, // 0x28 - 0x2B
    { 0x33, 0 },    { 0x0C, 0 },    { 0x34, 0 },    { 0x35, 0 }, // 0x2C - 0x2F
    { 0x0B, 0 },    { 0x02, 0 },    { 0x03, 0 },    { 0x04, 0 }, // 0x30 - 0x33
    { 0x05, 0 },    { 0x06, 0 },    { 0x07, 0 },    { 0x08, 0 }, // 0x34 - 0x37
    { 0x09, 0 },    { 0x0A, 0 },    { 0x27, 1 },    { 0x27, 0 }, // 0x38 - 0x3B
    { 0x33, 1 },    { 0x0D, 0 },    { 0x34, 1 },    { 0x35, 1 }, // 0x3C - 0x3F
    { 0x03, 1 },    { 0x1E, 1 },    { 0x30, 1 },    { 0x2E, 1 }, // 0x40 - 0x43
    { 0x20, 1 },    { 0x12, 1 },    { 0x21, 1 },    { 0x22, 1 }, // 0x44 - 0x47
    { 0x23, 1 },    { 0x17, 1 },    { 0x24, 1 },    { 0x25, 1 }, // 0x48 - 0x4B
    { 0x26, 1 },    { 0x32, 1 },    { 0x31, 1 },    { 0x18, 1 }, // 0x4C - 0x4F
    { 0x19, 1 },    { 0x10, 1 },    { 0x13, 1 },    { 0x1F, 1 }, // 0x50 - 0x53
    { 0x14, 1 },    { 0x16, 1 },    { 0x2F, 1 },    { 0x11, 1 }, // 0x54 - 0x57
    { 0x2D, 1 },    { 0x15, 1 },    { 0x2C, 1 },    { 0x1A, 0 }, // 0x58 - 0x5B
    { 0x2B, 0 },    { 0x1B, 0 },    { 0x07, 1 },    { 0x0C, 1 }, // 0x5C - 0x5F
    { 0x29, 0 },    { 0x1E, 0 },    { 0x30, 0 },    { 0x2E, 0 }, // 0x60 - 0x63
    { 0x20, 0 },    { 0x12, 0 },    { 0x21, 0 },    { 0x22, 0 }, // 0x64 - 0x67
    { 0x23, 0 },    { 0x17, 0 },    { 0x24, 0 },    { 0x25, 0 }, // 0x68 - 0x6B
    { 0x26, 0 },    { 0x32, 0 },    { 0x31, 0 },    { 0x18, 0 }, // 0x6C - 0x6F
    { 0x19, 0 },    { 0x10, 0 },    { 0x13, 0 },    { 0x1F, 0 }, // 0x70 - 0x73
    { 0x14, 0 },    { 0x16, 0 },    { 0x2F, 0 },    { 0x11, 0 }, // 0x74 - 0x77
    { 0x2D, 0 },    { 0x15, 0 },    { 0x2C, 0 },    { 0x1A, 1 }, // 0x78 - 0x7B
    { 0x2B, 1 },    { 0x1B, 1 },    { 0x29, 1 },    { 0x0E, 0 }  // 0x7C - 0x7F
};



static int deliver_scan_code(struct cons_state * state, struct key_code * key) {
    struct v3_keyboard_event key_event;
    struct v3_keyboard_event key_shift;
    uint_t cap = key->capital;

    key_event.status = 0;
    key_event.scan_code = (uint8_t)key->scan_code;

    PrintDebug(VM_NONE, VCORE_NONE, "Scan code: 0x%x\n", key_event.scan_code);


    if (cap) {
	key_shift.status = 0;
	key_shift.scan_code = (uint8_t)0x2A;

	if (v3_deliver_keyboard_event(state->vm, &key_shift) == -1) {
	    PrintError(VM_NONE, VCORE_NONE, "Video: Error delivering key event\n");
	    return -1;
	}
    }

    // Press
    if (v3_deliver_keyboard_event(state->vm, &key_event) == -1) {
	PrintError(VM_NONE, VCORE_NONE, "Video: Error delivering key event\n");
	return -1;
    }

    // Release
    key_event.scan_code = key_event.scan_code | 0x80;
  
    if (v3_deliver_keyboard_event(state->vm, &key_event) == -1) {
	PrintError(VM_NONE, VCORE_NONE, "Video: Error delivering key event\n");
	return -1;
    }


    if (cap) {
        key_shift.scan_code = 0x2A | 0x80;

	if (v3_deliver_keyboard_event(state->vm, &key_shift) == -1) {
	    PrintError(VM_NONE, VCORE_NONE, "Video: Error delivering key event\n");
	    return -1;
	}
    }

    PrintDebug(VM_NONE, VCORE_NONE, "Finished with Key delivery\n");
    return 0;
}




static int recv_all(v3_sock_t socket, char * buf, int length) {
    int bytes_read = 0;
    
    PrintDebug(VM_NONE, VCORE_NONE, "Reading %d bytes\n", length - bytes_read);
    while (bytes_read < length) {
        int tmp_bytes = v3_socket_recv(socket, buf + bytes_read, length - bytes_read);
        PrintDebug(VM_NONE, VCORE_NONE, "Received %d bytes\n", tmp_bytes);

        if (tmp_bytes == 0) {
            PrintError(VM_NONE, VCORE_NONE, "Connection Closed unexpectedly\n");
            return 0;
        } else if (tmp_bytes == -1) {
	    PrintError(VM_NONE, VCORE_NONE, "Socket Error in for V3_RECV\n");
	    return -1;
	}

        bytes_read += tmp_bytes;
    }
    
    return bytes_read;
}


static int send_all(const v3_sock_t sock, const char * buf, const int len){
    int bytes_left = len;

    while (bytes_left != 0) {
	int written = 0;

	if ((written = v3_socket_send(sock, buf + (len - bytes_left), bytes_left)) == -1) {
	    return -1;
	}

	bytes_left -= written;
    }
    return 0;
}

// Translate attribute color into terminal escape sequence color
static const uint8_t fg_color_map[] = {
    30, 34, 32, 36, 31, 35, 33, 37, 90, 94, 92, 96, 91, 95, 93, 97
};

static const uint8_t bg_color_map[] = {
    40, 44, 42, 46, 41, 45, 43, 47, 100, 104, 102, 106, 101, 105, 103, 107
};


#define INT_TO_CHAR(index, buf, val)					\
    do {								\
	uint8_t base = '0';						\
	if ((val) >= 100) buf[(index)++] = base + ((val) / 100);	\
	if ((val) >= 10)  buf[(index)++] = base + ((val) / 10);		\
	buf[(index)++] = base + ((val) % 10);				\
    } while (0)


static int send_update(struct cons_state * state, uint8_t  x, uint8_t  y, uint8_t attrib, uint8_t val) {
    uint8_t fg_color = fg_color_map[(attrib & 0x0f) % 16];
    uint8_t bg_color = bg_color_map[(attrib & 0xf0) % 16];
    uint8_t buf[64];
    int ret = 0;
    int i = 0;

    memset(buf, 0, 32);

    buf[i++] = ESC_CHAR;
    buf[i++] = '[';

    INT_TO_CHAR(i, buf, y + 1);

    buf[i++] = ';';

    INT_TO_CHAR(i, buf, x + 1);

    buf[i++] = 'H';
    buf[i++] = ESC_CHAR;
    buf[i++] = '[';
    buf[i++] = '0';
    buf[i++] = 'm';
    buf[i++] = ESC_CHAR;
    buf[i++] = '[';

    INT_TO_CHAR(i, buf, fg_color);

    buf[i++] = ';';

    INT_TO_CHAR(i, buf, bg_color);

    buf[i++] = 'm';

    // Add value

    buf[i++] = ESC_CHAR;
    buf[i++] = '[';
    INT_TO_CHAR(i, buf, y + 1);
    buf[i++] = ';';
    INT_TO_CHAR(i, buf, x + 1);
    buf[i++] = 'H';
    buf[i++] = val;

    PrintDebug(VM_NONE, VCORE_NONE, "printing value '%c'\n", val);

    if (state->connected) {
	uint64_t start, end;

	rdtscll(start);
	ret =  send_all(state->client_fd, buf, i);
	rdtscll(end);

	PrintDebug(VM_NONE, VCORE_NONE, "Sendall latency=%d cycles\n", (uint32_t)(end - start));
    }

    return ret;
}



static int cursor_update(uint_t x, uint_t y, void * private_data) {
    struct cons_state * state = (struct cons_state *)private_data;
    uint8_t buf[16];
    int ret = 0;
    addr_t irq_state = 0;
    int i = 0;

    memset(buf, 0, 16);

    buf[i++] = ESC_CHAR;
    buf[i++] = '[';
    INT_TO_CHAR(i, buf, y + 1);
    buf[i++] = ';';
    INT_TO_CHAR(i, buf, x + 1);
    buf[i++] = 'H';


    irq_state = v3_lock_irqsave(state->cons_lock);

    if (state->connected) {
	ret = send_all(state->client_fd, buf, 16);
    }

    v3_unlock_irqrestore(state->cons_lock, irq_state);
    
    return ret;
}


static int screen_update(uint_t x, uint_t y, uint_t length, void * private_data) {
    struct cons_state * state = (struct cons_state *)private_data;
    uint_t offset = (x * BYTES_PER_COL) + (y * BYTES_PER_ROW);
    uint8_t fb_buf[length];
    int i = 0;
    uint_t cur_x = x;
    uint_t cur_y = y;
    addr_t irq_state = 0;
    int ret = 0;

    memset(fb_buf, 0, length);
    


    irq_state = v3_lock_irqsave(state->cons_lock);

    v3_cons_get_fb(state->frontend_dev, fb_buf, offset, length);

    v3_unlock_irqrestore(state->cons_lock, irq_state);


    for (i = 0; i < length; i += 2) {
	uint_t col_index = i;
	uint8_t col[2];

	col[0] = fb_buf[col_index];     // Character
	col[1] = fb_buf[col_index + 1]; // Attribute

	irq_state = v3_lock_irqsave(state->cons_lock);

	if (send_update(state, cur_x, cur_y, col[1], col[0]) == -1) {
	    PrintError(VM_NONE, VCORE_NONE, "Could not send attribute to telnet session\n");
	    ret = -1;
	    break;
	}

	v3_unlock_irqrestore(state->cons_lock, irq_state);
				    

	// CAUTION: the order of these statements is critical
	// cur_y depends on the previous value of cur_x
	cur_y = cur_y + ((cur_x + 1) / NUM_COLS);
	cur_x = (cur_x + 1) % NUM_COLS;
    }


    return ret;
}

static int scroll(int rows, void * private_data) {
    struct cons_state * state = (struct cons_state *)private_data;
    addr_t irq_state = 0;
    int ret = 0;

    if (rows > 0) {
	int i = 0;

	irq_state = v3_lock_irqsave(state->cons_lock);

	for (i = rows; i > 0; i--) {
	    uint8_t message[2] = { ESC_CHAR, 'D' };

	    if (state->connected) {
		if (send_all(state->client_fd, message, sizeof(message)) == -1) {
		    PrintError(VM_NONE, VCORE_NONE, "Could not send scroll command\n");
		    ret = -1;
		    break;
		}
	    }
	}
	
	v3_unlock_irqrestore(state->cons_lock, irq_state);

    } else if (rows < 0) {
	ret = screen_update(0, 0, SCREEN_SIZE, private_data);
    }

    return ret;
}



static struct v3_console_ops cons_ops = {
    .update_screen = screen_update, 
    .update_cursor = cursor_update,
    .scroll = scroll,
    .set_text_resolution = NULL,
};

static int cons_free(struct cons_state * state) {

    // kill thread... ?

    v3_lock_deinit(&(state->cons_lock));

    V3_Free(state);
    return 0;
}


static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))cons_free,
};



static int key_handler( struct cons_state * state, uint8_t ascii) {
    PrintDebug(VM_NONE, VCORE_NONE, "Character recieved: 0x%x\n", ascii);


    if (ascii == ESC_CHAR) { // Escape Key
	// This means that another 2 characters are pending
	// receive it and deliver accordingly
	char esc_seq[2] = {0, 0};

	int recv = recv_all(state->client_fd, esc_seq, 2);

	if (recv == -1) {
	    PrintError(VM_NONE, VCORE_NONE, "Video: Error getting key from network\n");
	    return -1;
	} else if (recv == 0) {
	    PrintDebug(VM_NONE, VCORE_NONE, "Video: Client Disconnected\n");
	    return -1;
	}


	if (esc_seq[0] != '[') {
	    PrintDebug(VM_NONE, VCORE_NONE, "Ignoring non handled escape sequence (codes = %d %d)\n", 
		       esc_seq[0], esc_seq[1]);
	    return 0;
	}


	if (esc_seq[1] == 'A') {	        // UP ARROW
	    struct key_code up = { 0x48, 0 };
	    deliver_scan_code(state, &up);
	} else if (esc_seq[1] == 'B') { 	// DOWN ARROW
	    struct key_code down = { 0x50, 0 };
	    deliver_scan_code(state, &down);
	} else if (esc_seq[1] == 'C') {  	// RIGHT ARROW
	    struct key_code right = { 0x4D, 0 };
	    deliver_scan_code(state, &right);
	} else if (esc_seq[1] == 'D') {	        // LEFT ARROW
	    struct key_code left = { 0x4B, 0 };
	    deliver_scan_code(state, &left);
	}   

    } else if (ascii < 0x80) { // printable
	const struct key_code * key = &(ascii_to_key_code[ascii]);

	if (deliver_scan_code(state, (struct key_code *)key) == -1) {
	    PrintError(VM_NONE, VCORE_NONE, "Could not deliver scan code to vm\n");
	    return -1;
	}

    } else {
	PrintError(VM_NONE, VCORE_NONE, "Invalid character received from network (%c) (code=%d)\n",
		   ascii, ascii);
	//	return 0;
    }
	
    return 0;
}

static int cons_server(void * arg) {
    struct cons_state * state = (struct cons_state *)arg;
    
    state->server_fd = v3_create_tcp_socket(state->vm);


    PrintDebug(VM_NONE, VCORE_NONE, "Video: Socket File Descriptor: 0x%p\n", state->server_fd);

    if (v3_socket_bind(state->server_fd, state->port) == -1) {
	PrintError(VM_NONE, VCORE_NONE, "Video: Failed to bind to socket %d\n", state->port);
    }

    if (v3_socket_listen(state->server_fd, 8) == -1) {
	PrintError(VM_NONE, VCORE_NONE, "Video: Failed to listen with socket 0x%p\n", state->server_fd);
    }

    while (1) {
	uint32_t client_ip;
	uint32_t client_port;
	uint8_t ascii_code = 0;
	int recv = 0;

	if ((state->client_fd = v3_socket_accept(state->server_fd, &client_ip,  &client_port)) == 0) {
	    PrintError(VM_NONE, VCORE_NONE, "Video: Failed to accept connection on port %d\n", client_port);
	}
	PrintDebug(VM_NONE, VCORE_NONE, "Accepted Telnet Console connection\n");
	state->connected = 1;

	screen_update(0, 0, SCREEN_SIZE, state);

	while (1) {
	    recv = recv_all(state->client_fd, &ascii_code, sizeof(ascii_code));

	    PrintDebug(VM_NONE, VCORE_NONE, "Telnet console Received %d bytes\n", recv);

	    if (recv == -1) {
		PrintError(VM_NONE, VCORE_NONE, "Video: Error getting key from network\n");
		break;
	    } else if (recv == 0) {
		PrintDebug(VM_NONE, VCORE_NONE, "Video: Client Disconnected\n");
		break;
	    }

	    if (key_handler(state, ascii_code) == -1) {
		PrintError(VM_NONE, VCORE_NONE, "Error in key handler\n");
		break;
	    }
	}

	state->connected = 0;
	v3_socket_close(state->client_fd);
    }
    
    return -1;
}


static int cons_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct cons_state * state = (struct cons_state *)V3_Malloc(sizeof(struct cons_state));
    v3_cfg_tree_t * frontend_cfg = v3_cfg_subtree(cfg, "frontend");
    struct vm_device * frontend = v3_find_dev(vm, v3_cfg_val(frontend_cfg, "tag"));
    char * dev_id = v3_cfg_val(cfg, "ID");

    if (!state) {
	PrintError(vm, VCORE_NONE, "Cannot allocate in init\n");
	return -1;
    }


    state->vm = vm;
    state->server_fd = 0;
    state->client_fd = 0;
    state->frontend_dev = frontend;
    state->port = atoi(v3_cfg_val(cfg, "port"));
    v3_lock_init(&(state->cons_lock));


    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, state);

    if (dev == NULL) {
	PrintError(vm, VCORE_NONE, "Could not attach device %s\n", dev_id);
	V3_Free(state);
	return -1;
    }


    v3_console_register_cga(frontend, &cons_ops, state);

    V3_CREATE_AND_START_THREAD(cons_server, state, "Telnet Console Network Server", 0);

    return 0;
}



device_register("TELNET_CONSOLE", cons_init)
