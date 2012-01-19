
dnl
dnl posix_allocate() function detection
dnl

AC_DEFUN([gl_POSIX_FALLOCATE], [
    dnl * Old glibcs have broken posix_fallocate(). Make sure not to use it.
    AC_TRY_LINK([
        #define _XOPEN_SOURCE 600
        #include <stdlib.h>
        #if defined(__GLIBC__) && (__GLIBC__ < 2 || __GLIBC_MINOR__ < 7)
            possibly broken posix_fallocate
        #endif
    ],
    [posix_fallocate(0, 0, 0);],
    [AC_DEFINE(
        [HAVE_POSIX_FALLOCATE],
        [1],
        [Define if you have a working posix_fallocate()])
    ])
])

dnl
dnl Filesystem information detection
dnl
dnl To get information about the disk, mount points, etc.
dnl

AC_DEFUN([AC_MC_GET_FS_INFO], [
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
])
