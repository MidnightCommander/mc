dnl
dnl Try using termcap database and link with libtermcap if possible.
dnl
AC_DEFUN([mc_USE_TERMCAP], [
	screen_msg="$screen_msg with termcap database"
	AC_MSG_NOTICE([using S-Lang screen library with termcap])
	AC_DEFINE(USE_TERMCAP, 1, [Define to use termcap database])

	ac_save_LIBS="$LIBS"
	AC_SEARCH_LIBS([tgoto], [termcap xcurses curses],
	    [MCLIBS="$MCLIBS $ac_cv_search_tgoto"],
	    [AC_MSG_ERROR([Could not find a library providing tgoto])]
	)
	LIBS="$ac_save_LIBS"
])

dnl
dnl Check if the installed S-Lang library uses termcap
dnl
AC_DEFUN([mc_SLANG_TERMCAP], [
    unset ac_cv_lib_termcap_tgoto

    AC_CACHE_CHECK([if S-Lang uses termcap], [mc_cv_slang_termcap], [
	ac_save_LIBS="$LIBS"
	LIBS="$LIBS -lslang"
	AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <slang.h>
	]],
	[[SLtt_get_terminfo(); SLtt_tgetflag((char*)"");]])],
	[mc_cv_slang_termcap=no], [mc_cv_slang_termcap=yes])

	LIBS="$ac_save_LIBS"
    ])

    if test x"$mc_cv_slang_termcap" = xyes; then
	mc_USE_TERMCAP
    fi
])
