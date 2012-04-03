#include "config.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "msglog.h"
#include "toc.h"
#include "datafile.h"
#include "isofile.h"

/* Expect to find root directory within this many sectors from the start */
#define ROOT_DIRECTORY_HORIZON 500

struct isofile_s {
  msglogger logger;
  datafile data;
  uint32_t num_sectors;
  uint32_t start_sector;
};

static bool isofile_find_start_sector(isofile i)
{
  uint32_t sec;
  uint8_t buf1[0x22], buf2[0x22];
  for (sec = 16; sec < ROOT_DIRECTORY_HORIZON; sec++) {
    if (!datafile_read(i->data, sec<<11, 6, buf1))
      return false;
    if (!memcmp(buf1, "\001CD001", 6))
      break;
    else if(!memcmp(buf1, "\377CD001", 6))
      return false;
  }
  if (sec >= ROOT_DIRECTORY_HORIZON)
    return false;
  msglog_debug(i->logger, "PVD is at %d", sec);
  if (!datafile_read(i->data, (sec<<11)+0x9c, 0x22, buf1))
    return false;
  while (++sec < ROOT_DIRECTORY_HORIZON) {
    if (!datafile_read(i->data, sec<<11, 0x22, buf2))
      return false;
    if (!memcmp(buf1, buf2, 0x22))
      break;
  }
  if (sec >= ROOT_DIRECTORY_HORIZON)
    return false;
  msglog_debug(i->logger, "Root directory is at %d", sec);
  sec = ((((((buf1[5]<<8)|buf1[4])<<8)|buf1[3])<<8)|buf1[2])+150-sec;
  msglog_debug(i->logger, "Session offset is %d", sec);
  i->start_sector = sec;
  return true;
}

bool isofile_read_sector(isofile i, uint32_t sector, uint8_t *buffer)
{
  if (sector < i->start_sector)
    return false;
  sector -= i->start_sector;
  if (sector >= i->num_sectors)
    return false;
  return datafile_read(i->data, sector<<11, 2048, buffer);
}

bool isofile_get_toc(isofile i, int session, dc_toc *toc)
{
  memset(toc, 0, sizeof(*toc));
  toc->entry[0] = MAKE_DC_TOC_ENTRY(150, 0, 0);
  toc->entry[1] = MAKE_DC_TOC_ENTRY(i->start_sector, 0, 4);
  toc->first = MAKE_DC_TOC_TRACK(1);
  toc->last = MAKE_DC_TOC_TRACK(2);
}

void isofile_delete(isofile i)
{
  if (i) {
    free(i);
  }
}

isofile isofile_new(msglogger logger, datafile data)
{
  isofile i = calloc(1, sizeof(struct isofile_s));
  if (i) {
    i->logger = logger;
    i->data = data;
    i->num_sectors = datafile_size(data)>>11;
    if (isofile_find_start_sector(i)) {
      return i;
    } else
      msglog_error(logger, "Unable to determine the start sector for ISO");
  } else
    msglog_oomerror(logger);
  return NULL;
}

