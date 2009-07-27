#serial 6

# How to list mounted file systems.

# Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2006 Free Software
# Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

dnl From Jim Meyering.
dnl
dnl This is not pretty.  I've just taken the autoconf code and wrapped
dnl it in an AC_DEFUN and made some other fixes.
dnl

# Replace Autoconf's AC_FUNC_GETMNTENT to work around a bug in Autoconf
# through Autoconf 2.59.  We can remove this once we assume Autoconf 2.60
# or later.
AC_DEFUN([AC_FUNC_GETMNTENT],
[# getmntent is in the standard C library on UNICOS, in -lsun on Irix 4,
# -lseq on Dynix/PTX, -lgen on Unixware.
AC_SEARCH_LIBS(getmntent, [sun seq gen])
AC_CHECK_FUNCS(getmntent)
])

# gl_LIST_MOUNTED_FILE_SYSTEMS([ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])
AC_DEFUN([gl_LIST_MOUNTED_FILE_SYSTEMS],
  [
AC_CHECK_FUNCS(listmntent getmntinfo)
AC_CHECK_HEADERS_ONCE(sys/param.h sys/statvfs.h)

# We must include grp.h before ucred.h on OSF V4.0, since ucred.h uses
# NGROUPS (as the array dimension for a struct member) without a definition.
AC_CHECK_HEADERS(sys/ucred.h, [], [], [#include <grp.h>])

AC_CHECK_HEADERS(sys/mount.h, [], [],
  [AC_INCLUDES_DEFAULT
   [#if HAVE_SYS_PARAM_H
     #include <sys/param.h>
    #endif]])

AC_CHECK_HEADERS(mntent.h sys/fs_types.h)
    getfsstat_includes="\
$ac_includes_default
#if HAVE_SYS_PARAM_H
# include <sys/param.h> /* needed by powerpc-apple-darwin1.3.7 */
#endif
#if HAVE_SYS_UCRED_H
# include <grp.h> /* needed for definition of NGROUPS */
# include <sys/ucred.h> /* needed by powerpc-apple-darwin1.3.7 */
#endif
#if HAVE_SYS_MOUNT_H
# include <sys/mount.h>
#endif
#if HAVE_SYS_FS_TYPES_H
# include <sys/fs_types.h> /* needed by powerpc-apple-darwin1.3.7 */
#endif
"
AC_CHECK_MEMBERS([struct fsstat.f_fstypename],,,[$getfsstat_includes])

# Determine how to get the list of mounted file systems.
ac_list_mounted_fs=

# If the getmntent function is available but not in the standard library,
# make sure LIBS contains the appropriate -l option.
AC_FUNC_GETMNTENT

# This test must precede the ones for getmntent because Unicos-9 is
# reported to have the getmntent function, but its support is incompatible
# with other getmntent implementations.

# NOTE: Normally, I wouldn't use a check for system type as I've done for
# `CRAY' below since that goes against the whole autoconf philosophy.  But
# I think there is too great a chance that some non-Cray system has a
# function named listmntent to risk the false positive.

if test x"$ac_list_mounted_fs" = x; then
  # Cray UNICOS 9
  AC_MSG_CHECKING([for listmntent of Cray/Unicos-9])
  AC_CACHE_VAL(fu_cv_sys_mounted_cray_listmntent,
    [fu_cv_sys_mounted_cray_listmntent=no
      AC_EGREP_CPP(yes,
        [#ifdef _CRAY
yes
#endif
        ], [test x"$ac_cv_func_listmntent" = xyes \
	    && fu_cv_sys_mounted_cray_listmntent=yes]
      )
    ]
  )
  AC_MSG_RESULT($fu_cv_sys_mounted_cray_listmntent)
  if test x"$fu_cv_sys_mounted_cray_listmntent" = xyes; then
    ac_list_mounted_fs=found
    AC_DEFINE(MOUNTED_LISTMNTENT, 1,
      [Define if there is a function named listmntent that can be used to
       list all mounted file systems.  (UNICOS)])
  fi
fi

if test x"$ac_list_mounted_fs" = x; then
  # AIX.
  AC_MSG_CHECKING([for mntctl function and struct vmount])
  AC_CACHE_VAL(fu_cv_sys_mounted_vmount,
  [AC_TRY_CPP([#include <fshelp.h>],
    fu_cv_sys_mounted_vmount=yes,
    fu_cv_sys_mounted_vmount=no)])
  AC_MSG_RESULT($fu_cv_sys_mounted_vmount)
  if test x"$fu_cv_sys_mounted_vmount" = xyes; then
    ac_list_mounted_fs=found
    AC_DEFINE(MOUNTED_VMOUNT, 1,
	[Define if there is a function named mntctl that can be used to read
         the list of mounted file systems, and there is a system header file
         that declares `struct vmount.'  (AIX)])
  fi
fi

if test x"$ac_cv_func_getmntent" = xyes; then

  # This system has the getmntent function.
  # Determine whether it's the one-argument variant or the two-argument one.

  if test x"$ac_list_mounted_fs" = x; then
    # 4.3BSD, SunOS, HP-UX, Dynix, Irix
    AC_MSG_CHECKING([for one-argument getmntent function])
    AC_CACHE_VAL(fu_cv_sys_mounted_getmntent1,
		 [AC_TRY_COMPILE([
/* SunOS 4.1.x /usr/include/mntent.h needs this for FILE */
#include <stdio.h>

#include <mntent.h>
#if !defined MOUNTED
# if defined _PATH_MOUNTED	/* GNU libc  */
#  define MOUNTED _PATH_MOUNTED
# endif
# if defined MNT_MNTTAB	/* HP-UX.  */
#  define MOUNTED MNT_MNTTAB
# endif
# if defined MNTTABNAME	/* Dynix.  */
#  define MOUNTED MNTTABNAME
# endif
#endif
],
                    [ struct mntent *mnt = 0; char *table = MOUNTED;
		      if (sizeof mnt && sizeof table) return 0;],
		    fu_cv_sys_mounted_getmntent1=yes,
		    fu_cv_sys_mounted_getmntent1=no)])
    AC_MSG_RESULT($fu_cv_sys_mounted_getmntent1)
    if test x"$fu_cv_sys_mounted_getmntent1" = xyes; then
      ac_list_mounted_fs=found
      AC_DEFINE(MOUNTED_GETMNTENT1, 1,
        [Define if there is a function named getmntent for reading the list
         of mounted file systems, and that function takes a single argument.
         (4.3BSD, SunOS, HP-UX, Dynix, Irix)])
    fi
  fi

  if test x"$ac_list_mounted_fs" = x; then
    # SVR4
    AC_MSG_CHECKING([for two-argument getmntent function])
    AC_CACHE_VAL(fu_cv_sys_mounted_getmntent2,
    [AC_EGREP_HEADER(getmntent, sys/mnttab.h,
      fu_cv_sys_mounted_getmntent2=yes,
      fu_cv_sys_mounted_getmntent2=no)])
    AC_MSG_RESULT($fu_cv_sys_mounted_getmntent2)
    if test x"$fu_cv_sys_mounted_getmntent2" = xyes; then
      ac_list_mounted_fs=found
      AC_DEFINE(MOUNTED_GETMNTENT2, 1,
        [Define if there is a function named getmntent for reading the list of
         mounted file systems, and that function takes two arguments.  (SVR4)])
      AC_CHECK_FUNCS(hasmntopt)
    fi
  fi

fi

if test x"$ac_list_mounted_fs" = x; then
  # DEC Alpha running OSF/1, and Apple Darwin 1.3.
  # powerpc-apple-darwin1.3.7 needs sys/param.h sys/ucred.h sys/fs_types.h

  AC_MSG_CHECKING([for getfsstat function])
  AC_CACHE_VAL(fu_cv_sys_mounted_getfsstat,
  [AC_TRY_LINK([
#include <sys/types.h>
#if HAVE_STRUCT_FSSTAT_F_FSTYPENAME
# define FS_TYPE(Ent) ((Ent).f_fstypename)
#else
# define FS_TYPE(Ent) mnt_names[(Ent).f_type]
#endif
]$getfsstat_includes
,
  [struct statfs *stats;
   int numsys = getfsstat ((struct statfs *)0, 0L, MNT_WAIT);
   char *t = FS_TYPE (*stats); ],
    fu_cv_sys_mounted_getfsstat=yes,
    fu_cv_sys_mounted_getfsstat=no)])
  AC_MSG_RESULT($fu_cv_sys_mounted_getfsstat)
  if test x"$fu_cv_sys_mounted_getfsstat" = xyes; then
    ac_list_mounted_fs=found
    AC_DEFINE(MOUNTED_GETFSSTAT, 1,
	      [Define if there is a function named getfsstat for reading the
               list of mounted file systems.  (DEC Alpha running OSF/1)])
  fi
fi

if test x"$ac_list_mounted_fs" = x; then
  # SVR3
  AC_MSG_CHECKING([for FIXME existence of three headers])
  AC_CACHE_VAL(fu_cv_sys_mounted_fread_fstyp,
    [AC_TRY_CPP([
#include <sys/statfs.h>
#include <sys/fstyp.h>
#include <mnttab.h>],
		fu_cv_sys_mounted_fread_fstyp=yes,
		fu_cv_sys_mounted_fread_fstyp=no)])
  AC_MSG_RESULT($fu_cv_sys_mounted_fread_fstyp)
  if test x"$fu_cv_sys_mounted_fread_fstyp" = xyes; then
    ac_list_mounted_fs=found
    AC_DEFINE(MOUNTED_FREAD_FSTYP, 1,
      [Define if (like SVR2) there is no specific function for reading the
       list of mounted file systems, and your system has these header files:
       <sys/fstyp.h> and <sys/statfs.h>.  (SVR3)])
  fi
fi

if test x"$ac_list_mounted_fs" = x; then
  # 4.4BSD and DEC OSF/1.
  AC_MSG_CHECKING([for getmntinfo function])
  AC_CACHE_VAL(fu_cv_sys_mounted_getmntinfo,
    [
      test x"$ac_cv_func_getmntinfo" = xyes \
	  && fu_cv_sys_mounted_getmntinfo=yes \
	  || fu_cv_sys_mounted_getmntinfo=no
    ])
  AC_MSG_RESULT($fu_cv_sys_mounted_getmntinfo)
  if test x"$fu_cv_sys_mounted_getmntinfo" = xyes; then
    AC_MSG_CHECKING([whether getmntinfo returns statvfs structures])
    AC_CACHE_VAL(fu_cv_sys_mounted_getmntinfo2,
      [
        AC_TRY_COMPILE([
#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#include <sys/types.h>
#if HAVE_SYS_MOUNT_H
# include <sys/mount.h>
#endif
#if HAVE_SYS_STATVFS_H
# include <sys/statvfs.h>
#endif
extern int getmntinfo (struct statfs **, int);
          ], [],
          [fu_cv_sys_mounted_getmntinfo2=no],
          [fu_cv_sys_mounted_getmntinfo2=yes])
      ])
    AC_MSG_RESULT([$fu_cv_sys_mounted_getmntinfo2])
    if test x"$fu_cv_sys_mounted_getmntinfo2" = xno; then
      ac_list_mounted_fs=found
      AC_DEFINE(MOUNTED_GETMNTINFO, 1,
	        [Define if there is a function named getmntinfo for reading the
                 list of mounted file systems and it returns an array of
                 'struct statfs'.  (4.4BSD, Darwin)])
    else
      ac_list_mounted_fs=found
      AC_DEFINE(MOUNTED_GETMNTINFO2, 1,
	        [Define if there is a function named getmntinfo for reading the
                 list of mounted file systems and it returns an array of
                 'struct statvfs'.  (NetBSD 3.0)])
    fi
  fi
fi

if test x"$ac_list_mounted_fs" = x; then
  # Ultrix
  AC_MSG_CHECKING([for getmnt function])
  AC_CACHE_VAL(fu_cv_sys_mounted_getmnt,
    [AC_TRY_CPP([
#include <sys/fs_types.h>
#include <sys/mount.h>],
		fu_cv_sys_mounted_getmnt=yes,
		fu_cv_sys_mounted_getmnt=no)])
  AC_MSG_RESULT($fu_cv_sys_mounted_getmnt)
  if test x"$fu_cv_sys_mounted_getmnt" = xyes; then
    ac_list_mounted_fs=found
    AC_DEFINE(MOUNTED_GETMNT, 1,
      [Define if there is a function named getmnt for reading the list of
       mounted file systems.  (Ultrix)])
  fi
fi

if test x"$ac_list_mounted_fs" = x; then
  # BeOS
  AC_CHECK_FUNCS(next_dev fs_stat_dev)
  AC_CHECK_HEADERS(fs_info.h)
  AC_MSG_CHECKING([for BEOS mounted file system support functions])
  if test x"$ac_cv_header_fs_info_h" = xyes \
      && test x"$ac_cv_func_next_dev" = xyes \
	&& test x"$ac_cv_func_fs_stat_dev" = xyes; then
    fu_result=yes
  else
    fu_result=no
  fi
  AC_MSG_RESULT($fu_result)
  if test x"$fu_result" = xyes; then
    ac_list_mounted_fs=found
    AC_DEFINE(MOUNTED_FS_STAT_DEV, 1,
      [Define if there are functions named next_dev and fs_stat_dev for
       reading the list of mounted file systems.  (BeOS)])
  fi
fi

if test x"$ac_list_mounted_fs" = x; then
  # SVR2
  AC_MSG_CHECKING([whether it is possible to resort to fread on /etc/mnttab])
  AC_CACHE_VAL(fu_cv_sys_mounted_fread,
    [AC_TRY_CPP([#include <mnttab.h>],
		fu_cv_sys_mounted_fread=yes,
		fu_cv_sys_mounted_fread=no)])
  AC_MSG_RESULT($fu_cv_sys_mounted_fread)
  if test x"$fu_cv_sys_mounted_fread" = xyes; then
    ac_list_mounted_fs=found
    AC_DEFINE(MOUNTED_FREAD, 1,
	      [Define if there is no specific function for reading the list of
               mounted file systems.  fread will be used to read /etc/mnttab.
               (SVR2) ])
  fi
fi

if test x"$ac_list_mounted_fs" = x; then
  AC_MSG_ERROR([could not determine how to read list of mounted file systems])
  # FIXME -- no need to abort building the whole package
  # Can't build mountlist.c or anything that needs its functions
fi

AS_IF([test x"$ac_list_mounted_fs" = xfound], [$1], [$2])

  ])

# Obtaining file system usage information.

# Copyright (C) 1997, 1998, 2000, 2001, 2003-2007 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# Written by Jim Meyering.

AC_DEFUN([gl_FSUSAGE],
[
  AC_LIBSOURCES([fsusage.c, fsusage.h])

  AC_CHECK_HEADERS_ONCE(sys/param.h)
  AC_CHECK_HEADERS_ONCE(sys/vfs.h sys/fs_types.h)
  AC_CHECK_HEADERS(sys/mount.h, [], [],
    [AC_INCLUDES_DEFAULT
     [#if HAVE_SYS_PARAM_H
       #include <sys/param.h>
      #endif]])
  gl_FILE_SYSTEM_USAGE([gl_cv_fs_space=yes], [gl_cv_fs_space=no])
  if test x"$gl_cv_fs_space" = xyes; then
    AC_LIBOBJ(fsusage)
    gl_PREREQ_FSUSAGE_EXTRA
  fi
])

# Try to determine how a program can obtain file system usage information.
# If successful, define the appropriate symbol (see fsusage.c) and
# execute ACTION-IF-FOUND.  Otherwise, execute ACTION-IF-NOT-FOUND.
#
# gl_FILE_SYSTEM_USAGE([ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])

AC_DEFUN([gl_FILE_SYSTEM_USAGE],
[

AC_MSG_NOTICE([checking how to get file system space usage])
ac_fsusage_space=no

# Perform only the link test since it seems there are no variants of the
# statvfs function.  This check is more than just AC_CHECK_FUNCS(statvfs)
# because that got a false positive on SCO OSR5.  Adding the declaration
# of a `struct statvfs' causes this test to fail (as it should) on such
# systems.  That system is reported to work fine with STAT_STATFS4 which
# is what it gets when this test fails.
if test x"$ac_fsusage_space" = xno; then
  # SVR4
  AC_CACHE_CHECK([for statvfs function (SVR4)], fu_cv_sys_stat_statvfs,
		 [AC_TRY_LINK([#include <sys/types.h>
#if defined __GLIBC__ && !defined __BEOS__
Do not use statvfs on systems with GNU libc, because that function stats
all preceding entries in /proc/mounts, and that makes df hang if even
one of the corresponding file systems is hard-mounted, but not available.
statvfs in GNU libc on BeOS operates differently: it only makes a system
call.
#endif

#ifdef __osf__
"Do not use Tru64's statvfs implementation"
#endif

#include <sys/statvfs.h>],
			      [struct statvfs fsd; statvfs (0, &fsd);],
			      fu_cv_sys_stat_statvfs=yes,
			      fu_cv_sys_stat_statvfs=no)])
  if test x"$fu_cv_sys_stat_statvfs" = xyes; then
    ac_fsusage_space=yes
    AC_DEFINE(STAT_STATVFS, 1,
	      [  Define if there is a function named statvfs.  (SVR4)])
  fi
fi

if test x"$ac_fsusage_space" = xno; then
  # DEC Alpha running OSF/1
  AC_MSG_CHECKING([for 3-argument statfs function (DEC OSF/1)])
  AC_CACHE_VAL(fu_cv_sys_stat_statfs3_osf1,
  [AC_TRY_RUN([
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mount.h>
  int
  main ()
  {
    struct statfs fsd;
    fsd.f_fsize = 0;
    return statfs (".", &fsd, sizeof (struct statfs)) != 0;
  }],
  fu_cv_sys_stat_statfs3_osf1=yes,
  fu_cv_sys_stat_statfs3_osf1=no,
  fu_cv_sys_stat_statfs3_osf1=no)])
  AC_MSG_RESULT($fu_cv_sys_stat_statfs3_osf1)
  if test x"$fu_cv_sys_stat_statfs3_osf1" = xyes; then
    ac_fsusage_space=yes
    AC_DEFINE(STAT_STATFS3_OSF1, 1,
	      [   Define if  statfs takes 3 args.  (DEC Alpha running OSF/1)])
  fi
fi

if test x"$ac_fsusage_space" = xno; then
# AIX
  AC_MSG_CHECKING([for two-argument statfs with statfs.bsize dnl
member (AIX, 4.3BSD)])
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
  int
  main ()
  {
  struct statfs fsd;
  fsd.f_bsize = 0;
  return statfs (".", &fsd) != 0;
  }],
  fu_cv_sys_stat_statfs2_bsize=yes,
  fu_cv_sys_stat_statfs2_bsize=no,
  fu_cv_sys_stat_statfs2_bsize=no)])
  AC_MSG_RESULT($fu_cv_sys_stat_statfs2_bsize)
  if test x"$fu_cv_sys_stat_statfs2_bsize" = xyes; then
    ac_fsusage_space=yes
    AC_DEFINE(STAT_STATFS2_BSIZE, 1,
[  Define if statfs takes 2 args and struct statfs has a field named f_bsize.
   (4.3BSD, SunOS 4, HP-UX, AIX PS/2)])
  fi
fi

if test x"$ac_fsusage_space" = xno; then
# SVR3
  AC_MSG_CHECKING([for four-argument statfs (AIX-3.2.5, SVR3)])
  AC_CACHE_VAL(fu_cv_sys_stat_statfs4,
  [AC_TRY_RUN([#include <sys/types.h>
#include <sys/statfs.h>
  int
  main ()
  {
  struct statfs fsd;
  return statfs (".", &fsd, sizeof fsd, 0) != 0;
  }],
    fu_cv_sys_stat_statfs4=yes,
    fu_cv_sys_stat_statfs4=no,
    fu_cv_sys_stat_statfs4=no)])
  AC_MSG_RESULT($fu_cv_sys_stat_statfs4)
  if test x"$fu_cv_sys_stat_statfs4" = xyes; then
    ac_fsusage_space=yes
    AC_DEFINE(STAT_STATFS4, 1,
	      [  Define if statfs takes 4 args.  (SVR3, Dynix, Irix, Dolphin)])
  fi
fi

if test x"$ac_fsusage_space" = xno; then
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
  int
  main ()
  {
  struct statfs fsd;
  fsd.f_fsize = 0;
  return statfs (".", &fsd) != 0;
  }],
  fu_cv_sys_stat_statfs2_fsize=yes,
  fu_cv_sys_stat_statfs2_fsize=no,
  fu_cv_sys_stat_statfs2_fsize=no)])
  AC_MSG_RESULT($fu_cv_sys_stat_statfs2_fsize)
  if test x"$fu_cv_sys_stat_statfs2_fsize" = xyes; then
    ac_fsusage_space=yes
    AC_DEFINE(STAT_STATFS2_FSIZE, 1,
[  Define if statfs takes 2 args and struct statfs has a field named f_fsize.
   (4.4BSD, NetBSD)])
  fi
fi

if test x"$ac_fsusage_space" = xno; then
  # Ultrix
  AC_MSG_CHECKING([for two-argument statfs with struct fs_data (Ultrix)])
  AC_CACHE_VAL(fu_cv_sys_stat_fs_data,
  [AC_TRY_RUN([#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_FS_TYPES_H
#include <sys/fs_types.h>
#endif
  int
  main ()
  {
  struct fs_data fsd;
  /* Ultrix's statfs returns 1 for success,
     0 for not mounted, -1 for failure.  */
  return statfs (".", &fsd) != 1;
  }],
  fu_cv_sys_stat_fs_data=yes,
  fu_cv_sys_stat_fs_data=no,
  fu_cv_sys_stat_fs_data=no)])
  AC_MSG_RESULT($fu_cv_sys_stat_fs_data)
  if test x"$fu_cv_sys_stat_fs_data" = xyes; then
    ac_fsusage_space=yes
    AC_DEFINE(STAT_STATFS2_FS_DATA, 1,
[  Define if statfs takes 2 args and the second argument has
   type struct fs_data.  (Ultrix)])
  fi
fi

if test x"$ac_fsusage_space" = xno; then
  # SVR2
  AC_TRY_CPP([#include <sys/filsys.h>
    ],
    AC_DEFINE(STAT_READ_FILSYS, 1,
      [Define if there is no specific function for reading file systems usage
       information and you have the <sys/filsys.h> header file.  (SVR2)])
    ac_fsusage_space=yes)
fi

AS_IF([test x"$ac_fsusage_space" = xyes], [$1], [$2])

])


# Check for SunOS statfs brokenness wrt partitions 2GB and larger.
# If <sys/vfs.h> exists and struct statfs has a member named f_spare,
# enable the work-around code in fsusage.c.
AC_DEFUN([gl_STATFS_TRUNCATES],
[
  AC_MSG_CHECKING([for statfs that truncates block counts])
  AC_CACHE_VAL(fu_cv_sys_truncating_statfs,
  [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#if !defined(sun) && !defined(__sun)
choke -- this is a workaround for a Sun-specific problem
#endif
#include <sys/types.h>
#include <sys/vfs.h>]],
    [[struct statfs t; long c = *(t.f_spare);
      if (c) return 0;]])],
    [fu_cv_sys_truncating_statfs=yes],
    [fu_cv_sys_truncating_statfs=no])])
  if test x"$fu_cv_sys_truncating_statfs" = xyes; then
    AC_DEFINE(STATFS_TRUNCATES_BLOCK_COUNTS, 1,
      [Define if the block counts reported by statfs may be truncated to 2GB
       and the correct values may be stored in the f_spare array.
       (SunOS 4.1.2, 4.1.3, and 4.1.3_U1 are reported to have this problem.
       SunOS 4.1.1 seems not to be affected.)])
  fi
  AC_MSG_RESULT($fu_cv_sys_truncating_statfs)
])


# Prerequisites of lib/fsusage.c not done by gl_FILE_SYSTEM_USAGE.
AC_DEFUN([gl_PREREQ_FSUSAGE_EXTRA],
[
  AC_REQUIRE([gl_AC_TYPE_UINTMAX_T])
  AC_CHECK_HEADERS(dustat.h sys/fs/s5param.h sys/filsys.h sys/statfs.h)
  gl_STATFS_TRUNCATES
])

dnl From Jim Meyering.
dnl
dnl See if struct statfs has the f_fstypename member.
dnl If so, define HAVE_STRUCT_STATFS_F_FSTYPENAME.
dnl

# Copyright (C) 1998, 1999, 2001, 2004, 2006 Free Software Foundation, Inc.
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FSTYPENAME],
[
  AC_CHECK_MEMBERS([struct statfs.f_fstypename],,,
    [
      #include <sys/types.h>
      #include <sys/param.h>
      #include <sys/mount.h>
    ])
])

dnl
dnl Filesystem information detection
dnl
dnl To get information about the disk, mount points, etc.
dnl

AC_DEFUN([AC_GET_FS_INFO], [
    AC_CHECK_HEADERS([fcntl.h utime.h])

    gl_LIST_MOUNTED_FILE_SYSTEMS([
	AC_DEFINE(HAVE_INFOMOUNT_LIST, 1,
	    [Define if the list of mounted filesystems can be determined])],
	[AC_MSG_WARN([could not determine how to read list of mounted fs])])

    gl_FSUSAGE
    gl_FSTYPENAME
])
