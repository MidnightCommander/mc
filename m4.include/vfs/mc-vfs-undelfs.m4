dnl MC_UNDELFS_CHECKS
dnl    Check for ext2fs undel support.
dnl    Set shell variable ext2fs_undel to "yes" if we have it,
dnl    "no" otherwise.  May define USE_EXT2FSLIB for cpp.
dnl    Will set EXT2FS_UNDEL_LIBS to required libraries.

AC_DEFUN([MC_UNDELFS_CHECKS], [
  ext2fs_undel=no
  EXT2FS_UNDEL_LIBS=
  AC_CHECK_HEADERS([ext2fs/ext2_fs.h linux/ext2_fs.h], [ext2_fs_h=yes; break])
  if test x"$ext2_fs_h" = xyes; then
    AC_CHECK_HEADERS([ext2fs/ext2fs.h], [ext2fs_ext2fs_h=yes], ,
		     [
#include <stdio.h>
#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>
#else
#undef umode_t
#include <linux/ext2_fs.h>
#endif
		     ])
    if test x"$ext2fs_ext2fs_h" = xyes; then
      AC_DEFINE(USE_EXT2FSLIB, 1,
		[Define to enable undelete support on ext2])
      ext2fs_undel=yes
      EXT2FS_UNDEL_LIBS="-lext2fs -lcom_err"
      AC_CHECK_TYPE(ext2_ino_t, ,
		    [AC_DEFINE(ext2_ino_t, ino_t,
			       [Define to ino_t if undefined.])],
		    [
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>
#else
#undef umode_t
#include <linux/ext2_fs.h>
#endif
#include <ext2fs/ext2fs.h>
		    ])
    fi
  fi
])

dnl
dnl Ext2fs undelete support
dnl
AC_DEFUN([AC_MC_VFS_UNDELFS],
[
    AC_ARG_ENABLE([undelfs],
                  [  --enable-vfs-undelfs    Support for ext2 undelete filesystem])

    if test x"$enable_vfs_undelfs" = x"yes" ; then

	MC_UNDELFS_CHECKS

	if test x"$ext2fs_undel" = xyes; then
	    enable_vfs_undelfs="yes"
	    AC_MC_VFS_ADDNAME([undelfs])
	    AC_DEFINE(ENABLE_VFS_UNDELFS, [1], [Support for ext2 undelfs])
	    AC_MSG_NOTICE([using ext2fs file recovery code])
	    MCLIBS="$MCLIBS $EXT2FS_UNDEL_LIBS"
	else
	    AC_ERROR([Ext2 libraries not found])
	fi
    fi
    AM_CONDITIONAL(ENABLE_VFS_UNDELFS, [test x"$enable_vfs_undelfs" = x"yes"])
])
