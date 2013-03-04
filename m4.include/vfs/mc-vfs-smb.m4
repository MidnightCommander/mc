dnl @synopsis AC_MC_VFS_SMB
dnl
dnl Adds Samba support
dnl
dnl @author Slava Zanko <slavazanko@gmail.com>
dnl @version 2013-03-05
dnl @license GPL
dnl @copyright Free Software Foundation, Inc.

dnl check for ncurses in user supplied path
AC_DEFUN([MC_CHECK_SAMBA_BY_PATH], [

    ac_samba_inc_path=[$1]
    ac_samba_lib_path=[$2]

    if test x"$ac_samba_inc_path" != x; then
        ac_samba_inc_path="-I"$ac_samba_inc_path
    fi

    if test x"$ac_samba_lib_path" != x; then
        ac_samba_lib_path="-L"$ac_samba_lib_path
    fi

    saved_CPPFLAGS="$CPPFLAGS"
    saved_LDFLAGS="$LDFLAGS"
    CPPFLAGS="$CPPFLAGS $ac_samba_inc_path"
    LDFLAGS="$LDFLAGS $ac_samba_lib_path"

    dnl Check for the headers
    AC_CHECK_HEADERS([libsmbclient.h],
	[AC_CHECK_LIB(smbclient, smbc_init, [found_samba=yes], [found_samba=no], [])],
	[found_samba=no], [])

    if test x"$found_samba" != x"yes"; then
	CPPFLAGS="$saved_CPPFLAGS"
	LDFLAGS="$saved_LDPFLAGS"
	AC_MSG_ERROR([Samba's development files not found])
    fi
])

dnl Samba support
AC_DEFUN([AC_MC_VFS_SMB],
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

	dnl checks for Samba-3
	AC_ARG_WITH([samba-includes],
	    AS_HELP_STRING([--with-samba-includes=@<:@DIR@:>@],
	        [set path to Samba includes @<:@default=/usr/include@:>@; make sense only if --enable-vfs-smb=yes]
	    ),
	    [ac_samba_inc_path="$withval"],
	    [ac_samba_inc_path=""]
	)

	AC_ARG_WITH([samba-libs],
	    AS_HELP_STRING([--with-samba-libs=@<:@DIR@:>@],
	        [set path to Samba library @<:@default=/usr/lib@:>@; make sense only if --enable-vfs-smb=yes]
	    ),
	    [ac_samba_lib_path="$withval"],
	    [ac_samba_lib_path=""]
	)

	if test x"$ac_samba_lib_path" != x -o x"$ac_samba_inc_path" != x; then
	    echo 'checking Samba headers in specified place ...'
	    MC_CHECK_SAMBA_BY_PATH([$ac_samba_inc_path],[$ac_samba_lib_path])
	else
	    found_samba=no
	    dnl Just in case: search Samba3's pkgconfig settings
	    PKG_CHECK_MODULES(SAMBA3, [smbclient >= 0.0.1], [found_samba=yes], [:])
	    if test x"$found_samba" = "xyes"; then
		MCLIBS="$MCLIBS $SAMBA3_LIBS"
		CPPFLAGS="$CPPFLAGS $SAMBA3_CFLAGS"
	    else
		if test x"$found_samba" = "xno"; then
		    dnl Search in /usr
		    found_samba=yes
		    echo 'checking Samba headers in /usr ...'
		    MC_CHECK_SAMBA_BY_PATH([],[])

		    if test x"$found_samba" = "xno"; then
			dnl Search in /usr/local
			found_samba=yes
			echo 'checking Samba headers in /usr/local ...'
			MC_CHECK_SAMBA_BY_PATH([/usr/local/include],[/usr/local/lib])
		    fi
		fi
	    fi
	fi

	if test x"$found_samba" != "xno"; then
	    AC_MC_VFS_ADDNAME([smb])
	    AC_DEFINE([ENABLE_VFS_SMB], [1], [Define to enable VFS over SMB])
	    LDFLAGS="$LDFLAGS -lsmbclient"
	    AM_CONDITIONAL([ENABLE_VFS_SMB], [test "1" = "1"])
	else
	    exit 1
	fi
    else
	AM_CONDITIONAL([ENABLE_VFS_SMB], [test "1" = "2"])
    fi
])
