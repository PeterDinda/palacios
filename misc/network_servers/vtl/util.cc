
#include "util.h"
#include <ctype.h> 

#include <string.h>


#if defined(USE_SSL)
//SSL specific include libraries
#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#endif

int compare_nocase(const string& s1, const string& s2)
{
  string::const_iterator p1 = s1.begin();
  string::const_iterator p2 = s2.begin();

  while((p1 != s1.end()) && (p2 != s2.end())) {
    if (toupper(*p1) != toupper(*p2)) {
      return (toupper(*p1) < toupper(*p2)) ? -1 : 1;
    }
    
    p1++;
    p2++;
  }
 
  /* This is a two layer tri op */
  return((s2.size() == s1.size()) ? 0 : 
	 (s1.size() < s2.size()) ? -1 : 1);
}

void ConvertHexEthernetAddressToBinary(const char* string, char address[6]) {
  for(int k = 0; k < 6; k++) {
      hexbytetobyte(&(string[2 * k]), address + k);
    }
} 

void ConvertBinaryEthernetAddressToHex(char address[6], char * string) {
  for (int j = 0; j < 6; j++) { 
    bytetohexbyte(address[j], &(string[2 * j]));
  }
}

void string_to_mac(string str, char mac[6]) {
  for(int k = 0; k < 6; k++) {
    hexbytetobyte(&(str[(2 * k) + k]), mac + k);
  }
}

void string_to_mac(const char * str, char mac[6]) {
  for(int k = 0; k < 6; k++) {
    hexbytetobyte(&(str[(2 * k) + k]), mac + k);
  }
}

void mac_to_string(char address[6], char * buf) {
  for (int i = 0; i < 6; i++) {
    bytetohexbyte(address[i], &(buf[3 * i]));
    buf[(3 * i) + 2] = ':';
  }
  buf[17] = 0;
}

void mac_to_string(char address[6], string * str) {
  EthernetAddrString buf;

  mac_to_string(address, buf);  
  *str = buf;
}

void ip_to_string(unsigned long addr, string * str) {
  struct in_addr addr_st;

  addr_st.s_addr = htonl(addr);
  *str = inet_ntoa(addr_st);
}


void ip_to_string(unsigned long addr, char * buf) {
  struct in_addr addr_st;
  char * tmp_str;

  addr_st.s_addr = htonl(addr);
  tmp_str = inet_ntoa(addr_st);

  memcpy(buf, tmp_str, strlen(tmp_str));
}

const char * ip_to_string(unsigned long addr) {
  struct in_addr addr_st;

  addr_st.s_addr = htonl(addr);
  return inet_ntoa(addr_st);
}


#if defined(USE_SSL) 
int readall(const int fd, SSL *ssl, char *buf, const int len, const int oneshot, const int awaitblock) {
  int rc;
  int left;


  left = len;

  while (left > 0) {
    if (ssl != NULL) {
      rc = SSL_read(ssl, &(buf[len - left]), left);
    } else {
      rc = read(fd, &(buf[len - left]), left);
    }

    if (oneshot) { 
      return rc;
    }

    if (rc <= 0) { 
      if (errno == EINTR) {
        continue;
      }
      if ((errno == EWOULDBLOCK) && awaitblock) {
        continue;
      }
      return rc;
    } else {
      left -= rc;
    }
  }
  return len;
}

int writeall(const int fd, SSL *ssl, const char *buf, const int len, const int oneshot, const int awaitblock)
{
  int rc;
  int left;
  

  left = len;

  while (left > 0) {
    if(ssl != NULL) {
      rc = SSL_write(ssl, &(buf[len - left]), left);
    } else {
      rc = write(fd, &(buf[len - left]), left);
    }
    if (oneshot) { 
      return rc;
    }
    if (rc <= 0) { 
      if (errno == EINTR) {
        continue;
      }
      if ((errno == EWOULDBLOCK) && awaitblock) {
        continue;
      }
      return rc;
    } else {
      left -= rc;
    }
  }
  return len;
}
#endif

int readall(const int fd, char *buf, const int len, const int oneshot, const int awaitblock) {
  int rc;
  int left;

  left = len;

  while (left > 0) {
    rc = read(fd, &(buf[len - left]), left);
    
    if (oneshot) { 
      return rc;
    }

    if (rc <= 0) { 
      if (errno == EINTR) {
        continue;
      }

      if ((errno == EWOULDBLOCK) && awaitblock) {
        continue;
      }

      return rc;
    } else {
      left -= rc;
    }
  }
  return len;
}

int writeall(const int fd, const char *buf, const int len, const int oneshot, const int awaitblock)
{
  int rc;
  int left;
  

  left = len;
  while (left > 0) {
    rc = write(fd, &(buf[len - left]), left);

    if (oneshot) { 
      return rc;
    }

    if (rc <= 0) { 
      if (errno == EINTR) {
        continue;
      }
      if ((errno == EWOULDBLOCK) && awaitblock) {
        continue;
      }
      return rc;
    } else {
      left -= rc;
    }
  }
  return len;
}



void printhexnybble(FILE *out,const char lower) {
  fputc( (lower >= 10) ? (lower - 10 + 'A') : (lower + '0'), 
	 out);
}

void printhexbyte(FILE *out,const char h) {
  char upper=(h >> 4) & 0xf;
  char lower=h & 0xf;

  printhexnybble(out, upper); 
  printhexnybble(out, lower);
}

void printhexbuffer(FILE *out, const char *buf, const int len) {
  int i;
  for (i = 0; i < len; i++) { 
    printhexbyte(out, buf[i]);
  }
}

void printhexshort(FILE *out, const short s) {
  printhexbuffer(out, (char*)&s, 2);
}

void printhexint(FILE *out, const int i) {
  printhexbuffer(out, (char*)&i, 4);
}


char hexnybbletonybble(const char hexnybble) {
  char x = toupper(hexnybble);
  if ((x >= '0') && (x <= '9')) {
    return x - '0';
  } else {
    return 10 + (x - 'A');
  }
}

void hexbytetobyte(const char hexbyte[2], char *byte) {
  *byte = ((hexnybbletonybble(hexbyte[0]) << 4) + 
	   (hexnybbletonybble(hexbyte[1]) & 0xf));
}

char nybbletohexnybble(const char nybble) {
  return (nybble >= 10) ? (nybble - 10 + 'A') : (nybble + '0');
}

void bytetohexbyte(const char byte, char hexbyte[2]) {
  hexbyte[0] = nybbletohexnybble((byte >> 4) & 0xf);
  hexbyte[1] = nybbletohexnybble(byte & 0xf);
}

EthernetAddr::EthernetAddr()  {
  memset(addr, 0, 6);
}


EthernetAddr::EthernetAddr(const EthernetAddr &rhs) {
  memcpy(addr, rhs.addr, 6);
}

EthernetAddr::EthernetAddr(const EthernetAddrString rhs) {
  SetToString(rhs);
}


const EthernetAddr & EthernetAddr::operator=(const EthernetAddr &rhs) {
  memcpy(addr, rhs.addr, 6);
  return *this;
}

bool EthernetAddr::operator==(const EthernetAddr &rhs) const {
  return (memcmp(addr, rhs.addr, 6) == 0);
}



void EthernetAddr::SetToString(const EthernetAddrString s) {
  int i,j;

  for (i = 0, j = 0; i < 6; i++, j += 3) {
    hexbytetobyte(&(s[j]), &(addr[i]));
  }

}

void EthernetAddr::GetAsString(EthernetAddrString s) const {
  int i,j;

  for (i = 0, j = 0; i < 6; i++, j += 3) {
    bytetohexbyte(addr[i], &(s[j]));
    if (i < 5) {
      s[j + 2] = ':';
    } else {
      s[j + 2] = 0;
    }
  }
}    


ostream & EthernetAddr::Print(ostream &os) const {
  EthernetAddrString s;
  
  GetAsString(s);
  os << "EthernetAddr(" << (char*)s << ")";
  return os;
}

#if defined(USE_SSL)
void EthernetAddr::Serialize(const int fd, SSL *ssl) const {
  if (writeall(fd, ssl, addr, 6, 0, 1) != 6) {
    throw SerializationException();
  }
}

void EthernetAddr::Unserialize(const int fd, SSL *ssl) {
  if (readall(fd, ssl, addr, 6, 0, 1) != 6) {
    throw SerializationException();
  }    
}

#endif
void EthernetAddr::Serialize(const int fd) const {
  if (writeall(fd, addr, 6, 0, 1) != 6) {
    throw SerializationException();
  }
}

void EthernetAddr::Unserialize(const int fd) {
  if (readall(fd, addr, 6, 0, 1) != 6) {
    throw SerializationException();
  }
}

