dnl MC_UNDELFS_CHECKS
dnl    Check for ext2fs undel support.
dnl    Set shell variable ext2fs_undel to "yes" if we have it,
dnl    "no" otherwise.  May define USE_EXT2FSLIB for cpp.
dnl    Will set EXT2FS_UNDEL_LIBS to required libraries.

AC_DEFUN([MC_UNDELFS_CHECKS], [
  ext2fs_undel=no
  EXT2FS_UNDEL_LIBS=
  AC_CHECK_HEADERS(linux/ext2_fs.h)
  if test x$ac_cv_header_linux_ext2_fs_h = xyes
  then
    AC_CHECK_HEADERS(ext2fs/ext2fs.h, , , [#include <stdio.h>
#include <linux/ext2_fs.h>])
    if test x$ac_cv_header_ext2fs_ext2fs_h = xyes
    then
      AC_DEFINE(USE_EXT2FSLIB, 1,
		[Define to enable undelete support on ext2])
      ext2fs_undel=yes
      EXT2FS_UNDEL_LIBS="-lext2fs -lcom_err"
      AC_CHECK_TYPE(ext2_ino_t, ,
		    [AC_DEFINE(ext2_ino_t, ino_t,
			       [Define to ino_t if undefined.])],
		    [#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
/* asm/types.h defines its own umode_t :-( */
#undef umode_t
#include <linux/ext2_fs.h>
#include <ext2fs/ext2fs.h>])
    fi
  fi
])



dnl MC_EXTFS_CHECKS
dnl    Check for tools used in extfs scripts.
AC_DEFUN([MC_EXTFS_CHECKS], [
    AC_PATH_PROG([ZIP], [zip], [/usr/bin/zip])
    AC_PATH_PROG([UNZIP], [unzip], [/usr/bin/unzip])
    AC_CACHE_CHECK([for zipinfo code in unzip], [mc_cv_have_zipinfo],
	[mc_cv_have_zipinfo=no
	if $UNZIP -Z </dev/null >/dev/null 2>&1; then
	    mc_cv_have_zipinfo=yes
	fi])
    if test "x$mc_cv_have_zipinfo" = xyes; then
	HAVE_ZIPINFO=1
    else
	HAVE_ZIPINFO=0
    fi
    AC_SUBST([HAVE_ZIPINFO])
    AC_PATH_PROG([PERL], [perl], [/usr/bin/perl])
])



dnl MC_MCSERVER_CHECKS
dnl    Check how mcserver should check passwords.
dnl    Possible methods are PAM, pwdauth and crypt.
dnl    The later works with both /etc/shadow and /etc/passwd.
dnl    If PAM is found, other methods are not checked.

AC_DEFUN([MC_MCSERVER_CHECKS], [

    dnl Check if PAM can be used for mcserv
    AC_CHECK_LIB(dl, dlopen, [LIB_DL="-ldl"])
    AC_CHECK_LIB(pam, pam_start, [
	AC_DEFINE(HAVE_PAM, 1,
		  [Define if PAM (Pluggable Authentication Modules) is available])
	MCSERVLIBS="-lpam $LIB_DL"
	mcserv_pam=yes], [], [$LIB_DL])

    dnl Check for crypt() - needed for both /etc/shadow and /etc/passwd.
    if test -z "$mcserv_pam"; then

	dnl Check for pwdauth() - used on SunOS.
	AC_CHECK_FUNCS([pwdauth])

	dnl Check for crypt()
	AC_CHECK_HEADERS([crypt.h], [crypt_header=yes])
	if test -n "$crypt_header"; then
	    save_LIBS="$LIBS"
	    LIBS=
	    AC_SEARCH_LIBS(crypt, [crypt crypt_i], [mcserv_auth=crypt])
	    MCSERVLIBS="$LIBS"
	    LIBS="$save_LIBS"
	    if test -n "$mcserv_auth"; then
		AC_DEFINE(HAVE_CRYPT, 1,
			  [Define to use crypt function in mcserv])

		dnl Check for shadow passwords
		AC_CHECK_HEADERS([shadow.h shadow/shadow.h],
				 [shadow_header=yes; break])
		if test -n "$shadow_header"; then
		    save_LIBS="$LIBS"
		    LIBS="$MCSERVLIBS"
		    AC_SEARCH_LIBS(getspnam, [shadow], [mcserv_auth=shadow])
		    MCSERVLIBS="$LIBS"
		    LIBS="$save_LIBS"
		    if test -n "$mcserv_auth"; then
			AC_DEFINE(HAVE_SHADOW, 1,
				  [Define to use shadow passwords for mcserv])
		    fi
		fi
	    fi
	fi
    fi

    AC_SUBST(MCSERVLIBS)
])



dnl MC_VFS_CHECKS
dnl   Check for various functions needed by libvfs.
dnl   This has various effects:
dnl     Sets MC_VFS_LIBS to libraries required
dnl     Sets vfs_flags to "pretty" list of vfs implementations we include.
dnl     Sets shell variable use_vfs to yes (default, --with-vfs) or
dnl        "no" (--without-vfs).

dnl Private define
AC_DEFUN([MC_WITH_VFS],[
  MC_EXTFS_CHECKS

  dnl FIXME: network checks should probably be in their own macro.
  AC_CHECK_LIB(nsl, t_accept)
  AC_CHECK_LIB(socket, socket)

  have_socket=no
  AC_CHECK_FUNCS(socket, have_socket=yes)
  if test $have_socket = no; then
    # socket is not in the default libraries.  See if it's in some other.
    for lib in bsd socket inet; do
      AC_CHECK_LIB([$lib], [socket], [
	  LIBS="$LIBS -l$lib"
	  have_socket=yes
	  AC_DEFINE(HAVE_SOCKET)
	  break])
    done
  fi

  have_gethostbyname=no
  AC_CHECK_FUNC(gethostbyname, have_gethostbyname=yes)
  if test $have_gethostbyname = no; then
    # gethostbyname is not in the default libraries.  See if it's in some other.
    for lib in bsd socket inet; do
      AC_CHECK_LIB([$lib], [gethostbyname],
		   [LIBS="$LIBS -l$lib"; have_gethostbyname=yes; break])
    done
  fi

  vfs_flags="cpiofs, extfs, tarfs"
  use_net_code=false
  if test $have_socket = yes; then
      AC_STRUCT_LINGER
      AC_CHECK_FUNCS(pmap_set, , [
	 AC_CHECK_LIB(rpc, pmap_set, [
	   LIBS="-lrpc $LIBS"
	  AC_DEFINE(HAVE_PMAP_SET)
	  ])])
      AC_CHECK_FUNCS(pmap_getport pmap_getmaps rresvport)
      dnl add for source routing support setsockopt
      AC_CHECK_HEADERS(rpc/pmap_clnt.h, , , [
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
					    ])
      dnl
      dnl mcfs support
      dnl
      AC_ARG_WITH(mcfs,
	[  --with-mcfs              Support mc-specific networking file system [[no]]],
	[if test "x$withval" != "xno"; then
	    AC_DEFINE(WITH_MCFS, 1, [Define to enable mc-specific networking file system])
	    vfs_flags="$vfs_flags, mcfs"
	    use_mcfs=yes
	    MC_MCSERVER_CHECKS
	fi]
      )
      vfs_flags="$vfs_flags, ftpfs, fish"
      use_net_code=true
  fi

  dnl
  dnl Samba support
  dnl
  use_smbfs=
  AC_ARG_WITH(samba,
  	  [  --with-samba             Support smb virtual file system [[no]]],
	  [if test "x$withval" != "xno"; then
  		  AC_DEFINE(WITH_SMBFS, 1, [Define to enable VFS over SMB])
	          vfs_flags="$vfs_flags, smbfs"
		  use_smbfs=yes
  	  fi
  ])

  if test -n "$use_smbfs"; then
  #################################################
  # set Samba configuration directory location
  configdir="/etc"
  AC_ARG_WITH(configdir,
  [  --with-configdir=DIR     Where the Samba configuration files are [[/etc]]],
  [ case "$withval" in
    yes|no)
    #
    # Just in case anybody does it
    #
	AC_MSG_WARN([--with-configdir called without argument - will use default])
    ;;
    * )
	configdir="$withval"
    ;;
  esac]
  )
  AC_SUBST(configdir)

  AC_ARG_WITH(codepagedir,
    [  --with-codepagedir=DIR   Where the Samba codepage files are],
    [ case "$withval" in
      yes|no)
      #
      # Just in case anybody does it
      #
	AC_MSG_WARN([--with-codepagedir called without argument - will use default])
      ;;
    esac]
  )
  fi

  AC_DEFINE(USE_VFS, 1, [Define to enable VFS support])
  if $use_net_code; then
     AC_DEFINE(USE_NETCODE, 1, [Define to use networked VFS])
  fi
])



AC_DEFUN([MC_VFS_CHECKS],[
	use_vfs=yes
	AC_ARG_WITH(vfs,
		[  --with-vfs               Compile with the VFS code [[yes]]],
		use_vfs=$withval
	)
	case $use_vfs in
		yes) 	MC_WITH_VFS;;
		no) 	use_vfs=no;;
		*)   	use_vfs=no;;
			dnl Should we issue a warning?
	esac
])



dnl
dnl Check for struct linger
dnl
AC_DEFUN([AC_STRUCT_LINGER], [
av_struct_linger=no
AC_MSG_CHECKING([struct linger is available])
AC_TRY_RUN([
#include <sys/types.h>
#include <sys/socket.h>

struct linger li;

int main ()
{
    li.l_onoff = 1;
    li.l_linger = 120;
    return 0;
}
],[
AC_DEFINE(HAVE_STRUCT_LINGER, 1,
	  [Define if `struct linger' is available])
av_struct_linger=yes
],[
av_struct_linger=no
],[
av_struct_linger=no
])
AC_MSG_RESULT([$av_struct_linger])
])


dnl
dnl Filesystem information detection
dnl
dnl To get information about the disk, mount points, etc.
dnl

AC_DEFUN([AC_GET_FS_INFO], [
    AC_CHECK_HEADERS(fcntl.h sys/dustat.h sys/param.h sys/statfs.h sys/fstyp.h)
    AC_CHECK_HEADERS(mnttab.h mntent.h utime.h sys/statvfs.h sys/vfs.h)
    AC_CHECK_HEADERS(sys/filsys.h sys/fs_types.h)
    AC_CHECK_HEADERS(sys/mount.h, , , [
#include <sys/param.h>
#include <sys/stat.h>
				      ])
    AC_CHECK_FUNCS(getmntinfo)

    dnl This configure.in code has been stolen from GNU fileutils-3.12.  Its
    dnl job is to detect a method to get list of mounted filesystems.

    AC_MSG_CHECKING([for d_ino member in directory struct])
    AC_CACHE_VAL(fu_cv_sys_d_ino_in_dirent,
    [AC_TRY_LINK([
#include <sys/types.h>
#ifdef HAVE_DIRENT_H
# include <dirent.h>
#else /* not HAVE_DIRENT_H */
# define dirent direct
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif /* HAVE_SYS_NDIR_H */
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif /* HAVE_SYS_DIR_H */
# ifdef HAVE_NDIR_H
#  include <ndir.h>
# endif /* HAVE_NDIR_H */
#endif /* HAVE_DIRENT_H */
    ],
      [struct dirent dp; dp.d_ino = 0;],
	fu_cv_sys_d_ino_in_dirent=yes,
	fu_cv_sys_d_ino_in_dirent=no)])
    AC_MSG_RESULT([$fu_cv_sys_d_ino_in_dirent])
    if test $fu_cv_sys_d_ino_in_dirent = yes; then
      AC_DEFINE(D_INO_IN_DIRENT, 1,
		[Define if `d_ino' is member of `struct directory'])
    fi

    # Determine how to get the list of mounted filesystems.
    list_mounted_fs=

    # If the getmntent function is available but not in the standard library,
    # make sure LIBS contains -lsun (on Irix4) or -lseq (on PTX).
    AC_FUNC_GETMNTENT

    if test $ac_cv_func_getmntent = yes; then

      # This system has the getmntent function.
      # Determine whether it's the one-argument variant or the two-argument one.

      if test -z "$list_mounted_fs"; then
	# SVR4
	AC_MSG_CHECKING([for two-argument getmntent function])
	AC_CACHE_VAL(fu_cv_sys_mounted_getmntent2,
	[AC_EGREP_HEADER(getmntent, sys/mnttab.h,
	  fu_cv_sys_mounted_getmntent2=yes,
	  fu_cv_sys_mounted_getmntent2=no)])
	AC_MSG_RESULT([$fu_cv_sys_mounted_getmntent2])
	if test $fu_cv_sys_mounted_getmntent2 = yes; then
	  list_mounted_fs=found
	  AC_DEFINE(MOUNTED_GETMNTENT2, 1,
		    [Define if function `getmntent' takes 2 arguments])
	fi
      fi

      if test -z "$list_mounted_fs"; then
	# 4.3BSD, SunOS, HP-UX, Dynix, Irix
	AC_MSG_CHECKING([for one-argument getmntent function])
	AC_CACHE_VAL(fu_cv_sys_mounted_getmntent1,
		     [test $ac_cv_header_mntent_h = yes \
		       && fu_cv_sys_mounted_getmntent1=yes \
		       || fu_cv_sys_mounted_getmntent1=no])
	AC_MSG_RESULT([$fu_cv_sys_mounted_getmntent1])
	if test $fu_cv_sys_mounted_getmntent1 = yes; then
	  list_mounted_fs=found
	  AC_DEFINE(MOUNTED_GETMNTENT1, 1,
		    [Define if function `getmntent' takes 1 argument])
	fi
      fi

    fi

    if test -z "$list_mounted_fs"; then
      # DEC Alpha running OSF/1.
      AC_MSG_CHECKING([for getfsstat function])
      AC_CACHE_VAL(fu_cv_sys_mounted_getsstat,
      [AC_TRY_LINK([
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/fs_types.h>],
      [struct statfs *stats;
      numsys = getfsstat ((struct statfs *)0, 0L, MNT_WAIT); ],
	fu_cv_sys_mounted_getsstat=yes,
	fu_cv_sys_mounted_getsstat=no)])
      AC_MSG_RESULT([$fu_cv_sys_mounted_getsstat])
      if test $fu_cv_sys_mounted_getsstat = yes; then
	list_mounted_fs=found
	AC_DEFINE(MOUNTED_GETFSSTAT, 1,
		  [Define if function `getfsstat' can be used])
      fi
    fi

    if test -z "$list_mounted_fs"; then
      # AIX.
      AC_MSG_CHECKING([for mntctl function and struct vmount])
      AC_CACHE_VAL(fu_cv_sys_mounted_vmount,
      [AC_TRY_CPP([#include <fshelp.h>],
	fu_cv_sys_mounted_vmount=yes,
	fu_cv_sys_mounted_vmount=no)])
      AC_MSG_RESULT([$fu_cv_sys_mounted_vmount])
      if test $fu_cv_sys_mounted_vmount = yes; then
	list_mounted_fs=found
	AC_DEFINE(MOUNTED_VMOUNT, 1,
		  [Define if function `mntctl' and `struct vmount' can be used])
      fi
    fi

    if test -z "$list_mounted_fs"; then
      # SVR3
      AC_MSG_CHECKING([for existence of three headers])
      AC_CACHE_VAL(fu_cv_sys_mounted_fread_fstyp,
	[AC_TRY_CPP([
#include <sys/statfs.h>
#include <sys/fstyp.h>
#include <mnttab.h>],
		    fu_cv_sys_mounted_fread_fstyp=yes,
		    fu_cv_sys_mounted_fread_fstyp=no)])
      AC_MSG_RESULT([$fu_cv_sys_mounted_fread_fstyp])
      if test $fu_cv_sys_mounted_fread_fstyp = yes; then
	list_mounted_fs=found
	AC_DEFINE(MOUNTED_FREAD_FSTYP, 1,
		  [Define if sys/statfs.h, sys/fstyp.h and mnttab.h
can be used together])
      fi
    fi

    if test -z "$list_mounted_fs"; then
      # 4.4BSD and DEC OSF/1.
      AC_MSG_CHECKING([for getmntinfo function])
      AC_CACHE_VAL(fu_cv_sys_mounted_getmntinfo,
	[
	  ok=
	  if test $ac_cv_func_getmntinfo = yes; then
	    AC_EGREP_HEADER(f_type;, sys/mount.h,
			    ok=yes)
	  fi
	  test -n "$ok" \
	      && fu_cv_sys_mounted_getmntinfo=yes \
	      || fu_cv_sys_mounted_getmntinfo=no
	])
      AC_MSG_RESULT([$fu_cv_sys_mounted_getmntinfo])
      if test $fu_cv_sys_mounted_getmntinfo = yes; then
	list_mounted_fs=found
	AC_DEFINE(MOUNTED_GETMNTINFO, 1,
		  [Define if `getmntinfo' function can be used])
	AC_MSG_CHECKING([if struct statfs has f_fstypename])
	AC_CACHE_VAL(fu_cv_sys_mounted_f_fstypename,
	  [AC_EGREP_HEADER([f_fstypename],
			   [sys/mount.h],
			   [fu_cv_sys_mounted_f_fstypename=yes],
			   [fu_cv_sys_mounted_f_fstypename=no])
	  ])
	AC_MSG_RESULT([$fu_cv_sys_mounted_f_fstypename])
        if test $fu_cv_sys_mounted_f_fstypename = yes; then
	  AC_DEFINE(HAVE_F_FSTYPENAME, 1,
		    [Define if `f_fstypename' is member of `struct statfs'])
	fi
      fi
    fi

    if test -z "$list_mounted_fs"; then
      # Ultrix
      AC_MSG_CHECKING([for getmnt function])
      AC_CACHE_VAL(fu_cv_sys_mounted_getmnt,
	[AC_TRY_CPP([
#include <sys/fs_types.h>
#include <sys/mount.h>],
		    fu_cv_sys_mounted_getmnt=yes,
		    fu_cv_sys_mounted_getmnt=no)])
      AC_MSG_RESULT([$fu_cv_sys_mounted_getmnt])
      if test $fu_cv_sys_mounted_getmnt = yes; then
	list_mounted_fs=found
	AC_DEFINE(MOUNTED_GETMNT, 1,
		  [Define if `getmnt' function can be used])
      fi
    fi

    if test -z "$list_mounted_fs"; then
      # SVR2
    AC_MSG_CHECKING([whether it is possible to resort to fread on /etc/mnttab])
      AC_CACHE_VAL(fu_cv_sys_mounted_fread,
	[AC_TRY_CPP([#include <mnttab.h>],
		    fu_cv_sys_mounted_fread=yes,
		    fu_cv_sys_mounted_fread=no)])
      AC_MSG_RESULT([$fu_cv_sys_mounted_fread])
      if test $fu_cv_sys_mounted_fread = yes; then
	list_mounted_fs=found
	AC_DEFINE(MOUNTED_FREAD, 1,
		 [Define if it's possible to resort to fread on /etc/mnttab])
      fi
    fi

    if test -z "$list_mounted_fs"; then
      AC_MSG_WARN([could not determine how to read list of mounted fs])
    else
      AC_DEFINE(HAVE_INFOMOUNT_LIST, 1,
		[Define if the list of mounted filesystems can be determined])
    fi

dnl This configure.in code has been stolen from GNU fileutils-3.12.  Its
dnl job is to detect a method to get file system information.

    AC_MSG_NOTICE([checking how to get filesystem space usage])
    space=no

    # Here we'll compromise a little (and perform only the link test)
    # since it seems there are no variants of the statvfs function.
    if test $space = no; then
      # SVR4
      AC_CHECK_FUNCS(statvfs)
      if test $ac_cv_func_statvfs = yes; then
	space=yes
	AC_DEFINE(STAT_STATVFS, 1,
		  [Define if function `statfs' can be used])
      fi
    fi

    if test $space = no; then
      # DEC Alpha running OSF/1
      AC_MSG_CHECKING([for 3-argument statfs function (DEC OSF/1)])
      AC_CACHE_VAL(fu_cv_sys_stat_statfs3_osf1,
      [AC_TRY_RUN([
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mount.h>
      main ()
      {
	struct statfs fsd;
	fsd.f_fsize = 0;
	exit (statfs (".", &fsd, sizeof (struct statfs)));
      }],
      fu_cv_sys_stat_statfs3_osf1=yes,
      fu_cv_sys_stat_statfs3_osf1=no,
      fu_cv_sys_stat_statfs3_osf1=no)])
      AC_MSG_RESULT($fu_cv_sys_stat_statfs3_osf1)
      if test $fu_cv_sys_stat_statfs3_osf1 = yes; then
	space=yes
	AC_DEFINE(STAT_STATFS3_OSF1, 1,
		  [Define if function `statfs' takes 3 arguments])
      fi
    fi

    if test $space = no; then
    # AIX
      AC_MSG_CHECKING([for two-argument statfs with statfs.bsize member (AIX, 4.3BSD)])
      AC_CACHE_VAL(fu_cv_sys_stat_statfs2_bsize,
      [AC_TRY_RUN([
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
      main ()
      {
      struct statfs fsd;
      fsd.f_bsize = 0;
      exit (statfs (".", &fsd));
      }],
      fu_cv_sys_stat_statfs2_bsize=yes,
      fu_cv_sys_stat_statfs2_bsize=no,
      fu_cv_sys_stat_statfs2_bsize=no)])
      AC_MSG_RESULT([$fu_cv_sys_stat_statfs2_bsize])
      if test $fu_cv_sys_stat_statfs2_bsize = yes; then
	space=yes
	AC_DEFINE(STAT_STATFS2_BSIZE, 1,
		  [Define if function `statfs' takes two arguments and
`bsize' is member of `struct statfs'])
      fi
    fi

    if test $space = no; then
    # SVR3
      AC_MSG_CHECKING([for four-argument statfs (AIX-3.2.5, SVR3)])
      AC_CACHE_VAL(fu_cv_sys_stat_statfs4,
      [AC_TRY_RUN([#include <sys/types.h>
#include <sys/statfs.h>
      main ()
      {
      struct statfs fsd;
      exit (statfs (".", &fsd, sizeof fsd, 0));
      }],
	fu_cv_sys_stat_statfs4=yes,
	fu_cv_sys_stat_statfs4=no,
	fu_cv_sys_stat_statfs4=no)])
      AC_MSG_RESULT([$fu_cv_sys_stat_statfs4])
      if test $fu_cv_sys_stat_statfs4 = yes; then
	space=yes
	AC_DEFINE(STAT_STATFS4, 1,
		  [Define if function `statfs' takes 4 arguments])
      fi
    fi

    if test $space = no; then
    # 4.4BSD and NetBSD
      AC_MSG_CHECKING([for two-argument statfs with statfs.fsize dnl
    member (4.4BSD and NetBSD)])
      AC_CACHE_VAL(fu_cv_sys_stat_statfs2_fsize,
      [AC_TRY_RUN([#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
      main ()
      {
      struct statfs fsd;
      fsd.f_fsize = 0;
      exit (statfs (".", &fsd));
      }],
      fu_cv_sys_stat_statfs2_fsize=yes,
      fu_cv_sys_stat_statfs2_fsize=no,
      fu_cv_sys_stat_statfs2_fsize=no)])
      AC_MSG_RESULT([$fu_cv_sys_stat_statfs2_fsize])
      if test $fu_cv_sys_stat_statfs2_fsize = yes; then
	space=yes
	AC_DEFINE(STAT_STATFS2_FSIZE, 1,
		  [Define if function `statfs' takes two arguments and
`fsize' is member of `struct statfs'])
      fi
    fi

    if test $space = no; then
      # Ultrix
      AC_MSG_CHECKING([for two-argument statfs with struct fs_data (Ultrix)])
      AC_CACHE_VAL(fu_cv_sys_stat_fs_data,
      [AC_TRY_RUN([
#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_FS_TYPES_H
#include <sys/fs_types.h>
#endif
      main ()
      {
      struct fs_data fsd;
      /* Ultrix's statfs returns 1 for success,
	 0 for not mounted, -1 for failure.  */
      exit (statfs (".", &fsd) != 1);
      }],
      fu_cv_sys_stat_fs_data=yes,
      fu_cv_sys_stat_fs_data=no,
      fu_cv_sys_stat_fs_data=no)])
      AC_MSG_RESULT([$fu_cv_sys_stat_fs_data])
      if test $fu_cv_sys_stat_fs_data = yes; then
	space=yes
	AC_DEFINE(STAT_STATFS2_FS_DATA, 1,
		  [Define if function `statfs' takes two arguments
and uses `struct fs_data'])
      fi
    fi

    dnl Not supported
    dnl if test $space = no; then
    dnl # SVR2
    dnl AC_TRY_CPP([#include <sys/filsys.h>],
    dnl   AC_DEFINE(STAT_READ_FILSYS) space=yes)
    dnl fi
])


dnl
dnl Try using termcap database and link with libtermcap if possible.
dnl
AC_DEFUN([MC_USE_TERMCAP], [
	screen_msg="$screen_msg with termcap database"
	AC_MSG_NOTICE([using S-Lang screen library with termcap])
	AC_DEFINE(USE_TERMCAP, 1, [Define to use termcap database])
	AC_CHECK_LIB(termcap, tgoto, [MCLIBS="$MCLIBS -ltermcap"], , [$LIBS])
])


dnl
dnl Common code for MC_WITH_SLANG and MC_WITH_MCSLANG
dnl
AC_DEFUN([_MC_WITH_XSLANG], [
    screen_type=slang
    AC_DEFINE(HAVE_SLANG, 1,
	      [Define to use S-Lang library for screen management])
])


dnl
dnl Check if the system S-Lang library can be used.
dnl If not, and $1 is "strict", exit, otherwise fall back to mcslang.
dnl
AC_DEFUN([MC_WITH_SLANG], [
    with_screen=slang

    dnl Unless external S-Lang was requested, reject S-Lang with UTF-8 hacks
    m4_if([$1], strict, ,
	  [AC_CHECK_LIB([slang], [SLsmg_write_nwchars],
			[AC_MSG_WARN([Rejecting S-Lang with UTF-8 support, \
it doesn't work well])
			with_screen=mcslang])])

    if test x$with_screen = xslang; then
	AC_CHECK_LIB([slang], [SLang_init_tty], [MCLIBS="$MCLIBS -lslang"],
		     [with_screen=mcslang])
    fi

    dnl Check the header
    if test x$with_screen = xslang; then
	slang_h_found=
	AC_CHECK_HEADERS([slang.h slang/slang.h],
			 [slang_h_found=yes; break])

	if test -z "$slang_h_found"; then
	    with_screen=mcslang
	fi
    fi

    dnl Check if the installed S-Lang library uses termcap
    if test x$with_screen = xslang; then
	screen_type=slang
	screen_msg="S-Lang library (installed on the system)"
	AC_DEFINE(HAVE_SYSTEM_SLANG, 1,
		  [Define to use S-Lang library installed on the system])
	ac_save_LIBS="$LIBS"
	LIBS="$LIBS -lslang"
	AC_TRY_LINK([ 
	#ifdef HAVE_SLANG_SLANG_H
	#include <slang/slang.h>
	#else
	#include <slang.h>
	#endif],
	[SLtt_get_terminfo();
	SLtt_tgetflag("");], 
	[LIBS="$ac_save_LIBS"],
	[LIBS="$ac_save_LIBS"; MC_USE_TERMCAP])
        _MC_WITH_XSLANG
    else
	m4_if([$1], strict,
	    [if test $with_screen != slang; then
		AC_MSG_ERROR([S-Lang library not found])
	    fi],
	    [MC_WITH_MCSLANG]
	)
    fi
])


dnl
dnl Use the included S-Lang library.
dnl
AC_DEFUN([MC_WITH_MCSLANG], [
    screen_type=slang
    screen_msg="Included S-Lang library (mcslang)"

    # Search for terminfo database.
    use_terminfo=
    if test x"$with_termcap" != xyes; then
	if test x"$with_termcap" = xno; then
	    use_terminfo=yes
	fi
	if test -n "$TERMINFO" && test -r "$TERMINFO/v/vt100"; then
	    use_terminfo=yes
	fi
	for dir in "/usr/share/terminfo" "/usr/lib/terminfo" \
		   "/usr/share/lib/terminfo" "/etc/terminfo" \
		   "/usr/local/lib/terminfo" "$HOME/.terminfo"; do
	    if test -r "$dir/v/vt100"; then
		use_terminfo=yes
	    fi
	done
    fi

    # If there is no terminfo, use termcap
    if test -z "$use_terminfo"; then
	MC_USE_TERMCAP
    fi

    _MC_WITH_XSLANG
])


dnl
dnl Use the ncurses library.  It can only be requested explicitly,
dnl so just fail if anything goes wrong.
dnl
dnl If ncurses exports the ESCDELAY variable it should be set to 0
dnl or you'll have to press Esc three times to dismiss a dialog box.
dnl
AC_DEFUN([MC_WITH_NCURSES], [
    dnl has_colors() is specific to ncurses, it's not in the old curses
    save_LIBS="$LIBS"
    LIBS=
    AC_SEARCH_LIBS([has_colors], [ncurses curses], [MCLIBS="$MCLIBS $LIBS"],
		   [AC_MSG_ERROR([Cannot find ncurses library])])

    dnl Check the header
    ncurses_h_found=
    AC_CHECK_HEADERS([ncurses/curses.h ncurses.h curses.h],
		     [ncurses_h_found=yes; break])

    if test -z "$ncurses_h_found"; then
	AC_MSG_ERROR([Cannot find ncurses header file])
    fi

    screen_type=ncurses
    screen_msg="ncurses library"
    AC_DEFINE(USE_NCURSES, 1,
	      [Define to use ncurses for screen management])

    AC_CACHE_CHECK([for ESCDELAY variable],
		   [mc_cv_ncurses_escdelay],
		   [AC_TRY_COMPILE([], [
			extern int ESCDELAY;
			int main ()
			{
			  ESCDELAY = 0;
			}
			],
			[mc_cv_ncurses_escdelay=yes],
			[mc_cv_ncurses_escdelay=no])
    ])
    if test "$mc_cv_ncurses_escdelay" = yes; then
	AC_DEFINE(HAVE_ESCDELAY, 1,
		  [Define if ncurses has ESCDELAY variable])
    fi

    AC_CHECK_FUNCS(resizeterm)
    LIBS="$save_LIBS"
])


dnl
dnl Check for ext2fs recovery code
dnl
AC_DEFUN([AC_EXT2_UNDEL], [
  MC_UNDELFS_CHECKS
  if test "$ext2fs_undel" = yes; then
     AC_MSG_NOTICE([using ext2fs file recovery code])
     vfs_flags="${vfs_flags}, undelfs"
     use_undelfs=yes
     MCLIBS="$MCLIBS $EXT2FS_UNDEL_LIBS"
  else
     AC_MSG_NOTICE([not using ext2fs file recovery code])
  fi
])

dnl The next line is for compatibility with gettext 0.10.x
AC_DEFUN([AM_GNU_GETTEXT_VERSION], [])

dnl
dnl Check whether the g_module_* family of functions works
dnl on this system.  We need to know that at the compile time to
dnl decide whether to link with X11.
dnl
AC_DEFUN([AC_G_MODULE_SUPPORTED], [
    AC_CACHE_CHECK([if gmodule functionality is supported], mc_cv_g_module_supported, [
	ac_save_CFLAGS="$CFLAGS"
	ac_save_LIBS="$LIBS"
	CFLAGS="$CFLAGS $GMODULE_CFLAGS"
	LIBS="$GMODULE_LIBS $LIBS"
	AC_TRY_RUN([
#include <gmodule.h>

int main ()
{
    int ret = (g_module_supported () == TRUE) ? 0 : 1;
    return ret;
}
	],
	    [mc_cv_g_module_supported=yes],
	    [mc_cv_g_module_supported=no],
	    [mc_cv_g_module_supported=no]
	)
	CFLAGS="$ac_save_CFLAGS"
	LIBS="$ac_save_LIBS"
    ])

if test "$mc_cv_g_module_supported" = yes; then
    AC_DEFINE(HAVE_GMODULE, 1,
	      [Define if gmodule functionality is supported])
fi
])
