dnl ri_GCC_WARNINGS()
dnl
dnl Adjust the CPPFLAGS, CFLAGS and CXXFLAGS for various GCC versions

AC_DEFUN([ri_GCC_WARNINGS],
  [AC_ARG_ENABLE([gcc-warnings],
    [AS_HELP_STRING([--enable-gcc-warnings[[[[=error]]]]],
                    [enable additional gcc warning flags])],
[
if test x"$GCC" = x"yes"; then
  ri_gcc_main=`echo __GNUC__ | $CPP -E - | grep -v ^\#`
  ri_gcc_minor=`echo __GNUC_MINOR__ | $CPP -E - | grep -v ^\#`
  
  ri_cppflags=""
  ri_cflags=""
  ri_cxxflags=""
  ri_bothflags=""

  ri_gcc_v2_95="false"
  test $ri_gcc_main -gt 2 && ri_gcc_v2_95="true"
  test $ri_gcc_main -eq 2 && test $ri_gcc_minor -ge 95 && ri_gcc_v2_95="true"

  ri_gcc_v3_2="false"
  test $ri_gcc_main -gt 3 && ri_gcc_v3_2="true"
  test $ri_gcc_main -eq 3 && test $ri_gcc_minor -ge 2 && ri_gcc_v3_2="true"

  ri_gcc_v3_3="false"
  test $ri_gcc_main -gt 3 && ri_gcc_v3_3="true"
  test $ri_gcc_main -eq 3 && test $ri_gcc_minor -ge 3 && ri_gcc_v3_3="true"

  ri_gcc_v3_4="false"
  test $ri_gcc_main -gt 3 && ri_gcc_v3_4="true"
  test $ri_gcc_main -eq 3 && test $ri_gcc_minor -ge 4 && ri_gcc_v3_4="true"

  if $ri_gcc_v3_4; then
    ri_bothflags="-Wextra -Wall -pedantic"
  else
    ri_bothflags="-W -Wall -pedantic"
  fi

  if $ri_gcc_v2_95; then
    # the following options are taken from the gcc-2.95 info page
    ri_bothflags="$ri_bothflags -Wcast-align"
    ri_bothflags="$ri_bothflags -Wcast-qual"
    ri_bothflags="$ri_bothflags -Wmissing-prototypes"
    ri_bothflags="$ri_bothflags -Wmultichar"
    ri_bothflags="$ri_bothflags -Wpointer-arith"
    ri_bothflags="$ri_bothflags -Wredundant-decls"
    ri_bothflags="$ri_bothflags -Wshadow"
    ri_bothflags="$ri_bothflags -Wsign-compare"
    ri_bothflags="$ri_bothflags -Wundef"
    ri_bothflags="$ri_bothflags -Wwrite-strings"
    ri_cflags="$ri_cflags -Waggregate-return"
    ri_cflags="$ri_cflags -Wbad-function-cast"
    ri_cflags="$ri_cflags -Wmissing-declarations"
    ri_cflags="$ri_cflags -Wnested-externs"
    ri_cflags="$ri_cflags -Wstrict-prototypes"
    ri_cflags="$ri_cflags -Wmissing-declarations"
  fi

  if $ri_gcc_v3_2; then
    # the following options are taken from the gcc-3.2.1 info page
    ri_cflags="$ri_cflags -Wdiv-by-zero"
    ri_bothflags="$ri_bothflags -Wfloat-equal"
  fi

  if $ri_gcc_v3_3; then
    # the following options are taken from the gcc-3.3.2 info page
    ri_cppflags="$ri_cppflags -Wendif-labels"
    ri_bothflags="$ri_bothflags -Wdisabled-optimization"
  fi

  if test x"$enable_gcc_warnings" = x"error"; then
    ri_bothflags="$ri_bothflags -Werror"
  fi

  CPPFLAGS="$CPPFLAGS $ri_cppflags"
  CFLAGS="$CFLAGS $ri_bothflags $ri_cflags"
  CXXFLAGS="$CXXFLAGS $ri_bothflags $ri_cxxflags"
fi

])])
