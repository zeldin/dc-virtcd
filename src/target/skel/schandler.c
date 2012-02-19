#include "pci.h"
#include "ether.h"
#include "proto.h"
#include "util.h"

#define MAX_CMD 47

static char param_cnt[MAX_CMD+1] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0-9 */
  0, 0, 0, 0, 0, 0, 4, 4, 0, 2,	/* 10-19 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 20-29 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 30-39 */
  0, 0, 0, 0, 0, 0, 0, 0,	/* 40-47 */
};

int gdrom_send_command(int cmd, int *params)
{
  int id, s;
  if(cmd<0 || cmd>MAX_CMD)
    return 0;
  s = getimask();
  setimask(15);
  id = send_command_packet(cmd, params, param_cnt[cmd]);
  setimask(s);
  return(id<0? 0 : id+1);
}

int low_gdrom_check_command(int id, int *status)
{
  int *p, r = check_command_packet(id-1);
  if(r<0)
    return 0;
  else if(!r)
    return 1;
  p = get_packet_slot(id-1);
  memcpy(status, p+3, 4*sizeof(int));
  if(p[2]<0)
    return -1;
  else
    return 2;
}

int gdrom_check_command(int id, int *status)
{
  int r, s;
  s = getimask();
  setimask(15);
  r = low_gdrom_check_command(id, status);
  /*
  if(r<0)
    wait_command_packet(send_command_packet(status[0]+600, 0L, 0));
  else
    wait_command_packet(send_command_packet(r+500, 0L, 0));    
  */
  setimask(s);
  return r;
}

int gdrom_mainloop()
{
  extern int num_commands, num_resends;
  static int fallback=0;
  int s = getimask();
  setimask(15);
  if(fallback) {
    idle();
    if(num_commands < 2)
      fallback = 0;
  } else {
    background_process();
    if(num_commands > 1 && num_resends > 20 * num_commands)
      fallback = 1;
  }
  setimask(s);
  return 0;
}

int gdrom_init()
{
  extern int num_commands, num_resends;
  int s = getimask();
  setimask(15);
  if(pci_setup() >=0 &&
     ether_setup() >= 0 &&
     wait_command_packet(send_command_packet(991, 0L, 0)) >= 0) {
    num_commands = num_resends = 0;
    setimask(s);
    return 0;
  } else {
    setimask(s);
    return -1;
  }
}

int gdrom_check_drive(int *ret)
{
  ret[0] = 1;
  ret[1] = 0x80;
  return 0;
}

int gdrom_reset()
{
  return 0;
}

int gdrom_sector_mode(int *mode)
{
  if(mode[0]==0)
    return 0;
  else if(mode[0]==1) {
    mode[1] = 8192;
    mode[2] = 1024;
    mode[3] = 2048;
  } else
    return -1;
}

int unimpl_syscall(int r4, int r5, int r6, int r7)
{
  setimask(15);
  wait_command_packet(send_command_packet(800+r7, 0L, 0));
  for(;;) {
    *(volatile unsigned int *)(void *)0xa05f8040 = 0x0000ff00;
    *(volatile unsigned int *)(void *)0xa05f8040 = 0x000000ff;
  }
  return -1;
}

void init(unsigned int my_ip, unsigned int server_ip, void *server_mac)
{
  ip_set_my_ip(&my_ip);
  set_server(&server_ip, server_mac);
}
