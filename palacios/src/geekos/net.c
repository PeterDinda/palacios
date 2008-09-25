/* Northwestern University */
/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */
#include <geekos/net.h>
#include <geekos/socket.h>
#include <geekos/ne2k.h>

void Init_Network() {
  init_socket_layer();
}




void test_network() {

    uchar_t local_addr[4];
    uchar_t remote_addr[4];

    local_addr[0] = 10;
    local_addr[1] = 0;
    local_addr[2] = 2;
    local_addr[3] = 21;

    set_ip_addr(local_addr);

    remote_addr[0] = 10;
    remote_addr[1] = 0;
    remote_addr[2] = 2;
    remote_addr[3] = 20;


    connect(remote_addr, 4301);

}
