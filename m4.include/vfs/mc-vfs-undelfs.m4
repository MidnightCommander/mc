dnl mc_UNDELFS_CHECKS
dnl    Check for ext2fs undel support.
dnl    Set shell variable ext2fs_undel to "yes" if we have it,
dnl    "no" otherwise.  May define ENABLE_VFS_UNDELFS for cpp.
dnl    Will set EXT2FS_UNDEL_LIBS to required libraries.

AC_DEFUN([mc_UNDELFS_CHECKS], [
    ext2fs_undel=no
    EXT2FS_UNDEL_LIBS=

    dnl Use result of mc_EXT2FS_ATTR that was called earlier
    if test "x$ext2fs_attr_msg" = "xyes"; then
	com_err=no

	PKG_CHECK_MODULES(COM_ERR, [com_err >= 1.42.4], [com_err=yes], [:])

	if test x"$com_err" = "xyes"; then
	    EXT2FS_UNDEL_LIBS="$EXT2FS_LIBS $COM_ERR_LIBS"
	    ext2fs_undel=yes
	fi
    fi

])

dnl
dnl Ext2fs undelete support
dnl
AC_DEFUN([mc_VFS_UNDELFS],
[
    AC_ARG_ENABLE([vfs-undelfs],
	AS_HELP_STRING([--enable-vfs-undelfs], [Support for ext2 undelete filesystem @<:@no@:>@]),
	[
	    if test "x$enableval" = "xno"; then
		enable_vfs_undelfs=no
	    else
		enable_vfs_undelfs=yes
	    fi
	],
	[enable_vfs_undelfs="no"])

    if test x"$enable_vfs" = x"yes" -a x"$enable_vfs_undelfs" != x"no"; then
	mc_UNDELFS_CHECKS

	if test x"$ext2fs_undel" = x"yes"; then
	    enable_vfs_undelfs="yes"
	    mc_VFS_ADDNAME([undelfs])
	    AC_DEFINE(ENABLE_VFS_UNDELFS, [1], [Support for ext2 undelfs])
	    AC_MSG_NOTICE([using ext2fs file recovery code])
	    MCLIBS="$MCLIBS $EXT2FS_UNDEL_LIBS"
	else
	    AC_MSG_ERROR([Ext2 libraries not found])
	fi
    fi
    AM_CONDITIONAL(ENABLE_VFS_UNDELFS, [test "$enable_vfs" = "yes" -a x"$enable_vfs_undelfs" = x"yes"])
])
