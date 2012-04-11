typedef struct datasource_s *datasource;
extern void datasource_delete(datasource ds);
extern datasource datasource_new_from_filename(msglogger logger, const char *filename);
extern bool datasource_realize(datasource ds);
extern void datasource_unrealize(datasource ds);
extern bool datasource_read_sector(datasource ds, uint32_t sector, uint8_t *buffer);
extern bool datasource_get_toc(datasource ds, int session, dc_toc *toc);
extern bool datasource_get_ipbin(datasource ds, uint32_t n, uint8_t *buffer);
extern int32_t datasource_get_1st_read_size(datasource ds);

