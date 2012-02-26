#include "serverport.h"

int main(int argc, char *argv[])
{
  serverport s;
  s = serverport_new();
  if (!s)
    return 1;
  serverport_run(s);
  serverport_delete(s);
  return 0;
}
