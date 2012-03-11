typedef struct datafile_s *datafile;
typedef uint32_t datafile_offset;
extern void datafile_delete(datafile d);
extern datafile datafile_new_from_filename(msglogger logger, const char *filename);
extern bool datafile_read(datafile d, datafile_offset offset, size_t size, void *buf);
extern size_t datafile_size(datafile d);


