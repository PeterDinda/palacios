#ifndef _util
#define _util


#include <stdio.h>
#include <iostream>
#include <string>


#ifdef linux
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#elif defined(WIN32)

#include <io.h>
#include <Winsock2.h>
#define read _read
#define write _write

#define EWOULDBLOCK WSAEWOULDBLOCK

typedef int socklen_t;
#endif





#define ETHERNET_HEADER_LEN 14
#define ETHERNET_DATA_MIN   46
#define ETHERNET_DATA_MAX   1500

#define ETHERNET_PACKET_LEN (ETHERNET_HEADER_LEN+ETHERNET_DATA_MAX)


#if defined(USE_SSL)
extern "C" {
#define OPENSSL_NO_KRB5
#include <openssl/ssl.h>
}
#endif

using namespace std;

#if defined(USE_SSL)
int readall(const int fd, SSL * ssl, char * buf, const int len, const int oneshot = 0, const int awaitblock = 1);
int writeall(const int fd, SSL * ssl, const char * buf, const int len, const int oneshot = 0, const int awaitblock = 1);
#endif
int readall(const int fd, char * buf, const int len, const int oneshot = 0, const int awaitblock = 1);
int writeall(const int fd, const char * buf, const int len, const int oneshot = 0, const int awaitblock = 1);



#if defined(WIN32) 
#define snprintf _snprintf
#endif


int compare_nocase(const string &s1, const string &s2);

struct SerializationException {};

void printhexnybble(FILE * out,const char lower);
void printhexbyte(FILE * out,const char h);
void printhexshort(FILE * out,const short h);
void printhexint(FILE * out,const int h);
void printhexbuffer(FILE * out, const char * buf, const int len);

void hexbytetobyte(const char hexbyte[2], char * byte);
void bytetohexbyte(const char byte, char hexbyte[2]);

void ConvertHexEthernetAddressToBinary(const char * string, char address[6]);
void ConvertBinaryEthernetAddressToHex(char address[6], char * string);

// How about a function that you might actually use...
void mac_to_string(char address[6], char * buf);
void mac_to_string(char address[6], string * str);
void string_to_mac(string str, char mac[6]);
void string_to_mac(const char * str, char mac[6]);

void ip_to_string(unsigned long addr, string * str);
void ip_to_string(unsigned long addr, char * buf);
const char * ip_to_string(unsigned long addr);

typedef char EthernetAddrString[(2 * 6) + 6];


struct EthernetAddr {
  char addr[6];

  EthernetAddr();
  EthernetAddr(const EthernetAddr &rhs);
  EthernetAddr(const EthernetAddrString rhs);
  const EthernetAddr & operator=(const EthernetAddr &rhs);

  bool operator==(const EthernetAddr &rhs) const;
 
  void SetToString(const EthernetAddrString s);
  void GetAsString(EthernetAddrString s) const;
  
#if defined(USE_SSL)
  void Serialize(const int fd, SSL * ssl) const;
  void Unserialize(const int fd,SSL * ssl);
#endif
  void Serialize(const int fd) const;
  void Unserialize(const int fd);


  ostream & Print(ostream &os) const;
};

inline ostream & operator<<(ostream &os, const EthernetAddr &e)
{
  return e.Print(os);
}

#endif
