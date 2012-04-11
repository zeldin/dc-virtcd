#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "msglog.h"
#include "toc.h"
#include "datasource.h"
#include "jukebox.h"
#include "server.h"

static void print_log(msglogger l, msglevel level, const char *msg)
{
  fprintf(stderr, "%s\n", msg);
}

int main(int argc, char *argv[])
{
  struct msglogger_s logger = {
    print_log
  };
  int i;
  server s;
  jukebox j = jukebox_new(&logger);
  if (!j)
    return 1;
  s = server_new(&logger, j);
  if (!s) {
    jukebox_delete(j);
    return 1;
  }

  for (i=1; i<argc; i++) {
    datasource d = datasource_new_from_filename(&logger, argv[i]);
    if (d != NULL)
      if(!jukebox_add_datasource(j, d))
	datasource_delete(d);
  }

  server_run(s);
  server_delete(s);
  jukebox_delete(j);
  return 0;
}
