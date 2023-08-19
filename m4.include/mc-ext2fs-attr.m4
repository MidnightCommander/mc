dnl
dnl Support for attributes on a Linux second extended file system
dnl
AC_DEFUN([mc_EXT2FS_ATTR],
[
    ext2fs_attr_msg="no"

    PKG_CHECK_MODULES(EXT2FS, [ext2fs >= 1.42.4], [found_ext2fs=yes], [:])

    if test x"$found_ext2fs" = "xyes"; then
        PKG_CHECK_MODULES(E2P, [e2p >= 1.42.4], [found_e2p=yes], [:])

        if test x"$found_e2p" = "xyes"; then
            AC_DEFINE(ENABLE_EXT2FS_ATTR, 1, [Define to enable support for ext2fs attributes])
            MCLIBS="$MCLIBS $E2P_LIBS"
            CPPFLAGS="$CPPFLAGS $EXT2FS_CFLAGS $E2P_CFLAGS"
            ext2fs_attr_msg="yes"
        else
            AC_MSG_WARN([e2p library not found or version too old (must be >= 1.42.4)])
            ext2fs_attr_msg="no"
        fi
    else
        AC_MSG_WARN([ext2fs library not found or version too old (must be >= 1.42.4)])
        ext2fs_attr_msg="no"
    fi

    AM_CONDITIONAL(ENABLE_EXT2FS_ATTR, [test "x$ext2fs_attr_msg" = "xyes"])
])
