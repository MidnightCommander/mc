dnl
dnl Check whether the g_module_* family of functions works
dnl on this system.  We need to know that at the compile time to
dnl decide whether to link with X11.
dnl
AC_DEFUN([AC_G_MODULE_SUPPORTED], [
    AC_CACHE_CHECK([if gmodule functionality is supported], mc_cv_g_module_supported, [
	ac_save_CFLAGS="$CFLAGS"
	ac_save_LIBS="$LIBS"
	CFLAGS="$CFLAGS $GMODULE_CFLAGS"
	LIBS="$GMODULE_LIBS $LIBS"
	AC_TRY_RUN([
#include <gmodule.h>

int main ()
{
    int ret = (g_module_supported () == TRUE) ? 0 : 1;
    return ret;
}
	],
	    [mc_cv_g_module_supported=yes],
	    [mc_cv_g_module_supported=no],
	    [mc_cv_g_module_supported=no]
	)
	CFLAGS="$ac_save_CFLAGS"
	LIBS="$ac_save_LIBS"
    ])

if test "$mc_cv_g_module_supported" = yes; then
    AC_DEFINE(HAVE_GMODULE, 1,
	      [Define if gmodule functionality is supported])
fi
])
