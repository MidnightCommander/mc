dnl Enable FISH protocol (classic)
AC_DEFUN([AC_MC_VFS_FISH],
[
    AC_ARG_ENABLE([vfs-fish],
                  [  --disable-vfs-fish           Support for FISH vfs [[yes]]])
    if test "x$enable_vfs_fish" != xno; then
        if test "x$enable_mvfs_fish" = "yes"; then
	    AC_ERROR([Internal FISH-fs conflicts with mvfs-based fish])
        fi
        enable_vfs_fish="yes"
	AC_MC_VFS_ADDNAME([fish])
	AC_DEFINE([ENABLE_VFS_FISH], [1], [Support for FISH vfs])
    fi
    AM_CONDITIONAL(ENABLE_VFS_FISH, [test "$enable_vfs_fish" = "yes"])
])
