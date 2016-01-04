dnl @synopsis mc_CHECK_SEARCH_TYPE
dnl
dnl Check search type in mc. Currently used glib-regexp or pcre
dnl
dnl @author Slava Zanko <slavazanko@gmail.com>
dnl @version 2009-06-19
dnl @license GPL
dnl @copyright Free Software Foundation, Inc.

AC_DEFUN([mc_CHECK_SEARCH_TYPE_PCRE],[
    AX_PATH_LIB_PCRE
    if test x"${PCRE_LIBS}" = x; then
	AC_MSG_ERROR([Your system don't have pcre library (or pcre devel stuff)])
    else
	SEARCH_TYPE="pcre"
	AC_DEFINE(SEARCH_TYPE_PCRE, 1, [Define to select 'pcre' search type])
    fi
])


AC_DEFUN([mc_CHECK_SEARCH_TYPE_GLIB],[
    $PKG_CONFIG --max-version 2.14 glib-2.0
    if test $? -eq 0; then
	AC_MSG_RESULT([[Selected 'glib' search engine, but you don't have glib >= 2.14. Trying to use 'pcre' engine], (WARNING)])
	mc_CHECK_SEARCH_TYPE_PCRE
    else
	AC_DEFINE(SEARCH_TYPE_GLIB, 1, [Define to select 'glib-regexp' search type])
    fi
])


AC_DEFUN([mc_CHECK_SEARCH_TYPE],[

    AC_ARG_WITH([search-engine],
        AS_HELP_STRING([--with-search-engine=type],
        [Select low-level search engine (since glib >= 2.14) @<:@glib|pcre@:>@])
      )
    case x$with_search_engine in
    xglib)
	SEARCH_TYPE="glib-regexp"
	;;
    xpcre)
	mc_CHECK_SEARCH_TYPE_PCRE
	;;
    x)
	SEARCH_TYPE="glib-regexp"
	;;
    *)
	AC_MSG_ERROR([Value of the search-engine is incorrect])
	;;
    esac

    if test x"$SEARCH_TYPE" = x"glib-regexp"; then
	mc_CHECK_SEARCH_TYPE_GLIB
    fi
])
