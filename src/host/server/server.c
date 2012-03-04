#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "msglog.h"
#include "server.h"
#include "serverport.h"

struct clientcontext_s {
  struct clientcontext_base_s base;
};

struct server_s {
  msglogger logger;
  serverport port;
};

static void clientcontext_delete(void *ctx, clientcontext client)
{
  free(client);
}

static clientcontext clientcontext_new(void *ctx)
{
  server s = ctx;
  clientcontext c = calloc(1, sizeof(struct clientcontext_s));
  if (c) {
    return c;
  } else
    msglog_oomerror(s->logger);
  return NULL;
}

static int handle_packet(void *ctx, clientcontext client, const int32_t *pkt, int cnt)
{
  int i;
  for (i=0; i<cnt; i++)
    printf(" %d", pkt[i]);
  printf("\n");
  return -1;
}

static struct serverfuncs_s funcs = {
  clientcontext_new,
  clientcontext_delete,
  handle_packet
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
    if ((s->port = serverport_new(logger, &funcs, s)) != NULL) {
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
