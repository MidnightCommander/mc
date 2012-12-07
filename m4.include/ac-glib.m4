dnl
dnl Check whether the g_module_* family of functions works
dnl on this system.  We need to know that at the compile time to
dnl decide whether to link with X11.
dnl
AC_DEFUN([AC_G_MODULE_SUPPORTED], [

    g_module_supported=""

    found_gmodule=no
    PKG_CHECK_MODULES(GMODULE, [gmodule-no-export-2.0 >= 2.12], [found_gmodule=yes], [:])
    if test x"$found_gmodule" = xyes; then
        g_module_supported="gmodule-no-export-2.0"
    else
        dnl try fallback to the generic gmodule
        PKG_CHECK_MODULES(GMODULE, [gmodule-2.0 >= 2.12], [found_gmodule=yes], [:])
        if test x"$found_gmodule" = xyes; then
            g_module_supported="gmodule-2.0"
        fi
    fi

    case x"$g_module_supported" in
        xgmodule-no-export-2.0|xgmodule-2.0)
            if test x`$PKG_CONFIG --variable=gmodule_supported "$g_module_supported"` = xtrue; then
                AC_DEFINE([HAVE_GMODULE], [1], [Defined if gmodule functionality is supported])
            else
                g_module_supported=""
            fi
            ;;
        *)
            g_module_supported=""
            ;;
    esac

    AM_CONDITIONAL([HAVE_GMODULE], [test x"$g_module_supported" != x])

    dnl
    dnl Try to find static libraries for glib and gmodule.
    dnl
    if test x$with_glib_static = xyes; then
        new_GLIB_LIBS=
        for i in $GLIB_LIBS; do
            case x$i in
            x-lglib*)
                lib=glib ;;
            x-lgmodule*)
                lib=gmodule ;;
            *)
                lib=
                add="$i" ;;
            esac

            if test -n "$lib"; then
                lib1=`echo $i | sed 's/^-l//'`
                if test -f "$GLIB_LIBDIR/lib${lib1}.a"; then
                    add="$GLIB_LIBDIR/lib${lib1}.a"
                else
                    if test -f "$GLIB_LIBDIR/lib${lib}.a"; then
                        add="$GLIB_LIBDIR/lib${lib}.a"
                    else
                        AC_MSG_ERROR([Cannot find static $lib])
                    fi
                fi
            fi
            new_GLIB_LIBS="$new_GLIB_LIBS $add"
        done
        GLIB_LIBS="$new_GLIB_LIBS"
    fi

])

AC_DEFUN([AC_CHECK_GLIB], [
    dnl
    dnl First try glib 2.x.
    dnl Keep this check close to the beginning, so that the users
    dnl without any glib won't have their time wasted by other checks.
    dnl

    AC_ARG_WITH([glib_static],
        AS_HELP_STRING([--with-glib-static], [Link glib statically @<:@no@:>@]))

    glib_found=no
    PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.12], [glib_found=yes], [:])
    if test x"$glib_found" = xno; then
        AC_MSG_ERROR([glib-2.0 not found or version too old (must be >= 2.12)])
    fi

])

