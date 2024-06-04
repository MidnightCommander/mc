dnl
dnl Internal editor support.
dnl
AC_DEFUN([mc_WITH_INTERNAL_EDIT], [

    AC_ARG_WITH([internal_edit],
        AS_HELP_STRING([--with-internal-edit], [Enable internal editor @<:@yes@:>@]))

    if test x$with_internal_edit != xno; then
            AC_DEFINE(USE_INTERNAL_EDIT, 1, [Define to enable internal editor])
            use_internal_edit=yes
            AC_MSG_NOTICE([using internal editor])
            edit_msg="yes"
    else
            use_internal_edit=no
            edit_msg="no"
    fi

    dnl ASpell support.
    AC_ARG_ENABLE([aspell],
        AS_HELP_STRING(
        [--enable-aspell@<:@=prefix@:>@],
        [Enable aspell support for internal editor @<:@no@:>@] and optionally set path to aspell installation prefix @<:@default=/usr@:>@),
        [
            if test "x$enableval" = xno; then
                enable_aspell=no
            else
                test -d "$enable_aspell/include" && CPPFLAGS="$CPPFLAGS -I$enable_aspell/include"
                enable_aspell=yes
            fi
        ],
        [enable_aspell=no]
    )

    if test x$with_internal_edit != xno -a x$enable_aspell != xno; then
            AC_CHECK_HEADERS([aspell.h], [], [
                AC_MSG_ERROR([Could not find aspell development headers])
            ], [])

            if test x"$g_module_supported" != x; then
                AC_DEFINE(HAVE_ASPELL, 1, [Define to enable aspell support])
                edit_msg="yes with aspell support"
                AC_MSG_NOTICE([using aspell for internal editor])
            else
                enable_aspell=no
                AC_MSG_ERROR([aspell support is disabled because gmodule support is not available])
            fi
    fi
])
