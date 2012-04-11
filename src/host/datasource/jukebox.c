#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "msglog.h"
#include "toc.h"
#include "datasource.h"
#include "jukebox.h"

struct jukebox_s {
  msglogger logger;
  unsigned sourcecount, alloccount;
  datasource *list;
};

void jukebox_delete(jukebox j)
{
  if (j) {
    int i;
    for (i=0; i<j->sourcecount; i++)
      datasource_delete(j->list[i]);
    if (j->list)
      free(j->list);
    free(j);
  }
}

extern jukebox jukebox_new(msglogger logger)
{
  jukebox j = calloc(1, sizeof(struct jukebox_s));
  if (j) {
    j->logger = logger;
    j->list = NULL;
    return j;
  } else
    msglog_oomerror(logger);
  return NULL;
}

extern bool jukebox_add_datasource(jukebox j, datasource ds)
{
  if (j->sourcecount >= j->alloccount) {
    datasource *newlist;
    unsigned cnt = j->alloccount + (j->alloccount >> 1);
    if (j->sourcecount >= cnt)
      cnt = j->sourcecount + 100;
    newlist = realloc(j->list, cnt*sizeof(datasource));
    if (newlist == NULL) {
      msglog_oomerror(j->logger);
      return false;
    }
    j->list = newlist;
    j->alloccount = cnt;
  }
  j->list[j->sourcecount++] = ds;
  return true;
}

extern datasource jukebox_get_datasource(jukebox j, uint32_t id)
{
  if (id >= j->sourcecount)
    return NULL;
  else
    return j->list[id];
}
