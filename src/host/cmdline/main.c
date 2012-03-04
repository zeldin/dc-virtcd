#include <stdio.h>

#include "msglog.h"
#include "serverport.h"

static void print_log(msglogger l, msglevel level, const char *msg)
{
  fprintf(stderr, "%s\n", msg);
}

int main(int argc, char *argv[])
{
  serverport s;
  struct msglogger_s logger = {
    print_log
  };
  s = serverport_new(&logger);
  if (!s)
    return 1;
  serverport_run(s);
  serverport_delete(s);
  return 0;
}
