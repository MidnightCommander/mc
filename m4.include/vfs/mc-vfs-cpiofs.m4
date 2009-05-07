dnl CPIO filesystem support
AC_DEFUN([AC_MC_VFS_CPIOFS],
[
    AC_ARG_ENABLE([vfs-cpio],
                  [  --disable-vfs-cpio    Support for cpio filesystem [[yes]]])
    if test "$enable_vfs_cpio" != "no"; then
	if test "$enable_mvfs_cpio" ; then
	    AC_ERROR([Internal CPIO-FS conflicts with mvfs-cpiofs])
	fi
	AC_DEFINE([ENABLE_VFS_CPIO], [1], [Support for cpio filesystem])
	AC_MC_VFS_ADDNAME([cpio])
	enable_vfs_cpio="yes"
    fi
    AM_CONDITIONAL(ENABLE_VFS_CPIO, [test "$enable_vfs_cpio" = "yes"])
])
