dnl SFS support
AC_DEFUN([AC_MC_VFS_SFS],
[
    AC_ARG_ENABLE([sfs],
                  [  --disable-vfs-sfs         Support for sfs [[yes]]])
    if test x"$enable_vfs_sfs" != x"no"; then
	enable_vfs_sfs="yes"
	AC_MC_VFS_ADDNAME([sfs])
	AC_DEFINE([ENABLE_VFS_SFS], [1], [Support for sfs])
    fi
    AM_CONDITIONAL(ENABLE_VFS_SFS, [test x"$enable_vfs_sfs" = x"yes"])
])
