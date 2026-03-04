dnl Enable SHELL protocol
AC_DEFUN([mc_VFS_SHELL],
[
    AC_ARG_ENABLE([vfs-shell],
		    AS_HELP_STRING([--enable-vfs-shell], [Support for SHELL filesystem @<:@yes@:>@]))
    if test "$enable_vfs" = "yes" -a "x$enable_vfs_shell" != xno; then
	enable_vfs_shell="yes"
	mc_VFS_ADDNAME([shell])
	AC_DEFINE([ENABLE_VFS_SHELL], [1], [Support for SHELL vfs])

	dnl Optional libssh2 transport for Shell VFS (password auth, native SSH)
	if test x"$found_libssh" != "xyes"; then
	    PKG_CHECK_MODULES(LIBSSH, [libssh2 >= 1.2.8], [found_libssh=yes], [:])
	fi
	if test x"$found_libssh" = "xyes"; then
	    AC_DEFINE([ENABLE_SHELL_SSH2], [1], [Use libssh2 for Shell VFS transport])
	fi
    fi
    AM_CONDITIONAL(ENABLE_VFS_SHELL, [test "$enable_vfs" = "yes" -a x"$enable_vfs_shell" = x"yes"])
])
