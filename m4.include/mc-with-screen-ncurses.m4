dnl
dnl Use the NCurses library.  It can only be requested explicitly,
dnl so just fail if anything goes wrong.
dnl
dnl Search order:
dnl 1) system path   (adjust by --with-ncurses-libdir=<DIR>, --with-ncurses-includedir=<DIR>)
dnl 2) pkg-config    (adjust by NCURSES_LIBS, NCURSES_CFLAGS)
dnl
dnl Preference:
dnl ncursesw > ncurses
dnl
dnl Rules:
dnl LIBS can be prepended
dnl CFLAGS can be appended (compiler optimizations and directives look at the last options)
dnl CPPFLAGS can be prepended (header search paths options look at the first path)
dnl

AC_DEFUN([mc_WITH_NCURSES], [
    save_LIBS="$LIBS"
    save_CFLAGS="$CFLAGS"
    save_CPPFLAGS="$CPPFLAGS"
    save_MCLIBS="$MCLIBS"

    AC_MSG_CHECKING([for specific NCurses libdir])
    AC_ARG_WITH([ncurses-libdir],
        [AS_HELP_STRING([--with-ncurses-libdir=@<:@DIR@:>@], [Path to NCurses library files])],
        [AS_IF([test ! -d "$withval"], [AC_MSG_ERROR([NCurses libdir path "$withval" not found])])
        LIBS="-L$withval $LIBS"
        LDFLAGS="-L$withval $LIBS"
        MCLIBS="-L$withval $MCLIBS"],
        [with_ncurses_libdir=no])
    AC_MSG_RESULT([$with_ncurses_libdir])

    AC_MSG_CHECKING([for specific NCurses includedir])
    AC_ARG_WITH([ncurses-includedir],
        [AS_HELP_STRING([--with-ncurses-includedir=@<:@DIR@:>@], [Path to NCurses header files])],
        [AS_IF([test ! -d "$withval"], [AC_MSG_ERROR([NCurses includedir path "$withval" not found])])
        CFLAGS="$save_CFLAGS -I$withval"],
        [with_ncurses_includedir=no])
    AC_MSG_RESULT([$with_ncurses_includedir])

    dnl
    dnl Check ncurses library
    dnl
    dnl has_colors is specific to ncurses, it's not in the old curses
    dnl search in default linker path and LDFLAGS -L options
    AC_SEARCH_LIBS([has_colors], [ncursesw ncurses],
        [AS_CASE(["$ac_cv_search_has_colors"],
            ["-lncursesw"], [screen_msg="NCursesw"],
            ["-lncurses"], [screen_msg="NCurses"],
            ["none required"], [screen_msg="NCurses static"], dnl or system native? Who knows
            [AC_MSG_ERROR([Unknown ac_cv_search_has_colors option "$ac_cv_search_has_colors"])])
        ],
        dnl 2. Library not found by system path, try pkg-config
        [PKG_CHECK_MODULES([NCURSES], [ncursesw],
            [   LIBS="$NCURSES_LIBS $save_LIBS"
                MCLIBS="$NCURSES_LIBS $save_MCLIBS"
                CFLAGS="$save_CFLAGS $NCURSES_CFLAGS"
                screen_msg="NCursesw"
                AC_CHECK_FUNC([has_colors], [], dnl Always validate pkg-config result
                    [AC_MSG_ERROR([NCursesw pkg-config insufficient])])
            ],
            [PKG_CHECK_MODULES([NCURSES], [ncurses],
                [   LIBS="$NCURSES_LIBS $save_LIBS"
                    MCLIBS="$NCURSES_LIBS $save_MCLIBS"
                    CFLAGS="$save_CFLAGS $NCURSES_CFLAGS"
                    screen_msg="NCurses"
                    AC_CHECK_FUNC([has_colors], [], dnl Always validate pkg-config result
                        [AC_MSG_ERROR([NCurses pkg-config insufficient])])
                ],
                [AC_MSG_ERROR([(N)Curses(w) library not found by system path neither pkg-config])])
            ])
        ])

    AC_SEARCH_LIBS([stdscr], [tinfow tinfo], [],
        dnl 2. Library not found by system path, try pkg-config
        [PKG_CHECK_MODULES([TINFOW], [tinfow], [],
            [AC_CHECK_FUNC([stdscr], [], dnl Always validate pkg-config result
                [AC_MSG_ERROR([Tinfow pkg-config insufficient])])],
            [PKG_CHECK_MODULES([TINFO], [tinfo],
                [AC_CHECK_FUNC([stdscr], [], dnl Always validate pkg-config result
                    [AC_MSG_ERROR([Tinfo pkg-config insufficient])])],
                [AC_MSG_ERROR([Tinfo(w) library not found by system path neither pkg-config])])
            ])
        ])

    AC_CHECK_FUNC([addwstr],
        [AC_DEFINE(HAVE_NCURSES_WIDECHAR, 1, [Define if ncurses library has wide characters support])],
        [AC_MSG_WARN([NCurses(w) library found without wide characters support])])

    dnl
    dnl Check ncurses header
    dnl
    dnl Set CPPFLAGS to avoid AC_CHECK_HEADERS warning "accepted by the compiler, rejected by the preprocessor!"
    CPPFLAGS="$CFLAGS"
    dnl first match wins, using break
    AC_CHECK_HEADERS([ncursesw/ncurses.h ncursesw/curses.h ncurses/ncurses.h ncurses/curses.h ncurses.h curses.h],
        [ncurses_h_found=yes; break],
        [ncurses_h_found=no])

    AS_IF([test x"$ncurses_h_found" != xyes],
        [AC_MSG_ERROR([NCurses(w) header file not found])])

    AC_CHECK_HEADERS([ncursesw/term.h ncurses/term.h term.h],
        [ncurses_term_h_found=yes; break],
        [ncurses_term_h_found=no])

    AS_IF([test x"$ncurses_term_h_found" != xyes],
        [AC_MSG_ERROR([NCurses(w) term.h header file not found])])

    dnl If ncurses exports the ESCDELAY variable it should be set to 0
    dnl or you'll have to press Esc three times to dismiss a dialog box.
    AC_CACHE_CHECK([for ESCDELAY variable], [mc_cv_ncurses_escdelay],
        [AC_LINK_IFELSE(
            [AC_LANG_PROGRAM([[extern int ESCDELAY;]],[[ESCDELAY = 0;]])],
            [mc_cv_ncurses_escdelay=yes], [mc_cv_ncurses_escdelay=no])
        ])

    AS_IF([test x"$mc_cv_ncurses_escdelay" = xyes],
        [AC_DEFINE([HAVE_ESCDELAY], [1], [Define if NCurses(w) has ESCDELAY variable])])

    AC_CHECK_FUNCS(resizeterm)

    AC_DEFINE([HAVE_NCURSES], [1], [Define to use NCurses for screen management])
    AC_DEFINE_UNQUOTED([NCURSES_LIB_DISPLAYNAME], ["$screen_msg"], [Define NCurses library display name])
])
