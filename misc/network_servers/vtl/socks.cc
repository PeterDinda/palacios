#include "socks.h"
#include <signal.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>



#if defined(__sparc__)
#include <sys/filio.h>
#endif

#if defined(__sparc__) || (defined(WIN32) && !defined(__CYGWIN__))
#define SOCKOPT_TYPE char *
#else
#define SOCKOPT_TYPE void *
#endif

#if defined(linux)
#define SOCKOPT_LEN_TYPE unsigned
#else
#define SOCKOPT_LEN_TYPE int
#endif



template <typename A, typename B> bool MIN(const A &a, const B &b) {
  return ((a < b) ? a : b);
}


int GetSockType(const SOCK fd) {
  int type;
  SOCKOPT_LEN_TYPE len = sizeof(int);
  
  if (getsockopt(fd, SOL_SOCKET, SO_TYPE, (SOCKOPT_TYPE)&type, &len)) {
    return -1;
  } else {
    return type;
  }
}


int IsSocket(const SOCK fd) {
  return (GetSockType(fd) >= 0);
}

int IsStreamSocket(const SOCK fd) {
  return (GetSockType(fd) == SOCK_STREAM);
}

int IsDatagramSocket(const SOCK fd) {
  return (GetSockType(fd) == SOCK_DGRAM);
}

#ifdef linux
int IsVirtualSocket(const SOCK fd) {
  struct stat mystat;
  fstat(fd, &mystat);

  return S_ISFIFO(mystat.st_mode);
}
#endif

int IsValidIPMulticastAddress(const unsigned adx) {
  
  //int x=(ntohl(port)>>24)&0xff;
  int x = (adx >> 24) & 0xff;

  if ((x < 224) || (x > 239)) {
    return 0;
  } else {
    return 1;
  }
}

void IPToHostname(const unsigned ip, char *name, const int namesize) { 
  struct in_addr ia;
  struct hostent * he;

  ia.s_addr = ip;

  he = gethostbyaddr((const char *)&ia, sizeof(ia), AF_INET);

  strncpy(name, he ? he->h_name : "UNKNOWN HOST", namesize - 1);
}


void PrintIPAddress(const unsigned adx, FILE *out) {
  fprintf(out,"%3d.%3d.%3d.%3d", 
	  (adx >> 24) & 0xff,
          (adx >> 16) & 0xff, 
	  (adx >> 8) & 0xff, 
	  (adx) & 0xff);
}

unsigned ToIPAddress(const char * hostname) {
  unsigned x;

  if ((x = inet_addr(hostname)) != INADDR_NONE) {
    return ntohl(x);
  } else {
    struct hostent * he;
    
    if ((he = gethostbyname(hostname)) == NULL) {
      return INADDR_NONE;
    } else {
      memcpy(&x, he->h_addr, 4);
      x = ntohl(x);
      return x;
    }
  }
}

unsigned long GetRemoteSockAddress(SOCK sock) {
  struct sockaddr_in remote_addr;
  unsigned long remote_ip;

  if (GetRemoteSockAddress(sock, (struct sockaddr *)&remote_addr) == -1) {
    return 0;
  }
  
  assert(remote_addr.sin_family == AF_INET);
  remote_ip = ntohl(remote_addr.sin_addr.s_addr);
  return remote_ip;
}

int GetRemoteSockAddress(SOCK sock, struct sockaddr * addr) {
  SOCKOPT_LEN_TYPE addr_len = sizeof(struct sockaddr);

  if (addr == NULL) {
    return -1;
  }
  
  if (getpeername(sock, addr, &addr_len) == -1) {
    return -1;
  }

  return 0;
}


unsigned long GetLocalSockAddress(SOCK sock) {
 struct sockaddr_in local_addr;
 unsigned long local_ip; 

 if (GetLocalSockAddress(sock, (struct sockaddr *)&local_addr) == -1) {
   return 0;
 }

 assert(local_addr.sin_family == AF_INET);
 local_ip = ntohl(local_addr.sin_addr.s_addr);
 return local_ip;
}

int GetLocalSockAddress(SOCK sock, struct sockaddr * addr) {
  SOCKOPT_LEN_TYPE addr_len = sizeof(struct sockaddr);

  if (addr == NULL) {
    return -1;
  }

  if (getsockname(sock, addr, &addr_len) == -1) {
    return -1;
  }

  return 0;
}


int GetLocalMacAddress(const string dev_name, char * buf) {
  return GetLocalMacAddress(dev_name.c_str(), buf);
}

int GetLocalMacAddress(const char * dev_name, char * buf) {
#ifdef linux
  struct ifreq mac_req;
  SOCK fd = socket(AF_INET, SOCK_STREAM, 0);

  snprintf(mac_req.ifr_name, IF_NAMESIZE, "%s", dev_name);

  if (ioctl(fd, SIOCGIFHWADDR, &mac_req) < 0) {
    cerr << "Error Could not get the local MAC Address" << endl;
    perror("perror: ");
    return -1;
  }
  
  memcpy(buf, mac_req.ifr_hwaddr.sa_data, 6);
#elif WIN32
  char temp_dev_name[256];
  PIP_ADAPTER_INFO temp_adapter;
  IP_ADAPTER_INFO AdapterInfo[16];       // Allocate information
  // for up to 16 NICs
  DWORD dwBufLen = sizeof(AdapterInfo);  // Save memory size of buffer
  
  DWORD dwStatus = GetAdaptersInfo(AdapterInfo,                 // [out] buffer to receive data
				   &dwBufLen);                  // [in] size of receive data buffer
  
  assert(dwStatus == ERROR_SUCCESS);  // Verify return value 
  
  temp_adapter = AdapterInfo;
  
  while(temp_adapter) {
    sprintf(temp_dev_name, "\\Device\\NPF_%s", temp_adapter->AdapterName);
    
    if (strcmp(dev_name, temp_dev_name) == 0) {
      memcpy(buf, temp_adapter->Address, 6);
      break;
    }	
    temp_adapter = temp_adapter->Next;
  } 
#endif
  return 0;
}


int GetOpenTcpPorts(int ** ports) {
#ifdef linux
  int proc_fd;
  int num_ports = 0;
  unsigned long rxq, txq, time_len, retr, inode, local_addr, rem_addr;
  int d, local_port, rem_port, scan_num, timer_run, uid, timeout, state;
  string proc_str;
  char more[512];


  enum {
    TCP_ESTABLISHED = 1,
    TCP_SYN_SENT,
    TCP_SYN_RECV,
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_TIME_WAIT,
    TCP_CLOSE,
    TCP_CLOSE_WAIT,
    TCP_LAST_ACK,
    TCP_LISTEN,
    TCP_CLOSING                 /* now a valid state */
  };

  /* 
     We do this because we use realloc, 
     so the first ptr-value must be null or we will realloc on some randome address
  */
  *ports = NULL;

  proc_fd = open("/proc/net/tcp", O_RDONLY);
  if (proc_fd == -1) {

    return -1;
  }
  /* This supports IPv6 which we will ignore for now...
    num = sscanf(line,
    "%d: %64[0-9A-Fa-f]:%X %64[0-9A-Fa-f]:%X %X %lX:%lX %X:%lX %lX %d %d %ld %512s\n",
    &d, local_addr, &local_port, rem_addr, &rem_port, &state,
    &txq, &rxq, &timer_run, &time_len, &retr, &uid, &timeout, &inode, more);
  */
  GetLine(proc_fd, proc_str);
 
  while (GetLine(proc_fd, proc_str)) {
    
    // We pretty much stole this from netstat.c in the net-tools package
    scan_num = sscanf(proc_str.c_str(),
		      "%d: %lX:%X %lX:%X %X %lX:%lX %X:%lX %lX %d %d %ld %512s\n",
		      &d, &local_addr, &local_port, &rem_addr, &rem_port, &state,
		      &txq, &rxq, &timer_run, &time_len, &retr, &uid, &timeout, &inode, more);
    
    if (state == TCP_LISTEN) {
      //printf("%s (%d)\n", ip_to_string(ntohl(local_addr)), local_port);
      *ports = (int *)realloc((*ports), sizeof(int) * (num_ports + 1));
 
     (*ports)[num_ports] = local_port;

      num_ports++;
    }
  }

  close(proc_fd);
  return num_ports;
#elif WIN32
  LPVOID error_msg;
  DWORD table_size = 0;
  PMIB_TCPTABLE tcp_table;
  DWORD dwError;
  
  // WINXP and higher
  // 	AllocateAndGetTcpExTableFromStack(&tcp_table, TRUE, GetProcessHeap(), 2, 2);
  
  dwError = GetTcpTable(NULL, &table_size, TRUE);
  if (dwError != ERROR_INSUFFICIENT_BUFFER) {
    cerr << "Error: " <<dwError <<   endl;
    FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		   NULL, dwError, 
		   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		   (LPTSTR) &error_msg, 0, NULL );
    cerr << (char *)error_msg << endl;
    return -1;
  }
  
  tcp_table = (PMIB_TCPTABLE)malloc(table_size);
   
  if (GetTcpTable(tcp_table, &table_size, TRUE)) {
    return -1;
  }
  
  *ports = (int *)malloc(sizeof(int) * tcp_table->dwNumEntries);
  
  for (unsigned int i = 0; i < tcp_table->dwNumEntries; i++) {
    //cerr << htons((WORD)tcp_table->table[i].dwLocalPort) << endl;
    (*ports)[i] = ntohs((WORD)tcp_table->table[i].dwLocalPort);
  }
  
  return tcp_table->dwNumEntries;
#endif
}

int GetOpenUdpPorts(int ** ports) {
#ifdef linux
  int proc_fd;
  int num_ports = 0;
  char more[512];
  int local_port, rem_port, d, state, timer_run, uid, timeout;
  unsigned long local_addr, rem_addr;
  unsigned long rxq, txq, time_len, retr, inode;
  int scan_num;
  string proc_str;
  /* 
     We do this because we use realloc, 
     so the first ptr-value must be null or we will realloc on some randome address
  */
  *ports = NULL;

  proc_fd = open("/proc/net/udp", O_RDONLY);
  if (proc_fd == -1) {

    return -1;
  }

  GetLine(proc_fd, proc_str);
 
  while (GetLine(proc_fd, proc_str)) {
    
    // We pretty much stole this from netstat.c in the net-tools package
    scan_num = sscanf(proc_str.c_str(),
                 "%d: %lX:%X %lX:%X %X %lX:%lX %X:%lX %lX %d %d %ld %512s\n",
                 &d, &local_addr, &local_port,
                 &rem_addr, &rem_port, &state,
		 &txq, &rxq, &timer_run, &time_len, &retr, &uid, &timeout, &inode, more);
    
    

    //printf("%s (%d)\n", ip_to_string(ntohl(local_addr)), local_port);
    if (state == 0x07) {
      *ports = (int *)realloc((*ports), sizeof(int) * (num_ports + 1));
      
      (*ports)[num_ports] = local_port;
      
      num_ports++;
    }
  }

  close(proc_fd);
  return num_ports;
#elif WIN32
  LPVOID error_msg;
  DWORD table_size = 0;
  PMIB_UDPTABLE udp_table;
  DWORD dwError;
  
  // WINXP and higher
  // 	AllocateAndGetTcpExTableFromStack(&tcp_table, TRUE, GetProcessHeap(), 2, 2);
  
  dwError = GetUdpTable(NULL, &table_size, TRUE);
  if (dwError != ERROR_INSUFFICIENT_BUFFER) {
    cerr << "Error: " <<dwError <<   endl;
    FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		   NULL, dwError, 
		   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		   (LPTSTR) &error_msg, 0, NULL );
    cerr << (char *)error_msg << endl;
    return -1;
  }
  
  udp_table = (PMIB_UDPTABLE)malloc(table_size);
   
  if (GetUdpTable(udp_table, &table_size, TRUE)) {
    return -1;
  }
  
  *ports = (int *)malloc(sizeof(int) * udp_table->dwNumEntries);
  
  for (unsigned int i = 0; i < udp_table->dwNumEntries; i++) {
    //cerr << htons((WORD)udp_table->table[i].dwLocalPort) << endl;
    (*ports)[i] = ntohs((WORD)udp_table->table[i].dwLocalPort);
  }
  
  return udp_table->dwNumEntries;
#endif
}



#define WELL_KNOWN_HOST ((char*)"www.cnn.com")
#define WELL_KNOWN_PORT 80

unsigned GetMyIPAddress() {
  static unsigned adx = 0;
  //static bool setup = false;
  char * host;
  short port;
  SOCK fd;

  host = getenv("RPS_WELL_KNOWN_HOST") ? getenv("RPS_WELL_KNOWN_HOST") : WELL_KNOWN_HOST;
  port = getenv("RPS_WELL_KNOWN_PORT") ? atoi(getenv("RPS_WELL_KNOWN_PORT")) : WELL_KNOWN_PORT;

  //  if (setup) {
  //  return adx;
  //  } else {
    // Connect to a well known machine and check out our socket's address
    if ((fd = CreateAndSetupTcpSocket()) == -1) {
      return adx;
    } else { 
      if (ConnectToHost(fd, host, port) == -1) {
        CLOSE(fd);
        return adx;
      }

      adx = GetLocalSockAddress(fd);
      
      CLOSE(fd);  
      return adx;
    }
    //}
}

SOCK CreateAndSetupUdpSocket(const int bufsize, const bool nonblocking) {
  SOCK mysocket;
  int val = 0;

  // create socket for connections
  if ((mysocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    return -1;
  }
    
  // set reuseaddr to avoid binding problems
  if (setsockopt(mysocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&val, sizeof(int))) {
    return -1;
  }
  
  val = bufsize;

  if (setsockopt(mysocket, SOL_SOCKET, SO_SNDBUF,
                 (const char*) &val, sizeof(val)) < 0) {
    CLOSE(mysocket);
    return -1;
  }

  val = bufsize;

  if (setsockopt(mysocket, SOL_SOCKET, SO_RCVBUF,
                 (const char*)&val, sizeof(val)) < 0) {
    CLOSE(mysocket);
    return -1;
  }

  if (nonblocking) {
    val = 1;
    if (IOCTL(mysocket, FIONBIO, &val)) {
      CLOSE(mysocket);
      return -1;
    }
  }

  return mysocket;
}


SOCK CreateAndSetupTcpSocket(const int bufsize, const bool nodelay, const bool nonblocking) {
  SOCK mysocket;
  int val = 1;

  // create socket for connections
  if ((mysocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    return -1;
  }
    
  // set reuseaddr to avoid binding problems
  if (setsockopt(mysocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&val, sizeof(int))) {
    return -1;
  }
  
  // Set nodelay so that our messages get
  if (nodelay) {
    val = 1;
    if (setsockopt(mysocket, IPPROTO_TCP, TCP_NODELAY, (const char *)&val, sizeof(int))) {
      CLOSE(mysocket);
      return -1;
    }
  }

  val = bufsize;

  if (setsockopt(mysocket, SOL_SOCKET, SO_SNDBUF,
                 (const char*) &val, sizeof(val)) < 0) {
    CLOSE(mysocket);
    return -1;
  }

  val = bufsize;

  if (setsockopt(mysocket, SOL_SOCKET, SO_RCVBUF,
                 (const char*)&val, sizeof(val)) < 0) {
    CLOSE(mysocket);
    return -1;
  }

  if (nonblocking) {
    val = 1;
    if (IOCTL(mysocket, FIONBIO, &val)) {
      CLOSE(mysocket);
      return -1;
    }
  }

  return mysocket;
}


SOCK CreateAndSetupUnixDomainSocket(const int bufsize, const bool nonblocking) {
  SOCK mysocket;
  int val;

  // create socket for connections
  if ((mysocket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    return -1;
  }
    
  // set reuseaddr to avoid binding problems
  if (setsockopt(mysocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&val, sizeof(int))) {
    return -1;
  }
  
  val = bufsize;

  if (setsockopt(mysocket, SOL_SOCKET, SO_SNDBUF,
                 (const char*)&val, sizeof(val)) < 0) {
    CLOSE(mysocket);
    return -1;
  }

  val = bufsize;

  if (setsockopt(mysocket, SOL_SOCKET, SO_RCVBUF,
                 (const char*)&val, sizeof(val)) < 0) {
    CLOSE(mysocket);
    return -1;
  }

  if (nonblocking) {
    val = 1;
    if (IOCTL(mysocket, FIONBIO, &val)) {
      CLOSE(mysocket);
      return -1;
    }
  }

  return mysocket;
}


int SetNoDelaySocket(const SOCK fd, const bool nodelay) {
  int val = nodelay == true;

  // Set nodelay so that our messages get
  return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&val, sizeof(int));
}


int BindSocket(const SOCK mysocket, const unsigned adx, const int myport) {  
  struct sockaddr_in my_sa;

  memset(&my_sa, 0, sizeof(my_sa));
  my_sa.sin_port = htons(myport);
  my_sa.sin_addr.s_addr = htonl(adx);
  my_sa.sin_family = AF_INET;

  if (bind(mysocket, (struct sockaddr *)&my_sa, sizeof(my_sa))) {
    return -1;
  }
  return 0;
}

int BindSocket(const SOCK mysocket, const int myport) {
  return BindSocket(mysocket, (unsigned)INADDR_ANY, myport);
}  

int BindSocket(const SOCK mysocket, const char *host_or_ip, const int myport) {
  return BindSocket(mysocket, ToIPAddress(host_or_ip), myport);
}

int BindSocket(const SOCK mysocket, const char *pathname) {
#if defined(WIN32) && !defined(__CYGWIN__)
  return -1;
#else 
  struct sockaddr_un my_sa;
  int len;
 
  memset(&my_sa, 0, sizeof(my_sa));
  my_sa.sun_family = AF_UNIX;
  strcpy(my_sa.sun_path, pathname);
  len = strlen(my_sa.sun_path) + sizeof(my_sa.sun_family);

  if (bind(mysocket, (struct sockaddr *)&my_sa,len)) {
    return -1;
  }
  return 0;
#endif
}  


int ListenSocket(const SOCK mysocket, const int maxc) {
  int maxcon = MIN(maxc, SOMAXCONN);
  return listen(mysocket, maxcon);
}

int ConnectToHost(const SOCK mysocket, const int hostip, const int port) {
  struct sockaddr_in sa;

  memset(&sa, 0, sizeof(sa));
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = htonl(hostip);
  sa.sin_family = AF_INET;

  return connect(mysocket, (struct sockaddr *)&sa, sizeof(sa));
}
  

int ConnectToHost(const SOCK mysocket, const char *host, const  int port) {
  return ConnectToHost(mysocket, ToIPAddress(host), port);
}


int ConnectToPath(const SOCK mysocket, const char *pathname) {
#if defined(WIN32) && !defined(__CYGWIN__)
  return -1;
#else 
  struct sockaddr_un my_sa;
  int len;

  memset(&my_sa, 0, sizeof(my_sa));
  my_sa.sun_family = AF_UNIX;
  strcpy(my_sa.sun_path, pathname);
  len = strlen(my_sa.sun_path) + sizeof(my_sa.sun_family);

  if (connect(mysocket, (struct sockaddr *)&my_sa,len)) {
    return -1;
  }
  return 0;
#endif
}
  


int JoinMulticastGroup(const SOCK mysocket, const unsigned adx) {
  if (!IsValidIPMulticastAddress(adx)) {
    return -1;
  }

  struct ip_mreq req;

  memset(&req, 0, sizeof(req));
  
  req.imr_multiaddr.s_addr = htonl(adx);
  req.imr_interface.s_addr = htonl(INADDR_ANY);
  
  if (setsockopt(mysocket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                 (const char*)&req, sizeof(req)) < 0) {
    return -1;
  }
  return 0;
}

int JoinMulticastGroup(const SOCK mysocket, const char *IP) {
  return JoinMulticastGroup(mysocket, ToIPAddress(IP));
}


int LeaveMulticastGroup(const SOCK mysocket, const unsigned adx) {
  if (!IsValidIPMulticastAddress(adx)) {
    return -1;
  }

  struct ip_mreq req;

  memset(&req, 0, sizeof(req));
  
  req.imr_multiaddr.s_addr = htonl(adx);
  req.imr_interface.s_addr = htonl(INADDR_ANY);
  
  if (setsockopt(mysocket, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                 (const char *) &req, sizeof(req)) < 0) {
    return -1;
  }

  return 0;
}


int LeaveMulticastGroup(const SOCK mysocket, const char *IP) {
  return LeaveMulticastGroup(mysocket, ToIPAddress(IP));
}



int SetMulticastTimeToLive(const SOCK mysocket, const unsigned char ttl) {
  if (setsockopt(mysocket, IPPROTO_IP, IP_MULTICAST_TTL,
                 (SOCKOPT_TYPE)&ttl, (SOCKOPT_LEN_TYPE)sizeof(ttl)) < 0) {
    return -1;
  }
  return 0;
}

int SendTo(const SOCK mysocket, 
           const unsigned ip, const int port, 
           const char *buf, const int len, const bool sendall) {
  struct sockaddr_in sa;
  
  memset(&sa, 0, sizeof(sockaddr_in));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(ip);
  sa.sin_port = htons(port);
  
  if (!sendall) {
    return sendto(mysocket, buf, len, 0, (struct sockaddr *)&sa, sizeof(sockaddr_in));
  } else {
    int left = len;
    int sent;
    while (left > 0) {
      sent = sendto(mysocket, &(buf[len - left]), left, 0, (struct sockaddr *)&sa, sizeof(sockaddr_in));
      if (sent < 0) {
        if (errno == EINTR) {
          continue;
        } else {
          return -1;
        }
      } else {
        left -= sent;
      }
    }
    return len;
  }
}


int ReceiveFrom(const SOCK mysocket, 
                const unsigned ip, const int port, 
                char *buf, const int len,
                const bool recvall)
{
  struct sockaddr_in sa;
  SOCKOPT_LEN_TYPE size = sizeof(sockaddr_in);
  
  memset(&sa, 0, sizeof(sockaddr_in));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(ip);
  sa.sin_port = htons(port);
  
  if (!recvall) {
    return recvfrom(mysocket, buf, len, 0, (struct sockaddr *)&sa, &size);
  } else {
    int left = len;	
    int received;
    while (left > 0) {
      received = recvfrom(mysocket, &(buf[len - left]), left, 0, (struct sockaddr *)&sa, &size);
      if (received < 0) {
        if (errno == EINTR) {
          continue;
        } else {
          return -1;
        }
      } else if (received == 0) {
        break;
      } else {	
        left -= received;
      }
    }
    return len - left;
  }
}

int SendTo(const SOCK mysocket, 
           const char *host_or_ip, const int port, 
           const char *buf, const int len, const bool sendall) {
  return SendTo(mysocket, ToIPAddress(host_or_ip), port, buf, len, sendall);
}

int ReceiveFrom(const SOCK mysocket, 
                const char *host_or_ip, const int port, 
                char *buf, const int len, const bool recvall) {
  return ReceiveFrom(mysocket, ToIPAddress(host_or_ip), port, buf, len, recvall);
}


#if defined(USE_SSL)

int Send(SOCK fd, SSL *ssl, const char *buf, const int len, const bool sendall) {
  if (!sendall) {
    if (ssl != NULL) {
      return SSL_write(ssl, buf, len);
    } else {
      return write(fd, buf, len);
    }
  } else {
    int left = len;
    int sent;
    while (left > 0) {
      if (ssl != NULL) {
	sent = SSL_write(ssl, &(buf[len - left]), left);
      } else {
	sent = write(fd, &(buf[len - left]), left);
      }

      if (sent < 0) {
	if (errno == EINTR) {
	  continue;
	} else {
	  return -1;
	}
      } else if (sent == 0) {
	break;
      } else {
	left -= sent;
      }
    }
    return len - left;
  }
}

int Receive(SOCK fd, SSL *ssl, char *buf, const int len, const bool recvall) {
  if (!recvall) {
    if (ssl != NULL) {
      return SSL_read(ssl, buf, len);
    } else {
      return read(fd, buf, len);
    }
  } else {
    int left = len;	
    int received;

    while (left > 0) {
      if (ssl != NULL) {
	received = SSL_read(ssl, &(buf[len - left]), left);
      } else {
	received = read(fd, &(buf[len - left]), left);
      }
      
      if (received < 0) {
	if (errno == EINTR) {
	  continue;
	} else {
	  return -1;
	}
      } else if (received == 0) {
	return 0;
      } else {	
	left -= received;
      }
    }
    return len - left;
  }
}


int GetLine(SOCK fd, SSL *ssl, string &s) {
  char c;
  s.erase(s.begin(), s.end());
  while (1) { 
    int rc = Receive(fd, ssl, &c, 1, true);
    if (rc < 0) { 
      return rc;
    } 
    if ((rc == 0) || (c == '\n')) {
      return s.size();
    }
    s += c;
  }
}


int PutLine(SOCK fd, SSL *ssl, const string &s) {
  string s2 = s;
  s2 += '\n';
  return (Send(fd, ssl, s2.c_str(), s2.size(), true) - 1);
}
#endif

int Send(SOCK fd,  const char *buf, const int len, const bool sendall) {
  if (!sendall) {
    return WRITE(fd, buf, len);
  } else {
    int left = len;
    int sent;
    while (left > 0) {
      sent = WRITE(fd, &(buf[len - left]), left);

      if (sent < 0) {
	if (errno == EINTR) {
	  continue;
	} else {
	  return -1;
	}
      } else if (sent == 0) {
	break;
      } else {
	left -= sent;
      }
    }
    return len - left;
  }
}

int Receive(SOCK fd, char *buf, const int len, const bool recvall) {
  if (!recvall) {
    return READ(fd, buf, len);
  } else {
    int left = len;	
    int received;

    while (left > 0) {
      received = READ(fd, &(buf[len - left]), left);
      
      if (received < 0) {
	if (errno == EINTR) {
	  continue;
	} else {
	  return -1;
	}
      } else if (received == 0) {
	return 0;
      } else {	
	left -= received;
      }
    }
    return len - left;
  }
}


int GetLine(SOCK fd, string &s) {
  char c;
  s.erase(s.begin(), s.end());

  while (1) { 
    int rc = Receive(fd, &c, 1, true);

    if (rc < 0) { 
      return rc;
    }

    if ((rc == 0) || (c == '\n')) {
      return s.size();
    }

    s += c;
  }
}


int PutLine(SOCK fd, const string &s) {
  string s2 = s;
  s2 += '\n';
  return (Send(fd, s2.c_str(), s2.size(), true) - 1);
}




int SetSignalHandler(const int signum, void (*handler)(int), const bool oneshot)
{
#if defined(WIN32) || defined(CYGWIN) // cygwin does not appear to have sigaction, so...
  signal(signum,handler);  //notice that this is oneshot
  return 0;
#else
  struct sigaction sa;

#if defined(__sparc__)
  sa.sa_handler= (void (*)(...)) handler;  // SUN FREAKS
#else
  sa.sa_handler=handler;
#endif  

  sigemptyset(&(sa.sa_mask));
#if defined(linux) 
#define SIGHAND_ONESHOT SA_ONESHOT
#endif
#if defined(__osf__) || defined(__FreeBSD__) || defined(__sparc__)
#define SIGHAND_ONESHOT SA_RESETHAND
#endif

  sa.sa_flags = ((oneshot == true) ? SIGHAND_ONESHOT : 0);
#if defined(linux)
  sa.sa_restorer = 0;
#endif

  return sigaction(signum, &sa, 0);
#endif
}


int IgnoreSignal(const int signum)
{
  return SetSignalHandler(signum, SIG_IGN);
}

int ListenToSignal(const int signum)
{
  return SetSignalHandler(signum, SIG_DFL);
}

#if defined(WIN32) && !defined(__CYGWIN__)

class SockInit {
public:
  SockInit() { 
    WSADATA foo;
    WSAStartup(MAKEWORD(2,0),&foo);
  }
  ~SockInit() { 
    if (WSAIsBlocking()) {
      WSACancelBlockingCall();
    }
    WSACleanup();
  }
};

SockInit thesockinit; // constructor should get called on startup.
#endif


