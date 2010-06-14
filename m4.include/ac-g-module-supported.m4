dnl
dnl Check whether the g_module_* family of functions works
dnl on this system.  We need to know that at the compile time to
dnl decide whether to link with X11.
dnl
AC_DEFUN([AC_G_MODULE_SUPPORTED], [
    AC_CACHE_CHECK([if gmodule functionality is supported],
    [mc_cv_g_module_supported],
    [
        mc_cv_g_module_supported=no;
        PKG_CHECK_MODULES(GMODULE, [gmodule-2.0 >= 2.8], [found_gmodule=yes], [:])
        if test x"$found_gmodule" = "xyes"; then
            mc_cv_g_module_supported=yes
        fi
    ])

    if test x"$mc_cv_g_module_supported" = "xyes"; then
        if test x`$PKG_CONFIG --variable=gmodule_supported gmodule-2.0` = xtrue; then
            GLIB_LIBS="$GMODULE_LIBS $GLIB_LIBS"
            CFLAGS="$GMODULE_CFLAGS $CFLAGS"
            AC_DEFINE([HAVE_GMODULE], [1],
                [Defined if gmodule functionality is supported])
        fi
    fi
])
