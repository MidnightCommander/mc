dnl Samba support
AC_DEFUN([AC_MC_VFS_SMB],
[
    AC_ARG_ENABLE([vfs-smb],
		AC_HELP_STRING([--enable-vfs-smb], [Support for SMB filesystem [[no]]]))
    if test "$enable_vfs" != "no" -a x"$enable_vfs_smb" != x"no"; then
	enable_vfs_smb="yes"
	AC_MC_VFS_ADDNAME([smb])
	AC_DEFINE([ENABLE_VFS_SMB], [1], [Define to enable VFS over SMB])

	# set Samba configuration directory location
	configdir="/etc"
	AC_ARG_WITH([smb-configdir],
		AC_HELP_STRING([--with-smb-configdir=DIR], [Where the Samba configuration files are [[/etc]]]),
		[ case "$withval" in
		    yes|no)
			# Just in case anybody does it
			AC_MSG_WARN([--with-smb-configdir called without argument - will use default])
			;;
		    *)
			configdir="$withval"
			;;
		esac
	])

	AC_ARG_WITH([smb-codepagedir],
		AC_HELP_STRING([--with-smb-codepagedir=DIR], [Where the Samba codepage files are]),
		[ case "$withval" in
		    yes|no)
			# Just in case anybody does it
			AC_MSG_WARN([--with-smb-codepagedir called without argument - will use default])
			;;
		    *)
			codepagedir="$withval"
		    ;;
		esac
	])


	AC_SUBST(configdir)
	AC_SUBST(codepagedir)

	AC_CONFIG_SUBDIRS([lib/vfs/mc-vfs/samba])
    fi

    AM_CONDITIONAL([ENABLE_VFS_SMB], [test x"$enable_vfs_smb" = x"yes"])
])
