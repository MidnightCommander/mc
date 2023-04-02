dnl @synopsis mc_CHECK_SEARCH_TYPE
dnl
dnl Check search type in mc. Currently used glib-regexp or pcre
dnl
dnl @author Slava Zanko <slavazanko@gmail.com>
dnl @author Andrew Borodin <aborodin@vmail.ru>
dnl @version 2023-03-22
dnl @license GPL
dnl @copyright Free Software Foundation, Inc.

AC_DEFUN([mc_CHECK_SEARCH_TYPE_PCRE],[
    AX_PATH_LIB_PCRE

    if test x"${PCRE_LIBS}" = x; then
	AC_MSG_ERROR([$1])
    fi

    SEARCH_TYPE="pcre"
])

AC_DEFUN([mc_CHECK_SEARCH_TYPE_PCRE2],[
    AX_CHECK_PCRE2([8], [], [:])

    if test $pcre2_cv_libpcre2 = yes; then
	SEARCH_TYPE="pcre2"
    else
	dnl pcre2 not found -- try pcre
	AC_MSG_WARN([Cannot find pcre2 library, trying pcre one...])
	mc_CHECK_SEARCH_TYPE_PCRE([$1])
    fi
])


AC_DEFUN([mc_CHECK_SEARCH_TYPE],[

    AC_ARG_WITH([search-engine],
        AS_HELP_STRING([--with-search-engine=type],
        [Select low-level search engine @<:@glib|pcre|pcre2@:>@])
      )

    case x$with_search_engine in
    xglib)
	SEARCH_TYPE="glib-regexp"
	;;
    xpcre)
	mc_CHECK_SEARCH_TYPE_PCRE([Cannot find pcre library])
	;;
    xpcre2)
	mc_CHECK_SEARCH_TYPE_PCRE2([Neither pcre2 nor pcre library found!])
	;;
    x)
	SEARCH_TYPE="glib-regexp"
	;;
    *)
	AC_MSG_ERROR([Value of the search-engine is incorrect])
	;;
    esac

    if test x"$SEARCH_TYPE" = x"glib-regexp"; then
	AC_DEFINE(SEARCH_TYPE_GLIB, 1, [Define to select 'glib-regexp' search type])
    else
	AC_DEFINE(SEARCH_TYPE_PCRE, 1, [Define to select 'pcre2' or 'pcre' search type])
    fi
])
