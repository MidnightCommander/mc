dnl
dnl Check whether to enable/disable assertions.
dnl

AC_DEFUN([mc_ASSERT],
[
    AC_ARG_ENABLE([assert],
    AS_HELP_STRING([--enable-assert], [turn on assertions [yes]]),
    [
        if test "x$enableval" = xno; then
            enable_assert=no
        else
            enable_assert=yes
        fi
    ],
    [enable_assert=yes])

    if test "x$enable_assert" = xno; then
        AC_DEFINE(G_DISABLE_ASSERT, 1, [Define to disable assertions])
    fi
])
