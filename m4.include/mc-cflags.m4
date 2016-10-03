dnl @synopsis mc_CHECK_CFLAGS
dnl
dnl Check flags supported by C compiler
dnl
dnl @author Slava Zanko <slavazanko@gmail.com>
dnl @version 2013-01-16
dnl @license GPL
dnl @copyright Free Software Foundation, Inc.

AC_DEFUN([mc_CHECK_ONE_CFLAG],[

  AC_MSG_CHECKING([whether ${CC} accepts $1])

  safe_CFLAGS=$CFLAGS

  case "$CC" in
    clang*)
      CFLAGS="-Werror $1"
      ;;
    *)
      CFLAGS="$1"
      ;;
  esac

  AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([], [[return 0;]])],
    [mc_check_one_cflag=yes],
    [mc_check_one_cflag=no])

  CFLAGS=$safe_CFLAGS
  AC_MSG_RESULT([$mc_check_one_cflag])

  if test x$mc_check_one_cflag = xyes; then
    mc_configured_cflags="$mc_configured_cflags $1"
  fi
])

AC_DEFUN([mc_CHECK_CFLAGS],[
    AC_LANG_PUSH(C)

    mc_configured_cflags=""

dnl Sorted -f options:
dnl AC_MSG_CHECKING([CC is $CC])
case "$CC" in
  gcc*)
    mc_CHECK_ONE_CFLAG([-fdiagnostics-show-option])
dnl    mc_CHECK_ONE_CFLAG([-fno-stack-protector])
    ;;
  *)
    ;;
esac

dnl Sorted -W options:
    mc_CHECK_ONE_CFLAG([-Wassign-enum])
    mc_CHECK_ONE_CFLAG([-Wbad-function-cast])
    mc_CHECK_ONE_CFLAG([-Wcomment])
    mc_CHECK_ONE_CFLAG([-Wconditional-uninitialized])
    mc_CHECK_ONE_CFLAG([-Wdeclaration-after-statement])
    mc_CHECK_ONE_CFLAG([-Wfloat-conversion])
    mc_CHECK_ONE_CFLAG([-Wfloat-equal])
    mc_CHECK_ONE_CFLAG([-Wformat])
    mc_CHECK_ONE_CFLAG([-Wformat-security])
    mc_CHECK_ONE_CFLAG([-Wformat-signedness])
    mc_CHECK_ONE_CFLAG([-Wimplicit])
    mc_CHECK_ONE_CFLAG([-Wignored-qualifiers])
    mc_CHECK_ONE_CFLAG([-Wlogical-not-parentheses])
    mc_CHECK_ONE_CFLAG([-Wmaybe-uninitialized])
    mc_CHECK_ONE_CFLAG([-Wmissing-braces])
    mc_CHECK_ONE_CFLAG([-Wmissing-declarations])
    mc_CHECK_ONE_CFLAG([-Wmissing-field-initializers])
    mc_CHECK_ONE_CFLAG([-Wmissing-format-attribute])
    mc_CHECK_ONE_CFLAG([-Wmissing-parameter-type])
    mc_CHECK_ONE_CFLAG([-Wmissing-prototypes])
    mc_CHECK_ONE_CFLAG([-Wmissing-variable-declarations])
    mc_CHECK_ONE_CFLAG([-Wnested-externs])
    mc_CHECK_ONE_CFLAG([-Wno-long-long])
    mc_CHECK_ONE_CFLAG([-Wno-unreachable-code])
    mc_CHECK_ONE_CFLAG([-Wparentheses])
    mc_CHECK_ONE_CFLAG([-Wpointer-arith])
    mc_CHECK_ONE_CFLAG([-Wpointer-sign])
    mc_CHECK_ONE_CFLAG([-Wredundant-decls])
    mc_CHECK_ONE_CFLAG([-Wreturn-type])
    mc_CHECK_ONE_CFLAG([-Wsequence-point])
    mc_CHECK_ONE_CFLAG([-Wshadow])
    mc_CHECK_ONE_CFLAG([-Wsign-compare])
dnl  mc_CHECK_ONE_CFLAG([-Wstrict-aliasing])
    mc_CHECK_ONE_CFLAG([-Wstrict-prototypes])
    mc_CHECK_ONE_CFLAG([-Wswitch])
    mc_CHECK_ONE_CFLAG([-Wswitch-default])
    mc_CHECK_ONE_CFLAG([-Wtype-limits])
    mc_CHECK_ONE_CFLAG([-Wundef])
    mc_CHECK_ONE_CFLAG([-Wuninitialized])
    mc_CHECK_ONE_CFLAG([-Wunreachable-code])
    mc_CHECK_ONE_CFLAG([-Wunused-but-set-variable])
    mc_CHECK_ONE_CFLAG([-Wunused-function])
    mc_CHECK_ONE_CFLAG([-Wunused-label])
    mc_CHECK_ONE_CFLAG([-Wunused-parameter])
    mc_CHECK_ONE_CFLAG([-Wunused-result])
    mc_CHECK_ONE_CFLAG([-Wunused-value])
    mc_CHECK_ONE_CFLAG([-Wunused-variable])
    mc_CHECK_ONE_CFLAG([-Wwrite-strings])

    AC_LANG_POP()
])
