dnl
dnl Subshell support.
dnl
AC_DEFUN([mc_SUBSHELL], [

    AC_MSG_CHECKING([for subshell support])
    AC_ARG_WITH(subshell,
            [  --with-subshell          Compile in concurrent subshell @<:@yes@:>@
  --with-subshell=optional Don't run concurrent shell by default @<:@no@:>@],
            [
                case "x$withval" in
                    xyes)
                        result="yes"
                        ;;
                    xoptional)
                        result="optional"
                        ;;
                    *)
                        result="no"
                        ;;
                esac
            ],
            [
                dnl Default: enable the subshell support
                result="yes"
            ])

    AC_MSG_RESULT([$result])

    if test "x$result" != xno; then
        AC_DEFINE(ENABLE_SUBSHELL, 1, [Define to enable subshell support])

        dnl openpty() can simplify opening of master/slave devices for subshell
        AC_CHECK_HEADERS([pty.h libutil.h util.h])
        AC_CHECK_FUNCS(openpty,,
            AC_CHECK_LIB(util,openpty,
                [AC_DEFINE(HAVE_OPENPTY)
                    LIBS="$LIBS -lutil"]
            )
        )

        if test "x$result" = xoptional; then
            AC_DEFINE(SUBSHELL_OPTIONAL, 1, [Define to make subshell support optional])
        fi
    fi

    subshell="$result"

    AM_CONDITIONAL(ENABLE_SUBSHELL, [test "x$result" != xno])
])
