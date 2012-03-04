#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "msglog.h"

void msglog_logv(msglogger l, msglevel level, const char *msg, va_list va)
{
  char buf[512];
  vsnprintf(buf, sizeof(buf), msg, va);
  (*l->log)(l, level, buf);
}

void msglog_log(msglogger l, msglevel level, const char *msg, ...)
{
  va_list va;
  va_start(va, msg);
  msglog_logv(l, level, msg, va);
  va_end(va);
}

void msglog_debug(msglogger l, const char *msg, ...)
{
  va_list va;
  va_start(va, msg);
  msglog_logv(l, MSG_DEBUG, msg, va);
  va_end(va);
}

void msglog_info(msglogger l, const char *msg, ...)
{
  va_list va;
  va_start(va, msg);
  msglog_logv(l, MSG_INFO, msg, va);
  va_end(va);
}

void msglog_notice(msglogger l, const char *msg, ...)
{
  va_list va;
  va_start(va, msg);
  msglog_logv(l, MSG_NOTICE, msg, va);
  va_end(va);
}

void msglog_warning(msglogger l, const char *msg, ...)
{
  va_list va;
  va_start(va, msg);
  msglog_logv(l, MSG_WARNING, msg, va);
  va_end(va);
}

void msglog_error(msglogger l, const char *msg, ...)
{
  va_list va;
  va_start(va, msg);
  msglog_logv(l, MSG_ERROR, msg, va);
  va_end(va);
}

void msglog_oomerror(msglogger l)
{
  msglog_error(l, "out of memory");
}

void msglog_perror(msglogger l, const char *prefix)
{
  const char *sysmsg = strerror(errno);
  if (prefix)
    msglog_error(l, "%s: %s", prefix, sysmsg);
  else
    msglog_error(l, "%s", sysmsg);
}
