dnl
dnl Try using termcap database and link with libtermcap if possible.
dnl
AC_DEFUN([mc_USE_TERMCAP], [
	screen_msg="$screen_msg with termcap database"
	AC_MSG_NOTICE([using S-Lang screen library with termcap])
	AC_DEFINE(USE_TERMCAP, 1, [Define to use termcap database])
	AC_CHECK_LIB(termcap, tgoto, [MCLIBS="$MCLIBS -ltermcap"], , [$LIBS])
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
#ifdef HAVE_SLANG_SLANG_H
#include <slang/slang.h>
#else
#include <slang.h>
#endif
		    ]],
		    [[SLtt_get_terminfo(); SLtt_tgetflag((char*)"");]])],
		    [mc_cv_slang_termcap=no],
		    [mc_cv_slang_termcap=yes])
	LIBS="$ac_save_LIBS"
    ])

    if test x"$mc_cv_slang_termcap" = xyes; then
	mc_USE_TERMCAP
    fi
])
