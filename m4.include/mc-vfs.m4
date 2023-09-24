AC_DEFUN([mc_VFS_ADDNAME],
[
    if test x"$vfs_flags" = "x" ; then
	vfs_flags="$1"
    else
	vfs_flags="$vfs_flags, $1"
    fi
])

m4_include([m4.include/vfs/rpc.m4])
m4_include([m4.include/vfs/socket.m4])
m4_include([m4.include/vfs/mc-vfs-extfs.m4])
m4_include([m4.include/vfs/mc-vfs-sfs.m4])
m4_include([m4.include/vfs/mc-vfs-ftp.m4])
m4_include([m4.include/vfs/mc-vfs-sftp.m4])
m4_include([m4.include/vfs/mc-vfs-shell.m4])
m4_include([m4.include/vfs/mc-vfs-undelfs.m4])
m4_include([m4.include/vfs/mc-vfs-tarfs.m4])
m4_include([m4.include/vfs/mc-vfs-cpiofs.m4])

dnl mc_VFS_CHECKS
dnl   Check for various functions needed by libvfs.
dnl   This has various effects:
dnl     Sets MC_VFS_LIBS to libraries required
dnl     Sets vfs_flags to "pretty" list of vfs implementations we include.
dnl     Sets shell variable enable_vfs to yes (default, --with-vfs) or
dnl        "no" (--without-vfs).

dnl Private define
AC_DEFUN([mc_ENABLE_VFS_NET],
[
    dnl FIXME: network checks should probably be in their own macro.
    AC_REQUIRE_SOCKET
    if test x"$have_socket" = xyes; then
        AC_CHECK_TYPE([nlink_t], ,
                        [AC_DEFINE_UNQUOTED([nlink_t], [unsigned int],
                            [Define to 'unsigned int' if <sys/types.h> does not define.])])
	AC_CHECK_TYPES([socklen_t],,,
	    [
#include <sys/types.h>
#include <sys/socket.h>
	    ])

	AC_CHECK_RPC

	enable_vfs_net=yes
	AC_DEFINE(ENABLE_VFS_NET, [1], [Define to enable network VFSes support])
    fi
])

AC_DEFUN([mc_VFS_CHECKS],
[
    vfs_type="normal"

    AC_ARG_ENABLE([vfs],
	AS_HELP_STRING([--disable-vfs], [Disable VFS]),
	[
	    if test "x$enableval" = "xno"; then
		enable_vfs=no
	    else
		enable_vfs=yes
	    fi
	],
	[enable_vfs=yes])

    if test x"$enable_vfs" = x"yes" ; then
	vfs_type="Midnight Commander Virtual Filesystem"
	AC_MSG_NOTICE([Enabling VFS code])
	AC_DEFINE(ENABLE_VFS, [1], [Define to enable VFS support])
    fi

    mc_VFS_CPIOFS
    mc_VFS_EXTFS
    mc_VFS_SHELL
    mc_VFS_FTP
    mc_VFS_SFS
    mc_VFS_SFTP
    mc_VFS_TARFS
    mc_VFS_UNDELFS

    AM_CONDITIONAL(ENABLE_VFS, [test x"$enable_vfs" = x"yes"])

    if test x"$enable_vfs_ftp" = x"yes" -o x"$enable_vfs_shell" = x"yes" -o x"$enable_vfs_sftp" = x"yes"; then
	mc_ENABLE_VFS_NET
    fi

    AM_CONDITIONAL([ENABLE_VFS_NET], [test x"$enable_vfs_net" = x"yes"])
])
