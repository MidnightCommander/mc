dnl @synopsis MC_UNIT_TESTS
dnl
dnl Check if unit tests enabled
dnl
dnl @author Slava Zanko <slavazanko@gmail.com>
dnl @version 2011-02-10
dnl @license GPL
dnl @copyright Free Software Foundation, Inc.

AC_DEFUN([MC_UNIT_TESTS],[

    AC_ARG_ENABLE(
        [tests],
        AS_HELP_STRING([--enable-tests], [Enable unit tests (see http://check.sourceforge.net/)])
    )

    if test x$enable_tests != xno; then
        PKG_CHECK_MODULES(
            CHECK,
            [check >= 0.9.8],
            [have_check=yes],
            [AC_MSG_WARN(['Check' utility not found. Check your environment])])
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
