typedef enum {
  MSG_DEBUG,
  MSG_INFO,
  MSG_NOTICE,
  MSG_WARNING,
  MSG_ERROR,
} msglevel;
typedef struct msglogger_s *msglogger;
struct msglogger_s {
  void (*log)(msglogger l, msglevel level, const char *msg);
};
extern void msglog_log(msglogger l, msglevel level, const char *msg, ...);
extern void msglog_debug(msglogger l, const char *msg, ...);
extern void msglog_info(msglogger l, const char *msg, ...);
extern void msglog_notice(msglogger l, const char *msg, ...);
extern void msglog_warning(msglogger l, const char *msg, ...);
extern void msglog_error(msglogger l, const char *msg, ...);
extern void msglog_perror(msglogger l, const char *prefix);
extern void msglog_oomerror(msglogger l);

