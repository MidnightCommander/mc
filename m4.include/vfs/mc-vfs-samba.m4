dnl Samba support
AC_DEFUN([mc_VFS_SMB],
[
    AC_ARG_ENABLE([vfs-smb],
		AS_HELP_STRING([--enable-vfs-smb], [Support for SMB filesystem @<:@no@:>@]),
		[
		    if test "x$enableval" = "xno"; then
			enable_vfs_smb=no
		    else
			enable_vfs_smb=yes
		    fi
		],
		[enable_vfs_smb=no])

    if test "$enable_vfs" = "yes" -a x"$enable_vfs_smb" != x"no"; then
	enable_vfs_smb="yes"
	mc_VFS_ADDNAME([smb])
	AC_DEFINE([ENABLE_VFS_SMB], [1], [Define to enable VFS over SMB])
    fi

    if test "$enable_vfs_smb" = "yes"; then
	AC_CONFIG_SUBDIRS([src/vfs/smbfs/helpers])

	AM_CONDITIONAL([ENABLE_VFS_SMB], [test "1" = "1"])

	# set configuration directory location
	smbconfigdir="/etc"
	AC_ARG_WITH(smb-configdir,
		    [  --with-smb-configdir=DIR    Where to put configuration files],
		    [ case "$withval" in
			    yes|no)
				# Just in case anybody does it
				AC_MSG_WARN([--with-smb-configdir called without argument - will use default])
				;;
			    *)
				smbconfigdir="$withval"
				;;
		    esac])

	AC_SUBST(smbconfigdir)

	# set codepage directory location
	AC_ARG_WITH(smb-codepagedir,
		    [  --with-smb-codepagedir=DIR  Where to put codepage files],
		    [ case "$withval" in
			yes|no)
			    # Just in case anybody does it
			    AC_MSG_WARN([--with-smb-codepagedir called without argument - will use default])
			    ;;
			*)
			    smbcodepagedir="$withval"
			    ;;
		 esac])

	# export variable for child process (configure of samba)
	export SMBCONFIGDIR="$smbconfigdir"
	export SMBCODEPAGEDIR="$smbcodepagedir"
    else
	AM_CONDITIONAL([ENABLE_VFS_SMB], [test "1" = "2"])
    fi
])
