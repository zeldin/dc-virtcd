AC_INIT(common/msglog.c)
AC_CONFIG_HEADER(config.h)

AC_CHECK_TOOL(AR, ar)
AC_PROG_CC
AC_C_BIGENDIAN
AC_CHECK_HEADERS(byteswap.h)
AC_CHECK_DECLS([__builtin_bswap32,bswap_32],,,[[
#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#endif
]])
AC_CHECK_FUNCS(pread)

AC_OUTPUT(Makefile common/Makefile network/Makefile server/Makefile
          datasource/Makefile cmdline/Makefile)
