dnl @synopsis mc_UNIT_TESTS
dnl
dnl Check if unit tests enabled
dnl
dnl @author Slava Zanko <slavazanko@gmail.com>
dnl @version 2011-02-10
dnl @license GPL
dnl @copyright Free Software Foundation, Inc.

AC_DEFUN([mc_UNIT_TESTS],[

    AC_ARG_ENABLE(
        [tests],
        AS_HELP_STRING([--enable-tests], [Enable unit tests (see http://libcheck.github.io/check/) @<:@auto@:>@])
    )

    dnl 'tests_msg' holds the human-readable message to show in configure's summary text.

    if test x$enable_tests == xno; then
        dnl The user explicitly specified '--disable-tests'.
        tests_msg="no"
    else
        PKG_CHECK_MODULES(
            CHECK,
            [check >= 0.9.8],
            [
                have_check=yes
                tests_msg="yes"
            ],
            [
                AC_MSG_WARN(['Check' testing framework not found. Check your environment])
                tests_msg="no ('Check' testing framework not found)"

                dnl The following behavior, of "exit if feature requested but not found", is just a
                dnl preference and can be safely removed.
                if test x$enable_tests == xyes; then
                    AC_MSG_ERROR([You explicitly specified '--enable-tests', but this requirement cannot be met.])
                fi
            ])
        AC_SUBST(CHECK_CFLAGS)
        AC_SUBST(CHECK_LIBS)
    fi
    AM_CONDITIONAL(HAVE_TESTS, test x"$have_check" = "xyes")

    # on cygwin, the linker does not accept the "-z" option
    case $host_os in
        cygwin*)
            TESTS_LDFLAGS="-Wl,--allow-multiple-definition"
            ;;
        *)
            TESTS_LDFLAGS="-Wl,-z,muldefs"
            ;;
    esac

    AC_SUBST(TESTS_LDFLAGS)
])
