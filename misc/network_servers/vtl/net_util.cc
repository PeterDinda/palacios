#include <stdlib.h>
#include <stdio.h>

#include "net_util.h"


void do_string_to_ipaddress(unsigned char * ip, IPADDRESS & ipaddress) {

}

unsigned long do_ipaddress_to_unsigned_long(IPADDRESS & ipaddress) {
	return 0;
}

// ip address conversion functions
void do_binary_to_ipaddress(unsigned char* ip,IPADDRESS& ipaddress) {
  ipaddress.a1 = ip[0];
  ipaddress.a2 = ip[1];
  ipaddress.a3 = ip[2];
  ipaddress.a4 = ip[3];
}


// ip address conversion functions
void do_binary_to_string(unsigned char* ip,char* buffer) {
  IPADDRESS ipaddress;
  do_binary_to_ipaddress(ip, ipaddress);
  do_ipaddress_to_string(ipaddress, buffer);
}

void do_ipaddress_to_string(IPADDRESS ipaddress, char* buffer) {
  sprintf(buffer,"%d.%d.%d.%d", ipaddress.a1, ipaddress.a2, ipaddress.a3, ipaddress.a4);
}

/*
// this function returns the ip protocol string based on the ip protocol number
char* return_ip_protocol(unsigned char protocol) {

  if(protocol == 0x1) {
      return "ICMP";
  } else if(protocol == 0x6) {
      return "TCP";
  } else if(protocol == 17) {
      return "UDP";
  } else if(protocol == 121) {
      return "SMP";
  } else {
    return "Unknown";
  }

  return 0;
}
*/
