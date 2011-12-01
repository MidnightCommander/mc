dnl Enable SFTP filesystem
AC_DEFUN([AC_MC_VFS_SFTP],
[
    AC_ARG_ENABLE([vfs-sftp],
                  AC_HELP_STRING([--enable-vfs-sftp], [Support for SFTP filesystem [[yes]]]))
    if test "$enable_vfs" != "no" -a x"$enable_vfs_sftp" != x"no"; then
        PKG_CHECK_MODULES(LIBSSH, [libssh2 >= 1.2.5], [found_libssh=yes], [:])
        if test x"$found_libssh" = "xyes"; then
            AC_MC_VFS_ADDNAME([sftp])
            AC_DEFINE([ENABLE_VFS_SFTP], [1], [Support for SFTP filesystem])
            MCLIBS="$MCLIBS $LIBSSH_LIBS"
            enable_vfs_sftp="yes"
        else
            enable_vfs_sftp="no"
        fi
    fi
    AM_CONDITIONAL([ENABLE_VFS_SFTP], [test "$enable_vfs" = "yes" -a x"$enable_vfs_sftp" = x"yes"])
])
