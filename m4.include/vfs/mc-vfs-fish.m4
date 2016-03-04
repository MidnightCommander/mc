dnl Enable FISH protocol (classic)
AC_DEFUN([mc_VFS_FISH],
[
    AC_ARG_ENABLE([vfs-fish],
		    AS_HELP_STRING([--enable-vfs-fish], [Support for FISH filesystem @<:@yes@:>@]))
    if test "$enable_vfs" = "yes" -a "x$enable_vfs_fish" != xno; then
	enable_vfs_fish="yes"
	mc_VFS_ADDNAME([fish])
	AC_DEFINE([ENABLE_VFS_FISH], [1], [Support for FISH vfs])
    fi
    AM_CONDITIONAL(ENABLE_VFS_FISH, [test "$enable_vfs" = "yes" -a x"$enable_vfs_fish" = x"yes"])
])
