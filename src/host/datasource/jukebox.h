typedef struct jukebox_s *jukebox;
extern void jukebox_delete(jukebox j);
extern jukebox jukebox_new(msglogger logger);
extern bool jukebox_add_datasource(jukebox j, datasource ds);
extern datasource jukebox_get_datasource(jukebox j, uint32_t id);
