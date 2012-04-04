#include "config.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "msglog.h"
#include "toc.h"
#include "datafile.h"
#include "datasource.h"
#include "isofsparser.h"

/* Expect to find root directory within this many sectors from the start */
#define ROOT_DIRECTORY_HORIZON 500

struct isofs_s {
  msglogger logger;
  datasource ds;
  uint32_t datatrack_lba, root_lba, root_length;
  uint8_t buffer[2048];
};

static int ntohlp(const uint8_t *ptr)
{
  return (ptr[0]<<24)|(ptr[1]<<16)|(ptr[2]<<8)|ptr[3];
}

static bool fncompare(const char *fn1, int fn1len, const char *fn2, int fn2len)
{
  while(fn2len--)
    if(!fn1len--)
      return *fn2 == ';';
    else if(toupper((unsigned char)*fn1++) != toupper((unsigned char)*fn2++))
      return 0;
  return fn1len == 0;
}

static bool isofs_find_datatrack(isofs i)
{
  int session;
  for (session = 1; session >= 0; --session) {
    dc_toc toc;
    if (!datasource_get_toc(i->ds, session, &toc))
      continue;
    int trk, first, last;
    first = GET_DC_TOC_TRACK(toc.first);
    last = GET_DC_TOC_TRACK(toc.last);
    if(first < 1 || last > 99 || first > last)
      continue;
    for(trk=last; trk>=first; --trk)
      if(GET_DC_TOC_ENTRY_CTRL(toc.entry[trk-1])&4) {
	i->datatrack_lba = GET_DC_TOC_ENTRY_LBA(toc.entry[trk-1]);
	msglog_debug(i->logger,
		     "Found data track in session %d, track %d at LBA %d",
		     session, trk, i->datatrack_lba);
	return true;
      }
  }
  return false;
}

static bool isofs_find_root_directory(isofs i)
{
  uint32_t sec;
  uint8_t *buf = i->buffer;
  for (sec = 16; sec < ROOT_DIRECTORY_HORIZON; sec++) {
    if (!datasource_read_sector(i->ds, i->datatrack_lba+sec, buf))
      return false;
    if (!memcmp(buf, "\001CD001", 6))
      break;
    else if(!memcmp(buf, "\377CD001", 6))
      return false;
  }
  if (sec >= ROOT_DIRECTORY_HORIZON)
    return false;
  sec += i->datatrack_lba;
  msglog_debug(i->logger, "Found PVD at LBA %d", sec);
  i->root_lba = ntohlp(buf+156+6)+150;
  i->root_length = ntohlp(buf+156+14);
  msglog_debug(i->logger, "Root directory is at LBA %d, length %d bytes",
	       i->root_lba, i->root_length);
  return true;
}

static bool isofs_find_entry(isofs i, const char *entryname,
			     uint32_t *sector, uint32_t *length, int enlen,
			     uint32_t dirsec, uint32_t dirlen, int dirflag)
{
  uint32_t len;
  uint8_t *buf = i->buffer;
  const uint8_t *p;
  dirflag = (dirflag? 2 : 0);
  while(dirlen > 0) {
    if (!datasource_read_sector(i->ds, dirsec, buf))
      return false;
    if (dirlen > 2048) {
      len = 2048;
      dirlen -= 2048;
      dirsec++;
    } else {
      len = dirlen;
      dirlen = 0;
    }
    for (p=buf; len>0; ) {
      if(!*p || *p>len)
	break;
      if (*p>32 && *p>32+p[32])
	if ((p[25]&2) == dirflag &&
	    fncompare(entryname, enlen, (const char *)(p+33), p[32])) {
	  if (sector)
	    *sector = ntohlp(p+6)+150;
	  if (length)
	    *length = ntohlp(p+14);
	  return true;
	}
      len -= *p;
      p += *p;
    }
  }
  return false;
}

bool isofs_find_file(isofs i, const char *filename,
		     uint32_t *sector, uint32_t *length)
{
  return isofs_find_entry(i, filename, sector, length, strlen(filename),
			  i->root_lba, i->root_length, 0);
}

uint32_t isofs_get_bootsector(isofs i, uint32_t n)
{
  return i->datatrack_lba + n;
}

void isofs_delete(isofs i)
{
  if (i) {
    free(i);
  }
}

isofs isofs_new(msglogger logger, datasource ds)
{
  isofs i = calloc(1, sizeof(struct isofs_s));
  if (i) {
    i->logger = logger;
    i->ds = ds;
    if (isofs_find_datatrack(i)) {
      if (isofs_find_root_directory(i)) {
	return i;
      } else
	msglog_error(logger, "No PVD found");
    } else
      msglog_error(logger, "No suitable data track found");
    isofs_delete(i);
  } else
    msglog_oomerror(logger);
  return NULL;
}
