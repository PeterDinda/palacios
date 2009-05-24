#ifndef _socks
#define _socks

#include "util.h"

#if defined(WIN32) && !defined(__CYGWIN__)
#include <ws2tcpip.h>
#include <Iphlpapi.h>
#include <winsock2.h>
#include <windows.h>
#else
extern "C" {
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/if_ether.h>

#if defined(USE_SSL)
#define OPENSSL_NO_KRB5
#include <openssl/ssl.h>
#endif // USE_SSL

#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
}
#endif

#include <string>

using namespace std;


#define TCP 0
#define UDP 1

#define SND_RCV_SOCKET_BUF_SIZE 65536
#ifndef INADDR_NONE
#define INADDR_NONE             0xffffffff
#endif


#if defined(WIN32) && !defined(__CYGWIN__)
#include <io.h>
#define WRITE(fd,buf,len) send(fd,buf,len,0)
#define READ(fd,buf,len) recv(fd,buf,len,0)
//#define WRITE(fd,buf,len) _write(_open_osfhandle(fd, 0),buf,len)
//#define READ(fd,buf,len) _read(_open_osfhandle(fd, 0),buf,len)

#define SOCK SOCKET

#define CLOSE(x) closesocket(x)
#define IOCTL(x,y,z) ioctlsocket((SOCKET)x,(long)y,(unsigned long *)z)
#else 

#define SOCK int

#if defined(USE_SSL)
#define WRITE(ssl,buf,len) write(ssl, buf, len)
#else 
#define WRITE(fd,buf,len) write(fd, buf, len)
#endif 

#define READ(fd,buf,len) read(fd, buf, len)
#define CLOSE(x) close(x)
#define IOCTL(x,y,z) ioctl(x, y, z)
#endif


SOCK CreateAndSetupTcpSocket(const int bufsize=SND_RCV_SOCKET_BUF_SIZE, 
                            const bool nodelay=true,
                            const bool nonblocking=false);

SOCK CreateAndSetupUdpSocket(const int bufsize=SND_RCV_SOCKET_BUF_SIZE, 
                            const bool nonblocking=false);

SOCK CreateAndSetupUnixDomainSocket(const int bufsize=SND_RCV_SOCKET_BUF_SIZE,
                                   const bool nonblocking=false);

int SetNoDelaySocket(const SOCK fd, const bool nodelay=true);

int IsSocket(const SOCK fd);
int IsStreamSocket(const SOCK fd);
int IsDatagramSocket(const SOCK fd);

#ifdef linux
int IsVirtualSocket(const int fd);
#endif

int BindSocket(const SOCK mysocket, const int myport);
int BindSocket(const SOCK mysocket, const unsigned adx, const int myport);
int BindSocket(const SOCK mysocket, const char *host_or_ip, const int myport);
int BindSocket(const SOCK mysocket, const char *pathname);

int ListenSocket(const SOCK mysocket, const int max=SOMAXCONN);

int ConnectToHost(const SOCK mysocket, const int hostip, const int port);
int ConnectToHost(const SOCK mysocket, const char *host, const int port);
int ConnectToPath(const SOCK mysocket, const char *pathname);

#if defined(USE_SSL) 
int Send(const SOCK fd, SSL *ssl, const char *buf, const int len, bool sendall=true);
int Receive(const SOCK fd, SSL *ssl, char *buf, const int len, bool recvall=true);
#endif 
int Send(const SOCK fd, const char *buf, const int len, bool sendall=true);
int Receive(const SOCK fd, char *buf, const int len, bool recvall=true);



int SendTo(const SOCK mysocket, 
           const unsigned ip, const int port, 
           const char *buf, const int len, bool sendall=true);
int ReceiveFrom(const SOCK mysocket, 
                const unsigned ip, const int port, 
                char *buf, const int len, const bool recvall=true);
int SendTo(const SOCK mysocket, 
           const char *host_or_ip, const int port, 
           const char *buf, const int len, const bool sendall=true);
int ReceiveFrom(const SOCK mysocket, 
                const char *host_or_ip, const int port, 
                char *buf, const int len, const bool recvall=true);


int JoinMulticastGroup(const SOCK mysocket, const char *IP);
int JoinMulticastGroup(const SOCK mysocket, const unsigned adx);
int LeaveMulticastGroup(const SOCK mysocket, const char *IP);
int LeaveMulticastGroup(const SOCK mysocket, const unsigned adx);
int SetMulticastTimeToLive(const SOCK mysocket, const unsigned char ttl);



unsigned long GetRemoteSockAddress(SOCK sock);
int GetRemoteSockAddress(SOCK sock, struct sockaddr * addr);

unsigned long GetLocalSockAddress(SOCK sock);
int GetLocalSockAddress(SOCK sock, struct sockaddr * addr);
int GetLocalMacAddress(const string dev_name, char * buf);
int GetLocalMacAddress(const char * dev_name, char * buf);
unsigned GetMyIPAddress();
unsigned ToIPAddress(const char *hostname);
void     PrintIPAddress(const unsigned adx, FILE *out=stderr);
void     IPToHostname(const unsigned ip, char *name, const int namesize);
int      IsValidIPMulticastAddress(const unsigned ipadx);


int GetOpenTcpPorts(int ** ports);
int GetOpenUdpPorts(int ** ports);
int IsPortOpen(int port_num, int proto = TCP);


int SetSignalHandler(const int signum, void (*handler)(int), const bool oneshot=false);
int IgnoreSignal(const int signum);
int ListenToSignal(const int signum);

#if defined(USE_SSL) 
int GetLine(SOCK fd, SSL *ssl, string &s);
int PutLine(SOCK fd, SSL *ssl, const string &s);
#endif
 
int GetLine(SOCK fd, string &s);
int PutLine(SOCK fd, const string &s);


#endif
