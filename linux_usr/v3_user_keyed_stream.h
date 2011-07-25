#ifndef __V3_USER_KSTREAM_H__
#define __V3_USER_KSTREAM_H__

#include <stdint.h>
#define sint64_t int64_t

#include "iface-keyed-stream-user.h"

int v3_user_keyed_stream_attach(char *dev, char *url);
int v3_user_keyed_stream_detach(int devfd);

int v3_user_keyed_stream_have_request(int devfd);
int v3_user_keyed_stream_pull_request(int devfd, struct palacios_user_keyed_stream_op **req);
int v3_user_keyed_stream_push_response(int devfd, struct palacios_user_keyed_stream_op *resp);


#endif
