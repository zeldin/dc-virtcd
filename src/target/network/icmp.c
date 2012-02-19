#include "icmp.h"
#include "ip.h"

void icmp_got_packet(void *pkt, int size, void *ippkt)
{
  int i, type = ((unsigned char *)pkt)[0], csum = 0;

  if(size & 1)
    ((unsigned char *)pkt)[size] = 0;

  for(i=0; i<((size+1)>>1); i++)
    csum += (unsigned short)~(i==1? 0 : ((short *)pkt)[i]);

  if((csum = ((unsigned short)csum)+(csum >> 16))>=0x10000)
    csum = (unsigned short)(csum + 1);

  if(((unsigned short *)pkt)[1] != csum)
    return;

  switch(type) {
   case 8: {
     ((unsigned char *)pkt)[0] = 0;
     csum = 0;
     for(i=0; i<((size+1)>>1); i++)
       csum += (unsigned short)~(i==1? 0 : ((short *)pkt)[i]);
     if((csum = ((unsigned short)csum)+(csum >> 16))>=0x10000)
       csum = (unsigned short)(csum + 1);
     ((short *)pkt)[1] = csum;
     ip_reply_packet(ippkt);
     break;
   }
  }
}
