#include "proto.h"
#include "util.h"

static int target_ip=-1;
static unsigned char target_hw[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
static int server_detected=0;

static int seq_id = 1001;

int num_commands = 0, num_resends = 0;

#define NUM_SLOTS 8
#define MAX_PKT 8

#define RESEND_TIME 4
#define RESEND_COUNT 100

static int slots[NUM_SLOTS][MAX_PKT];
static int resend_time[NUM_SLOTS];
static int resend_count[NUM_SLOTS];
static int pkt_size[NUM_SLOTS];

int get_server_ip(unsigned int *addr)
{
  *addr = target_ip;
  return server_detected;
}

int get_server_mac(unsigned char *mac)
{
  memcpy(mac, target_hw, 6);
  return server_detected;
}

static char *extra_addr;

static void handle_extra(unsigned char *src, int sz, void *ret)
{
  int c, n;
  while(sz-->0)
    if((c = *src++) & 0x80) {
      n = c&0xf;
      if(c & 0x10) {
	if(--sz<0) return;
	n = (n<<8) | *src++;
      }
      if(c & 0x40) {
	if((sz-=4)<0) return;
	((unsigned char *)&extra_addr)[0] = *src++;
	((unsigned char *)&extra_addr)[1] = *src++;
	((unsigned char *)&extra_addr)[2] = *src++;
	((unsigned char *)&extra_addr)[3] = *src++;
      }
      if(c & 0x20) {
	if(--sz<0) return;
	memset(extra_addr, *src++, n);
	extra_addr += n;
      } else {
	if((sz -= n) < 0) return;
	if((n&3) || (((long)extra_addr)&3)) {
	  memcpy(extra_addr, src, n);
	  src += n;
	  extra_addr += n;
	} else {
	  n>>=2;
	  while(n--) {
	    int a = *src++;
	    a |= (*src++)<<8;
	    a |= (*src++)<<16;
	    a |= (*src++)<<24;
	    *(int *)extra_addr = a;
	    extra_addr += 4;
	  }
	}
      }
    } else if(c==1) {
      if(sz<16) return;
      sz-=16;
      memcpy(ret, src, 16);
      src += 16;
    }
}

void set_server(void *ip, void *mac)
{
  memcpy(&target_ip, ip, 4);
  memcpy(target_hw, mac, 6);
}

void proto_got_packet(void *pkt, int sz, void *ip_pkt)
{
  int phase, n, p[3];
  if(sz < 12)
    return;
  memcpy(p, pkt, 12);
  n = p[1];
  if(n<0 || n>=NUM_SLOTS || p[0]<=0 || p[0] != slots[n][0])
    return;
  if(!server_detected) {
    set_server(((unsigned char *)ip_pkt)+12, ((unsigned char *)ip_pkt)-8);
    server_detected = 1;
  }
  if(p[2]>=0 && (phase=p[2]&~0xffff) && (slots[n][2] & 0xffff)<100) {
    slots[n][2] = (slots[n][2] & 0xffff) | phase;
    resend_time[n] = RESEND_TIME;
    resend_count[n] = RESEND_COUNT;
    udp_send_packet(target_hw, target_ip, CLIENT_PORT, SERVER_PORT,
		    slots[n], pkt_size[n]);  
    num_commands ++;
  } else {
    slots[n][0] = -1;
    slots[n][2] = p[2];
    bzero(&slots[n][3], 16);
  }
  if(sz > 12)
    handle_extra(((unsigned char *)pkt)+12, sz-12, &slots[n][3]);
}

void background_process()
{
  int i;
  ether_check_events();
  for(i=0; i<NUM_SLOTS; i++)
    if(slots[i][0]>0 && --resend_time[i]<0) {
      if(--resend_count[i]) {
	udp_send_packet(target_hw, target_ip, CLIENT_PORT, SERVER_PORT,
			slots[i], pkt_size[i]);
	resend_time[i] = RESEND_TIME;
	num_resends++;
      } else {
	slots[i][0] = -1;
	slots[i][2] = -1;
      }
    }
}

void idle()
{
#if 1
  volatile unsigned int *vbl = (volatile unsigned int *)(void *)0xa05f810c;

  while (!(*vbl & 0x01ff));
  while (*vbl & 0x01ff);
#else
  int i;
  for(i=0; i<200000; i++);
#endif
  background_process();
}

static int find_slot()
{
  int i, c=0;

  for(;;) {
    for(i=0; i<NUM_SLOTS; i++)
      if(!slots[i][0])
	return i;
    if(c<10) {
      ether_check_events();
      c++;
    } else     
      idle();
  }
}

int send_command_packet(int cmd, void *param, int pcnt)
{
  int *pkt;
  int n = find_slot();
  pkt = slots[n];
  pkt[0] = seq_id++;
  pkt[1] = n;
  pkt[2] = cmd;
  if(pcnt)
    memcpy(pkt+3, param, pcnt*sizeof(int));
  resend_time[n] = RESEND_TIME;
  resend_count[n] = RESEND_COUNT;
  udp_send_packet(target_hw, target_ip, CLIENT_PORT, SERVER_PORT,
		  pkt, (pkt_size[n]=(pcnt+3)*sizeof(int)));  
  num_commands++;
  if(seq_id < 0)
    seq_id = 3;
  return n;
}

int check_command_packet(int n)
{
  ether_check_events();
  if(n<0 || n>=NUM_SLOTS || !slots[n][0])
    return -1;
  if(slots[n][0]>0)
    return 0;
  slots[n][0] = 0;
  return 1;
}

int wait_command_packet(int n)
{
  if(n<0 || n>=NUM_SLOTS)
    return -1;
  while(!check_command_packet(n))
    idle();
  return slots[n][2];  
}

int *get_packet_slot(int n)
{
  return slots[n];
}
