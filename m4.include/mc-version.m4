dnl @synopsis mc_VERSION
dnl
dnl Get current version of Midnight Commander from git tags
dnl
dnl @author Slava Zanko <slavazanko@gmail.com>
dnl @version 2021-04-04
dnl @license GPL
dnl @copyright Free Software Foundation, Inc.
dnl @modified Andrew Borodin <aborodin@vmail.ru>

AC_DEFUN([mc_VERSION],[
    if test -f ${srcdir}/mc-version.h; then
        VERSION=$(grep '^#define MC_CURRENT_VERSION' ${srcdir}/mc-version.h | ${SED-sed} 's/.*"\(.*\)"$/\1/')
    else
        VERSION="unknown"
    fi

    dnl Version without dashes for the man page
    MAN_VERSION=`echo $VERSION | sed 's/^\([[^\-]]*\).*/\1/'`
    AC_SUBST(MAN_VERSION)
])
