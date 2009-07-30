dnl CPIO filesystem support
AC_DEFUN([AC_MC_VFS_CPIOFS],
[
    AC_ARG_ENABLE([vfs-cpio],
                  [  --enable-vfs-cpio       Support for cpio filesystem [[yes]]])
    if test x"$enable_vfs_cpio" != x"no"; then
	enable_vfs_cpio="yes"
	AC_DEFINE([ENABLE_VFS_CPIO], [1], [Support for cpio filesystem])
	AC_MC_VFS_ADDNAME([cpio])
    fi
    AM_CONDITIONAL(ENABLE_VFS_CPIO, [test x"$enable_vfs_cpio" = x"yes"])
])
