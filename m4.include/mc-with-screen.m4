dnl
dnl Common code for MC_WITH_SLANG and MC_WITH_MCSLANG
dnl
AC_DEFUN([_MC_WITH_XSLANG], [
    screen_type=slang
    AC_DEFINE(HAVE_SLANG, 1,
          [Define to use S-Lang library for screen management])
])

dnl
dnl Check if private functions are available for linking
dnl
AC_DEFUN([MC_SLANG_PRIVATE], [
    AC_CACHE_CHECK([if S-Lang exports private functions],
		[mc_cv_slang_private], [
	ac_save_LIBS="$LIBS"
	LIBS="$LIBS -lslang"
	AC_TRY_LINK([
		     #ifdef HAVE_SLANG_SLANG_H
		     #include <slang/slang.h>
		     #else
		     #include <slang.h>
		     #endif
		     #if SLANG_VERSION >= 10000
		     extern unsigned int SLsys_getkey (void);
		     #else
		     extern unsigned int _SLsys_getkey (void);
		     #endif
		    ], [
		     #if SLANG_VERSION >= 10000
		     _SLsys_getkey ();
		     #else
		     SLsys_getkey ();
		     #endif
		    ],
		    [mc_cv_slang_private=yes],
		    [mc_cv_slang_private=no])
	LIBS="$ac_save_LIBS"
    ])

    if test x$mc_cv_slang_private = xyes; then
	AC_DEFINE(HAVE_SLANG_PRIVATE, 1,
		  [Define if private S-Lang functions are available])
    fi
])


dnl
dnl Check if the system S-Lang library can be used.
dnl If not, and $1 is "strict", exit, otherwise fall back to mcslang.
dnl
AC_DEFUN([MC_WITH_SLANG], [
    with_screen=slang

    dnl Check the header
    slang_h_found=
    AC_CHECK_HEADERS([slang.h slang/slang.h],
		     [slang_h_found=yes; break])
    if test -z "$slang_h_found"; then
	AC_MSG_ERROR([Slang header not found])
    fi

    dnl Check if termcap is needed.
    dnl This check must be done before anything is linked against S-Lang.
    if test x$with_screen = xslang; then
	MC_SLANG_TERMCAP
    fi

    dnl Check the library
    if test x$with_screen = xslang; then
	AC_CHECK_LIB([slang], [SLang_init_tty], [MCLIBS="$MCLIBS -lslang"],
		     AC_MSG_ERROR([Slang library not found]), ["$MCLIBS"])
    fi

    dnl Unless external S-Lang was requested, reject S-Lang with UTF-8 hacks
    if test x$with_screen = xslang; then
	AC_CHECK_LIB(
	    [slang],
	    [SLsmg_write_nwchars],
	    [AC_MSG_ERROR([Rejecting S-Lang with UTF-8 support, it's not fully supported yet])])
    fi

    if test x$with_screen = xslang; then
	MC_SLANG_PRIVATE
	screen_type=slang
	screen_msg="S-Lang library (installed on the system)"
    else
	AC_MSG_ERROR([S-Lang library not found])
    fi

    _MC_WITH_XSLANG
])

dnl
dnl Use the ncurses library.  It can only be requested explicitly,
dnl so just fail if anything goes wrong.
dnl
dnl If ncurses exports the ESCDELAY variable it should be set to 0
dnl or you'll have to press Esc three times to dismiss a dialog box.
dnl
AC_DEFUN([MC_WITH_NCURSES], [
    dnl has_colors() is specific to ncurses, it's not in the old curses
    save_LIBS="$LIBS"
    ncursesw_found=
    LIBS=
    AC_SEARCH_LIBS([addwstr], [ncursesw ncurses curses], [MCLIBS="$MCLIBS $LIBS";ncursesw_found=yes],
		   [AC_MSG_WARN([Cannot find ncurses library, that support wide characters])])

    if test -z "$ncursesw_found"; then
    LIBS=
    AC_SEARCH_LIBS([has_colors], [ncurses curses], [MCLIBS="$MCLIBS $LIBS"],
		   [AC_MSG_ERROR([Cannot find ncurses library])])
    fi

    dnl Check the header
    ncurses_h_found=
    AC_CHECK_HEADERS([ncursesw/curses.h ncurses/curses.h ncurses.h curses.h],
		     [ncurses_h_found=yes; break])

    if test -z "$ncurses_h_found"; then
	AC_MSG_ERROR([Cannot find ncurses header file])
    fi

    screen_type=ncurses
    screen_msg="ncurses library"
    AC_DEFINE(USE_NCURSES, 1,
	      [Define to use ncurses for screen management])

    AC_CACHE_CHECK([for ESCDELAY variable],
		   [mc_cv_ncurses_escdelay],
		   [AC_TRY_LINK([], [
			extern int ESCDELAY;
			ESCDELAY = 0;
			],
			[mc_cv_ncurses_escdelay=yes],
			[mc_cv_ncurses_escdelay=no])
    ])
    if test "$mc_cv_ncurses_escdelay" = yes; then
	AC_DEFINE(HAVE_ESCDELAY, 1,
		  [Define if ncurses has ESCDELAY variable])
    fi

    AC_CHECK_FUNCS(resizeterm)
    LIBS="$save_LIBS"
])

dnl
dnl Use the ncurses library.  It can only be requested explicitly,
dnl so just fail if anything goes wrong.
dnl
dnl If ncurses exports the ESCDELAY variable it should be set to 0
dnl or you'll have to press Esc three times to dismiss a dialog box.
dnl

AC_DEFUN([MC_WITH_NCURSESW], [
    dnl has_colors() is specific to ncurses, it's not in the old curses
    save_LIBS="$LIBS"
    LIBS=
    AC_SEARCH_LIBS([has_colors], [ncursesw], [MCLIBS="$MCLIBS $LIBS"],
		   [AC_MSG_ERROR([Cannot find ncursesw library])])

    dnl Check the header
    ncurses_h_found=
    AC_CHECK_HEADERS([ncursesw/curses.h],
		     [ncursesw_h_found=yes; break])

    if test -z "$ncursesw_h_found"; then
	AC_MSG_ERROR([Cannot find ncursesw header file])
    fi

    screen_type=ncursesw
    screen_msg="ncursesw library"
    AC_DEFINE(USE_NCURSESW, 1,
	      [Define to use ncursesw for screen management])

    AC_CACHE_CHECK([for ESCDELAY variable],
		   [mc_cv_ncursesw_escdelay],
		   [AC_TRY_LINK([], [
			extern int ESCDELAY;
			ESCDELAY = 0;
			],
			[mc_cv_ncursesw_escdelay=yes],
			[mc_cv_ncursesw_escdelay=no])
    ])
    if test "$mc_cv_ncursesw_escdelay" = yes; then
	AC_DEFINE(HAVE_ESCDELAY, 1,
		  [Define if ncursesw has ESCDELAY variable])
    fi

    AC_CHECK_FUNCS(resizeterm)
    LIBS="$save_LIBS"
])
