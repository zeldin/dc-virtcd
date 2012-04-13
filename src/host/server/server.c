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

#define DOWNLOAD_AREA_START 0x8c008000
#define DOWNLOAD_AREA_END   0x8cf10000
#define DOWNLOAD_MAIN_START 0x8c010000

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

static bool write_data(server s, struct extra_response *extra,
		       const void *data, size_t len, uint32_t addr)
{
  uint8_t header[6] = {
    (len>>8)|0xd0, len&0xff,
    addr&0xff, (addr>>8)&0xff, (addr>>16)&0xff, addr>>24
  };
  if (!len)
    return true;
  return serverport_add_extra(s->port, extra, header, 6) &&
    serverport_add_extra(s->port, extra, data, len);
}

static int32_t select_binary(server s, clientcontext client, uint32_t id)
{
  int32_t r;
  datasource ds = jukebox_get_datasource(s->jbox, id);
  if (!clientcontext_set_datasource(client, ds) ||
      ds == NULL)
    return -2;
  r = datasource_get_1st_read_size(ds);
  return (r<0? -2 : r);
}

static int32_t low_download(server s, clientcontext client,
			    struct extra_response *extra,
			    uint32_t addr, uint32_t cnt)
{
  uint8_t buf[2048];
  if (addr < DOWNLOAD_MAIN_START) {
    uint32_t offs = addr & 0x7fff;
    if (!datasource_get_ipbin(client->dsource, offs>>11, buf) ||
	!write_data(s, extra, buf+(offs&2047), cnt, addr))
      return -4;
    else
      return 0;
  } else {
    uint32_t offs = addr - DOWNLOAD_MAIN_START;
    int32_t len = datasource_get_1st_read_size(client->dsource);
    if (len < 0 || offs + cnt > (uint32_t)len) {
      msglog_error(s->logger, "Download after end of binary...");
      return -3;
    }
    if (!datasource_get_1st_read(client->dsource, offs>>11, buf) ||
	!write_data(s, extra, buf+(offs&2047), cnt, addr))
      return -4;
    else
      return 0;
  }
}

static int32_t download(server s, clientcontext client,
			struct extra_response *extra,
			uint32_t addr, uint32_t cnt)
{
  if (client->dsource == NULL) {
    msglog_error(s->logger, "Download request without a datasource");
    return -2;
  }
  if (addr < DOWNLOAD_AREA_START || addr > DOWNLOAD_AREA_END ||
      cnt > DOWNLOAD_AREA_END - addr) {
    msglog_error(s->logger, "Download request outside allowed area");
    return -2;
  }
  if (!cnt)
    return 0;
  while (((addr + cnt - 1)&~2047) != (addr&~2047)) {
    uint32_t chunk = (addr|2047)+1-addr;
    int32_t r = low_download(s, client, extra, addr, chunk);
    if (r)
      return r;
    addr += chunk;
    cnt -= chunk;
  }
  return low_download(s, client, extra, addr, cnt);
}

static int32_t handle_packet(void *ctx, clientcontext client, const int32_t *pkt, int cnt, struct extra_response *extra)
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
	return download(s, client, extra,
			(cnt > 1? (uint32_t)pkt[1] : 0),
			(cnt > 2? (uint32_t)pkt[2] : 0));
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
