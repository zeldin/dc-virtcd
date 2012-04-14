#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "msglog.h"
#include "serverport.h"
#include "bswap.h"

#define CLIENT_PORT 4781
#define SERVER_PORT 4782

#define MAX_PKT 8
#define MAX_EXTRA 1280  /* Should be less than MTU-28 */

typedef clientcontext level4_index[256];
typedef level4_index *level3_index[256];
typedef level3_index *level2_index[256];
typedef level2_index *level1_index[256];

struct serverport_s {
  msglogger logger;
  serverfuncs funcs;
  void *ctx;
  int sockfd;
  level1_index clients;
  uint8_t extra_buffer[MAX_EXTRA];
};

struct extra_response {
  void *buffer;
  size_t size;
};

static void clientcontext_delete(serverport s, clientcontext c)
{
  (*s->funcs->clientcontext_delete)(s->ctx, c);
}

static clientcontext clientcontext_new(serverport s, struct in_addr in)
{
  clientcontext c = (*s->funcs->clientcontext_new)(s->ctx);
  if (c) {
    ((struct clientcontext_base_s *)c)->client_addr = in.s_addr;
    return c;
  }
  return NULL;
}

static clientcontext serverport_get_clientcontext_for_address(serverport s,
							      struct in_addr in)
{
  clientcontext c;
  level2_index *l2;
  level3_index *l3;
  level4_index *l4;
  uint32_t a = ntohl(in.s_addr);
  uint8_t a1 = a >> 24;
  uint8_t a2 = a >> 16;
  uint8_t a3 = a >> 8;
  uint8_t a4 = a;

  l2 = s->clients[a1];
  if (l2 == NULL) {
    int i;
    msglog_debug(s->logger, "Creating new level 2 index for prefix %d", a1);
    l2 = calloc(1, sizeof(level2_index));
    if (l2 == NULL) {
      msglog_oomerror(s->logger);
      return NULL;
    }
    for (i=0; i<256; i++)
      (*l2)[i] = NULL;
    s->clients[a1] = l2;
  }
  l3 = (*l2)[a2];
  if (l3 == NULL) {
    int i;
    msglog_debug(s->logger, "Creating new level 3 index for prefix %d.%d", a1, a2);
    l3 = calloc(1, sizeof(level3_index));
    if (l3 == NULL) {
      msglog_oomerror(s->logger);
      return NULL;
    }
    for (i=0; i<256; i++)
      (*l3)[i] = NULL;
    (*l2)[a2] = l3;
  }
  l4 = (*l3)[a3];
  if (l4 == NULL) {
    int i;
    msglog_debug(s->logger, "Creating new level 4 index for prefix %d.%d.%d", a1, a2, a3);
    l4 = calloc(1, sizeof(level4_index));
    if (l4 == NULL) {
      msglog_oomerror(s->logger);
      return NULL;
    }
    for (i=0; i<256; i++)
      (*l4)[i] = NULL;
    (*l3)[a3] = l4;
  }
  c = (*l4)[a4];
  if (c == NULL) {
    msglog_debug(s->logger, "Creating new client context for %d.%d.%d.%d", a1, a2, a3, a4);
    (*l4)[a4] = c = clientcontext_new(s, in);
  }
  return c;
}

serverport serverport_new(msglogger logger, serverfuncs funcs, void *ctx)
{
  serverport s = calloc(1, sizeof(struct serverport_s));
  if (s) {
    int i;
    s->logger = logger;
    s->funcs = funcs;
    s->ctx = ctx;
    for (i=0; i<256; i++)
      s->clients[i] = NULL;
    if ((s->sockfd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0) {
      struct sockaddr_in addr;
      memset(&addr, 0, sizeof(addr));
      addr.sin_family = AF_INET;
      addr.sin_port = htons(SERVER_PORT);
      addr.sin_addr.s_addr = INADDR_ANY;
      if (bind(s->sockfd, (struct sockaddr *)&addr, sizeof(addr)) >= 0) {
	return s;
      } else
	msglog_perror(logger, "bind");
    } else
      msglog_perror(logger, "socket");
    serverport_delete(s);
  } else
    msglog_oomerror(logger);
  return NULL;
}

void serverport_delete(serverport s)
{
  if (s) {
    int i, j, k, l;
    if (s->sockfd >= 0)
      close(s->sockfd);
    for (i=0; i<256; i++) {
      level2_index *l2 = s->clients[i];
      if (l2 != NULL) {
	for (j=0; j<256; j++) {
	  level3_index *l3 = (*l2)[j];
	  if (l3 != NULL) {
	    for (k=0; k<256; k++) {
	      level4_index *l4 = (*l3)[k];
	      if (l4 != NULL) {
		for (l=0; l<256; l++) {
		  clientcontext c = (*l4)[l];
		  if (c != NULL)
		    clientcontext_delete(s, c);
		}
		free(l4);
	      }
	    }
	    free(l3);
	  }
	}
	free(l2);
      }
    }
    free(s);
  }
}

bool serverport_add_extra(serverport s, struct extra_response *extra, const void *data, size_t len)
{
  if (!len)
    return true;
  size_t old = extra->size;
  if (!old)
    old = 3*sizeof(int32_t);
  if (len > MAX_EXTRA - old) {
    msglog_error(s->logger, "Overfull extra");
    return false;
  }
  if (extra->buffer == NULL)
    extra->buffer = s->extra_buffer;
  memcpy(((uint8_t *)extra->buffer) + old, data, len);
  extra->size = old+len;
  return true;
}

void serverport_run_once(serverport s)
{
  ssize_t size;
  int32_t pkt[MAX_PKT];
  struct sockaddr_in src_addr;
  socklen_t addrlen = sizeof(src_addr);
  size = recvfrom(s->sockfd, pkt, sizeof(pkt), 0,
		  (struct sockaddr *)&src_addr, &addrlen);
  if (size<0) {
    msglog_perror(s->logger, "recvfrom");
    return;
  }
  if (size<12 || size>MAX_PKT*4 || (size&3)!=0) {
    msglog_warning(s->logger, "Invalid packet length");
    return;
  }
  size /= sizeof(int32_t);
  msglog_debug(s->logger, "Got a message from %s:%d, %d values",
	       inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port), size);
  clientcontext c = serverport_get_clientcontext_for_address(s, src_addr.sin_addr);
  if (c == NULL)
    return;
  int i;
  int32_t rc;
#ifdef WORDS_BIGENDIAN
  for (i=2; i<size; i++)
    pkt[i] = SWAP32(pkt[i]);
#endif
  struct extra_response extra = { NULL, 0 };
  rc = (*s->funcs->handle_packet)(s->ctx, c, pkt+2, size-2, &extra);
  if (rc != -1) {
#ifdef WORDS_BIGENDIAN
    pkt[2] = SWAP32(rc);
#else
    pkt[2] = rc;
#endif
    if (extra.buffer) {
      memcpy(extra.buffer, pkt, 3*sizeof(int32_t));
      size = sendto(s->sockfd, extra.buffer, extra.size, 0,
		    (struct sockaddr *)&src_addr, addrlen);
    } else
      size = sendto(s->sockfd, pkt, 3*sizeof(int32_t), 0,
		    (struct sockaddr *)&src_addr, addrlen);
    if (size<0)
      msglog_perror(s->logger, "sendto");
  }
}

void serverport_run(serverport s)
{
  for(;;) {
    serverport_run_once(s);
  }
}
