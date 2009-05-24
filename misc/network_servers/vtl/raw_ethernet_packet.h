#ifndef _raw_ethernet_packet
#define _raw_ethernet_packet
#include <iostream>
#include <stdio.h>

#include "socks.h"

#ifdef linux
#include <sys/socket.h>
#include <netinet/in.h>
#elif defined(WIN32)


#endif



#ifdef USE_SSL
extern "C" {
#define OPENSSL_NO_KRB5
#include <openssl/ssl.h>
}
#endif

class Packet;

using namespace std;


#define ETHERNET_HEADER_LEN 14
#define ETHERNET_DATA_MIN   46
#define ETHERNET_DATA_MAX   1500

#define ETHERNET_PACKET_LEN (ETHERNET_HEADER_LEN+ETHERNET_DATA_MAX)


#define SERIALIZATION_CLOSED -1
#define SERIALIZATION_ERROR -2

struct RawEthernetPacket {
  
  char pkt[2 + 4 + ETHERNET_PACKET_LEN];
  char * type;
  size_t * size;
  char * data;

  size_t get_size() const;
  void set_size(size_t new_size);
  
  char * get_type();
  void set_type(const char * new_type);

  char * get_data();
  
  int length() const { return sizeof(pkt);}


  RawEthernetPacket();
  RawEthernetPacket(const RawEthernetPacket &rhs);
  RawEthernetPacket(const char *data, const size_t size);
  const RawEthernetPacket & operator= (const RawEthernetPacket &rhs);
  virtual ~RawEthernetPacket();

  int SerializeToBuf(char ** buf) const;
  void UnserializeFromBuf(char * buf);

#ifdef USE_SSL
  int Serialize(const SOCK fd, SSL *ssl) const;
  int Unserialize(const SOCK fd, SSL *ssl);
#endif
  int Serialize(const SOCK fd) const;
  int Unserialize(const SOCK fd);

  int UdpSerialize(const SOCK fd,struct sockaddr *serveraddr) const;
  int UdpUnserialize(const SOCK fd);
  
  int VtpSerialize(const SOCK fd, struct in_addr * serveraddr) const;
  int VtpUnserialize(const SOCK fd, struct in_addr * serveraddr);




  void Print(unsigned size=ETHERNET_PACKET_LEN, FILE *out=stdout) const;
  ostream & Print(ostream &os) const;
};

inline ostream & operator<<(ostream &os, const RawEthernetPacket &p) {
  return p.Print(os);
}
#endif
