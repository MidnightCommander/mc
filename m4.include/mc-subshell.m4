dnl
dnl Subshell support.
dnl
AC_DEFUN([mc_SUBSHELL], [

    AC_MSG_CHECKING([for subshell support])
    AC_ARG_WITH(subshell,
            [  --with-subshell          Compile in concurrent subshell @<:@yes@:>@
  --with-subshell=optional Don't run concurrent shell by default @<:@no@:>@],
            [
                result=no
                if test x$withval = xoptional; then
                    AC_DEFINE(SUBSHELL_OPTIONAL, 1,  [Define to make subshell support optional])
                    result="optional"
                fi
                if test x$withval = xyes; then
                    result="yes"
                fi
            ],
            [
                dnl Default: enable the subshell support
                result="yes"
            ])

    if test "x$result" != xno; then
        AC_DEFINE(ENABLE_SUBSHELL, 1, [Define to enable subshell support])
    fi

    AC_MSG_RESULT([$result])
    subshell="$result"

    AM_CONDITIONAL(ENABLE_SUBSHELL, [test "x$result" != xno])
])
