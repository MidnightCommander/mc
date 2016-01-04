dnl SFS support
AC_DEFUN([mc_VFS_SFS],
[
    AC_ARG_ENABLE([vfs-sfs],
		    AS_HELP_STRING([--enable-vfs-sfs], [Support for sfs filesystem @<:@yes@:>@]))
    if test "$enable_vfs" = "yes" -a x"$enable_vfs_sfs" != x"no"; then
	enable_vfs_sfs="yes"
	mc_VFS_ADDNAME([sfs])
	AC_DEFINE([ENABLE_VFS_SFS], [1], [Support for sfs])
    fi
    AM_CONDITIONAL(ENABLE_VFS_SFS, [test "$enable_vfs" = "yes" -a x"$enable_vfs_sfs" = x"yes"])
])
