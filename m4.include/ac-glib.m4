dnl
dnl Check whether the g_module_* family of functions works
dnl on this system.  We need to know that at the compile time to
dnl decide whether to link with X11.
dnl
AC_DEFUN([mc_GMODULE_SUPPORTED], [

    g_module_supported=""

    if test x$with_glib_static = xyes; then
        AC_MSG_WARN([Static build is enabled... Cannot use GModule])
    else
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

        if test x"$g_module_supported" != x; then
            GLIB_LIBS="$GMODULE_LIBS"
            GLIB_CFLAGS="$GMODULE_CFLAGS"
        fi
    fi

    AM_CONDITIONAL([HAVE_GMODULE], [test x"$g_module_supported" != x])
])

AC_DEFUN([mc_CHECK_STATIC_GLIB], [
    dnl
    dnl Try to find static libraries for glib and gmodule.
    dnl
    AC_ARG_WITH([glib_static],
                AS_HELP_STRING([--with-glib-static], [Link glib statically @<:@no@:>@]),
                [
                    if test x$withval = xno; then
                        with_glib_static=no
                    else
                        with_glib_static=yes
                    fi
                ],
                [with_glib_static=no])

    if test x$with_glib_static = xyes; then
        dnl Redefine GLIB_LIBS using --static option to define linker flags for static linking
        GLIB_LIBS=`pkg-config --libs --static glib-2.0`

        GLIB_LIBDIR=`pkg-config --variable=libdir glib-2.0`
        new_GLIB_LIBS=
        for i in $GLIB_LIBS; do
            case x$i in
            x-lglib*)
                lib=glib ;;
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

    glib_found=no
    PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.12], [glib_found=yes], [:])
    if test x"$glib_found" = xno; then
        AC_MSG_ERROR([glib-2.0 not found or version too old (must be >= 2.12)])
    fi

    mc_CHECK_STATIC_GLIB
    mc_GMODULE_SUPPORTED
])

