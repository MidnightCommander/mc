dnl
dnl Support for attributes on a Linux second extended file system
dnl
dnl if --enable-ext2f-attr is specified and the libraries are not found, configure fails.
dnl If --enable-ext2f-attr is not specified and the libraries are not found, configure continues with warning.

AC_DEFUN([mc_EXT2FS_ATTR],
[
    EXT2FS_MIN_VERSION="1.42.4"

    AC_ARG_ENABLE([ext2fs-attr],
        AS_HELP_STRING([--enable-ext2fs-attr],
        [Support for ext2/3/4 attributes @<:@yes@:>@]),
        [
            if test "x$enableval" = xno; then
                enable_ext2fs_attr=no
            else
                enable_ext2fs_attr=yes
            fi
        ],
        [enable_ext2fs_attr=auto])

    case "x$enable_ext2fs_attr" in
        xyes|xauto)
            PKG_CHECK_MODULES(EXT2FS, [ext2fs >= $EXT2FS_MIN_VERSION], [found_ext2fs=yes], [:])

            if test x"$found_ext2fs" = "xyes"; then
                PKG_CHECK_MODULES(E2P, [e2p >= $EXT2FS_MIN_VERSION], [found_e2p=yes], [:])

                if test x"$found_e2p" = "xyes"; then
                    ext2fs_attr_msg="yes"
                    AC_DEFINE(ENABLE_EXT2FS_ATTR, 1, [Define to enable support for ext2fs attributes])
                    MCLIBS="$MCLIBS $E2P_LIBS"
                    CPPFLAGS="$CPPFLAGS $EXT2FS_CFLAGS $E2P_CFLAGS"
                else
                    ext2fs_attr_nok_msg="e2p library not found or version too old ($EXT2FS_MIN_VERSION or higher is required)"
                    if test "x$enable_ext2fs_attr" = "xauto"; then
                        AC_MSG_WARN([$ext2fs_attr_nok_msg)])
                    else
                        AC_MSG_ERROR([$ext2fs_attr_nok_msg])
                    fi
                    ext2fs_attr_msg="no"
                fi
            else
                ext2fs_attr_nok_msg="ext2fs library not found or version too old ($EXT2FS_MIN_VERSION or higher is required)"
                if test "x$enable_ext2fs_attr" = "xauto"; then
                    AC_MSG_WARN([$ext2fs_attr_nok_msg])
                else
                    AC_MSG_ERROR([$ext2fs_attr_nok_msg])
                fi
                ext2fs_attr_msg="no"
            fi
            ;;
        *)
            ext2fs_attr_msg="no"
            ;;
    esac

    AM_CONDITIONAL(ENABLE_EXT2FS_ATTR, [test "x$ext2fs_attr_msg" = "xyes"])
])
