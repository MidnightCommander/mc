# gnulib-common.m4
# serial 101
dnl Copyright (C) 2007-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_PREREQ([2.62])


# gl_CONDITIONAL(conditional, condition)
# is like AM_CONDITIONAL(conditional, condition), except that it does not
# produce an error
#   configure: error: conditional "..." was never defined.
#   Usually this means the macro was only invoked conditionally.
# when only invoked conditionally. Instead, in that case, both the _TRUE
# and the _FALSE case are disabled.
AC_DEFUN([gl_CONDITIONAL],
[
  pushdef([AC_CONFIG_COMMANDS_PRE], [:])dnl
  AM_CONDITIONAL([$1], [$2])
  popdef([AC_CONFIG_COMMANDS_PRE])dnl
  if test -z "${[$1]_TRUE}" && test -z "${[$1]_FALSE}"; then
    [$1]_TRUE='#'
    [$1]_FALSE='#'
  fi
])

dnl gl_CONDITIONAL_HEADER([foo.h])
dnl takes a shell variable GL_GENERATE_FOO_H (with value true or false) as input
dnl and produces
dnl   - an AC_SUBSTed variable FOO_H that is either a file name or empty, based
dnl     on whether GL_GENERATE_FOO_H is true or false,
dnl   - an Automake conditional GL_GENERATE_FOO_H that evaluates to the value of
dnl     the shell variable GL_GENERATE_FOO_H.
AC_DEFUN([gl_CONDITIONAL_HEADER],
[
  m4_pushdef([gl_header_name], AS_TR_SH(m4_toupper($1)))
  m4_pushdef([gl_generate_var], [GL_GENERATE_]AS_TR_SH(m4_toupper($1)))
  m4_pushdef([gl_generate_cond], [GL_GENERATE_]AS_TR_SH(m4_toupper($1)))
  case "$gl_generate_var" in
    false) gl_header_name='' ;;
    true)
      dnl It is OK to use a .h file in lib/ from within tests/, but not vice
      dnl versa.
      if test -z "$gl_header_name"; then
        gl_header_name="${gl_source_base_prefix}$1"
      fi
      ;;
    *) echo "*** gl_generate_var is not set correctly" 1>&2; exit 1 ;;
  esac
  AC_SUBST(gl_header_name)
  gl_CONDITIONAL(gl_generate_cond, [$gl_generate_var])
  m4_popdef([gl_generate_cond])
  m4_popdef([gl_generate_var])
  m4_popdef([gl_header_name])
])
