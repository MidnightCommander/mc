dnl
dnl Check if the system S-Lang library can be used.
dnl If not, and $1 is "strict", exit, otherwise fall back to mcslang.
dnl
AC_DEFUN([MC_CHECK_SLANG_BY_PATH], [

    ac_slang_inc_path=[$1]
    ac_slang_lib_path=[$2]

    if test x"$ac_slang_inc_path" != x; then
        ac_slang_inc_path="-I"$ac_slang_inc_path
    fi

    if test x"$ac_slang_lib_path" != x; then
        ac_slang_lib_path="-L"$ac_slang_lib_path
    fi

    saved_CFLAGS="$CFLAGS"
    saved_CPPFLAGS="$CPPFLAGS"
    saved_LDFLAGS="$LDFLAGS"

    CFLAGS="$CFLAGS $ac_slang_inc_path $ac_slang_lib_path"
    CPPFLAGS="$saved_CPPFLAGS $ac_slang_inc_path $ac_slang_lib_path"

    dnl Check the header
    AC_MSG_CHECKING([for slang.h])
    AC_PREPROC_IFELSE(
	[
	    AC_LANG_PROGRAM([#include <slang.h>],[return 0;])
	],
	[
	    AC_MSG_RESULT(yes)
	    if test x"$ac_slang_inc_path" = x; then
		ac_slang_inc_path="-I/usr/include"
	    fi
	    if test x"$ac_slang_lib_path" = x; then
		ac_slang_lib_path="-L/usr/lib"
	    fi
	    found_slang=yes
	    AC_DEFINE(HAVE_SLANG_H, 1,[Define to use slang.h])

	],
	[
	    AC_MSG_RESULT(no)
	    AC_MSG_CHECKING([for slang/slang.h])
	    AC_PREPROC_IFELSE(
		[
		    AC_LANG_PROGRAM([#include <slang/slang.h>],[return 0;])
		],
		[
		    AC_MSG_RESULT(yes)
		    if test x"$ac_slang_inc_path" = x; then
			ac_slang_inc_path="-I/usr/include"
		    fi
		    if test x"$ac_slang_lib_path" = x; then
			ac_slang_lib_path="-L/usr/lib"
		    fi
		    found_slang=yes
		    AC_DEFINE(HAVE_SLANG_SLANG_H, 1,[Define to use slang.h])
		],
		[
		    AC_MSG_RESULT(no)
		    found_slang=no
		    error_msg_slang="Slang header not found"
		]
	    )
	],
    )

    dnl Check if termcap is needed.
    dnl This check must be done before anything is linked against S-Lang.
    if test x"$found_slang" = x"yes"; then
	CFLAGS="$saved_CFLAGS $ac_slang_inc_path $ac_slang_lib_path"
	LDFLAGS="$saved_LDFLAGS $ac_slang_lib_path"
	CPPFLAGS="$saved_CPPFLAGS $ac_slang_inc_path $ac_slang_lib_path"

        MC_SLANG_TERMCAP
        if test x"$mc_cv_slang_termcap"  = x"yes"; then
	    ac_slang_lib_path="$ac_slang_lib_path -ltermcap"
	    CFLAGS="$saved_CFLAGS $ac_slang_inc_path $ac_slang_lib_path"
	    CPPFLAGS="$saved_CPPFLAGS $ac_slang_inc_path $ac_slang_lib_path"
	    LDFLAGS="$saved_LDFLAGS $ac_slang_lib_path"
        fi
    fi
    dnl Check the library
    if test x"$found_slang" = x"yes"; then
	unset ac_cv_lib_slang_SLang_init_tty
        AC_CHECK_LIB(
            [slang],
            [SLang_init_tty],
            [:],
            [
                found_slang=no
                error_msg_slang="S-lang library not found"
            ]
        )
    fi
    dnl check if S-Lang have version 2.0 or newer
    if test x"$found_slang" = x"yes"; then
        AC_MSG_CHECKING([for S-Lang version 2.0 or newer])
        AC_TRY_RUN([
#ifdef HAVE_SLANG_SLANG_H
#include <slang/slang.h>
#else
#include <slang/slang.h>
#endif
int main (void)
{
#if SLANG_VERSION >= 20000
    return 0;
#else
    return 1;
#endif
}
],
	    [mc_slang_is_valid_version=yes],
	    [mc_slang_is_valid_version=no],
	    [mc_slang_is_valid_version=no]
	)
	if test x$mc_slang_is_valid_version = xno; then
            found_slang=no
            error_msg_slang="S-Lang library version 2.0 or newer not found"
	fi
	AC_MSG_RESULT($mc_slang_is_valid_version)
    fi

    dnl Unless external S-Lang was requested, reject S-Lang with UTF-8 hacks
    if test x"$found_slang" = x"yes"; then
	unset ac_cv_lib_slang_SLsmg_write_nwchars
        AC_CHECK_LIB(
            [slang],
            [SLsmg_write_nwchars],
            [
                found_slang=no
                error_msg_slang="Rejecting S-Lang with UTF-8 support, it's not fully supported yet"
            ],
            [:]
        )
    fi

    if test x"$found_slang" = x"yes"; then
        screen_type=slang
        screen_msg="S-Lang library (installed on the system)"

        AC_DEFINE(HAVE_SLANG, 1,
            [Define to use S-Lang library for screen management])

        MCLIBS="$MCLIBS $ac_slang_lib_path -lslang"
    fi
    CFLAGS="$saved_CFLAGS"
    CPPFLAGS="$saved_CPPFLAGS"
    LDFLAGS="$saved_LDFLAGS"
])

dnl
dnl Use the slang library.
dnl
AC_DEFUN([MC_WITH_SLANG], [
    with_screen=slang
    found_slang=yes
    error_msg_slang=""

    AC_ARG_WITH([slang-includes],
        AC_HELP_STRING([--with-slang-includes=@<:@DIR@:>@],
            [set path to SLANG includes @<:@default=/usr/include@:>@; may sense only if --with-screen=slang]
        ),
        [ac_slang_inc_path="$withval"],
        [ac_slang_inc_path=""]
    )

    AC_ARG_WITH([slang-libs],
        AC_HELP_STRING([--with-slang-libs=@<:@DIR@:>@],
            [set path to SLANG library @<:@default=/usr/lib@:>@; may sense only if --with-screen=slang]
        ),
        [ac_slang_lib_path="$withval"],
        [ac_slang_lib_path=""]
    )
    echo 'checking SLANG-headers in default place ...'
    MC_CHECK_SLANG_BY_PATH([$ac_slang_inc_path],[$ac_slang_lib_path])
    if test x"$found_slang" = "xno"; then
        ac_slang_inc_path="/usr/include"
        ac_slang_lib_path="/usr/lib"

        echo 'checking SLANG-headers in /usr ...'
        MC_CHECK_SLANG_BY_PATH([$ac_slang_inc_path],[$ac_slang_lib_path])
        if test x"$found_slang" = "xno"; then
            ac_slang_inc_path="/usr/local/include"
            ac_slang_lib_path="/usr/local/lib"

            echo 'checking SLANG-headers in /usr/local ...'
            MC_CHECK_SLANG_BY_PATH( $ac_slang_inc_path , $ac_slang_lib_path )
            if test x"$found_slang" = "xno"; then
                AC_MSG_ERROR([$error_msg_slang])
            fi
        fi
    fi

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

    if test x"$ncursesw_found" = "x"; then
    LIBS=
    AC_SEARCH_LIBS([has_colors], [ncurses curses], [MCLIBS="$MCLIBS $LIBS"],
		   [AC_MSG_ERROR([Cannot find ncurses library])])
    fi

    dnl Check the header
    ncurses_h_found=
    AC_CHECK_HEADERS([ncursesw/curses.h ncurses/curses.h ncurses.h curses.h],
		     [ncurses_h_found=yes; break])

    if test x"$ncurses_h_found" = "x"; then
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
    if test x"$mc_cv_ncurses_escdelay" = xyes; then
	AC_DEFINE(HAVE_ESCDELAY, 1,
		  [Define if ncurses has ESCDELAY variable])
    fi

    AC_CHECK_FUNCS(resizeterm)
    LIBS="$save_LIBS"
])

dnl
dnl Use the ncursesw library.  It can only be requested explicitly,
dnl so just fail if anything goes wrong.
dnl
dnl If ncursesw exports the ESCDELAY variable it should be set to 0
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

    if test  x"$ncursesw_h_found" = "x"; then
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
    if test x"$mc_cv_ncursesw_escdelay" = xyes; then
	AC_DEFINE(HAVE_ESCDELAY, 1,
		  [Define if ncursesw has ESCDELAY variable])
    fi

    AC_CHECK_FUNCS(resizeterm)
    LIBS="$save_LIBS"
])
