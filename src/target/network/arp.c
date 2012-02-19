#include "arp.h"
#include "ether.h"

#define NUM_ARP_ENTRIES 64

static int used_entries = 0, entry_robin = 0;

static struct arp_entry {
  unsigned char ip[4];
  unsigned char hw[6];
} entries[NUM_ARP_ENTRIES];

static struct arp_entry *arp_add_entry(unsigned char *ip, unsigned char *hw)
{
  struct arp_entry *e = &entries[entry_robin++];
  int i;

  if(entry_robin > used_entries)
    used_entries = entry_robin;

  if(entry_robin >= NUM_ARP_ENTRIES)
    entry_robin = 0;

  for(i=0; i<4; i++)
    e->ip[i] = ip[i];
  for(i=0; i<6; i++)
    e->hw[i] = hw[i];

  return e;
}

static struct arp_entry *arp_find_ip(unsigned char *ip)
{
  int i;
  struct arp_entry *e = entries;

  for(i=0; i<used_entries; i++, e++)
    if(e->ip[0] == ip[0] && e->ip[1] == ip[1] &&
       e->ip[2] == ip[2] && e->ip[3] == ip[3])
      return e;

  return 0;
}


void arp_got_packet(void *pkt, int size)
{
  struct {
    unsigned short hwspace;
    unsigned short protospace;
    unsigned char hwlength;
    unsigned char protolength;
    unsigned short opcode;
  } *arp_pkt = pkt;
  unsigned char *senderhw = (unsigned char *)(arp_pkt+1);
  unsigned char *senderproto = senderhw + arp_pkt->hwlength;
  unsigned char *targethw = senderproto + arp_pkt->protolength;
  unsigned char *targetproto = targethw + arp_pkt->hwlength;
  unsigned int myip;
  int i, merge_flag = 0;
  struct arp_entry *entry;

  /* Discard short ARP packet... */
  if(targetproto + arp_pkt->protolength - (unsigned char *)pkt > size)
    return;

  /* Only ethernet & IP below this point... */
  if(htons(arp_pkt->hwspace) != 1 || htons(arp_pkt->protospace) != 0x800 ||
     arp_pkt->hwlength != 6 || arp_pkt->protolength != 4)
    return;

  if((entry = arp_find_ip(senderproto)) != 0) {
    for(i=0; i<6; i++)
      entry->hw[i] = senderhw[i];
    merge_flag = 1;
  }

  if(!ip_get_my_ip(&myip))
    return;

  for(i=0; i<4; i++)
    if(targetproto[i] != ((unsigned char *)&myip)[i])
      return;

  if(!merge_flag) {
    arp_add_entry(senderproto, senderhw);
  }

  if(htons(arp_pkt->opcode) != 1)
    return;

  for(i=0; i<6; i++) {
    (((unsigned char *)pkt)-14)[i] = targethw[i] = senderhw[i];
    (((unsigned char *)pkt)-8)[i] = senderhw[i] = ether_MAC[i];
  }

  for(i=0; i<4; i++) {
    targetproto[i] = senderproto[i];
    senderproto[i] = ((unsigned char *)&myip)[i];
  }

  ether_send_packet(((unsigned char *)pkt)-14, 42);
}
