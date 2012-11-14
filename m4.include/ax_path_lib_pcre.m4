dnl @synopsis AX_PATH_LIB_PCRE [(A/NA)]
dnl
dnl check for pcre lib and set PCRE_LIBS and PCRE_CPPFLAGS accordingly.
dnl
dnl also provide --with-pcre option that may point to the $prefix of
dnl the pcre installation - the macro will check $pcre/include and
dnl $pcre/lib to contain the necessary files.
dnl
dnl the usual two ACTION-IF-FOUND / ACTION-IF-NOT-FOUND are supported
dnl and they can take advantage of the LIBS/CFLAGS additions.
dnl
dnl @author Guido U. Draheim <guidod@gmx.de>
dnl @author Slava Zanko <slavazanko@gmail.com>
dnl @version 2009-07-06
dnl @license GPLWithACException

AC_DEFUN([AX_PATH_LIB_PCRE],[dnl
AC_MSG_CHECKING([lib pcre])
AC_ARG_WITH([pcre],
            AS_HELP_STRING([--with-pcre@<:@=prefix@:>@], [Compile pcre part (via libpcre check)]),
            ,
            [with_pcre="yes"]
)
if test x"$with_pcre" = "xno" ; then
  AC_MSG_RESULT([disabled])
  m4_ifval($2,$2)
else

  AC_MSG_RESULT([(testing)])

  if test "x$with_pcre" = "xyes" ; then
    PCRE_CPPFLAGS="`pcre-config --cflags`"
    PCRE_LIBS="`pcre-config --libs`"
  else
    test_PCRE_LIBS="-L$with_pcre/lib"
    test_PCRE_CPPFLAGS="-I$with_pcre/include"

     OLDLDFLAGS="$LDFLAGS" ; LDFLAGS="$LDFLAGS $test_PCRE_LIBS"
     OLDCPPFLAGS="$CPPFLAGS" ; CPPFLAGS="$CPPFLAGS $test_PCRE_CPPFLAGS"

     AC_CHECK_LIB(pcre, pcre_compile)

     if test x"$ac_cv_lib_pcre_pcre_compile" = x"yes" ; then
        AC_MSG_RESULT(setting PCRE_LIBS -L$with_pcre/lib -lpcre)

        PCRE_LIBS=$test_PCRE_LIBS
        PCRE_CPPFLAGS=$test_PCRE_CPPFLAGS

        AC_MSG_CHECKING([lib pcre])
        AC_MSG_RESULT([$PCRE_LIBS])
        m4_ifval($1,$1)
     else
        AC_MSG_CHECKING([lib pcre])
        AC_MSG_RESULT([no, (WARNING)])
        m4_ifval($2,$2)
     fi

     CPPFLAGS="$OLDCFPPLAGS"
     LDFLAGS="$OLDLDFLAGS"

  fi
fi

AC_SUBST([PCRE_LIBS])
AC_SUBST([PCRE_CPPFLAGS])

])
