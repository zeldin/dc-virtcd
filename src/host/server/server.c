#include "config.h"

#include <stdlib.h>

#include "msglog.h"
#include "server.h"
#include "serverport.h"

struct server_s {
  msglogger logger;
  serverport port;
};

void server_delete(server s)
{
  if (s) {
    serverport_delete(s->port);
    free(s);
  }
}

server server_new(msglogger logger)
{
  server s = calloc(1, sizeof(struct server_s));
  if (s) {
    s->logger = logger;
    if ((s->port = serverport_new(logger)) != NULL) {
      return s;
    }
    server_delete(s);
  } else
    msglog_oomerror(logger);
  return NULL;
}

void server_run_once(server s)
{
  serverport_run_once(s->port);
}

void server_run(server s)
{
  serverport_run(s->port);
}
