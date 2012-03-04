#include <stdio.h>

#include "msglog.h"
#include "server.h"

static void print_log(msglogger l, msglevel level, const char *msg)
{
  fprintf(stderr, "%s\n", msg);
}

int main(int argc, char *argv[])
{
  server s;
  struct msglogger_s logger = {
    print_log
  };
  s = server_new(&logger);
  if (!s)
    return 1;
  server_run(s);
  server_delete(s);
  return 0;
}
