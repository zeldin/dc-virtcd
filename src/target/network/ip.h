unsigned short htons(unsigned short n);
void ip_got_packet(void *pkt, int size);
void ip_low_reply_packet(void *pkt);
void ip_reply_packet(void *pkt);
void ip_set_my_ip(unsigned int *addr);
int ip_get_my_ip(unsigned int *addr);
void ip_send_packet(void *target_hw, void *pkt);
