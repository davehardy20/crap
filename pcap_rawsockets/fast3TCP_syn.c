#include <arpa/inet.h>
#include <ctype.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

struct PseudoHdr {
  unsigned long saddr;
  unsigned long daddr;
  char reserved;
  unsigned char protocol;
  unsigned short length;
};

#define PSEUDO sizeof(struct pseudohdr)
#define TCPHDR sizeof(struct tcphdr)
#define Z_NL 4294967295

unsigned short c_sum(unsigned short* data, int nbytes) {
  unsigned long sum = 0;

  for (; nbytes > 1; nbytes -= 2) {
    sum += *data++;
  }

  if (nbytes == 1) {
    sum += *(unsigned char*) data;
  }

  sum = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);

  return ~sum;
}

//unsigned char* buildTCPDatagram(struct sockaddr_in* src_sin, struct sockaddr_in* dst_sin, const unsigned char* msg, unsigned int msgSize);
unsigned int buildPseudoHdrBuffer(unsigned int src_addr, unsigned int dst_addr, unsigned int protocol, const unsigned char* hdrData, unsigned int hdrBytes,
const char* msgData, unsigned int msgBytes, unsigned short** buffer);

int main(int argc, char** argv) {
  const char* dest_ip = (argc > 1 ? argv[1] : "127.0.0.1"); // destination IP
  const unsigned short dest_port = (argc > 2 ? atoi(argv[1]) : 80); // destination port
  unsigned short i;

  // root-privileges needed for the following operation
  int s = socket(PF_INET, SOCK_RAW, IPPROTO_TCP);

  if (s <= 0) {
    perror("[open_sockraw] socket()");
    return 1;
  }

  int enable = 1;
  if (setsockopt(s, IPPROTO_IP, IP_HDRINCL, &enable, sizeof(enable)) < 0) {
    printf("Error: setsockopt() - Cannot set HDRINCL:\n");
    return 1;
  }

  // We no longer need root privileges
  setuid(getuid());

  // build up out source and destination sock addresses
  struct sockaddr_in src_sin;
  struct sockaddr_in dst_sin;

  src_sin.sin_family = PF_INET;
  src_sin.sin_addr.s_addr = inet_addr("127.0.0.1");

  dst_sin.sin_family = PF_INET;
  dst_sin.sin_addr.s_addr = inet_addr(dest_ip);

  const char* msg = "Hello World";
  const unsigned int msgSize = strlen(msg);
  const unsigned int datagramSize = sizeof(struct ip) + sizeof(struct tcphdr) + strlen(msg);
  unsigned char* datagram = calloc(1, datagramSize);
  const unsigned int hdrBytes = sizeof(struct tcphdr);

  // setup useful pointers to locations within the datagram
  struct ip* iph = (struct ip*) datagram;
  struct tcphdr* tcph = (struct tcphdr*)(datagram + sizeof(struct ip));
  unsigned char* data = datagram + sizeof(struct ip) + sizeof(struct tcphdr);

  // build IP header
  iph->ip_hl = hdrBytes >> 2;
  iph->ip_v = 4;
  iph->ip_tos = 0;
  iph->ip_len = htons(datagramSize);
  iph->ip_off = 0;
  iph->ip_ttl = 64;
  iph->ip_p = IPPROTO_TCP;
  iph->ip_sum = 0;
  iph->ip_src.s_addr = src_sin.sin_addr.s_addr;
  iph->ip_dst.s_addr = dst_sin.sin_addr.s_addr;

  // now we compute the checksum for the IP header (albeit this is optional)
  // iph->ip_sum = c_sum((unsigned short*) iph, sizeof(struct ip));

  // build TCP header
  tcph->ack_seq = htonl(0);
  tcph->res1 = 0;
  tcph->doff = hdrBytes >> 2;
  tcph->fin = 0;
  tcph->syn = 1;
  tcph->rst = 0;
  tcph->psh = 0;
  tcph->ack = 0;
  tcph->urg = 0;
  tcph->res2 = 0;
  tcph->window = htons(512);
  tcph->check = 0;
  tcph->urg_ptr = htons(0);
 

  struct PseudoHdr pseudoHdr;

  pseudoHdr.saddr = src_sin.sin_addr.s_addr; 
  pseudoHdr.daddr = dst_sin.sin_addr.s_addr;
  pseudoHdr.reserved = 0;
  pseudoHdr.protocol = IPPROTO_TCP;
  pseudoHdr.length = htons(hdrBytes + msgSize);

  unsigned int bufSize = sizeof(struct PseudoHdr) + hdrBytes + msgSize;

 for (i = 1025; i < 11025; i++) {

    src_sin.sin_port = htons(i);
    dst_sin.sin_port = htons(dest_port);
    tcph->source = src_sin.sin_port;
    tcph->dest = dst_sin.sin_port;

    iph->ip_id = htons(i);
    tcph->seq = htonl(i);
    tcph->check = 0;


    unsigned char* buf = calloc(1, bufSize); 
    int offset = 0;
    memcpy(buf + offset, &pseudoHdr, sizeof(struct PseudoHdr)); offset += sizeof(struct PseudoHdr);
    memcpy(buf + offset, tcph, hdrBytes); offset += hdrBytes;
    memcpy(buf + offset, msg, msgSize);

//    unsigned short* buffer = 0;
//    unsigned int bufferSize = buildPseudoHdrBuffer(src_sin.sin_addr.s_addr, dst_sin.sin_addr.s_addr, IPPROTO_TCP,
//    (const unsigned char*) tcph, sizeof(struct tcphdr), msg, msgSize, &buffer);

    tcph->check = c_sum((unsigned short*)buf, bufSize);
    free(buf);

    if (msgSize > 0) {
      memcpy(data, msg, msgSize);
    }

    if (sendto(s, datagram, datagramSize, 0, (struct sockaddr*) &dst_sin, sizeof(dst_sin)) < 0) {
      printf("Error with sendto() -- %s (%d)\n", strerror(errno), errno);
      break;
    }
  }
  free(datagram);

  return 0;
}


unsigned int buildPseudoHdrBuffer(unsigned int src_addr, unsigned int dst_addr, unsigned int protocol, const unsigned char* hdrData, unsigned int hdrBytes,
const char* msgData, unsigned int msgBytes, unsigned short** buffer) {

  struct PseudoHdr pseudoHdr;

  pseudoHdr.saddr = src_addr;
  pseudoHdr.daddr = dst_addr;
  pseudoHdr.reserved = 0;
  pseudoHdr.protocol = protocol;
  pseudoHdr.length = htons(hdrBytes + msgBytes);

  unsigned int bufSize = sizeof(struct PseudoHdr) + hdrBytes + msgBytes;
  unsigned char* buf = calloc(1, bufSize);
  int offset = 0;

  memcpy(buf + offset, &pseudoHdr, sizeof(struct PseudoHdr)); offset += sizeof(struct PseudoHdr);
  memcpy(buf + offset, hdrData, hdrBytes); offset += hdrBytes;
  memcpy(buf + offset, msgData, msgBytes);

  *buffer = (uint16_t*) buf;

  return bufSize;
}
