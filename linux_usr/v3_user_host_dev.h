#ifndef _V3_USER_HOST_DEV_
#define _V3_USER_HOST_DEV_

#include <stdint.h>
#include "iface-host-dev.h"

int v3_user_host_dev_rendezvous(char *vmdev, char *url); // returns devfd for use in poll/select
int v3_user_host_dev_depart(int devfd);

int v3_user_host_dev_have_request(int devfd);
int v3_user_host_dev_pull_request(int devfd, struct palacios_host_dev_host_request_response **req);
int v3_user_host_dev_push_response(int devfd, struct palacios_host_dev_host_request_response *resp);

uint64_t v3_user_host_dev_read_guest_mem(int devfd, void *gpa, void *dest, uint64_t len);
uint64_t v3_user_host_dev_write_guest_mem(int devfd, void *gpa, void *src, uint64_t len);
int      v3_user_host_dev_inject_guest_irq(int devfd, uint8_t irq);

#endif

