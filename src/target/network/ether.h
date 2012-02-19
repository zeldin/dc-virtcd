int ether_setup(void);
void ether_teardown(void);
void ether_check_events(void);
void ether_send_packet(void *pkt, int size);

extern unsigned char ether_MAC[6];

