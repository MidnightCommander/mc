dnl
dnl Support for background operations
dnl

AC_DEFUN([mc_BACKGROUND],
[
    AC_ARG_ENABLE([background],
    AS_HELP_STRING([--enable-background], [Support for background file operations [yes]]),
    [
        if test "x$enableval" = xno; then
            enable_background=no
        else
            enable_background=yes
        fi
    ],
    [enable_background=yes])

    if test "x$enable_background" = xyes; then
        AC_DEFINE(ENABLE_BACKGROUND, 1, [Define to enable background file operations])
    fi

    AM_CONDITIONAL(ENABLE_BACKGROUND, [test "x$enable_background" = xyes])
])
