#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "msglog.h"
#include "toc.h"
#include "datafile.h"
#include "nrgfile.h"
#include "bswap.h"

#define MAX_TRACKS 100

#ifdef WORDS_BIGENDIAN
#define MKID(a,b,c,d) (((a)<<24)|((b)<<16)|((c)<<8)|(d))
#else
#define MKID(a,b,c,d) (((d)<<24)|((c)<<16)|((b)<<8)|(a))
#endif

typedef struct nrgtrack_s {
  uint32_t first_sector, last_sector;
  uint32_t secsize, dataoffs, secskip;
} nrgtrack;

struct nrgfile_s {
  msglogger logger;
  datafile data;
  dc_toc toc;
  nrgtrack track[99], current_track;
};

static bool nrgfile_read_cuex(nrgfile n, uint32_t offs, uint32_t sz)
{
  struct { uint8_t mm, nn, ii, zz; uint32_t lba; } cuex[2*MAX_TRACKS+2];
  int i, cnt;
  if (sz > sizeof(cuex) || sz < 16 || (sz & 15)) {
    msglog_error(n->logger, "Invalid length of CUEX chunk");
    return false;
  }
  if (!datafile_read(n->data, offs, sz, cuex))
    return false;
  cnt = sz>>3;
#ifndef WORDS_BIGENDIAN
  for (i=0; i<cnt; i++)
    cuex[i].lba = SWAP32(cuex[i].lba);
#endif
  if (cuex[0].nn != 0x00 || cuex[cnt-1].nn != 0xaa ||
      cuex[0].ii != 0x00 || cuex[cnt-1].ii != 0x01) {
    msglog_error(n->logger, "Incorrect header/footer of CUEX chunk");
    return false;
  }
  for (i=0; i<cnt; i++) {
    if (cuex[i].ii == 0x01)
      if (cuex[i].nn == 0xaa) {
	n->toc.dunno = (cuex[i].mm << 24)|(cuex[i].lba + 150);
      } else {
	if (cuex[i].nn != 0&& 
	    (cuex[i].nn&0xf0)<0xa0 && (cuex[i].nn&0x0f)<0x0a) {
	  int entry = (cuex[i].nn >> 4)*10 + (cuex[i].nn & 0xf);
	  n->toc.entry[entry-1] = (cuex[i].mm << 24)|(cuex[i].lba + 150);
	  if (entry < GET_DC_TOC_TRACK(n->toc.first))
	    n->toc.first = (cuex[i].mm << 24)|MAKE_DC_TOC_TRACK(entry);
	  if (entry > GET_DC_TOC_TRACK(n->toc.last))
	    n->toc.last = (cuex[i].mm << 24)|MAKE_DC_TOC_TRACK(entry);
	  n->track[entry-1].first_sector = cuex[i].lba + 150;
	  n->track[entry-1].last_sector = cuex[i+1].lba + 150;
	} else {
	  msglog_error(n->logger, "Invalid track number %x in CUEX chunk",
		       cuex[i].nn);
	  return false;
	}
      }
  }
  return true;
}

static bool nrgfile_read_daox(nrgfile n, uint32_t offs, uint32_t sz)
{
  uint8_t daox[22+42*MAX_TRACKS];
  const uint8_t *trk;
  int i, cnt;
  if (sz > sizeof(daox) || sz < 22 || ((sz - 22) % 42)) {
    msglog_error(n->logger, "Invalid length of DAOX chunk");
    return false;
  }
  if (!datafile_read(n->data, offs, sz, daox))
    return false;
  cnt = (sz - 22)/42;
  trk = daox+22;
  if (cnt != daox[21]-daox[20]+1) {
    msglog_error(n->logger, "Invalid length of DAOX chunk");
    return false;
  }
  for (i=0; i<cnt; i++) {
#ifndef WORDS_BIGENDIAN
    int j;
#endif
    uint32_t trkdata[8];
    memcpy(trkdata, trk+10, sizeof(trkdata));
#ifndef WORDS_BIGENDIAN
    for (j=0; i<8; j++)
      trkdata[i] = SWAP32(trkdata[i]x);
#endif
    if (trkdata[2] || trkdata[4] || trkdata[6]) {
      /* We only handle 32-bit offsets */
      msglog_error(n->logger, "Invalid file offset in DAOX");
      return false;
    }
    int entry = daox[20]+i;
    if (entry >= 1 && entry <= 99) {
      n->track[entry-1].secsize  = trkdata[0];
      n->track[entry-1].dataoffs = trkdata[5];
    } else {
      msglog_error(n->logger, "Invalid track number %d in DAOX chunk", entry);
      return false;
    }
    trk += 42;
  }
  return true;
}

static bool nrgfile_read_metadata(nrgfile n, uint32_t offs)
{
  size_t sz = datafile_size(n->data);
  for (;;) {
    uint32_t chdr[2];
    if (!datafile_read(n->data, offs, sizeof(chdr), chdr))
      return false;
#ifndef WORDS_BIGENDIAN
    chdr[1] = SWAP32(chdr[1]);
#endif
    if (chdr[0] == MKID('E','N','D','!') && chdr[1] == 0)
      return true;

    offs += 8;
    if (chdr[1] > sz - offs) {
      msglog_error(n->logger, "Corrupt NRG5 footer");
      return false;
    }

    switch(chdr[0]) {
    case MKID('C','U','E','S'):
    case MKID('D','A','O','I'):
      msglog_error(n->logger, "CUES/DAOI chunk not supported");
      return false;

    case MKID('C','U','E','X'):
      if (!nrgfile_read_cuex(n, offs, chdr[1]))
	return false;
      break;

    case MKID('D','A','O','X'):
      if (!nrgfile_read_daox(n, offs, chdr[1]))
	return false;
      break;

    case MKID('S','I','N','F'):
    case MKID('E','T','N','F'):
    case MKID('E','T','N','2'):
    case MKID('M','T','Y','P'):
      /* ignored */
      break;

    case MKID('E','N','D','!'):
      msglog_error(n->logger, "Broken END! chunk found");
      return false;

    default:
      msglog_error(n->logger, "Unexpected NRG chunk \"%.4s\"",
		   (const char *)&chdr[0]);
      return false;
    }

    offs += chdr[1];
  }
}

static uint32_t nrgfile_low_check(msglogger logger, datafile data, bool complain)
{
  uint32_t n5[3];
  size_t sz = datafile_size(data);
  if (sz < 12) {
    if (complain)
      msglog_error(logger, "NRG file too short");
    return 0;
  }
  if (!datafile_read(data, sz-12, sizeof(n5), n5))
    return 0;
  if (memcmp(n5, "NER5", 4)) {
    if (complain)
      msglog_error(logger, "Not a NRG5 file");
    return 0;
  }
#ifndef WORDS_BIGENDIAN
  n5[2] = SWAP32(n5[2]);
#endif
  if (n5[1] || !n5[2] || n5[2] >= sz-12) {
    if (complain)
      msglog_error(logger, "Invalid NRG5 footer");
    return 0;
  }
  return n5[2];
}

static bool nrgfile_find_track(nrgfile n, uint32_t sector)
{
  int i;
  for(i=0; i<99; i++)
    if(n->track[i].secsize && n->track[i].first_sector <= sector &&
       n->track[i].last_sector > sector) {
      uint32_t secskip;
      switch(n->track[i].secsize) {
      case 2048:
	secskip = 0;
	break;
      case 2056:
      case 2336:
	secskip = 8;
	break;
      default:
	msglog_warning(n->logger, "Attempt to read from track with "
		       "unsupported sector size (%d)", n->track[i].secsize);
	return false;
      }
      n->current_track = n->track[i];
      n->current_track.secskip = secskip;
      return true;
    }
  return false;
}

void nrgfile_delete(nrgfile n)
{
  if (n) {
    free(n);
  }
}

nrgfile nrgfile_new(msglogger logger, datafile data)
{
  nrgfile n = calloc(1, sizeof(struct nrgfile_s));
  uint32_t metadata;
  if (n) {
    n->logger = logger;
    n->data = data;
    memset(&n->toc, 0, sizeof(n->toc));
    n->toc.first = MAKE_DC_TOC_TRACK(0xaa);
    memset(n->track, 0, sizeof(n->track));
    memset(&n->current_track, 0, sizeof(n->current_track));
    if ((metadata = nrgfile_low_check(n->logger, n->data, true))) {
      if (nrgfile_read_metadata(n, metadata))
	return n;
    }
    nrgfile_delete(n);
  } else
    msglog_oomerror(logger);
  return NULL;
}

bool nrgfile_check(msglogger logger, datafile data)
{
  return nrgfile_low_check(logger, data, false) != 0;
}

bool nrgfile_read_sector(nrgfile n, uint32_t sector, uint8_t *buffer)
{
  if (sector < n->current_track.first_sector ||
      sector >= n->current_track.last_sector) {
    if (!nrgfile_find_track(n, sector))
      return false;
  }
  return datafile_read(n->data, n->current_track.dataoffs +
		       (sector - n->current_track.first_sector) *
		       n->current_track.secsize +
		       n->current_track.secskip, 2048, buffer);
}

bool nrgfile_get_toc(nrgfile n, int session, dc_toc *toc)
{
  if (session == 0) {
    *toc = n->toc;
    return true;
  } else
    return false;
}
