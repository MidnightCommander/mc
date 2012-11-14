dnl check for ncurses in user supplied path
AC_DEFUN([MC_CHECK_NCURSES_BY_PATH], [

    ac_ncurses_inc_path=[$1]
    ac_ncurses_lib_path=[$2]

    if test x"$ac_ncurses_inc_path" != x; then
        ac_ncurses_inc_path="-I"$ac_ncurses_inc_path
    fi

    if test x"$ac_ncurses_lib_path" != x; then
        ac_ncurses_lib_path="-L"$ac_ncurses_lib_path
    fi

    saved_CPPFLAGS="$CPPFLAGS"
    saved_LDFLAGS="$LDFLAGS"
    CPPFLAGS="$CPPFLAGS $ac_ncurses_inc_path"
    LDFLAGS="$LDFLAGS $ac_ncurses_lib_path"

    dnl Check for the headers
    dnl Both headers should be in the same directory
    dnl AIX term.h is unusable for mc
    AC_MSG_CHECKING([for ncurses/ncurses.h and ncurses/term.h])
    AC_PREPROC_IFELSE(
        [
            AC_LANG_PROGRAM([[#include <ncurses/ncurses.h>
                              #include <ncurses/term.h>
                            ]],[[return 0;]])
        ],
        [
            AC_MSG_RESULT(yes)
            if test x"$ac_ncurses_inc_path" = x; then
                ac_ncurses_inc_path="-I/usr/include"
            fi
            if test x"$ac_ncurses_lib_path" = x; then
              ac_ncurses_lib_path="-L/usr/lib"
            fi
            found_ncurses=yes
            AC_DEFINE(HAVE_NCURSES_NCURSES_H, 1,
                      [Define to 1 if you have the <ncurses/ncurses.h> header file.])
            AC_DEFINE(HAVE_NCURSES_TERM_H, 1,
                      [Define to 1 if you have the <ncurses/term.h> header file.])
        ],
        [
            AC_MSG_RESULT(no)
            found_ncurses=no
            error_msg_ncurses="ncurses header not found"
        ],
    )

    if test x"$found_ncurses" = x"yes"; then
        screen_type=ncurses
        screen_msg="Ncurses library"

        AC_DEFINE(HAVE_NCURSES, 1,
                  [Define to use ncurses library for screen management])

        MCLIBS="$MCLIBS $ac_ncurses_lib_path"
    else
        CPPFLAGS="$saved_CPPFLAGS"
        LDFLAGS="$saved_LDPFLAGS"
        AC_MSG_ERROR([$error_msg_ncurses])
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

    dnl get the user supplied include path
    AC_ARG_WITH([ncurses-includes],
        AS_HELP_STRING([--with-ncurses-includes=@<:@DIR@:>@],
            [set path to ncurses includes @<:@default=/usr/include@:>@; make sense only if --with-screen=ncurses; for /usr/local/include/ncurses specify /usr/local/include]
        ),
        [ac_ncurses_inc_path="$withval"],
        [ac_ncurses_inc_path=""]
    )

    dnl get the user supplied lib path
    AC_ARG_WITH([ncurses-libs],
        AS_HELP_STRING([--with-ncurses-libs=@<:@DIR@:>@],
            [set path to ncurses library @<:@default=/usr/lib@:>@; make sense only if --with-screen=ncurses]
        ),
        [ac_ncurses_lib_path="$withval"],
        [ac_ncurses_lib_path=""]
    )

    dnl we need at least the inc path, the lib may be in a std location
    if test x"$ac_ncurses_inc_path" != x; then
        dnl check the user supplied location
        MC_CHECK_NCURSES_BY_PATH([$ac_ncurses_inc_path],[$ac_ncurses_lib_path])

        LIBS=
        AC_SEARCH_LIBS([has_colors], [ncurses], [MCLIBS="$MCLIBS $LIBS"], 
                       [AC_MSG_ERROR([Cannot find ncurses library])])

        screen_type=ncurses
        screen_msg="Ncurses library"
        AC_DEFINE(USE_NCURSES, 1, 
                  [Define to use ncurses for screen management])
    else
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
        screen_msg="Ncurses library"
        AC_DEFINE(USE_NCURSES, 1, 
                  [Define to use ncurses for screen management])
    fi

    dnl check for ESCDELAY
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

    dnl check for resizeterm
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
    screen_msg="Ncursesw library"
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
