
dnl
dnl posix_allocate() function detection
dnl

AC_DEFUN([gl_POSIX_FALLOCATE], [
    dnl * Old glibcs have broken posix_fallocate(). Make sure not to use it.
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
        #define _XOPEN_SOURCE 600
        #include <stdlib.h>
        #if defined(__GLIBC__) && (__GLIBC__ < 2 || __GLIBC_MINOR__ < 7)
            possibly broken posix_fallocate
        #endif
    ]],
    [[posix_fallocate(0, 0, 0);]])],
    [AC_DEFINE(
        [HAVE_POSIX_FALLOCATE],
        [1],
        [Define if you have a working posix_fallocate()])
    ])
])

dnl
dnl Get from the coreutils package (stat-prog.m4 serial 7)
dnl

AC_DEFUN([mc_cu_PREREQ_STAT_PROG],
[
  AC_REQUIRE([gl_FSUSAGE])
  AC_REQUIRE([gl_FSTYPENAME])
  AC_CHECK_HEADERS_ONCE([OS.h netinet/in.h sys/param.h sys/vfs.h])

  dnl Check for vfs.h first, since this avoids a warning with nfs_client.h
  dnl on Solaris 8.
  test $ac_cv_header_sys_param_h = yes &&
  test $ac_cv_header_sys_mount_h = yes &&
  AC_CHECK_HEADERS([nfs/vfs.h],
    [AC_CHECK_HEADERS([nfs/nfs_client.h])])

  statvfs_includes="\
AC_INCLUDES_DEFAULT
#include <sys/statvfs.h>
"
  statfs_includes="\
AC_INCLUDES_DEFAULT
#ifdef HAVE_SYS_VFS_H
# include <sys/vfs.h>
#elif defined HAVE_SYS_MOUNT_H && defined HAVE_SYS_PARAM_H
# include <sys/param.h>
# include <sys/mount.h>
# if defined HAVE_NETINET_IN_H && defined HAVE_NFS_NFS_CLNT_H && defined HAVE_NFS_VFS_H
#  include <netinet/in.h>
#  include <nfs/nfs_clnt.h>
#  include <nfs/vfs.h>
# endif
#elif defined HAVE_OS_H
# include <fs_info.h>
#endif
"
  if case "$fu_cv_sys_stat_statvfs$fu_cv_sys_stat_statvfs64" in
       *yes*) ;; *) false;; esac &&
     { AC_CHECK_MEMBERS([struct statvfs.f_basetype],,, [$statvfs_includes])
       test $ac_cv_member_struct_statvfs_f_basetype = yes ||
       { AC_CHECK_MEMBERS([struct statvfs.f_fstypename],,, [$statvfs_includes])
         test $ac_cv_member_struct_statvfs_f_fstypename = yes ||
         { test $ac_cv_member_struct_statfs_f_fstypename != yes &&
           { AC_CHECK_MEMBERS([struct statvfs.f_type],,, [$statvfs_includes])
             test $ac_cv_member_struct_statvfs_f_type = yes; }; }; }; }
  then
    AC_CHECK_MEMBERS([struct statvfs.f_namemax],,, [$statvfs_includes])
    AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM(
         [$statvfs_includes],
         [static statvfs s;
          return (s.s_fsid ^ 0) == 0;])],
      [AC_DEFINE([STRUCT_STATVFS_F_FSID_IS_INTEGER], [1],
         [Define to 1 if the f_fsid member of struct statvfs is an integer.])])
  else
    AC_CHECK_MEMBERS([struct statfs.f_namelen, struct statfs.f_type,
                     struct statfs.f_frsize],,, [$statfs_includes])
    if test $ac_cv_header_OS_h != yes; then
      AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM(
           [$statfs_includes],
           [static statfs s;
            return (s.s_fsid ^ 0) == 0;])],
        [AC_DEFINE([STRUCT_STATFS_F_FSID_IS_INTEGER], [1],
           [Define to 1 if the f_fsid member of struct statfs is an integer.])])
    fi
  fi
])


dnl
dnl Filesystem information detection
dnl
dnl To get information about the disk, mount points, etc.
dnl

AC_DEFUN([mc_AC_GET_FS_INFO], [
    gl_MOUNTLIST
    if test $gl_cv_list_mounted_fs = yes; then
      gl_PREREQ_MOUNTLIST_EXTRA
    fi

    AC_CHECK_HEADERS([fcntl.h utime.h])

    gl_LIST_MOUNTED_FILE_SYSTEMS([
	AC_DEFINE(HAVE_INFOMOUNT_LIST, 1,
	    [Define if the list of mounted filesystems can be determined])],
	[AC_MSG_WARN([could not determine how to read list of mounted fs])])

    gl_FSUSAGE
    if test $gl_cv_fs_space = yes; then
	gl_PREREQ_FSUSAGE_EXTRA
    fi
    gl_FSTYPENAME

    gl_POSIX_FALLOCATE

    mc_cu_PREREQ_STAT_PROG
])
