#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "msglog.h"
#include "toc.h"
#include "datafile.h"
#include "isofile.h"
#include "nrgfile.h"
#include "datasource.h"
#include "isofsparser.h"

#define MAX_1ST_READ_SIZE (16*1024*1024)

#define DESCRAMBLE_CHUNK (2048*1024)
#define MAX_DESCRAMBLE_CHUNK_COUNT ((MAX_1ST_READ_SIZE+DESCRAMBLE_CHUNK-1)/DESCRAMBLE_CHUNK)

typedef struct descrambler_buffer_s {
  uint16_t idx[DESCRAMBLE_CHUNK/32];
  uint8_t secbuf[2048];
  uint8_t dscrambuf[DESCRAMBLE_CHUNK];
} *descrambler_buffer;

typedef struct descrambler_s {
  descrambler_buffer buffer;
  uint32_t offs, len;
  bool last;
  uint16_t seed[MAX_DESCRAMBLE_CHUNK_COUNT];
  bool seed_valid[MAX_DESCRAMBLE_CHUNK_COUNT];
} *descrambler;

typedef struct realization_s *realization;

struct datasource_s {
  msglogger logger;
  realization realization;
  unsigned realize_count;
  char *filename;
};

struct realization_s {
  msglogger logger;
  datafile data;
  isofile iso;
  nrgfile nrg;
  descrambler dscram;
  uint32_t bootsector0;
  uint32_t bootfile_sector, bootfile_length;
  bool (*read_sector)(realization r, uint32_t sector, uint8_t *buffer);
  bool (*get_toc)(realization r, int session, dc_toc *toc);
  bool (*get_ipbin)(realization r, uint32_t n, uint8_t *buffer);
  int32_t (*get_1st_read_size)(realization r);
  bool (*get_1st_read)(realization r, uint32_t n, uint8_t *buffer);
};

static descrambler descrambler_new(msglogger logger)
{
  descrambler d = calloc(1, sizeof(struct descrambler_s));
  if (d) {
    int i;
    d->buffer = NULL;
    d->offs = 0;
    d->len = 0;
    d->last = false;
    for(i=0; i<MAX_DESCRAMBLE_CHUNK_COUNT; i++)
      d->seed_valid[i] = false;
    return d;
  } else
    msglog_oomerror(logger);
  return NULL;
}

static void descrambler_delete(descrambler d)
{
  if (d) {
    if (d->buffer)
      free(d->buffer);
    free(d);
  }
}

static bool realization_read_sector_from_isofile(realization r,
						 uint32_t sector,
						 uint8_t *buffer)
{
  return isofile_read_sector(r->iso, sector, buffer);
}

static bool realization_get_toc_from_isofile(realization r, int session,
					     dc_toc *toc)
{
  return isofile_get_toc(r->iso, session, toc);
}

static bool realization_read_sector_from_nrgfile(realization r,
						 uint32_t sector,
						 uint8_t *buffer)
{
  return nrgfile_read_sector(r->nrg, sector, buffer);
}

static bool realization_get_toc_from_nrgfile(realization r, int session,
					     dc_toc *toc)
{
  return nrgfile_get_toc(r->nrg, session, toc);
}

static bool realization_get_ipbin_from_datasource(realization r,
						  uint32_t n, uint8_t *buffer)
{
  return r->read_sector(r, r->bootsector0+n, buffer);
}

static int32_t realization_get_1st_read_size_generic(realization r)
{
  return r->bootfile_length;
}

static bool realization_get_1st_read_from_datasource(realization r,
						     uint32_t n, uint8_t *buffer)
{
  return r->read_sector(r, r->bootfile_sector+n, buffer);
}

static void realization_delete(realization r)
{
  if (r) {
    if (r->dscram)
      descrambler_delete(r->dscram);
    if (r->iso)
      isofile_delete(r->iso);
    if (r->nrg)
      nrgfile_delete(r->nrg);
    if (r->data)
      datafile_delete(r->data);
    free(r);
  }
}

static uint16_t compute_descrambling(uint16_t seed, uint32_t slice,
				     uint16_t *idx)
{
  int32_t i;

  slice >>= 5;

  for(i = 0; i < slice; i++)
    idx[i] = i;

  for(i = slice-1; i >= 0; --i) {

    seed = (seed * 2109 + 9273) & 0x7fff;

    uint16_t x = (((seed + 0xc000) & 0xffff) * (uint32_t)i) >> 16;

    uint16_t tmp = idx[i];
    idx[i] = idx[x];
    idx[x] = tmp;
  }

  return seed;
}

static bool realization_fill_and_descramble(realization r)
{
  descrambler d = r->dscram;
  uint32_t n = d->offs;
  uint8_t *buffer = d->buffer->dscrambuf;
  uint32_t cnt = d->len;
  int chunk = n / (DESCRAMBLE_CHUNK>>11);
  uint16_t *idx = d->buffer->idx;
  uint8_t *secbuf = d->buffer->secbuf;

  if (!d->seed_valid[chunk]) {
    int i;
    for (i=0; i<chunk; i++)
      if (d->seed_valid[i] && !d->seed_valid[i+1]) {
	d->seed[i+1] = compute_descrambling(d->seed[i], DESCRAMBLE_CHUNK, idx);
	d->seed_valid[i+1] = true;
      }
    if (!d->seed_valid[chunk]) {
      msglog_error(r->logger, "Unable to compute seed for chunk %d!", chunk);
      return false;
    }
  }

  uint16_t seed = d->seed[chunk];
  uint32_t slice = DESCRAMBLE_CHUNK;
  while (cnt >= 2048)
    if (slice > cnt)
      slice >>= 1;
    else {
      seed = compute_descrambling(seed, slice, idx);
      int32_t x = (slice>>5)-1;
      uint32_t disp = 0;
      while (x>=0) {
	if (!((~x)&((2048>>5)-1))) {
	  if (!r->get_1st_read(r, n++, secbuf))
	    return false;
	  disp = 0;
	}
	memcpy(buffer+32*idx[x], secbuf+disp, 32);
	disp += 32;
	--x;
      }
      buffer += slice;
      cnt -= slice;
    }
  if (cnt > 0) {
    uint32_t disp = 0;
    if (!r->get_1st_read(r, n, secbuf))
      return false;
    while (cnt >= 32)
      if (slice > cnt)
	slice >>= 1;
      else {
	seed = compute_descrambling(seed, slice, idx);
	int32_t x = (slice>>5)-1;
	while (x>=0) {
	  memcpy(buffer+32*idx[x], secbuf+disp, 32);
	  disp += 32;
	  --x;
	}
	buffer += slice;
	cnt -= slice;
      }
    if (cnt > 0)
      memcpy(buffer, secbuf+disp, cnt);
  }

  if (!d->last) {
    d->seed[chunk+1] = seed;
    d->seed_valid[chunk+1] = true;
  }
  return true;
}

static bool realization_descramble_1st_read(realization r, uint32_t n, uint8_t *buffer)
{
  descrambler d = r->dscram;
  if (d->buffer == NULL) {
    if((d->buffer = malloc(sizeof(struct descrambler_buffer_s))) == NULL) {
      msglog_oomerror(r->logger);
      return false;
    }
    msglog_debug(r->logger, "Allocated a descrambling buffer");
    d->offs = 0;
    d->len = 0;
    d->last = false;
  }

  if (n < d->offs || ((n - d->offs)<<11) >= d->len) {
    d->offs = n & ~((DESCRAMBLE_CHUNK>>11)-1);
    d->len = 0;
    d->last = false;
    int32_t totsize = r->get_1st_read_size(r);
    if (totsize < 0 || ((uint32_t)totsize) <= (n << 11)) {
      msglog_error(r->logger, "Descrambling after end of 1ST_READ.BIN");
      return false;
    }
    if (((uint32_t)totsize) > ((d->offs << 11)+DESCRAMBLE_CHUNK))
      d->len = DESCRAMBLE_CHUNK;
    else {
      d->len = ((uint32_t)totsize) - (d->offs << 11);
      d->last = true;
    }
    if (!realization_fill_and_descramble(r)) {
      d->offs = 0;
      d->len = 0;
      d->last = false;
      return false;
    }
  }

  n -= d->offs;
  if (d->last && (n<<11)+2048 >= d->len) {
    memset(buffer, 0, 2048);
    memcpy(buffer, d->buffer->dscrambuf + (n<<11), d->len - (n<<11));
    free(d->buffer);
    msglog_debug(r->logger, "Freed the descrambling buffer");
    d->buffer = NULL;
    d->offs = 0;
    d->len = 0;
    d->last = false;
  } else
    memcpy(buffer, d->buffer->dscrambuf + (n<<11), 2048);
  return true;
}

static bool realization_setup_fs(realization r, datasource ds, isofs fs)
{
  uint8_t ip0000[2048];
  char bootname[20];
  int namelen;
  r->bootsector0 = isofs_get_bootsector(fs, 0);
  if (!datasource_read_sector(ds, r->bootsector0, ip0000))
    return false;
  if (memcmp(ip0000, "SEGA SEGAKATANA ", 16)) {
    msglog_error(r->logger, "Invalid bootsector");
    return false;
  }
  r->get_ipbin = realization_get_ipbin_from_datasource;
  memcpy(bootname, ip0000+0x60, 16);
  for(namelen=16; namelen>0; --namelen)
    if(bootname[namelen-1] != ' ')
      break;
  bootname[namelen] = 0;
  msglog_debug(r->logger, "Boot filename: \"%s\"", bootname);
  if (!isofs_find_file(fs, bootname,
		       &r->bootfile_sector, &r->bootfile_length)) {
    msglog_error(r->logger, "Boot file \"%s\" not found!", bootname);
    return false;
  }
  r->get_1st_read_size = realization_get_1st_read_size_generic;
  r->get_1st_read = realization_get_1st_read_from_datasource;
  msglog_debug(r->logger, "Found file at LBA %lu, size is %lu bytes",
	       (unsigned long)r->bootfile_sector,
	       (unsigned long)r->bootfile_length);
  return true;
}

static bool realization_setup_descrambling(realization r)
{
  /* FIXME */
  if (false)
    /* No descrambling needed */
    return true;

  if ((r->dscram = descrambler_new(r->logger)) == NULL) {
      msglog_oomerror(r->logger);
      return false;
  }

  r->dscram->seed[0] = r->get_1st_read_size(r) & 0xffff;
  r->dscram->seed_valid[0] = true;

  return true;
}

static bool realization_setup_disc(realization r, datasource ds)
{
  isofs fs;
  bool result = false;

  if ((r->data = datafile_new_from_filename(r->logger, ds->filename)) == NULL)
    return false;

  if (nrgfile_check(r->logger, r->data)) {

    if ((r->nrg = nrgfile_new(r->logger, r->data)) == NULL)
      return false;

    r->read_sector = realization_read_sector_from_nrgfile;
    r->get_toc = realization_get_toc_from_nrgfile;

  } else {

    if ((r->iso = isofile_new(r->logger, r->data)) == NULL)
      return false;

    r->read_sector = realization_read_sector_from_isofile;
    r->get_toc = realization_get_toc_from_isofile;

  }

  fs = isofs_new(r->logger, ds);
  if (fs != NULL) {
    result = realization_setup_fs(r, ds, fs);
    isofs_delete(fs);
  }

  if (result)
    if (r->get_1st_read_size(r) > MAX_1ST_READ_SIZE) {
      msglog_error(r->logger, "Main binary is too lage (>16MiB)");
      result = false;
    } else
      result = realization_setup_descrambling(r);

  return result;
}

static realization realization_new(msglogger logger)
{
  realization r = calloc(1, sizeof(struct realization_s));
  if (r) {
    r->logger = logger;
    r->data = NULL;
    r->iso = NULL;
    r->nrg = NULL;
    r->dscram = NULL;
    r->read_sector = NULL;
    r->get_toc = NULL;
    r->get_ipbin = NULL;
    r->get_1st_read_size = NULL;
    r->get_1st_read = NULL;
    return r;
  } else
    msglog_oomerror(logger);
  return NULL;
}

bool datasource_realize(datasource ds)
{
  if (ds->realize_count) {
    ds->realize_count++;
    return true;
  }
  if ((ds->realization = realization_new(ds->logger)) == NULL)
    return false;
  if (!realization_setup_disc(ds->realization, ds)) {
    realization_delete(ds->realization);
    ds->realization = NULL;
    return false;
  }
  ds->realize_count = 1;
  return true;
}

void datasource_unrealize(datasource ds)
{
  if (!ds->realize_count) {
    msglog_warning(ds->logger, "Unrealized already unrealized datasource");
    return;
  }
  if (!--ds->realize_count) {
    realization_delete(ds->realization);
    ds->realization = NULL;
  }
}

bool datasource_read_sector(datasource ds, uint32_t sector, uint8_t *buffer)
{
  if (ds->realization)
    return ds->realization->read_sector(ds->realization, sector, buffer);
  else {
    msglog_error(ds->logger, "Read sector on unrealized datasource");
    return false;
  }
}

extern bool datasource_get_toc(datasource ds, int session, dc_toc *toc)
{
  if (ds->realization)
    return ds->realization->get_toc(ds->realization, session, toc);
  else {
    msglog_error(ds->logger, "Get TOC on unrealized datasource");
    return false;
  }
}

extern bool datasource_get_ipbin(datasource ds, uint32_t n, uint8_t *buffer)
{
  if (ds->realization)
    return ds->realization->get_ipbin(ds->realization, n, buffer);
  else {
    msglog_error(ds->logger, "Get IP.BIN on unrealized datasource");
    return false;
  }
}

extern int32_t datasource_get_1st_read_size(datasource ds)
{
  if (ds->realization)
    return ds->realization->get_1st_read_size(ds->realization);
  else {
    msglog_error(ds->logger, "Get 1ST_READ.BIN size on unrealized datasource");
    return -1;
  }
}

extern bool datasource_get_1st_read(datasource ds, uint32_t n, uint8_t *buffer)
{
  if (ds->realization) {
    if (ds->realization->dscram)
      return realization_descramble_1st_read(ds->realization, n, buffer);
    else
      return ds->realization->get_1st_read(ds->realization, n, buffer);
  } else {
    msglog_error(ds->logger, "Get 1ST_READ.BIN on unrealized datasource");
    return false;
  }
}

void datasource_delete(datasource ds)
{
  if (ds) {
    if (ds->realize_count > 0)
      msglog_warning(ds->logger, "Deleting datasource with realize_count > 0");
    if (ds->realization)
      realization_delete(ds->realization);
    if (ds->filename)
      free(ds->filename);
    free(ds);
  }
}

datasource datasource_new_from_filename(msglogger logger, const char *filename)
{
  datasource ds = calloc(1, sizeof(struct datasource_s));
  if (ds) {
    ds->logger = logger;
    ds->realization = NULL;
    if ((ds->filename = strdup(filename)) != NULL)
      return ds;
    else
      msglog_oomerror(logger);
    datasource_delete(ds);
  } else
    msglog_oomerror(logger);
  return NULL;
}
