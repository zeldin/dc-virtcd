#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>

#include "msglog.h"
#include "datafile.h"

struct datafile_s {
  msglogger logger;
  int fd;
};

void datafile_delete(datafile d)
{
  if (d) {
    if (d->fd >= 0)
      close(d->fd);
    free(d);
  }
}

datafile datafile_new_from_filename(msglogger logger, const char *filename)
{
  datafile d = calloc(1, sizeof(struct datafile_s));
  if (d) {
    d->logger = logger;
    d->fd = open(filename, O_RDONLY);
    if (d->fd >= 0){
      return d;
    } else
      msglog_perror(logger, filename);
    datafile_delete(d);
  } else
    msglog_oomerror(logger);
  return NULL;
}

bool datafile_read(datafile d, datafile_offset offset, size_t size, void *buf)
{
#ifdef HAVE_PREAD
  ssize_t sz;
  sz = pread(d->fd, buf, size, offset);
  if (sz < 0)
    msglog_perror(d->logger, "pread");
  else if (sz != size)
    msglog_warning(d->logger, "End of file reached");
  else
    return true;
  return false;
#else
  off_t pos;
  pos = lseek(d->fd, offset, SEEK_SET);
  if (pos < 0)
    msglog_perror(d->logger, "lseek");
  else if (pos != offset)
    msglog_warning(d->logger, "End of file reached");
  else {
    ssize_t sz;
    sz = read(d->fd, buf, size);
    if (sz < 0)
      msglog_perror(d->logger, "read");
    else if (sz != size)
      msglog_warning(d->logger, "End of file reached");
    else
      return true;
  }
  return false;
#endif
}

size_t datafile_size(datafile d)
{
  off_t pos;
  pos = lseek(d->fd, 0, SEEK_END);
  if (pos < 0)
    msglog_perror(d->logger, "lseek");
  else
    return pos;
  return 0;
}

