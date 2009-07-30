dnl Enable FISH protocol (classic)
AC_DEFUN([AC_MC_VFS_FISH],
[
    AC_ARG_ENABLE([vfs-fish],
                  [  --enable-vfs-fish       Support for FISH vfs [[yes]]])
    if test "x$enable_vfs_fish" != xno; then
        enable_vfs_fish="yes"
	AC_MC_VFS_ADDNAME([fish])
	AC_DEFINE([ENABLE_VFS_FISH], [1], [Support for FISH vfs])
    fi
    AM_CONDITIONAL(ENABLE_VFS_FISH, [test x"$enable_vfs_fish" = x"yes"])
])
