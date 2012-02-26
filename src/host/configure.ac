AC_INIT(Makefile.in)
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

AC_OUTPUT(Makefile network/Makefile cmdline/Makefile)
