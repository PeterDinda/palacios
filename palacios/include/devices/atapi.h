#ifndef __ATAPI_H__
#define __ATAPI_H__

#ifdef __V3VEE__

#include <devices/ide.h>
#include <palacios/vm_dev.h>

int handle_atapi_packet_command(struct vm_device * dev, 
				struct channel_t * channel, 
				ushort_t val);

void rd_init_send_atapi_command(struct vm_device * dev, 
				struct channel_t * channel, 
				Bit8u command, int req_length, 
				int alloc_length, bool lazy);

void rd_ready_to_send_atapi(struct vm_device * dev, 
			    struct channel_t * channel);

void rd_atapi_cmd_error(struct vm_device * dev, 
			struct channel_t * channel, 
			sense_t sense_key, asc_t asc);

void rd_atapi_cmd_nop(struct vm_device * dev, struct channel_t * channel);
void rd_identify_ATAPI_drive(struct vm_device * dev, struct channel_t * channel);

#endif // ! __V3VEE__


#endif
