typedef struct isofs_s *isofs;
extern void isofs_delete(isofs i);
extern isofs isofs_new(msglogger logger, datasource ds);
extern bool isofs_find_file(isofs i, const char *filename,
			    uint32_t *sector, uint32_t *length);
extern uint32_t isofs_get_bootsector(isofs i, uint32_t n);
