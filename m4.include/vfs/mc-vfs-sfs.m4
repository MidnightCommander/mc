dnl SFS support
AC_DEFUN([AC_MC_VFS_SFS],
[
    AC_ARG_ENABLE([sfs],
                  [  --disable-vfs-sfs         Support for sfs [[yes]]])
    if test "$enable_vfs_sfs" != "no"; then
	if test "$enable_mvfs_sfs" ; then
	    AC_ERROR([Internal SFS conflicts with mvfs-sfs])
	fi
	enable_vfs_sfs="yes"
	AC_MC_VFS_ADDNAME([sfs])
	AC_DEFINE([ENABLE_VFS_SFS], [1], [Support for sfs])
    fi
    AM_CONDITIONAL(ENABLE_VFS_SFS, [test "$enable_vfs_sfs" = "yes"])
])
