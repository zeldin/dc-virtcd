typedef struct serverport_s *serverport;
typedef struct clientcontext_s *clientcontext;
struct clientcontext_base_s {
  uint32_t client_addr;
};
typedef struct serverfuncs_s {
  clientcontext (*clientcontext_new)(void *ctx);
  void (*clientcontext_delete)(void *ctx, clientcontext client);
  int32_t (*handle_packet)(void *ctx, clientcontext client, const int32_t *pkt, int cnt);
} *serverfuncs;
extern serverport serverport_new(msglogger logger, serverfuncs funcs, void *ctx);
extern void serverport_delete(serverport s);
extern void serverport_run_once(serverport s);
extern void serverport_run(serverport s);
