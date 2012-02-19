#include "ip.h"
#include "ether.h"
#include "udp.h"
#include "icmp.h"

unsigned short htons(unsigned short n)
{
  return (n>>8) | (n<<8);
}

static unsigned short id = 4711;

void ip_got_packet(void *pkt, int size)
{
  struct {
    unsigned char vers_ihl;
    unsigned char servicetype;
    unsigned short totlength;
    unsigned short ident;
    unsigned short fragment;
    unsigned char ttl;
    unsigned char proto;
    unsigned short hcsum;
    unsigned char source_ip[4];
    unsigned char dest_ip[4];
  } *ip_pkt = pkt;
  unsigned char *options = (unsigned char *)(ip_pkt + 1);
  int hdr_len, i, sz, csum = 0;

  if(size < 20 || (ip_pkt->vers_ihl>>4) != 4)
    return;

  if((hdr_len = ip_pkt->vers_ihl & 0xf)<5 ||
     hdr_len > ((sz = htons(ip_pkt->totlength))>>2) ||
     sz > size)
    return;

  pkt = ((unsigned int *)pkt) + hdr_len;
  size = sz - (hdr_len<<2);

  for(i=0; i<(hdr_len<<1); i++)
    csum += (unsigned short)~(i==5? 0 : ((short *)ip_pkt)[i]);

  if((csum = ((unsigned short)csum)+(csum >> 16))>=0x10000)
    csum = (unsigned short)(csum + 1);

  if(!csum)
    csum = 0xffff;

  if(ip_pkt->hcsum != csum)
    return;

  switch(ip_pkt->proto) {
   case 1: icmp_got_packet(pkt, size, ip_pkt); break;
/* case 6: tcp_got_packet(pkt, size, ip_pkt); break; */
   case 17: udp_got_packet(pkt, size, ip_pkt); break;
  }
}

void ip_low_reply_packet(void *pkt)
{
  int sz = htons(((unsigned short *)pkt)[1])+14;
  int i, csum = 0;
  int hdr_len = (*(char *)pkt) & 0xf;

  ((unsigned short *)pkt)[2] = htons(id++);

  for(i=0; i<(hdr_len<<1); i++)
    csum += (unsigned short)~(i==5? 0 : ((short *)pkt)[i]);

  if((csum = ((unsigned short)csum)+(csum >> 16))>=0x10000)
    csum = (unsigned short)(csum + 1);

  ((unsigned short *)pkt)[5] = (csum? csum : 0xffff);
  
  ether_send_packet(((unsigned char *)pkt)-14, sz);
}

void ip_reply_packet(void *pkt)
{
  unsigned char *p = ((unsigned char *)pkt)-14;
  int i, t;

  for(i=0; i<6; i++) {
    t = p[i]; p[i] = p[6+i]; p[6+i] = t;
  }
  for(i=0; i<4; i++) {
    t = p[14+12+i]; p[14+12+i] = p[14+12+4+i]; p[14+12+4+i] = t;
  }

  ip_low_reply_packet(pkt);
}

void ip_send_packet(void *target_hw, void *pkt)
{
  unsigned char *p = ((unsigned char *)pkt)-14;
  int sz = htons(((unsigned short *)pkt)[1])+14;
  int i, csum = 0;

  ((unsigned short *)pkt)[0] = 5 | (4<<4);
  ((unsigned short *)pkt)[2] = htons(id++);
  ((unsigned short *)pkt)[3] = 2<<5;

  for(i=0; i<10; i++)
    csum += (unsigned short)~(i==5? 0 : ((short *)pkt)[i]);

  if((csum = ((unsigned short)csum)+(csum >> 16))>=0x10000)
    csum = (unsigned short)(csum + 1);

  ((unsigned short *)pkt)[5] = (csum? csum : 0xffff);

  ((unsigned short *)p)[6] = 8;
  for(i=0; i<6; i++) {
    p[i] = ((unsigned char *)target_hw)[i];
    p[i+6] = ether_MAC[i];
  }
  ether_send_packet(((unsigned char *)pkt)-14, sz);
}

static unsigned int my_ip = 0;
static int ip_is_set = 0;

void ip_set_my_ip(unsigned int *addr)
{
  my_ip = *addr;
  ip_is_set = 1;
}

int ip_get_my_ip(unsigned int *addr)
{
  *addr = my_ip;
  return ip_is_set;
}
