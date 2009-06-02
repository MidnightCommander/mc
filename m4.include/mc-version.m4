dnl @synopsis MC_VERSION
dnl
dnl get current version of Midnight Commander from git tags
dnl
dnl @author Slava Zanko <slavazanko@gmail.com>
dnl @version 2009-06-02
dnl @license GPL
dnl @copyright Free Software Foundation, Inc.

AC_DEFUN([MC_VERSION],[
    if test ! -f ${srcdir}/version.h; then
        ${srcdir}/maint/version.sh ${srcdir}
    fi
    if test -f ${srcdir}/version.h; then
        VERSION=$(cat ${srcdir}/version.h| grep '^#define MC_CURRENT_VERSION'| sed -r 's/.*"(.*)"$/\1/')
    else
        VERSION="unknown"
    fi
    AC_SUBST(VERSION)
])
