#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#endif

#include "msglog.h"
#include "serverport.h"

#define CLIENT_PORT 4781
#define SERVER_PORT 4782

#define MAX_PKT 8

#ifdef HAVE_DECL___BUILTIN_BSWAP32
#define SWAP32 __builtin_bswap32
#else
#ifdef HAVE_DECL_BSWAP_32
#define SWAP32 bswap_32
#else
#define SWAP16(n) (((uint16_t)((n)<<8))|(((uint16_t)(n))>>8))
#define SWAP32(n) ((SWAP16((uint32_t)(n))<<16)|SWAP16(((uint32_t)(n))>>16))
#endif
#endif

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
  int i, rc;
#ifdef WORDS_BIGENDIAN
  for (i=2; i<size; i++)
    pkt[i] = SWAP32(pkt[i]);
#endif
  rc = (*s->funcs->handle_packet)(s->ctx, c, pkt, size);
}

void serverport_run(serverport s)
{
  for(;;) {
    serverport_run_once(s);
  }
}
