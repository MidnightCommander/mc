dnl @synopsis MC_VERSION
dnl
dnl Check search type in mc. Currently used glib-regexp or pcre
dnl
dnl @author Slava Zanko <slavazanko@gmail.com>
dnl @version 2009-06-19
dnl @license GPL
dnl @copyright Free Software Foundation, Inc.

AC_DEFUN([MC_CHECK_SEARCH_TYPE],[

    AC_ARG_WITH([search-engine],
        AC_HELP_STRING([--search-engine=type],
        [Select low-level search engine (since glib >= 2.14). [[glib pcre]]])
      )
    case x$with_search_engine in
    xglib)
	SEARCH_TYPE="glib-regexp"
	;;
    xpcre)
	SEARCH_TYPE="pcre"
	;;
    x)
	SEARCH_TYPE="glib-regexp"
	;;
    *)
	AC_MSG_ERROR([Value of the search-engine is incorrect])
	;;
    esac

    $PKG_CONFIG --max-version 2.14 glib-2.0
    if test $? -eq 0; then
	if test ! x"$with_search_engine" = x -a x"$SEARCH_TYPE" = xglib; then
	    AC_MSG_ERROR([Selected 'glib' search engine, but you don't have glib >= 2.14])
	fi
	AX_PATH_LIB_PCRE
	if test x"${PCRE_LIBS}" = x; then
	    AC_MSG_ERROR([Your system have glib < 2.14 and don't have pcre library (or pcre devel stuff)])
	fi
	AC_DEFINE(SEARCH_TYPE_PCRE, 1, [Define to select 'pcre' search type])
    else
	if test x"$SEARCH_TYPE" = xpcre; then
	    AX_PATH_LIB_PCRE
	    if test x"${PCRE_LIBS}" = x; then
		AC_MSG_ERROR([Your system don't have pcre library (or pcre devel stuff)])
	    fi
	    AC_DEFINE(SEARCH_TYPE_PCRE, 1, [Define to select 'pcre' search type])
	else
	    AC_DEFINE(SEARCH_TYPE_GLIB, 1, [Define to select 'glib-regexp' search type])
	fi
    fi
])
