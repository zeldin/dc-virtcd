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
#include "datasource.h"

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
  bool (*read_sector)(realization r, uint32_t sector, uint8_t *buffer);
  bool (*get_toc)(realization r, int session, dc_toc *toc);
};

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

static void realization_delete(realization r)
{
  if (r) {
    if (r->iso)
      isofile_delete(r->iso);
    if (r->data)
      datafile_delete(r->data);
    free(r);
  }
}

static bool realization_setup_disc(realization r, datasource ds)
{
  if ((r->data = datafile_new_from_filename(r->logger, ds->filename)) == NULL)
    return false;

  if ((r->iso = isofile_new(r->logger, r->data)) == NULL)
    return false;

  r->read_sector = realization_read_sector_from_isofile;
  r->get_toc = realization_get_toc_from_isofile;

  return true;
}

static realization realization_new(msglogger logger)
{
  realization r = calloc(1, sizeof(struct realization_s));
  if (r) {
    r->logger = logger;
    r->data = NULL;
    r->iso = NULL;
    r->read_sector = NULL;
    r->get_toc = NULL;
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
