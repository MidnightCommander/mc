dnl CPIO filesystem support
AC_DEFUN([mc_VFS_CPIOFS],
[
    AC_ARG_ENABLE([vfs-cpio],
		    AS_HELP_STRING([--enable-vfs-cpio], [Support for cpio filesystem @<:@yes@:>@]))
    if test "$enable_vfs" = "yes" -a x"$enable_vfs_cpio" != x"no"; then
	enable_vfs_cpio="yes"
	AC_DEFINE([ENABLE_VFS_CPIO], [1], [Support for cpio filesystem])
	mc_VFS_ADDNAME([cpio])
    fi
    AM_CONDITIONAL(ENABLE_VFS_CPIO, [test "$enable_vfs" = "yes" -a x"$enable_vfs_cpio" = x"yes"])
])
