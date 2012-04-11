#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "msglog.h"
#include "toc.h"
#include "datasource.h"
#include "jukebox.h"
#include "server.h"
#include "serverport.h"

struct clientcontext_s {
  struct clientcontext_base_s base;
  datasource dsource;
};

struct server_s {
  msglogger logger;
  jukebox jbox;
  serverport port;
};

static void clientcontext_delete(void *ctx, clientcontext client)
{
  if (client) {
    if (client->dsource)
      datasource_unrealize(client->dsource);
    free(client);
  }
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

static bool clientcontext_set_datasource(clientcontext client, datasource ds)
{
  if (ds == client->dsource)
    return true;
  if (client->dsource) {
    datasource_unrealize(client->dsource);
    client->dsource = NULL;
  }
  if (ds && !datasource_realize(ds))
    return false;
  client->dsource = ds;
  return true;
}

static int32_t select_binary(server s, clientcontext client, uint32_t id)
{
  datasource ds = jukebox_get_datasource(s->jbox, id);
  if (!clientcontext_set_datasource(client, ds) ||
      ds == NULL)
    return -2;
  return 17; /* FIXME */
}

static int32_t handle_packet(void *ctx, clientcontext client, const int32_t *pkt, int cnt)
{
  server s = ctx;
  int i;
  for (i=0; i<cnt; i++)
    printf(" %d", pkt[i]);
  printf("\n");

  if (cnt > 0) {
    uint16_t cmd = (uint16_t)pkt[0];
    if (cmd < 48) {
      /* gdrom command */
      switch(cmd) {
	/* FIXME */
      }
    } else if(cmd >= 990) {
      /* monitor command */
      switch(cmd-990) {
      case 0:
      case 1:
	return 0;
      case 7:
	/* FIXME */
	break;
      case 8:
	return select_binary(s, client, (cnt > 1? (uint32_t)pkt[1] : 0));
      case 9:
	return 0;
      }
    } else if(cmd >= 800 && cmd < 816) {
      /* unimplemented syscall */
      return -2;
    } else if(cmd >= 500 && cmd < 700) {
      /* status feedback */
      return -2;
    }
  }
  
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

server server_new(msglogger logger, jukebox jbox)
{
  server s = calloc(1, sizeof(struct server_s));
  if (s) {
    s->logger = logger;
    s->jbox = jbox;
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
