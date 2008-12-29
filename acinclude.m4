dnl MC_UNDELFS_CHECKS
dnl    Check for ext2fs undel support.
dnl    Set shell variable ext2fs_undel to "yes" if we have it,
dnl    "no" otherwise.  May define USE_EXT2FSLIB for cpp.
dnl    Will set EXT2FS_UNDEL_LIBS to required libraries.

AC_DEFUN([MC_UNDELFS_CHECKS], [
  ext2fs_undel=no
  EXT2FS_UNDEL_LIBS=
  AC_CHECK_HEADERS([ext2fs/ext2_fs.h linux/ext2_fs.h], [ext2_fs_h=yes; break])
  if test x$ext2_fs_h = xyes; then
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
    if test x$ext2fs_ext2fs_h = xyes; then
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

  vfs_flags="cpiofs, extfs, tarfs"
  use_net_code=false

  AC_ARG_ENABLE([netcode],
		[  --enable-netcode         Support for networking [[yes]]])

  if test "x$enable_netcode" != xno; then
    dnl FIXME: network checks should probably be in their own macro.
    AC_SEARCH_LIBS(socket, [xnet bsd socket inet], [have_socket=yes])
    if test x$have_socket = xyes; then
      AC_SEARCH_LIBS(gethostbyname, [bsd socket inet netinet])
      AC_CHECK_MEMBERS([struct linger.l_linger], , , [
#include <sys/types.h>
#include <sys/socket.h>
		       ])
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

  dnl
  dnl Ext2fs undelete support
  dnl
  AC_ARG_WITH(ext2undel,
    [  --with-ext2undel         Compile with ext2 undelete code [[yes if found]]],
    [if test x$withval != xno; then
      if test x$withval != xyes; then
	LDFLAGS="$LDFLAGS -L$withval/lib"
	CPPFLAGS="$CPPFLAGS -I$withval/include"
      fi
      AC_EXT2_UNDEL
    fi],[
    dnl Default: detect
    AC_CHECK_LIB(ext2fs, ext2fs_close, [AC_EXT2_UNDEL], , [-lcom_err])
  ])

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
dnl Check if private functions are available for linking
dnl
AC_DEFUN([MC_SLANG_PRIVATE], [
    AC_CACHE_CHECK([if S-Lang exports private functions],
		   [mc_cv_slang_private], [
	ac_save_LIBS="$LIBS"
	LIBS="$LIBS -lslang"
	AC_TRY_LINK([
		     #ifdef HAVE_SLANG_SLANG_H
		     #include <slang/slang.h>
		     #else
		     #include <slang.h>
		     #endif
		     #if SLANG_VERSION >= 10000
		     extern unsigned int SLsys_getkey (void);
		     #else
		     extern unsigned int _SLsys_getkey (void);
		     #endif
		    ], [
		     #if SLANG_VERSION >= 10000
		     _SLsys_getkey ();
		     #else
		     SLsys_getkey ();
		     #endif
		    ],
		    [mc_cv_slang_private=yes],
		    [mc_cv_slang_private=no])
	LIBS="$ac_save_LIBS"
    ])

    if test x$mc_cv_slang_private = xyes; then
	AC_DEFINE(HAVE_SLANG_PRIVATE, 1,
		  [Define if private S-Lang functions are available])
    fi
])


dnl
dnl Check if the installed S-Lang library uses termcap
dnl
AC_DEFUN([MC_SLANG_TERMCAP], [
    AC_CACHE_CHECK([if S-Lang uses termcap], [mc_cv_slang_termcap], [
	ac_save_LIBS="$LIBS"
	LIBS="$LIBS -lslang"
	AC_TRY_LINK([
		     #ifdef HAVE_SLANG_SLANG_H
		     #include <slang/slang.h>
		     #else
		     #include <slang.h>
		     #endif
		    ],
		    [SLtt_get_terminfo(); SLtt_tgetflag("");],
		    [mc_cv_slang_termcap=no],
		    [mc_cv_slang_termcap=yes])
	LIBS="$ac_save_LIBS"
    ])

    if test x$mc_cv_slang_termcap = xyes; then
	MC_USE_TERMCAP
    fi
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

    dnl Check the header
    slang_h_found=
    AC_CHECK_HEADERS([slang.h slang/slang.h],
		     [slang_h_found=yes; break])
    if test -z "$slang_h_found"; then
	with_screen=mcslang
    fi

    dnl Check if termcap is needed.
    dnl This check must be done before anything is linked against S-Lang.
    if test x$with_screen = xslang; then
	MC_SLANG_TERMCAP
    fi

    dnl Check the library
    if test x$with_screen = xslang; then
	AC_CHECK_LIB([slang], [SLang_init_tty], [MCLIBS="$MCLIBS -lslang"],
		     [with_screen=mcslang], ["$MCLIBS"])
    fi

    dnl Unless external S-Lang was requested, reject S-Lang with UTF-8 hacks
    if test x$with_screen = xslang; then
	:
	m4_if([$1], strict, ,
	      [AC_CHECK_LIB([slang], [SLsmg_write_nwchars],
	    		    [AC_MSG_WARN([Rejecting S-Lang with UTF-8 support, \
it's not fully supported yet])
	      with_screen=mcslang])])
    fi

    if test x$with_screen = xslang; then
	AC_DEFINE(HAVE_SYSTEM_SLANG, 1,
		  [Define to use S-Lang library installed on the system])
	MC_SLANG_PRIVATE
	screen_type=slang
	screen_msg="S-Lang library (installed on the system)"
    else
	m4_if([$1], strict,
	    [if test $with_screen != slang; then
		AC_MSG_ERROR([S-Lang library not found])
	    fi],
	    [MC_WITH_MCSLANG]
	)
    fi

    _MC_WITH_XSLANG
])


dnl
dnl Use the included S-Lang library.
dnl
AC_DEFUN([MC_WITH_MCSLANG], [
    screen_type=mcslang
    screen_msg="Included S-Lang library (mcslang)"

    dnl Type checks from S-Lang sources
    AC_CHECK_SIZEOF(short, 2)
    AC_CHECK_SIZEOF(int, 4)
    AC_CHECK_SIZEOF(long, 4)
    AC_CHECK_SIZEOF(float, 4)
    AC_CHECK_SIZEOF(double, 8)
    AC_TYPE_OFF_T
    AC_CHECK_SIZEOF(off_t)
    AC_CHECK_TYPES(long long)
    AC_CHECK_SIZEOF(long long)
    AC_CHECK_FUNCS(atexit on_exit)

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
    ncursesw_found=
    LIBS=
    AC_SEARCH_LIBS([addwstr], [ncursesw ncurses curses], [MCLIBS="$MCLIBS $LIBS";ncursesw_found=yes],
		   [AC_MSG_WARN([Cannot find ncurses library, that support wide characters])])

    if test -z "$ncursesw_found"; then
    LIBS=
    AC_SEARCH_LIBS([has_colors], [ncurses curses], [MCLIBS="$MCLIBS $LIBS"],
		   [AC_MSG_ERROR([Cannot find ncurses library])])
    fi

    dnl Check the header
    ncurses_h_found=
    AC_CHECK_HEADERS([ncursesw/curses.h ncurses/curses.h ncurses.h curses.h],
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
		   [AC_TRY_LINK([], [
			extern int ESCDELAY;
			ESCDELAY = 0;
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
dnl Use the ncurses library.  It can only be requested explicitly,
dnl so just fail if anything goes wrong.
dnl
dnl If ncurses exports the ESCDELAY variable it should be set to 0
dnl or you'll have to press Esc three times to dismiss a dialog box.
dnl
AC_DEFUN([MC_WITH_NCURSESW], [
    dnl has_colors() is specific to ncurses, it's not in the old curses
    save_LIBS="$LIBS"
    LIBS=
    AC_SEARCH_LIBS([has_colors], [ncursesw], [MCLIBS="$MCLIBS $LIBS"],
		   [AC_MSG_ERROR([Cannot find ncursesw library])])

    dnl Check the header
    ncurses_h_found=
    AC_CHECK_HEADERS([ncursesw/curses.h],
		     [ncursesw_h_found=yes; break])

    if test -z "$ncursesw_h_found"; then
	AC_MSG_ERROR([Cannot find ncursesw header file])
    fi

    screen_type=ncursesw
    screen_msg="ncursesw library"
    AC_DEFINE(USE_NCURSESW, 1,
	      [Define to use ncursesw for screen management])

    AC_CACHE_CHECK([for ESCDELAY variable],
		   [mc_cv_ncursesw_escdelay],
		   [AC_TRY_LINK([], [
			extern int ESCDELAY;
			ESCDELAY = 0;
			],
			[mc_cv_ncursesw_escdelay=yes],
			[mc_cv_ncursesw_escdelay=no])
    ])
    if test "$mc_cv_ncursesw_escdelay" = yes; then
	AC_DEFINE(HAVE_ESCDELAY, 1,
		  [Define if ncursesw has ESCDELAY variable])
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
