dnl mc_EXTFS_CHECKS
dnl    Check for tools used in extfs scripts.

dnl FIXME: make this configurable
AC_DEFUN([mc_EXTFS_CHECKS], [
    AC_PATH_PROG([ZIP], [zip], [/usr/bin/zip])
    AC_PATH_PROG([UNZIP], [unzip], [/usr/bin/unzip])
    AC_CACHE_CHECK([for zipinfo code in unzip], [mc_cv_have_zipinfo],
	[mc_cv_have_zipinfo=no
	if $UNZIP -Z </dev/null >/dev/null 2>&1; then
	    mc_cv_have_zipinfo=yes
	fi])
    if test x"$mc_cv_have_zipinfo" = xyes; then
	HAVE_ZIPINFO=1
    else
	HAVE_ZIPINFO=0
    fi
    AC_SUBST([HAVE_ZIPINFO])
])


dnl Enable Extfs (classic)
AC_DEFUN([mc_VFS_EXTFS],
[
    AC_ARG_ENABLE([vfs-extfs],
		    AS_HELP_STRING([--enable-vfs-extfs], [Support for extfs filesystem @<:@yes@:>@]))
    if test "$enable_vfs" = "yes" -a x"$enable_vfs_extfs" != x"no"; then
	mc_EXTFS_CHECKS
	enable_vfs_extfs="yes"
	mc_VFS_ADDNAME([extfs])
	AC_DEFINE([ENABLE_VFS_EXTFS], [1], [Support for extfs])
    fi
    AM_CONDITIONAL(ENABLE_VFS_EXTFS, [test "$enable_vfs" = "yes" -a x"$enable_vfs_extfs" = x"yes"])
])
