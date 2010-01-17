dnl @synopsis MC_CHECK_CFLAGS
dnl
dnl Check flags supported by compilator
dnl
dnl @author Slava Zanko <slavazanko@gmail.com>
dnl @version 2009-10-29
dnl @license GPL
dnl @copyright Free Software Foundation, Inc.

AC_DEFUN([MC_CHECK_ONE_CFLAG],[

  AC_MSG_CHECKING([if gcc accepts $1])

  safe_CFLAGS=$CFLAGS
  CFLAGS="$1"

  AC_TRY_COMPILE(, [
  return 0;
  ],
  [
    mc_check_one_cflag=yes
  ],
  [
    mc_check_one_cflag=no
  ])

  CFLAGS=$safe_CFLAGS
  AC_MSG_RESULT([$mc_check_one_cflag])

  if test x$mc_check_one_cflag = xyes; then
    mc_configured_cflags="$mc_configured_cflags $1"
  fi

])

AC_DEFUN([MC_CHECK_CFLAGS],[
    mc_configured_cflags=""

dnl Sorted -f options:
dnl AC_MSG_CHECKING([CC is $CC])
case "$CC" in
  gcc*)
    MC_CHECK_ONE_CFLAG([-fdiagnostics-show-option])
dnl    MC_CHECK_ONE_CFLAG([-fno-stack-protector])
    ;;
  *)
    ;;
esac

dnl Sorted -W options:
    MC_CHECK_ONE_CFLAG([-Wcomment])
    MC_CHECK_ONE_CFLAG([-Wdeclaration-after-statement])
    MC_CHECK_ONE_CFLAG([-Wformat])
    MC_CHECK_ONE_CFLAG([-Wimplicit-function-declaration])
    MC_CHECK_ONE_CFLAG([-Wimplicit-int])
    MC_CHECK_ONE_CFLAG([-Wmissing-braces])
    MC_CHECK_ONE_CFLAG([-Wmissing-declarations])
    MC_CHECK_ONE_CFLAG([-Wmissing-parameter-type])
    MC_CHECK_ONE_CFLAG([-Wmissing-prototypes])
    MC_CHECK_ONE_CFLAG([-Wnested-externs])
    MC_CHECK_ONE_CFLAG([-Wno-long-long])
    MC_CHECK_ONE_CFLAG([-Wno-unreachable-code])
    MC_CHECK_ONE_CFLAG([-Wparentheses])
    MC_CHECK_ONE_CFLAG([-Wpointer-sign])
    MC_CHECK_ONE_CFLAG([-Wreturn-type])
dnl    MC_CHECK_ONE_CFLAG([-Wsequence-point])
    MC_CHECK_ONE_CFLAG([-Wshadow])
    MC_CHECK_ONE_CFLAG([-Wsign-compare])
dnl    MC_CHECK_ONE_CFLAG([-Wstrict-aliasing])
    MC_CHECK_ONE_CFLAG([-Wswitch])
    MC_CHECK_ONE_CFLAG([-Wuninitialized])
    MC_CHECK_ONE_CFLAG([-Wunused-function])
    MC_CHECK_ONE_CFLAG([-Wunused-label])
    MC_CHECK_ONE_CFLAG([-Wunused-parameter])
    MC_CHECK_ONE_CFLAG([-Wunused-result])
    MC_CHECK_ONE_CFLAG([-Wunused-value])
    MC_CHECK_ONE_CFLAG([-Wunused-variable])
    MC_CHECK_ONE_CFLAG([-Wwrite-strings])
])
