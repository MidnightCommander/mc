dnl @synopsis MC_DOC_LINGUAS
dnl
dnl Check if unit tests enabled
dnl
dnl @author Slava Zanko <slavazanko@gmail.com>
dnl @version 2011-02-10
dnl @license GPL
dnl @copyright Free Software Foundation, Inc.

AC_DEFUN([MC_DOC_LINGUAS],[
    dnl Determine which help translations we want to install.
    ALL_DOC_LINGUAS="es hu it pl ru sr"

    DOC_LINGUAS=
    if test "x$USE_NLS" = xyes; then
        if test -z "$LINGUAS"; then
            langs="`grep -v '^#' $srcdir/po/LINGUAS`"
        else
            langs="$LINGUAS"
        fi
    else
        langs=
    fi

    for h_lang in $ALL_DOC_LINGUAS; do
        for lang in $langs; do
            if test "$lang" = "$h_lang"; then
                DOC_LINGUAS="$DOC_LINGUAS $lang"
                break
            fi
        done
    done
    AC_SUBST(DOC_LINGUAS)
])
