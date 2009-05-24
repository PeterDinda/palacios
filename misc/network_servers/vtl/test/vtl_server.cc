#include "vtl_util.h"
#include "if.h"
#include "debug.h"



#define IFACE_NAME "vmnet1"

DEBUG_DECLARE();

int main(int argc, char ** argv) {
  RawEthernetPacket pkt;

  char * iface_name = IFACE_NAME;

  if (argc == 2) {
    iface_name = argv[1];
  }
  iface_t * iface = if_connect(iface_name, IF_RW);

  debug_init("./vtp.log");

  while (if_read_pkt(iface, &pkt) == 0) {

    printf("READ packet\n");
    if (is_tcp_pkt(&pkt)) {
      printf("TCP!!\n");
    }

  }

}
