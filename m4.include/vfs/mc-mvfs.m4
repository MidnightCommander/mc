AC_DEFUN([AC_PREPARE_MVFS],
[
     AM_CONDITIONAL(ENABLE_MVFS, [test "$enable_mvfs" = "yes"])
])

AC_DEFUN([AC_REQUIRE_MVFS],
[
    echo "libmvfs required ... checking ...";
    if test "$got_mvfs" = "yes" ; then
	echo "mvfs already enabled"
    else
	PKG_CHECK_MODULES([MVFS], [libmvfs])
	AC_DEFINE(ENABLE_MVFS, 1, [Enabled mvfs-based virtual filesystems])
	got_mvfs="yes"
	AC_MC_VFS_ADDNAME([mvfs])
    fi
    AM_CONDITIONAL(ENABLE_MVFS, [test "$enable_mvfs" = "yes"])
])

AC_DEFUN([AC_MVFS_FS], [
    AC_PREPARE_MVFS
    AC_ARG_ENABLE([mvfs-$1],[  --enable-mvfs-$1    Support for $3 (via libmvfs)])
    if test "$enable_mvfs_$1" == "yes" ; then
	AC_REQUIRE_MVFS
	AC_DEFINE(ENABLE_MVFS_$2, 1, [$1 (via libmvfs)])
	AC_MC_VFS_ADDNAME([mvfs-$1])
    fi
    AM_CONDITIONAL(ENABLE_MVFS_$2, [test "$enable_mvfs_$1" == "yes"])
])

AC_DEFUN([AC_MVFS_NINEP], [AC_MVFS_FS([9p],    [NINEP], [9P Filesystem])])
AC_DEFUN([AC_MVFS_LOCAL], [AC_MVFS_FS([local], [LOCAL], [Local filesystem])])
AC_DEFUN([AC_MVFS_FISH],  [AC_MVFS_FS([fish],  [FISH],  [Fish remote filesystem])])

AC_DEFUN([AC_MC_MVFS_FILESYSTEMS],
[
    AC_PREPARE_MVFS
    AC_MVFS_NINEP
    AC_MVFS_LOCAL
    AC_MVFS_FISH
])
