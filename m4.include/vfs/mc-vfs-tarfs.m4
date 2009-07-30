dnl TAR filesystem support
AC_DEFUN([AC_MC_VFS_TARFS],
[
    AC_ARG_ENABLE([vfs-tar],
                  [  --enable-vfs-tar        Support for tar filesystem [[yes]]])
    if test x"$enable_vfs_tar" != x"no"; then
	enable_vfs_tar="yes"
	AC_MC_VFS_ADDNAME([tar])
	AC_DEFINE([ENABLE_VFS_TAR], [1], [Support for tar filesystem])
    fi
    AM_CONDITIONAL(ENABLE_VFS_TAR, [test x"$enable_vfs_tar" = x"yes"])
])
