dnl Samba support
AC_DEFUN([AC_MC_VFS_SMB],
[
    AC_ARG_ENABLE([vfs-smb],
		AC_HELP_STRING([--enable-vfs-smb], [Support for SMB filesystem [[no]]]),
		[
		    if test "x$enableval" = "xno"; then
			enable_vfs_smb=no
		    else
			enable_vfs_smb=yes
		    fi
		],
		[enable_vfs_smb=no])

    if test "$enable_vfs" != "no" -a x"$enable_vfs_smb" != x"no"; then
	enable_vfs_smb="yes"
	AC_MC_VFS_ADDNAME([smb])
	AC_DEFINE([ENABLE_VFS_SMB], [1], [Define to enable VFS over SMB])
    fi

    if test "$enable_vfs_smb" = "yes"; then
	AC_CONFIG_SUBDIRS([lib/vfs/mc-vfs/samba])
    fi

    AM_CONDITIONAL([ENABLE_VFS_SMB], [test x"$enable_vfs_smb" = x"yes"])
])
