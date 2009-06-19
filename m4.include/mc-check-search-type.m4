dnl @synopsis MC_VERSION
dnl
dnl Check search type in mc. Currently used glib-regexp or pcre
dnl
dnl @author Slava Zanko <slavazanko@gmail.com>
dnl @version 2009-06-19
dnl @license GPL
dnl @copyright Free Software Foundation, Inc.

AC_DEFUN([MC_CHECK_SEARCH_TYPE],[

    $PKG_CONFIG --max-version 2.14 glib-2.0
    if test $? -eq 0; then
        AX_PATH_LIB_PCRE
        if test x"${PCRE_LIBS}" = x; then
            AC_MSG_ERROR([Your system have glib < 2.14 and don't have pcre library (or pcre devel stuff)])
        fi
        SEARCH_TYPE="pcre"
    else
        SEARCH_TYPE="glib-regexp"
    fi
])
