
dnl X11 support.
dnl Used to read keyboard modifiers when running under X11.
AC_DEFUN([mc_WITH_X], [

    AC_PATH_XTRA

    if test x"$no_x" = xyes; then
        textmode_x11_support="no"
    else
        AC_DEFINE([HAVE_TEXTMODE_X11_SUPPORT], [1],
                        [Define to enable getting events from X Window System])
        textmode_x11_support="yes"

        CPPFLAGS="$CPPFLAGS $X_CFLAGS"

        if test x"$g_module_supported" = x; then
            MCLIBS="$MCLIBS $X_LIBS $X_PRE_LIBS -lX11 $X_EXTRA_LIBS"
        fi
    fi

    AM_CONDITIONAL([HAVE_TEXTMODE_X11_SUPPORT], [test x"$textmode_x11_support" = x"yes"])
])
