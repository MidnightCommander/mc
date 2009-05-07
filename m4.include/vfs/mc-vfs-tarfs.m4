dnl TAR filesystem support
AC_DEFUN([AC_MC_VFS_TARFS],
[
    AC_ARG_ENABLE([vfs-tar],
                  [  --disable-vfs-tar     Support for tar filesystem [[yes]]])
    if test "$enable_vfs_tar" != "no"; then
	if test "$enable_mvfs_tar" ; then
	    AC_ERROR([Internal TAR-FS conflicts with mvfs-tarfs])
	fi
	enable_vfs_tar="yes"
	AC_MC_VFS_ADDNAME([tar])
	AC_DEFINE([ENABLE_VFS_TAR], [1], [Support for tar filesystem])
    fi
    AM_CONDITIONAL(ENABLE_VFS_TAR, [test "$enable_vfs_tar" = "yes"])
])
