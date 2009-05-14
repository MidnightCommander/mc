AC_DEFUN([AC_MC_VFS_MCFS_SET],
[
	AC_REQUIRE_SOCKET
	AC_CHECK_RPC
	AC_DEFINE(ENABLE_VFS_MCFS, 1, [Define to enable mc-specific networking file system])
	AC_MC_VFS_ADDNAME([mcfs])
        use_net_code=true
])

AC_DEFUN([AC_MC_VFS_MCFS],
[
    AC_ARG_ENABLE([vfs-mcfs],
                  [  --enable-vfs-mcfs    Enable Support MCFS (mc's network filesystem)])
    if test "$enable_vfs_mcfs" = "yes" ; then
        AC_MC_VFS_MCFS_SET
    fi
])
