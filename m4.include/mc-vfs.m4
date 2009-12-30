AC_DEFUN([AC_MC_VFS_ADDNAME],
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
m4_include([m4.include/vfs/mc-vfs-fish.m4])
m4_include([m4.include/vfs/mc-vfs-undelfs.m4])
m4_include([m4.include/vfs/mc-vfs-tarfs.m4])
m4_include([m4.include/vfs/mc-vfs-cpiofs.m4])
m4_include([m4.include/vfs/mc-vfs-mcfs.m4])
m4_include([m4.include/vfs/mc-vfs-samba.m4])
m4_include([m4.include/vfs/mc-mvfs.m4])

dnl MC_VFS_CHECKS
dnl   Check for various functions needed by libvfs.
dnl   This has various effects:
dnl     Sets MC_VFS_LIBS to libraries required
dnl     Sets vfs_flags to "pretty" list of vfs implementations we include.
dnl     Sets shell variable enable_vfs to yes (default, --with-vfs) or
dnl        "no" (--without-vfs).

dnl Private define
AC_DEFUN([MC_WITH_VFS],
[
    use_net_code=false

    AC_ARG_ENABLE([netcode],
		[  --enable-netcode        Support for networking [[yes]]])

    if test "x$enable_netcode" != xno; then
	dnl FIXME: network checks should probably be in their own macro.
	AC_SEARCH_LIBS(socket, [xnet bsd socket inet], [have_socket=yes])
	if test x"$have_socket" = xyes; then
	    AC_SEARCH_LIBS(gethostbyname, [bsd socket inet netinet])
	    AC_CHECK_MEMBERS([struct linger.l_linger], , , [
#include <sys/types.h>
#include <sys/socket.h>
	    ])

	    AC_CHECK_RPC
	    AC_REQUIRE_SOCKET

      MC_MCSERVER_CHECKS

      use_net_code=true
    fi
  fi


  AC_DEFINE(ENABLE_VFS, 1, [Define to enable VFS support])
  if $use_net_code; then
    AC_DEFINE(USE_NETCODE, 1, [Define to use networked VFS])
  fi
])

AC_DEFUN([AC_MC_VFS_CHECKS],[
    AC_ARG_ENABLE([vfs],
                  [  --disable-vfs           Disable VFS])
    if test x"$enable_vfs" != x"no" ; then
	enable_vfs="yes"
	vfs_type="Midnight Commander Virtual Filesystem"

	AC_MSG_NOTICE([Enabling VFS code])

	AC_MC_VFS_CPIOFS
	AC_MC_VFS_TARFS
	AC_MC_VFS_FTP
	AC_MC_VFS_FISH
	AC_MC_VFS_EXTFS
	AC_MC_VFS_SFS
	AC_MC_VFS_UNDELFS
	AC_MC_VFS_MCFS
	AC_MC_VFS_SAMBA

	MC_WITH_VFS

    else
	vfs_type="Plain OS filesystem"
	AM_CONDITIONAL(ENABLE_VFS_CPIO, [false])
	AM_CONDITIONAL(ENABLE_VFS_TAR, [false])
	AM_CONDITIONAL(ENABLE_VFS_FTP, [false])
	AM_CONDITIONAL(ENABLE_VFS_FISH, [false])
	AM_CONDITIONAL(ENABLE_VFS_EXTFS, [false])
	AM_CONDITIONAL(ENABLE_VFS_SFS, [false])
	AM_CONDITIONAL(ENABLE_VFS_UNDELFS, [false])
    fi

    AM_CONDITIONAL(ENABLE_VFS, [test x"$enable_vfs" = x"yes"])
])
