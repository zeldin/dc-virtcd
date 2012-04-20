typedef struct nrgfile_s *nrgfile;
extern void nrgfile_delete(nrgfile n);
extern nrgfile nrgfile_new(msglogger logger, datafile data);
extern bool nrgfile_check(msglogger logger, datafile data);
extern bool nrgfile_read_sector(nrgfile n, uint32_t sector, uint8_t *buffer);
extern bool nrgfile_get_toc(nrgfile n, int session, dc_toc *toc);

