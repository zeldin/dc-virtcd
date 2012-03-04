typedef struct server_s *server;
extern void server_delete(server s);
extern server server_new(msglogger logger);
extern void server_run_once(server s);
extern void server_run(server s);

