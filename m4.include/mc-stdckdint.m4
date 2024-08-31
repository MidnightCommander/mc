dnl
dnl Check <stdckdint.h> that is like C23.
dnl

AC_DEFUN([mc_CHECK_HEADER_STDCKDINT],
[
    AC_CHECK_HEADERS_ONCE([stdckdint.h])
    if test $ac_cv_header_stdckdint_h = yes; then
        GL_GENERATE_STDCKDINT_H=false
    else
        GL_GENERATE_STDCKDINT_H=true
    fi
    gl_CONDITIONAL_HEADER([stdckdint.h])

    dnl We need the following in order to create <stdckdint.h> when the system
    dnl doesn't have one that works with the given compiler.
    if test "$GL_GENERATE_STDCKDINT_H" = "true"; then
        sed -e 1h -e '1s,.*,/* DO NOT EDIT! GENERATED AUTOMATICALLY! */,' -e s,bool,gboolean, -e 1G \
            $ac_abs_confdir/lib/stdckdint.in.h > $ac_abs_confdir/lib/stdckdint.h
    else
        rm -f "$ac_abs_confdir/lib/stdckdint.h"
    fi
])
