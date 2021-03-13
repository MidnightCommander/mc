dnl @synopsis mc_VERSION
dnl
dnl Get current version of Midnight Commander from git tags
dnl
dnl @author Slava Zanko <slavazanko@gmail.com>
dnl @version 2021-03-13
dnl @license GPL
dnl @copyright Free Software Foundation, Inc.
dnl @modified Andrew Borodin <aborodin@vmail.ru>

AC_DEFUN([mc_VERSION],[
    if test ! -f ${srcdir}/version.h; then
        ${srcdir}/maint/utils/version.sh ${srcdir}
    fi
    if test -f ${srcdir}/version.h; then
        VERSION=$(grep '^#define MC_CURRENT_VERSION' ${srcdir}/version.h | sed 's/.*"\(.*\)"$/\1/')
    else
        VERSION="unknown"
    fi
    AC_SUBST(VERSION)

    dnl Version without dashes for the man page
    DISTR_VERSION=`echo $VERSION | sed 's/^\([[^\-]]*\).*/\1/'`

    AC_SUBST(DISTR_VERSION)
])
