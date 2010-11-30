dnl
dnl Check whether the g_module_* family of functions works
dnl on this system.  We need to know that at the compile time to
dnl decide whether to link with X11.
dnl
AC_DEFUN([AC_G_MODULE_SUPPORTED], [

    g_module_supported=""
    if test x"$no_x" = xyes; then
        textmode_x11_support="no"
    else
        found_gmodule=no
        PKG_CHECK_MODULES(GMODULE, [gmodule-no-export-2.0 >= 2.8], [found_gmodule=yes], [:])
        if test x"$found_gmodule" = xyes; then
            g_module_supported="gmodule-no-export-2.0"
        else
            dnl try fallback to the generic gmodule
            PKG_CHECK_MODULES(GMODULE, [gmodule-2.0 >= 2.8], [found_gmodule=yes], [:])
            if test x"$found_gmodule" = xyes; then
                g_module_supported="gmodule-2.0"
            fi
        fi

        CPPFLAGS="$CPPFLAGS $X_CFLAGS"
        case x"$g_module_supported" in
            xgmodule-no-export-2.0|xgmodule-2.0)
                if test x`$PKG_CONFIG --variable=gmodule_supported "$g_module_supported"` = xtrue; then
                    AC_DEFINE([HAVE_GMODULE], [1], [Defined if gmodule functionality is supported])
                else
                    g_module_supported=""
                fi
                ;;
            *)
                MCLIBS="$X_LIBS $X_PRE_LIBS -lX11 $X_EXTRA_LIBS"
                g_module_supported=""
                ;;
        esac

        AC_DEFINE([HAVE_TEXTMODE_X11_SUPPORT], [1],
                        [Define to enable getting events from X Window System])
        textmode_x11_support="yes"
    fi

    AM_CONDITIONAL([HAVE_GMODULE], [test x"$g_module_supported" != x])
])
