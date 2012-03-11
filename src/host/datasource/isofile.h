typedef struct isofile_s *isofile;
extern void isofile_delete(isofile i);
extern isofile isofile_new(msglogger logger, datafile data);
extern bool isofile_read_sector(isofile i, uint32_t sector, uint8_t *buffer);
extern bool isofile_get_toc(isofile i, int session, dc_toc *toc);

