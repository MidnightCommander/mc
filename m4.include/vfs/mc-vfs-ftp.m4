dnl Enable FTP filesystem (classic)
AC_DEFUN([AC_MC_VFS_FTP],
[
    AC_ARG_ENABLE([vfs-ftp],
		  [  --disable-vfs-ftp     Support for FTP vfs [[yes]]])
    if test x"$enable_vfs_ftp" != x"no"; then
	enable_vfs_ftp="yes"
	AC_MC_VFS_ADDNAME([ftp])
	AC_DEFINE([ENABLE_VFS_FTP], [1], [Support for FTP (classic)])
    fi
    AM_CONDITIONAL([ENABLE_VFS_FTP], [test x"$enable_vfs_ftp" = x"yes"])
])
