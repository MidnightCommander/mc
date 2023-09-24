dnl Enable SHELL protocol
AC_DEFUN([mc_VFS_SHELL],
[
    AC_ARG_ENABLE([vfs-shell],
		    AS_HELP_STRING([--enable-vfs-shell], [Support for SHELL filesystem @<:@yes@:>@]))
    if test "$enable_vfs" = "yes" -a "x$enable_vfs_shell" != xno; then
	enable_vfs_shell="yes"
	mc_VFS_ADDNAME([shell])
	AC_DEFINE([ENABLE_VFS_SHELL], [1], [Support for SHELL vfs])
    fi
    AM_CONDITIONAL(ENABLE_VFS_SHELL, [test "$enable_vfs" = "yes" -a x"$enable_vfs_shell" = x"yes"])
])
