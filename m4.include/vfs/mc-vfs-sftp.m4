dnl Enable SFTP filesystem
AC_DEFUN([mc_VFS_SFTP],
[
    AC_ARG_ENABLE([vfs-sftp],
                  AS_HELP_STRING([--enable-vfs-sftp], [Support for SFTP filesystem [auto]]))
    if test "$enable_vfs" != "no" -a x"$enable_vfs_sftp" != x"no"; then
        PKG_CHECK_MODULES(LIBSSH, [libssh2 >= 1.2.5], [found_libssh=yes], [:])
        if test x"$found_libssh" = "xyes"; then
            mc_VFS_ADDNAME([sftp])
            AC_DEFINE([ENABLE_VFS_SFTP], [1], [Support for SFTP filesystem])
            MCLIBS="$MCLIBS $LIBSSH_LIBS"
            enable_vfs_sftp="yes"
        else
            if test x"$enable_vfs_sftp" = x"yes"; then
                dnl user explicitly requested feature
                AC_MSG_ERROR([libssh2 >= 1.2.5 library not found])
            fi
            enable_vfs_sftp="no"
        fi
    fi
    AM_CONDITIONAL([ENABLE_VFS_SFTP], [test "$enable_vfs" = "yes" -a x"$enable_vfs_sftp" = x"yes"])
])
