dnl
dnl Check for size of d_name dirent member
dnl
AC_DEFUN([AC_SHORT_D_NAME_LEN], [
AC_MSG_CHECKING([filename fits on dirent.d_name])
AC_CACHE_VAL(ac_cv_dnamesize, [
OCFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -I$srcdir"
AC_TRY_RUN([
#include <src/fs.h>

int main ()
{
   struct dirent ddd;

   if (sizeof (ddd.d_name) < 12)
	return 0;
   else
   	return 1; 
}

],[
    ac_cv_dnamesize="no"
], [
    ac_cv_dnamesize="yes"
], [
# Cannot find out, so assume no
    ac_cv_dnamesize="no"
])
CFLAGS="$OCFLAGS"
])
if test x$ac_cv_dnamesize = xno; then
    AC_DEFINE(NEED_EXTRA_DIRENT_BUFFER)
fi
AC_MSG_RESULT([$ac_cv_dnamesize])
])

dnl
dnl Filesystem information detection
dnl
dnl To get information about the disk, mount points, etc.
dnl

AC_DEFUN([AC_GET_FS_INFO], [
    AC_CHECK_HEADERS(fcntl.h sys/dustat.h sys/param.h sys/statfs.h sys/fstyp.h)
    AC_CHECK_HEADERS(mnttab.h mntent.h utime.h sys/statvfs.h sys/vfs.h)
    AC_CHECK_HEADERS(sys/mount.h sys/filsys.h sys/fs_types.h)
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
      AC_DEFINE(D_INO_IN_DIRENT)
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
	  AC_DEFINE(MOUNTED_GETMNTENT2)
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
	  AC_DEFINE(MOUNTED_GETMNTENT1)
	fi
      fi

      if test -z "$list_mounted_fs"; then
	AC_MSG_WARN([could not determine how to read list of mounted fs])
	CPPFLAGS="$CPPFLAGS -DNO_INFOMOUNT"
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
	AC_DEFINE(MOUNTED_GETFSSTAT)
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
	AC_DEFINE(MOUNTED_VMOUNT)
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
	AC_DEFINE(MOUNTED_FREAD_FSTYP)
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
	AC_DEFINE(MOUNTED_GETMNTINFO)
	AC_MSG_CHECKING([if struct statfs has f_fstypename])
	AC_CACHE_VAL(fu_cv_sys_mounted_f_fstypename,
	  [
	    AC_EGREP_HEADER(f_type;, sys/mount.h, ok=yes, ok=)
	    test -n "$ok" \
		&& fu_cv_sys_mounted_f_fstypename=yes \
		|| fu_cv_sys_mounted_f_fstypename=no
	  ])
	AC_MSG_RESULT([$fu_cv_sys_mounted_f_fstypename])
        if test $fu_cv_sys_mounted_f_fstypename = yes; then
	  AC_DEFINE(HAVE_F_FSTYPENAME)
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
	AC_DEFINE(MOUNTED_GETMNT)
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
	AC_DEFINE(MOUNTED_FREAD)
      fi
    fi

    if test -z "$list_mounted_fs"; then
      AC_MSG_WARN([could not determine how to read list of mounted fs])
      CPPFLAGS="$CPPFLAGS -DNO_INFOMOUNT"
      # FIXME -- no need to abort building the whole package
      # Can't build mountlist.c or anything that needs its functions
    fi

dnl This configure.in code has been stolen from GNU fileutils-3.12.  Its
dnl job is to detect a method to get file system information.

    AC_CHECKING([how to get filesystem space usage])
    space=no

    # Here we'll compromise a little (and perform only the link test)
    # since it seems there are no variants of the statvfs function.
    if test $space = no; then
      # SVR4
      AC_CHECK_FUNCS(statvfs)
      if test $ac_cv_func_statvfs = yes; then
	space=yes
	AC_DEFINE(STAT_STATVFS)
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
	AC_DEFINE(STAT_STATFS3_OSF1)
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
	AC_DEFINE(STAT_STATFS2_BSIZE)
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
	AC_DEFINE(STAT_STATFS4)
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
	AC_DEFINE(STAT_STATFS2_FSIZE)
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
	AC_DEFINE(STAT_STATFS2_FS_DATA)
      fi
    fi

    dnl Not supported
    dnl if test $space = no; then
    dnl # SVR2
    dnl AC_TRY_CPP([#include <sys/filsys.h>],
    dnl   AC_DEFINE(STAT_READ_FILSYS) space=yes)
    dnl fi
])

dnl AC_TRY_WARNINGS(INCLUDES, FUNCTION-BODY,
dnl             ACTION-IF-NO-WARNINGS [, ACTION-IF-WARNINGS-OR-ERROR])
AC_DEFUN([AC_TRY_WARNINGS],
[cat > conftest.$ac_ext <<EOF
dnl This sometimes fails to find confdefs.h, for some reason.
dnl [#]line __oline__ "[$]0"
[#]line __oline__ "configure"
#include "confdefs.h"
[$1]
int main() { return 0; }
int t() {
[$2]
; return 0; }
EOF
ac_compile_warn='${CC-cc} -c $CFLAGS $CPPFLAGS conftest.$ac_ext 2>&1'
if { if eval $ac_compile_warn; then :; else echo arning; fi; } | grep arning 1>&AC_FD_CC 2>&AC_FD_CC; then
  ifelse([$4], , :, [rm -rf conftest*
  $4])
ifelse([$3], , , [else
  rm -rf conftest*
  $3
])dnl
fi
rm -f conftest*]
)

AC_DEFUN([AC_USE_SUNOS_CURSES], [
	search_ncurses=false
	screen_manager="SunOS 4.x /usr/5include curses"
	AC_MSG_RESULT([Using SunOS 4.x /usr/5include curses])
	AC_DEFINE(USE_SUNOS_CURSES)
	AC_DEFINE(NO_COLOR_CURSES)
	AC_DEFINE(USE_SYSV_CURSES)
	CPPFLAGS="$CPPFLAGS -I/usr/5include"
	XCURSES="xcurses.o /usr/5lib/libcurses.a /usr/5lib/libtermcap.a"
	AC_MSG_RESULT([Please note that some screen refreshs may fail])
	AC_MSG_WARN([Reconsider using Slang])
])

AC_DEFUN([AC_USE_OSF1_CURSES], [
       AC_MSG_RESULT([Using OSF1 curses])
       search_ncurses=false
       screen_manager="OSF1 curses"
       AC_DEFINE(NO_COLOR_CURSES)
       AC_DEFINE(USE_SYSV_CURSES)
       XCURSES="xcurses.o"
       LIBS="$LIBS -lcurses"
])

AC_DEFUN([AC_USE_SYSV_CURSES], [
	AC_MSG_RESULT([Using SysV curses])
	AC_DEFINE(USE_SYSV_CURSES)
	XCURSES=""
	search_ncurses=false
	screen_manager="SysV/curses"
	LIBS="$LIBS -lcurses"
])

AC_DEFUN([AC_USE_TERMINFO], [
	AC_DEFINE(SLANG_TERMINFO)
	AC_MSG_RESULT([Using SLang screen manager/terminfo])
	slang_term=" with terminfo"
])

AC_DEFUN([AC_USE_TERMCAP], [
	AC_MSG_RESULT([Using SLang screen manager/termcap])
	AC_DEFINE(USE_TERMCAP)
	dnl Check with $LIBS at the end so that it works with ELF libs.
	AC_CHECK_LIB(termcap, tgoto, LIBS="$LIBS -ltermcap", , $LIBS)
	slang_term=" with termcap"
])
	
AC_DEFUN([AC_WITH_SLANG], [
	AC_DEFINE(HAVE_SLANG)
	search_ncurses=false
	if $slang_use_system_installed_lib
	then
	    AC_DEFINE(HAVE_SYSTEM_SLANG)
	    LSLANG="-lslang"
	    screen_manager="SLang (system-installed library)"
	    AC_MSG_RESULT([Using system installed SLang library])
	    rm -f slang/slang.h
	    ac_save_LIBS="$LIBS"
	    LIBS="$LIBS $LSLANG"
	    AC_TRY_RUN(
	    [ 
	    #ifdef SLANG_H_INSIDE_SLANG_DIR
	    #include <slang/slang.h>
	    #else
	    #include <slang.h>
	    #endif
	    int main(void){
		SLtt_get_terminfo();
		SLtt_tgetflag("");
		return 0;
	    } ], 
	    [LIBS="$ac_save_LIBS"; AC_USE_TERMINFO], 
	    [LIBS="$ac_save_LIBS"; AC_USE_TERMCAP])
	else
	    MCCPPFLAGS="$MCCPPFLAGS -I\$(slangdir)"
    	    LIBSLANG="libmcslang.a"
	    screen_manager="SLang"
	    LSLANG="-lmcslang"
	    CPPFLAGS="$CPPFLAGS -I../slang"
	    fastdepslang=fastdepslang
	    mkdir -p slang
	    rm -f slang/slang.h
	    case "$srcdir" in
		/*) ln -sf  $srcdir/slang/slang-mc.h slang/slang.h;;
		*)  ln -sf  ../$srcdir/slang/slang-mc.h slang/slang.h;;
	    esac
	fi
	if $slang_check_lib
	then
	    use_terminfo=false
	    for dir in 	/usr/lib /usr/share/lib /usr/local/lib /lib \
			/usr/local/share /usr/share
	    do
		if test -d $dir/terminfo; then
		use_terminfo=true; 
		break
		fi
	    done
	    if $use_terminfo; then
		AC_USE_TERMINFO
	    else
		AC_USE_TERMCAP
	    fi
        fi]
)

AC_DEFUN([AC_WITH_EDIT], [
	AC_DEFINE(USE_INTERNAL_EDIT)
	LIBEDIT_A="libedit.a"
	MCEDIT="mcedit"
	LEDIT="-ledit"
	EDIT_msg="yes"
	AC_MSG_RESULT([will call internal editor])
])

AC_DEFUN([AC_EXT2_UNDEL], [
  GNOME_UNDELFS_CHECKS
  if test "$ext2fs_undel" = yes; then
     AC_MSG_RESULT([With ext2fs file recovery code])
     vfs_flags="${vfs_flags}, undelfs"
     undelfs_o="undelfs.o"
     LIBS="$LIBS $EXT2FS_UNDEL_LIBS"
  else
     AC_MSG_WARN([No ext2fs file recovery code])
  fi
])

dnl
dnl Parameters: directory filename LIBS_append CPPFLAGS_append nicename
dnl
AC_DEFUN([AC_NCURSES], [
    if $search_ncurses
    then
        if test -f $1/$2
	then
	    AC_MSG_RESULT([Found ncurses on $1/$2])
 	    LIBS="$LIBS $3"
	    CPPFLAGS="$CPPFLAGS $4"
	    search_ncurses=false
	    screen_manager=$5
	    AC_DEFINE(USE_NCURSES)
	fi
    fi
])
