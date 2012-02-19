extern int get_server_ip(unsigned int *addr);
extern int get_server_mac(unsigned char *mac);
extern void set_server(void *ip, void *mac);
extern void proto_got_packet(void *pkt, int sz, void *ip_pkt);
extern int send_command_packet(int cmd, void *param, int pcnt);
extern int check_command_packet(int n);
extern int wait_command_packet(int n);
extern void background_process(void);
extern void idle(void);
extern int *get_packet_slot(int n);

#define CLIENT_PORT 4781
#define SERVER_PORT 4782

