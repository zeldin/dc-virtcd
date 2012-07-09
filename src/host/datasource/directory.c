#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "msglog.h"
#include "toc.h"
#include "datafile.h"
#include "directory.h"
#include "bswap.h"

#define BASE_SECTOR 45150

typedef struct direntry_s *direntry;
typedef struct dirnode_s *dirnode;

typedef struct datanode_s {
  uint32_t first_sector, num_sectors;
  bool (*read_sector)(directory d, uint32_t sector, uint8_t *buffer, struct datanode_s *node);
} *datanode;

struct direntry_s {
  struct datanode_s node;
  direntry next;
  char *path;
  dirnode dirnode;
  uint32_t size;
};

struct dirnode_s {
  struct datanode_s node;
  direntry first;
  uint8_t *dirdata;
};

struct directory_s {
  msglogger logger;
  dirnode rootdir;
  datanode current;
  datafile current_file;
  datanode ip_bin;
  uint32_t fs_num_sectors;
};

static void dirnode_delete(dirnode dn);
static dirnode dirnode_new(directory d, const char *path);

static void direntry_delete(direntry de)
{
  if (de) {
    if (de->dirnode)
      dirnode_delete(de->dirnode);
    if (de->path)
      free(de->path);
    free(de);
  }
}

static bool direntry_read_sector(directory d, uint32_t sector,
				 uint8_t *buffer, struct datanode_s *node)
{
  direntry de = (direntry)node;
  if (d->current_file == NULL)
    if ((d->current_file =
	 datafile_new_from_filename(d->logger, de->path)) == NULL)
      return false;
  if (sector >= 16) /* special case for IP.BIN */
    sector -= de->node.first_sector;
  sector <<= 11;
  if (sector < de->size && sector+2048 > de->size) {
    /* final sector */
    uint32_t len = de->size - sector;
    memset(buffer+len, 0, 2048-len);
    return datafile_read(d->current_file, sector, len, buffer);
  } else
    return datafile_read(d->current_file, sector, 2048, buffer);
}

static direntry direntry_new(directory d, const char *base_path,
			     struct dirent *dire)
{
  msglogger logger = d->logger;
  direntry de = calloc(1, sizeof(struct direntry_s));
  if (de) {
    struct stat buf;
    de->next = NULL;
    de->node.read_sector = direntry_read_sector;
    if ((de->path = malloc(strlen(base_path)+strlen(dire->d_name)+4))) {
      sprintf(de->path, "%s/%s", base_path, dire->d_name);
      if (stat(de->path, &buf)>=0) {
	de->size = buf.st_size;
	de->node.num_sectors = (buf.st_size + 2047)>>11;
	if (!S_ISDIR(buf.st_mode) ||
	    (de->dirnode = dirnode_new(d, de->path)))
	  return de;
      } else
	msglog_perror(logger, de->path);
    } else
      msglog_oomerror(logger);
    direntry_delete(de);
  } else
    msglog_oomerror(logger);
  return NULL;
}

static void dirnode_delete(dirnode dn)
{
  if (dn) {
    direntry de;
    while((de = dn->first)) {
      dn->first = de->next;
      direntry_delete(de);
    }
    if (dn->dirdata != NULL)
      free(dn->dirdata);
    free(dn);
  }
}

static bool skip_dirent(struct dirent *dire)
{
  if (dire->d_name[0] == '.' &&
      (dire->d_name[1] == 0 ||
       (dire->d_name[1] == '.' && dire->d_name[2] == 0)))
    return true;
  return false;
}

static bool dirnode_read_sector(directory d, uint32_t sector,
				uint8_t *buffer, struct datanode_s *node)
{
  dirnode dn = (dirnode)node;
  memcpy(buffer, dn->dirdata+((sector-dn->node.first_sector)<<11), 2048);
  return true;
}

static datanode dirnode_find_sector(directory d, dirnode dn, uint32_t sector)
{
  direntry de;
  if (sector >= dn->node.first_sector &&
      sector - dn->node.first_sector < dn->node.num_sectors)
    return &dn->node;
  for (de = dn->first; de != NULL; de = de->next)
    if (sector >= de->node.first_sector &&
	sector - de->node.first_sector < de->node.num_sectors) {
      if (de->dirnode)
	return dirnode_find_sector(d, de->dirnode, sector);
      return &de->node;
    }
  return NULL;
}

static void make_iso_name(char *buf, const char *src)
{
  const char *slash = strrchr(src, '/');
  int cnt = 0;
  if (slash)
    src = slash+1;
  while (cnt < 31 && *src) {
    int ch = *src++;
    *buf++ = toupper(ch);
    cnt++;
  }
  *buf = 0;
  if (!strchr(buf, ';')) {
    *buf++ = ';';
    *buf++ = '1';
    *buf = 0;
  }
}

static void dirnode_make_record(uint8_t *data, uint32_t sec, uint32_t len,
				uint8_t ty, uint8_t nl, const char *name)
{
  uint8_t del = (nl+34)&~1;
  memset(data, 0, del);
  sec += BASE_SECTOR-150;
  data[0] = del;
#ifdef WORDS_BIGENDIAN
  memcpy(data+6, &sec, 4);
  memcpy(data+14, &len, 4);
  sec = SWAP32(sec);
  len = SWAP32(len);
  memcpy(data+2, &sec, 4);
  memcpy(data+10, &len, 4);
#else
  memcpy(data+2, &sec, 4);
  memcpy(data+10, &len, 4);
  sec = SWAP32(sec);
  len = SWAP32(len);
  memcpy(data+6, &sec, 4);
  memcpy(data+14, &len, 4);
#endif
  data[18] = 0x64;
  data[19] = 1;
  data[20] = 1;
  data[25] = ty;
  data[32] = nl;
  if (nl)
    memcpy(data+33, name, nl);
}

static uint32_t dirnode_make_dirdata(directory d, dirnode dn, uint32_t sector,
				     uint8_t *data, dirnode parent)
{
  direntry de;
  uint32_t cnt = 0, offs = 68;
  if (!parent)
    parent = dn;
  if (data) {
    dirnode_make_record(data, dn->node.first_sector, dn->node.num_sectors<<11,
			2, 1, "\0");
    dirnode_make_record(data+34, parent->node.first_sector,
			parent->node.num_sectors<<11, 2, 1, "\1");
  }
  for (de = dn->first; de != NULL; de = de->next) {
    char iso_name[40];
    uint8_t nl, del, et;
    uint32_t ln;
    if (de->dirnode) {
      et = 2;
      ln = de->dirnode->node.num_sectors << 11;
    } else {
      et = 0;
      ln = de->size;
    }
    make_iso_name(iso_name, de->path);
    nl = strlen(iso_name);
    del = (nl+34)&~1;
    if (offs + del > 2048) {
      if (data) {
	memset(data+offs, 0, 2048-offs);
	data += 2048;
      }
      offs = 0;
      cnt++;
    }
    if (data)
      dirnode_make_record(data+offs, de->node.first_sector, ln, et, nl,
			  iso_name);
    offs += del;
  }
  if (offs) {
    if (data)
      memset(data+offs, 0, 2048-offs);
    cnt++;
  }
  return cnt;
}

static uint32_t dirnode_layout(directory d, dirnode dn, uint32_t sector,
			       dirnode parent)
{
  direntry de;
  uint32_t size = dirnode_make_dirdata(d, dn, sector, NULL, parent);
  dn->node.first_sector = sector;
  dn->node.num_sectors = size;
  for (de = dn->first; de != NULL; de = de->next) {
    if (de->dirnode) {
      de->node.num_sectors = dirnode_layout(d, de->dirnode, sector+size, dn);
      if (!de->node.num_sectors)
	return 0;
    }
    de->node.first_sector = sector+size;
    size += de->node.num_sectors;
  }
  if (dn->dirdata != NULL)
    free(dn->dirdata);
  if (!(dn->dirdata = malloc(dn->node.num_sectors<<11))) {
      msglog_oomerror(d->logger);
      return 0;
  }
  dirnode_make_dirdata(d, dn, sector, dn->dirdata, parent);
  return size;
}

static dirnode dirnode_new(directory d, const char *path)
{
  msglogger logger = d->logger;
  dirnode dn = calloc(1, sizeof(struct dirnode_s));
  if (dn) {
    DIR *dir;
    struct dirent *dire;
    dn->first = NULL;
    dn->dirdata = NULL;
    dn->node.read_sector = dirnode_read_sector;
    if ((dir = opendir(path))) {
      while((dire = readdir(dir))) {
	if (skip_dirent(dire))
	  continue;
	direntry de = direntry_new(d, path, dire);
	if (!de) {
	  dirnode_delete(dn);
	  closedir(dir);
	  return NULL;
	}
	de->next = dn->first;
	dn->first = de;
      }
      closedir(dir);
      return dn;
    } else
      msglog_perror(logger, path);
    dirnode_delete(dn);
  } else
    msglog_oomerror(logger);
  return NULL;
}

void directory_delete(directory d)
{
  if (d) {
    if (d->rootdir)
      dirnode_delete(d->rootdir);
    if (d->current_file)
      datafile_delete(d->current_file);
    free(d);
  }
}

static datanode directory_find_sector(directory d, uint32_t sector)
{
  if (d->current_file != NULL) {
    datafile_delete(d->current_file);
    d->current_file = NULL;
  }
  if (sector >= d->rootdir->node.first_sector &&
      sector - d->rootdir->node.first_sector < d->fs_num_sectors)
    return dirnode_find_sector(d, d->rootdir, sector);
  else if (sector < 16 && d->ip_bin && sector < d->ip_bin->num_sectors)
    return d->ip_bin;
  else
    return NULL;
}

static datanode directory_find_ip_bin(directory d)
{
  direntry de;
  for (de = d->rootdir->first; de != NULL; de = de->next)
    if (!de->dirnode) {
      char iso_name[40];
      make_iso_name(iso_name, de->path);
      if (!strcmp(iso_name, "IP.BIN;1"))
	return &de->node;
    }
  return NULL;
}

directory directory_new(msglogger logger, const char *dirname)
{
  directory d = calloc(1, sizeof(struct directory_s));
  if (d) {
    d->logger = logger;
    d->current = NULL;
    d->current_file = NULL;
    if ((d->rootdir = dirnode_new(d, dirname)) &&
	(d->fs_num_sectors = dirnode_layout(d, d->rootdir, 20, NULL))) {
      d->ip_bin = directory_find_ip_bin(d);
      return d;
    }
    directory_delete(d);
  } else
    msglog_oomerror(logger);
  return NULL;
}

bool directory_check(msglogger logger, const char *dirname)
{
  struct stat buf;
  if (stat(dirname, &buf)<0) {
    msglog_perror(logger, dirname);
    return false;
  }
  return S_ISDIR(buf.st_mode);
}

static bool directory_create_pvd(directory d, uint8_t *buffer)
{
  memset(buffer, 0, 2048);
  buffer[0] = 1;
  strcpy(buffer+1, "CD001");
  buffer[6] = 1;
  strcpy(buffer+8, "Solaris");
  memset(buffer+15, ' ', 0x28-15);
  strcpy(buffer+0x28, "CDROM");
  memset(buffer+0x2d, ' ', 0x48-0x2d);
  buffer[0x50] = 0x1c;
  buffer[0x57] = 0x1c;
  buffer[0x78] = 1;
  buffer[0x7b] = 1;
  buffer[0x7c] = 1;
  buffer[0x7f] = 1;
  buffer[0x81] = 8;
  buffer[0x82] = 8;
  buffer[0x84] = 10;
  buffer[0x8b] = 10;
  buffer[0x8c] = 0x12;
  buffer[0x97] = 0x14;
  memcpy(buffer+156, d->rootdir->dirdata, 34);
  memset(buffer+0xbe, ' ', 0x23e -0xbe);
  strcpy(buffer+0x23e, "MKISOFS ISO 9660 FILESYSTEM BUILDER & "
	 "CDRECORD CD-R/DVD CREATOR");
  memset(buffer+0x27d, ' ', 0x32d-0x27d);
  strcpy(buffer+0x32d, "2001121616590900");
  buffer[0x33d] = 4;
  strcpy(buffer+0x33e, "2001121616590900");
  buffer[0x34e] = 4;
  strcpy(buffer+0x34f, "0000000000000000");
  strcpy(buffer+0x360, "2001121616590900");
  buffer[0x370] = 4;
  buffer[0x371] = 1;
  memset(buffer+0x373, ' ', 0x573-0x373);
}

bool directory_read_sector(directory d, uint32_t sector, uint8_t *buffer)
{
  if (sector >= BASE_SECTOR) {
    sector -= BASE_SECTOR;
    if (sector == 16)
      return directory_create_pvd(d, buffer);
    if ((d->current &&
	 sector >= d->current->first_sector &&
	 (sector - d->current->first_sector) < d->current->num_sectors) ||
	(d->current = directory_find_sector(d, sector)))
      return d->current->read_sector(d, sector, buffer, d->current);
  }
  msglog_warning(d->logger, "read from unassigned sector %d", sector);
  return false;
}

bool directory_get_toc(directory d, int session, dc_toc *toc)
{
  memset(toc, 0, sizeof(*toc));
  toc->entry[0] = MAKE_DC_TOC_ENTRY(150, 1, 0);
  toc->entry[1] = MAKE_DC_TOC_ENTRY(BASE_SECTOR, 1, 4);
  toc->dunno = MAKE_DC_TOC_ENTRY(BASE_SECTOR+d->rootdir->node.first_sector+
				 d->fs_num_sectors, 1, 4);
  toc->first = MAKE_DC_TOC_TRACK(1);
  toc->last = MAKE_DC_TOC_TRACK(2);
  return true;
}

