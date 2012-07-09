typedef struct directory_s *directory;
extern void directory_delete(directory d);
extern directory directory_new(msglogger logger, const char *dirname);
extern bool directory_check(msglogger logger, const char *dirname);
extern bool directory_read_sector(directory d, uint32_t sector, uint8_t *buffer);
extern bool directory_get_toc(directory d, int session, dc_toc *toc);
