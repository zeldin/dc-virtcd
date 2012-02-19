AC_INIT(launcher/main.c)

DCAS='sh-elf-as -little'
DCLD='sh-elf-ld -EL'
DCCC='sh-elf-gcc -ml -m4-single-only'
DCAR='sh-elf-ar'
DCOBJCOPY='sh-elf-objcopy'
DCCFLAGS='-Os'
AC_SUBST(DCAS)
AC_SUBST(DCLD)
AC_SUBST(DCCC)
AC_SUBST(DCAR)
AC_SUBST(DCOBJCOPY)
AC_SUBST(DCCFLAGS)

AC_OUTPUT(Makefile common/Makefile network/Makefile skel/Makefile launcher/Makefile)
