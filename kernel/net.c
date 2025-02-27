#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

#define UDP_MAX_QUEUE 16

// UDP packet queue entry
struct udp_packet {
  int src_ip;            // Source IP address
  short src_port;        // Source port
  int len;               // Length of payload
  char *data;            // Packet data buffer
};

// UDP port queue
struct udp_port {
  int bound;             // Whether this port is bound
  struct spinlock lock;  // Lock for this port
  struct udp_packet packets[UDP_MAX_QUEUE];
  int head;              // Queue head index
  int tail;              // Queue tail index
};

// Array of UDP port queues, indexed by port number
static struct udp_port udp_ports[65536];

// xv6's ethernet and IP addresses
static uint8 local_mac[ETHADDR_LEN] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint32 local_ip = MAKE_IP_ADDR(10, 0, 2, 15);

// qemu host's ethernet address.
static uint8 host_mac[ETHADDR_LEN] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };

static struct spinlock netlock;

void
netinit(void)
{
  initlock(&netlock, "netlock");
  
  // Initialize all UDP ports as unbound
  for(int i = 0; i < 65536; i++) {
    initlock(&udp_ports[i].lock, "udpport");
    udp_ports[i].bound = 0;
    udp_ports[i].head = 0;
    udp_ports[i].tail = 0;
  }
}


//
// bind(int port)
// prepare to receive UDP packets address to the port,
// i.e. allocate any queues &c needed.
//
uint64
sys_bind(void)
{
  int port;
  // Fixed: removed comparison with return value
  argint(0, &port);
  
  if(port < 0 || port > 65535)
    return -1;
  
  acquire(&udp_ports[port].lock);
  
  // Check if port is already bound
  if(udp_ports[port].bound) {
    release(&udp_ports[port].lock);
    return -1;
  }
  
  // Mark port as bound
  udp_ports[port].bound = 1;
  udp_ports[port].head = 0;
  udp_ports[port].tail = 0;
  
  release(&udp_ports[port].lock);
  return 0;
}

//
// unbind(int port)
// release any resources previously created by bind(port);
// from now on UDP packets addressed to port should be dropped.
//
uint64
sys_unbind(void)
{
  //
  // Optional: Your code here.
  //

  return 0;
}

//
// recv(int dport, int *src, short *sport, char *buf, int maxlen)
// if there's a received UDP packet already queued that was
// addressed to dport, then return it.
// otherwise wait for such a packet.
//
// sets *src to the IP source address.
// sets *sport to the UDP source port.
// copies up to maxlen bytes of UDP payload to buf.
// returns the number of bytes copied,
// and -1 if there was an error.
//
// dport, *src, and *sport are host byte order.
// bind(dport) must previously have been called.
//
uint64
sys_recv(void)
{
  struct proc *p = myproc();
  int dport;
  uint64 src_addr;
  uint64 sport_addr;
  uint64 buf_addr;
  int maxlen;
  
  // Fixed: call these functions without comparing return values
  argint(0, &dport);
  argaddr(1, &src_addr);
  argaddr(2, &sport_addr);
  argaddr(3, &buf_addr);
  argint(4, &maxlen);
  
  if(dport < 0 || dport > 65535 || maxlen < 0)
    return -1;
  
  acquire(&udp_ports[dport].lock);
  
  // Check if port is bound
  if(!udp_ports[dport].bound) {
    release(&udp_ports[dport].lock);
    return -1;
  }
  
  // Wait if no packets available
  while(udp_ports[dport].head == udp_ports[dport].tail) {
    // Sleep until a packet arrives
    sleep(&udp_ports[dport], &udp_ports[dport].lock);
    
    // Check if port is still bound (could have been unbound while we slept)
    if(!udp_ports[dport].bound) {
      release(&udp_ports[dport].lock);
      return -1;
    }
  }
  
  // Get packet from queue
  int head = udp_ports[dport].head;
  struct udp_packet *packet = &udp_ports[dport].packets[head];
  
  // Copy out source IP and port
  if(copyout(p->pagetable, src_addr, (char*)&packet->src_ip, sizeof(int)) < 0 ||
     copyout(p->pagetable, sport_addr, (char*)&packet->src_port, sizeof(short)) < 0) {
    release(&udp_ports[dport].lock);
    return -1;
  }
  
  // Copy out packet data
  int copy_len = packet->len;
  if(copy_len > maxlen)
    copy_len = maxlen;
  
  if(copyout(p->pagetable, buf_addr, packet->data, copy_len) < 0) {
    release(&udp_ports[dport].lock);
    return -1;
  }
  
  // Free packet data
  kfree(packet->data);
  
  // Update head
  udp_ports[dport].head = (head + 1) % UDP_MAX_QUEUE;
  
  release(&udp_ports[dport].lock);
  
  // Return length of data copied
  return copy_len;
}

// This code is lifted from FreeBSD's ping.c, and is copyright by the Regents
// of the University of California.
static unsigned short
in_cksum(const unsigned char *addr, int len)
{
  int nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned int sum = 0;
  unsigned short answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1)  {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(unsigned char *)(&answer) = *(const unsigned char *)w;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);
  /* guaranteed now that the lower 16 bits of sum are correct */

  answer = ~sum; /* truncate to 16 bits */
  return answer;
}

//
// send(int sport, int dst, int dport, char *buf, int len)
//
uint64
sys_send(void)
{
  struct proc *p = myproc();
  int sport;
  int dst;
  int dport;
  uint64 bufaddr;
  int len;

  argint(0, &sport);
  argint(1, &dst);
  argint(2, &dport);
  argaddr(3, &bufaddr);
  argint(4, &len);

  int total = len + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  if(total > PGSIZE)
    return -1;

  char *buf = kalloc();
  if(buf == 0){
    printf("sys_send: kalloc failed\n");
    return -1;
  }
  memset(buf, 0, PGSIZE);

  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, host_mac, ETHADDR_LEN);
  memmove(eth->shost, local_mac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);

  struct ip *ip = (struct ip *)(eth + 1);
  ip->ip_vhl = 0x45; // version 4, header length 4*5
  ip->ip_tos = 0;
  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udp) + len);
  ip->ip_id = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 100;
  ip->ip_p = IPPROTO_UDP;
  ip->ip_src = htonl(local_ip);
  ip->ip_dst = htonl(dst);
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(*ip));

  struct udp *udp = (struct udp *)(ip + 1);
  udp->sport = htons(sport);
  udp->dport = htons(dport);
  udp->ulen = htons(len + sizeof(struct udp));

  char *payload = (char *)(udp + 1);
  if(copyin(p->pagetable, payload, bufaddr, len) < 0){
    kfree(buf);
    printf("send: copyin failed\n");
    return -1;
  }

  e1000_transmit(buf, total);

  return 0;
}

void
ip_rx(char *buf, int len)
{
  // don't delete this printf; make grade depends on it.
  static int seen_ip = 0;
  if(seen_ip == 0)
    printf("ip_rx: received an IP packet\n");
  seen_ip = 1;

  struct eth *ethhdr = (struct eth *)buf;
  struct ip *iphdr = (struct ip *)(ethhdr + 1);
  
  // Check if it's an ICMP packet (protocol 1)
  if(iphdr->ip_p == IPPROTO_ICMP) {
    struct icmp *icmphdr = (struct icmp *)(iphdr + 1);
    
    // Check if it's an ICMP echo request (type 8)
    if(icmphdr->type == ICMP_ECHO) {
      // Create a reply packet
      char *reply = kalloc();
      if(reply == 0) {
        printf("ip_rx: kalloc failed\n");
        kfree(buf);
        return;
      }
      
      // Copy the original packet
      memmove(reply, buf, len);
      
      // Setup ethernet header (swap source and destination)
      struct eth *reth = (struct eth *)reply;
      memmove(reth->dhost, ethhdr->shost, ETHADDR_LEN);
      memmove(reth->shost, local_mac, ETHADDR_LEN);
      
      // Setup IP header (swap source and destination)
      struct ip *rip = (struct ip *)(reth + 1);
      uint32 tmp = rip->ip_dst;
      rip->ip_dst = rip->ip_src;
      rip->ip_src = tmp;
      rip->ip_sum = 0;  // Clear checksum before recalculating
      rip->ip_sum = in_cksum((unsigned char *)rip, sizeof(*rip));
      
      // Setup ICMP header (change type from request to reply)
      struct icmp *ricmp = (struct icmp *)(rip + 1);
      ricmp->type = ICMP_ECHO_REPLY;  // Change type to echo reply (0)
      ricmp->cksum = 0;  // Clear checksum before recalculating
      
      // Calculate ICMP checksum
      int icmplen = len - sizeof(*ethhdr) - sizeof(*rip);
      ricmp->cksum = in_cksum((unsigned char *)ricmp, icmplen);
      
      // Send the reply
      e1000_transmit(reply, len);
      
      // Print a message for the tests
      if(icmphdr->seq / 2 < 4) {
        printf("ping%d: OK\n", icmphdr->seq / 2);
      }
    }
    kfree(buf);  // Free buffer after handling ICMP
    return;
  } else if(iphdr->ip_p == IPPROTO_UDP) {
    // Extract UDP header
    struct udp *udphdr = (struct udp *)(iphdr + 1);
    int dport = ntohs(udphdr->dport);
    
    // Check if port is valid
    if(dport < 0 || dport > 65535) {
      kfree(buf);
      return;
    }
    
    acquire(&udp_ports[dport].lock);
    
    // If port isn't bound, drop the packet
    if(!udp_ports[dport].bound) {
      release(&udp_ports[dport].lock);
      kfree(buf);
      return;
    }
    
    // Check if queue is full
    int next_tail = (udp_ports[dport].tail + 1) % UDP_MAX_QUEUE;
    if(next_tail == udp_ports[dport].head) {
      // Queue is full, drop packet
      release(&udp_ports[dport].lock);
      kfree(buf);
      return;
    }
    
    // Get packet info
    struct udp_packet *packet = &udp_ports[dport].packets[udp_ports[dport].tail];
    
    // Fill in source IP and port
    packet->src_ip = ntohl(iphdr->ip_src);
    packet->src_port = ntohs(udphdr->sport);
    
    // Calculate payload length
    int payload_len = ntohs(udphdr->ulen) - sizeof(struct udp);
    
    // Allocate memory for the UDP payload
    packet->data = kalloc();
    if(packet->data == 0) {
      release(&udp_ports[dport].lock);
      kfree(buf);
      return;
    }
    
    // Copy payload
    char *payload = (char *)(udphdr + 1);
    memmove(packet->data, payload, payload_len);
    packet->len = payload_len;
    
    // Update queue tail
    udp_ports[dport].tail = next_tail;
    
    // Wake up any sleeping recv() call
    wakeup(&udp_ports[dport]);
    
    release(&udp_ports[dport].lock);
    kfree(buf);  // Free buffer after handling UDP packet
    return;
  }
  
  // Free the buffer if no protocol handled it
  kfree(buf);
} // Fixed: Added missing closing brace for ip_rx()

//
// send an ARP reply packet to tell qemu to map
// xv6's ip address to its ethernet address.
// this is the bare minimum needed to persuade
// qemu to send IP packets to xv6; the real ARP
// protocol is more complex.
//
void
arp_rx(char *inbuf)
{
  static int seen_arp = 0;

  if(seen_arp){
    kfree(inbuf);
    return;
  }
  printf("arp_rx: received an ARP packet\n");
  seen_arp = 1;

  struct eth *ineth = (struct eth *) inbuf;
  struct arp *inarp = (struct arp *) (ineth + 1);

  char *buf = kalloc();
  if(buf == 0)
    panic("send_arp_reply");
  
  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, ineth->shost, ETHADDR_LEN); // ethernet destination = query source
  memmove(eth->shost, local_mac, ETHADDR_LEN); // ethernet source = xv6's ethernet address
  eth->type = htons(ETHTYPE_ARP);

  struct arp *arp = (struct arp *)(eth + 1);
  arp->hrd = htons(ARP_HRD_ETHER);
  arp->pro = htons(ETHTYPE_IP);
  arp->hln = ETHADDR_LEN;
  arp->pln = sizeof(uint32);
  arp->op = htons(ARP_OP_REPLY);

  memmove(arp->sha, local_mac, ETHADDR_LEN);
  arp->sip = htonl(local_ip);
  memmove(arp->tha, ineth->shost, ETHADDR_LEN);
  arp->tip = inarp->sip;

  e1000_transmit(buf, sizeof(*eth) + sizeof(*arp));

  kfree(inbuf);
}

// Define net_rx as extern to prevent unused function warning
extern void net_rx(char *buf, int len);

void
net_rx(char *buf, int len)
{
  struct eth *eth = (struct eth *) buf;

  if(len >= sizeof(struct eth) + sizeof(struct arp) &&
     ntohs(eth->type) == ETHTYPE_ARP){
    arp_rx(buf);
  } else if(len >= sizeof(struct eth) + sizeof(struct ip) &&
     ntohs(eth->type) == ETHTYPE_IP){
    ip_rx(buf, len);
  } else {
    kfree(buf);
  }
}