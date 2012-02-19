void udp_got_packet(void *pkt, int size, void *ip_pkt);
void udp_send_packet(void *target_hw, unsigned int target_ip,
		     unsigned short src_port, unsigned short target_port,
		     void *pkt, unsigned size);


		     
