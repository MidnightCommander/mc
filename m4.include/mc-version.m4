dnl @synopsis MC_VERSION
dnl
dnl Get current version of Midnight Commander from git tags
dnl
dnl @author Slava Zanko <slavazanko@gmail.com>
dnl @version 2009-12-30
dnl @license GPL
dnl @copyright Free Software Foundation, Inc.

AC_DEFUN([MC_VERSION],[
    if test ! -f ${srcdir}/version.h; then
        ${srcdir}/maint/version.sh ${srcdir}
    fi
    if test -f ${srcdir}/version.h; then
        VERSION=$(grep '^#define MC_CURRENT_VERSION' ${srcdir}/version.h | sed 's/.*"\(.*\)"$/\1/')
    else
        VERSION="unknown"
    fi
    AC_SUBST(VERSION)

    dnl Version and Release without dashes for the distro packages
    DISTR_VERSION=`echo $VERSION | sed 's/^\([[^\-]]*\).*/\1/'`
    DISTR_RELEASE=`echo $VERSION | sed 's/^[[^\-]]*\-\(.*\)/\1/' | sed 's/-/./g'`

    if test `echo $VERSION | grep -c '\-pre'` -ne 0; then
        DISTR_RELEASE="0.$DISTR_RELEASE"
    else
        if test `echo $VERSION | grep -c '\-'` -eq 0; then
            DISTR_RELEASE=1
        else
            DISTR_RELEASE="2.$DISTR_RELEASE"
        fi
    fi

    AC_SUBST(DISTR_VERSION)
    AC_SUBST(DISTR_RELEASE)

])
