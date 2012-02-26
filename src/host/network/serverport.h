typedef struct serverport_s *serverport;
typedef struct clientcontext_s *clientcontext;
extern serverport serverport_new(void);
extern void serverport_delete(serverport s);
extern void serverport_run_once(serverport s);
extern void serverport_run(serverport s);
