dnl
dnl XView & SlingShot library checking
dnl (c) 1995 Jakub Jelinek
dnl

dnl Set xview_includes, xview_libraries, and no_xview (initially yes).
dnl Also sets xview_no_private_headers to yes if there are no xview_private
dnl headers in the system.
AC_DEFUN(AC_PATH_XVIEW,
[
no_xview=yes
AC_ARG_WITH(xview, [--with-xview              Use the XView toolkit],no_xview=)

AC_ARG_WITH(xview-includes, [--with-xview-includes=path  Specifies XView includes directory],
[
if test x$withval = xyes; then
    AC_MSG_WARN(Usage is: --with-xview-includes=path)
    xview_includes=NONE
    no_xview=
else
    xview_includes=$withval
fi
],
[
xview_includes=NONE
])dnl
AC_ARG_WITH(xview-libraries, [--with-xview-libraries=path  Specifies XView libraries directory],
[
if test x$withval = xyes; then
    AC_MSG_WARN(Usage is: --with-xview-libraries=path)
    xview_libraries=NONE
    no_xview=
else
    xview_libraries=$withval
fi
],
[
xview_libraries=NONE
])dnl

if test "$no_xview" != yes; then
    if test "$no_x" = yes; then
        no_xview=yes
    fi
fi
if test "$no_xview" != yes; then
AC_MSG_CHECKING(for XView)
if test x$xview_libraries = xNONE; then 
    if test x$xview_includes = xNONE; then
AC_CACHE_VAL(ac_cv_path_xview,
[
    no_xview=yes
AC_PATH_XVIEW_XMKMF
    if test "x$no_xview" = xyes; then
AC_PATH_XVIEW_DIRECT
    fi
    if test "x$no_xview" = xyes; then
        ac_cv_path_xview="no_xview=yes"
    else
        ac_cv_path_xview="no_xview= ac_xview_includes=$ac_xview_includes ac_xview_libraries=$ac_xview_libraries ac_xview_no_private_headers=$ac_xview_no_private_headers"
    fi
])dnl
        eval "$ac_cv_path_xview"
    fi
fi

if test "x$no_xview" = xyes; then
    AC_MSG_RESULT(no)
else
    if test "x$xview_includes" = x || test "x$xview_includes" = xNONE; then
        xview_includes=$ac_xview_includes
    fi
    if test "x$xview_libraries" = x || test "x$xview_libraries" = xNONE; then
        xview_libraries=$ac_xview_libraries
    fi
    xview_no_private_headers=$ac_xview_no_private_headers
    ac_cv_path_xview="no_xview= ac_xview_includes=$xview_includes ac_xview_libraries=$xview_libraries ac_xview_no_private_headers=$ac_xview_no_private_headers" 
    if test "x$xview_libraries" != x; then
	ac_msg_xview="libraries $xview_libraries"
    else
    	ac_msg_xview=""
    fi
    if test "x$xview_includes" != x; then
        if test "x$ac_msg_xview" != x; then
	    ac_msg_xview="$ac_msg_xview, "
	fi
	ac_msg_xview="${ac_msg_xview}headers $xview_includes"
    fi
    if test "x$xview_no_private_headers" = xyes; then
        if test "x$ac_msg_xview" != x; then
	    ac_msg_xview="$ac_msg_xview, "
	fi
	ac_msg_xview="${ac_msg_xview}without xview_private headers"
    fi	
    AC_MSG_RESULT([$ac_msg_xview])
fi
fi
])

dnl Internal subroutine of AC_PATH_XVIEW
dnl Set ac_xview_includes, ac_xview_libraries, and no_xview (initially yes).
AC_DEFUN(AC_PATH_XVIEW_XMKMF,
[rm -fr conftestdir
if mkdir conftestdir; then
  cd conftestdir
  # Make sure to not put "make" in the Imakefile rules, since we grep it out.
  cat > Imakefile <<'EOF'
#include <XView.tmpl>
acfindxv:
	@echo 'ac_im_library_dest="${LIBRARY_DEST}"; ac_im_header_dest="${HEADER_DEST}"'
EOF
  if (xmkmf) >/dev/null 2>/dev/null && test -f Makefile; then
    no_xview=
    # GNU make sometimes prints "make[1]: Entering...", which would confuse us.
    eval `make acfindxv 2>/dev/null | grep -v make`
    # Screen out bogus values from the imake configuration.
    if test -f "$ac_im_header_dest/xview/xview.h"; then
        ac_xview_includes="$ac_im_header_dest"
    else
	no_xview=yes
    fi
    if test -d "$ac_im_library_dest"; then
        ac_xview_libraries="$ac_im_library_dest"
    else
	no_xview=yes
    fi
  fi
  if test "x$no_xview" != xyes; then
    if test -f "$ac_xview_includes/xview_private/draw_impl.h"; then
	ac_xview_no_private_headers=
    else
	ac_xview_no_private_headers=yes
    fi
  fi
  cd ..
  rm -fr conftestdir
fi
])

dnl Internal subroutine of AC_PATH_XVIEW
dnl Set ac_xview_includes, ac_xview_libraries, and no_xview (initially yes).
AC_DEFUN(AC_PATH_XVIEW_DIRECT,
[test -z "$xview_direct_test_library" && xview_direct_test_library=xview
test -z "$xview_direct_test_function" && xview_direct_test_function=xv_unique_key
test -z "$xview_direct_test_include" && xview_direct_test_include=xview/xview.h
AC_TRY_CPP([#include <$xview_direct_test_include>],
[no_xview= ac_xview_includes=],
[  for ac_dir in               \
    $OPENWINHOME/include    \
    /usr/openwin/include      \
    /usr/openwin/share/include \
                              \
    /usr/X11R6/include        \
    /usr/X11R5/include        \
    /usr/X11R4/include        \
                              \
    /usr/include/X11R6        \
    /usr/include/X11R5        \
    /usr/include/X11R4        \
                              \
    /usr/local/X11R6/include  \
    /usr/local/X11R5/include  \
    /usr/local/X11R4/include  \
                              \
    /usr/local/include/X11R6  \
    /usr/local/include/X11R5  \
    /usr/local/include/X11R4  \
                              \
    /usr/X11/include          \
    /usr/include/X11          \
    /usr/local/X11/include    \
    /usr/local/include/X11    \
                              \
    /usr/X386/include         \
    /usr/x386/include         \
    /usr/XFree86/include/X11  \
                              \
    /usr/include              \
    /usr/local/include        \
    /usr/unsupported/include  \
    /usr/athena/include       \
    /usr/local/x11r5/include  \
    /usr/lpp/Xamples/include  \
    ; \
  do
    if test -r "$ac_dir/$xview_direct_test_include"; then
      no_xview= ac_xview_includes=$ac_dir
      break
    fi
  done])

if test "x$no_xview" != xyes; then
    if test "x$ac_xview_includes" != x; then
        if test -f "$ac_xview_includes/xview_private/draw_impl.h"; then
	    ac_xview_no_private_headers=
        else
	    ac_xview_no_private_headers=yes
        fi
    else
AC_TRY_CPP([#include <xview_private/draw_impl.h>],
[ac_xview_no_private_headers=],[ac_xview_no_private_headers=yes])
    fi
fi

# Check for the libraries.
# See if we find them without any special options.
# Don't add to $LIBS permanently.
ac_save_LIBS="$LIBS"
ac_save_LDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS $X_LIBS"
LIBS="-l$xview_direct_test_library -lolgx $X_EXTRA_LIBS -lX11 $X_PRE_LIBS $LIBS"
AC_TRY_LINK([#include <$xview_direct_test_include>
], [${xview_direct_test_function}()],
[LIBS="$ac_save_LIBS" LDFLAGS="$ac_save_LDFLAGS" no_xview= ac_xview_libraries=],
[LIBS="$ac_save_LIBS" LDFLAGS="$ac_save_LDFLAGS"
# First see if replacing the include by lib works.
for ac_dir in `echo "$ac_xview_includes" | sed s/include/lib/` \
    $OPENWINHOME/lib    \
    $OPENWINHOME/share/lib \
    /usr/openwin/lib      \
    /usr/openwin/share/lib \
                          \
    /usr/X11R6/lib        \
    /usr/X11R5/lib        \
    /usr/X11R4/lib        \
                          \
    /usr/lib/X11R6        \
    /usr/lib/X11R5        \
    /usr/lib/X11R4        \
                          \
    /usr/local/X11R6/lib  \
    /usr/local/X11R5/lib  \
    /usr/local/X11R4/lib  \
                          \
    /usr/local/lib/X11R6  \
    /usr/local/lib/X11R5  \
    /usr/local/lib/X11R4  \
                          \
    /usr/X11/lib          \
    /usr/lib/X11          \
    /usr/local/X11/lib    \
    /usr/local/lib/X11    \
                          \
    /usr/X386/lib         \
    /usr/x386/lib         \
    /usr/XFree86/lib/X11  \
                          \
    /usr/lib              \
    /usr/local/lib        \
    /usr/unsupported/lib  \
    /usr/athena/lib       \
    /usr/local/x11r5/lib  \
    /usr/lpp/Xamples/lib  \
    ; \
do
  for ac_extension in a so sl; do
    if test -r $ac_dir/lib${xview_direct_test_library}.$ac_extension; then
      no_xview= ac_xview_libraries=$ac_dir
      break 2
    fi
  done
done])])

dnl Substitute XVIEW_LIBS and XVIEW_CFLAGS and 
dnl HAVE_XVIEW, which is either yes or no.
dnl Both contain X_LIBS resp. X_CFLAGS inside
dnl Also substitutes HAVE_XVIEW_PRIVATE_HEADERS
dnl if there are xview_private headers in the system
AC_DEFUN(AC_PATH_XVIEW_XTRA,
[AC_REQUIRE([AC_PATH_XVIEW])dnl
if test "$no_xview" = yes; then 
  # Not all programs may use this symbol, but it does not hurt to define it.
  XVIEW_CFLAGS="$X_CFGLAGS $XVIEW_CFLAGS -DXVIEW_MISSING"
else
  if test -n "$xview_includes"; then
    XVIEW_CFLAGS="$X_CFLAGS $XVIEW_CFGLAGS"
    if test "$xview_includes" != "$x_includes"; then
      XVIEW_CPPFLAGS="-I$xview_includes"
    fi
  fi

  # It would be nice to have a more robust check for the -R ld option than
  # just checking for Solaris.
  # It would also be nice to do this for all -L options, not just this one.
  if test -n "$xview_libraries"; then
    if test "$xview_libraries" = "$x_libraries"; then
      XVIEW_LIBS="$X_LIBS $XVIEW_LIBS"
    else
      XVIEW_LIBS="$X_LIBS $XVIEW_LIBS -L$xview_libraries"
      if test "`(uname) 2>/dev/null`" = SunOS &&
        uname -r | grep '^5' >/dev/null; then
        XVIEW_LIBS="$XVIEW_LIBS -R$xview_libraries"
      fi
    fi
  fi
fi
if test "x$no_xview" = xyes; then
  HAVE_XVIEW=no
else
  HAVE_XVIEW=yes
fi
if test "x$xview_no_private_headers" = xyes; then
  HAVE_XVIEW_PRIVATE_HEADERS=no
else
  HAVE_XVIEW_PRIVATE_HEADERS=yes
fi
AC_SUBST(XVIEW_CFLAGS)dnl
AC_SUBST(XVIEW_CPPFLAGS)dnl
AC_SUBST(XVIEW_LIBS)dnl
AC_SUBST(HAVE_XVIEW)dnl
AC_SUBST(HAVE_XVIEW_PRIVATE_HEADERS)dnl
])dnl

dnl Internal subroutine of AC_PATH_SLINGSHOT
AC_DEFUN(AC_PATH_SLINGSHOT_DIRECT,
[
AC_TRY_CPP([#include <sspkg/rectobj.h>],[no_ss= ac_ss_includes=],
[  for ac_dir in               \
    $OPENWINHOME/include    \
    /usr/openwin/include      \
    /usr/openwin/share/include \
                              \
    /usr/X11R6/include        \
    /usr/X11R5/include        \
    /usr/X11R4/include        \
                              \
    /usr/include/X11R6        \
    /usr/include/X11R5        \
    /usr/include/X11R4        \
                              \
    /usr/local/X11R6/include  \
    /usr/local/X11R5/include  \
    /usr/local/X11R4/include  \
                              \
    /usr/local/include/X11R6  \
    /usr/local/include/X11R5  \
    /usr/local/include/X11R4  \
                              \
    /usr/X11/include          \
    /usr/include/X11          \
    /usr/local/X11/include    \
    /usr/local/include/X11    \
                              \
    /usr/X386/include         \
    /usr/x386/include         \
    /usr/XFree86/include/X11  \
                              \
    /usr/include              \
    /usr/local/include        \
    /usr/unsupported/include  \
    /usr/athena/include       \
    /usr/local/x11r5/include  \
    /usr/lpp/Xamples/include  \
    ; \
  do
    if test -r "$ac_dir/sspkg/rectobj.h"; then
      no_ss= ac_ss_includes=$ac_dir
      break
    fi
  done])

# Check for the libraries.
# See if we find them without any special options.
# Don't add to $LIBS permanently.
ac_save_LIBS="$LIBS"
ac_save_LDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS $XVIEW_LIBS"
LIBS="-lsspkg -lm -lxview -lolgx $X_EXTRA_LIBS -lX11 $X_PRE_LIBS $LIBS"
AC_TRY_LINK([#include <sspkg/rectobj.h>
], [rectobj_get_selected_list()],
[LIBS="$ac_save_LIBS" LDFLAGS="$ac_save_LDFLAGS" no_ss= ac_ss_libraries=],
[LIBS="$ac_save_LIBS" LDFLAGS="$ac_save_LDFLAGS"
# First see if replacing the include by lib works.
for ac_dir in `echo "$ac_ss_includes" | sed s/include/lib/` \
    $OPENWINHOME/lib    \
    $OPENWINHOME/share/lib \
    /usr/openwin/lib      \
    /usr/openwin/share/lib \
                          \
    /usr/X11R6/lib        \
    /usr/X11R5/lib        \
    /usr/X11R4/lib        \
                          \
    /usr/lib/X11R6        \
    /usr/lib/X11R5        \
    /usr/lib/X11R4        \
                          \
    /usr/local/X11R6/lib  \
    /usr/local/X11R5/lib  \
    /usr/local/X11R4/lib  \
                          \
    /usr/local/lib/X11R6  \
    /usr/local/lib/X11R5  \
    /usr/local/lib/X11R4  \
                          \
    /usr/X11/lib          \
    /usr/lib/X11          \
    /usr/local/X11/lib    \
    /usr/local/lib/X11    \
                          \
    /usr/X386/lib         \
    /usr/x386/lib         \
    /usr/XFree86/lib/X11  \
                          \
    /usr/lib              \
    /usr/local/lib        \
    /usr/unsupported/lib  \
    /usr/athena/lib       \
    /usr/local/x11r5/lib  \
    /usr/lpp/Xamples/lib  \
    ; \
do
  for ac_extension in a so sl; do
    if test -r $ac_dir/libsspkg.$ac_extension; then
      no_ss= ac_ss_libraries=$ac_dir
      break 2
    fi
  done
done])])

dnl Set ss_includes, ss_libraries, and no_ss (initially yes).
AC_DEFUN(AC_PATH_SLINGSHOT,
[AC_REQUIRE([AC_PATH_XVIEW_XTRA])dnl
AC_MSG_CHECKING(for SlingShot)
AC_ARG_WITH(ss, [--with-ss         Use the SlingShot extension])

AC_ARG_WITH(ss-includes, [--with-ss-includes=path  Specifies SlingShot includes directory],
[
if test x$withval = xyes; then
    AC_MSG_WARN(Usage is: --with-ss-includes=path)
    ss_includes=NONE
else
    ss_includes=$withval
fi
],
[
ss_includes=NONE
])dnl
AC_ARG_WITH(ss-libraries, [--with-ss-libraries=path  Specifies SlingShot libraries directory],
[
if test x$withval = xyes; then
    AC_MSG_WARN(Usage is: --with-ss-libraries=path)
    ss_libraries=NONE
else
    ss_libraries=$withval
fi
],
[
ss_libraries=NONE
])dnl

if test "x$with_ss" = xno; then
    no_ss=yes
else
    if test "x$ss_includes" != xNONE && test "x$ss_libraries" != xNONE; then
        no_ss=
    else
AC_CACHE_VAL(ac_cv_path_ss,
[
    no_ss=yes
AC_PATH_SLINGSHOT_DIRECT
    if test "x$no_ss" = xyes; then
        ac_cv_path_ss="ac_noss=yes"
    else
        ac_cv_path_ss="ac_ss_includes=$ac_ss_includes ac_ss_libraries=$ac_ss_libraries"
    fi
])dnl
        eval "$ac_cv_path_ss"
    fi
fi
fi
if test "x$no_ss" = xyes; then
    AC_MSG_RESULT(no)
else
    if test "x$ss_includes" = x || test "x$ss_includes" = xNONE; then
        ss_includes=$ac_ss_includes
    fi
    if test "x$ss_libraries" = x || test "x$ss_libraries" = xNONE; then
        ss_libraries=$ac_ss_libraries
    fi
    ac_cv_path_ss="no_ss= ac_ss_includes=$ss_includes ac_ss_libraries=$ss_libraries" 
    if test "x$ss_libraries" = x; then
        if test "x$ss_includes" = x; then
	    AC_MSG_RESULT(yes)
	else
    	    AC_MSG_RESULT([headers $ss_includes])
	fi
    else
        if test "x$ss_includes" = x; then
    	    AC_MSG_RESULT([libraries $ss_libraries])
	else
    	    AC_MSG_RESULT([libraries $ss_libraries, headers $ss_includes])
	fi
    fi
fi
])

dnl Substitute SLINGSHOT_LIBS and SLINGSHOT_CFLAGS and 
dnl HAVE_SLINGSHOT, which is either yes or no.
dnl Both contain XVIEW_LIBS resp. XVIEW_CFLAGS inside
AC_DEFUN(AC_PATH_SLINGSHOT_XTRA,
[AC_REQUIRE([AC_PATH_SLINGSHOT])dnl
if test "$no_ss" = yes; then 
  # Not all programs may use this symbol, but it does not hurt to define it.
  SLINGSHOT_CFLAGS="$XVIEW_CFGLAGS $SLINGSHOT_CFLAGS -DSLINGSHOT_MISSING"
else
  if test -n "$ss_includes"; then
    SLINGSHOT_CFLAGS="$XVIEW_CFLAGS $SLINGSHOT_CFGLAGS -I$ss_includes"
  fi

  # It would be nice to have a more robust check for the -R ld option than
  # just checking for Solaris.
  # It would also be nice to do this for all -L options, not just this one.
  if test -n "$ss_libraries"; then
    SLINGSHOT_LIBS="$XVIEW_LIBS $SLINGSHOT_LIBS -L$ss_libraries"
    if test "`(uname) 2>/dev/null`" = SunOS &&
      uname -r | grep '^5' >/dev/null; then
      SLINGSHOT_LIBS="$SLINGSHOT_LIBS -R$ss_libraries"
    fi
  fi
fi
if test "x$no_ss" = xyes; then
  HAVE_SLINGSHOT=no
else
  HAVE_SLINGSHOT=yes
fi
AC_SUBST(SLINGSHOT_CFLAGS)dnl
AC_SUBST(SLINGSHOT_LIBS)dnl
AC_SUBST(HAVE_SLINGSHOT)dnl
])dnl

dnl
dnl XView library checking end
dnl

dnl
dnl Check for struct linger
dnl
AC_DEFUN(AC_STRUCT_LINGER, [
av_struct_linger=no
AC_MSG_CHECKING(struct linger is available)
AC_TRY_RUN([
#include <sys/types.h>
#include <sys/socket.h>

struct linger li;

main ()
{
    li.l_onoff = 1;
    li.l_linger = 120;
    exit (0);
}
],[
AC_DEFINE(HAVE_STRUCT_LINGER)
av_struct_linger=yes
],[
av_struct_linger=no
],[
av_struct_linger=no
])
AC_MSG_RESULT($av_struct_linger)
])

dnl
dnl Check for size of d_name dirent member
dnl
AC_DEFUN(AC_SHORT_D_NAME_LEN, [
AC_MSG_CHECKING(filename fits on dirent.d_name)
AC_CACHE_VAL(ac_cv_dnamesize, [
OCFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -I$srcdir"
AC_TRY_RUN([
#include <src/fs.h>

main ()
{
   struct dirent ddd;

   if (sizeof (ddd.d_name) < 12)
	exit (0);
   else
   	exit (1); 
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
AC_MSG_RESULT($ac_cv_dnamesize)
])

dnl
dnl Filesystem information detection
dnl
dnl To get information about the disk, mount points, etc.
dnl

AC_DEFUN(AC_GET_FS_INFO, [
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
    AC_MSG_RESULT($fu_cv_sys_d_ino_in_dirent)
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
	AC_MSG_RESULT($fu_cv_sys_mounted_getmntent2)
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
	AC_MSG_RESULT($fu_cv_sys_mounted_getmntent1)
	if test $fu_cv_sys_mounted_getmntent1 = yes; then
	  list_mounted_fs=found
	  AC_DEFINE(MOUNTED_GETMNTENT1)
	fi
      fi

      if test -z "$list_mounted_fs"; then
	AC_WARN([could not determine how to read list of mounted fs])
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
      AC_MSG_RESULT($fu_cv_sys_mounted_getsstat)
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
      AC_MSG_RESULT($fu_cv_sys_mounted_vmount)
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
      AC_MSG_RESULT($fu_cv_sys_mounted_fread_fstyp)
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
      AC_MSG_RESULT($fu_cv_sys_mounted_getmntinfo)
      if test $fu_cv_sys_mounted_getmntinfo = yes; then
	list_mounted_fs=found
	AC_DEFINE(MOUNTED_GETMNTINFO)
      fi
    fi

    # FIXME: add a test for netbsd-1.1 here

    if test -z "$list_mounted_fs"; then
      # Ultrix
      AC_MSG_CHECKING([for getmnt function])
      AC_CACHE_VAL(fu_cv_sys_mounted_getmnt,
	[AC_TRY_CPP([
#include <sys/fs_types.h>
#include <sys/mount.h>],
		    fu_cv_sys_mounted_getmnt=yes,
		    fu_cv_sys_mounted_getmnt=no)])
      AC_MSG_RESULT($fu_cv_sys_mounted_getmnt)
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
      AC_MSG_RESULT($fu_cv_sys_mounted_fread)
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

    AC_CHECKING(how to get filesystem space usage)
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
      AC_MSG_RESULT($fu_cv_sys_stat_statfs2_bsize)
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
      AC_MSG_RESULT($fu_cv_sys_stat_statfs4)
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
      AC_MSG_RESULT($fu_cv_sys_stat_statfs2_fsize)
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
      AC_MSG_RESULT($fu_cv_sys_stat_fs_data)
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

dnl AC_CHECK_HEADER_IN_PATH(HEADER-FILE, ADDITIONAL_PATH, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])
AC_DEFUN(AC_CHECK_HEADER_IN_PATH,
[dnl Do the transliteration at runtime so arg 1 can be a shell variable.
ac_safe=`echo "$1" | tr './\055' '___'`
AC_MSG_CHECKING([for $1])
AC_CACHE_VAL(ac_cv_header_in_path_$ac_safe,
[AC_TRY_CPP([#include <$1>], ac_header_in_path=yes, [
  ac_header_in_path_found=no
  for ac_header_in_path_value in [$2]; do
    ac_in_path_save_CPPFLAGS=$CPPFLAGS
    CPPFLAGS="$CPPFLAGS -I$ac_header_in_path_value"
    AC_TRY_CPP([#include <$1>], [
ac_header_in_path_found=yes
ac_header_in_path=$ac_header_in_path_value
], )
    CPPFLAGS=$ac_in_path_save_CPPFLAGS
    if test x$ac_header_in_path_found = xyes; then
        break
    fi
  done
  if test $ac_header_in_path_found = xno; then
    ac_header_in_path=no
  fi
])
  eval "ac_cv_header_in_path_$ac_safe=$ac_header_in_path"
])dnl
eval "ac_header_in_path=`echo '$ac_cv_header_in_path_'$ac_safe`"
if test "$ac_header_in_path" = no; then
  AC_MSG_RESULT(no)
ifelse([$4], , , [$4
])dnl
else
  if test -n "$ac_header_in_path"; then
      AC_MSG_RESULT($ac_header_in_path)
  else
      AC_MSG_RESULT(yes)
  fi
  if test x$ac_header_in_path = xyes; then
      ac_header_in_path=
      eval "ac_cv_header_in_path_$ac_safe="
  fi
  ifelse([$3], , , [$3
])dnl
fi
])

dnl Hope I can check for libXpm only in the X11 library directory
AC_DEFUN(AC_LIB_XPM, [
AC_MSG_CHECKING(for -lXpm)
AC_CACHE_VAL(ac_cv_has_xpm, [
    ac_cv_has_xpm=no
    if test x$no_x = xyes; then
	:
    else
        has_xpm_save_LIBS=$LIBS
	LIBS="-lXpm $X_EXTRA_LIBS -lX11 $X_PRE_LIBS $LIBS"
	has_xpm_save_LDFLAGS=$LDFLAGS
	LDFLAGS="$LDFLAGS $X_LIBS"
	has_xpm_save_CFLAGS=$CFLAGS
	CFLAGS="$CFLAGS $X_CFLAGS"
	AC_TRY_LINK([
#include <X11/Xlib.h>
#include <X11/xpm.h>
], [XpmLibraryVersion();], ac_cv_has_xpm=yes)
	CFLAGS="$has_xpm_save_CFLAGS"
	LDFLAGS="$has_xpm_save_LDFLAGS"
	LIBS="$has_xpm_save_LIBS"
    fi
])
AC_MSG_RESULT($ac_cv_has_xpm)
])

dnl Hope I can check for libXext only in the X11 library directory
dnl and shape.h will be in X11/extensions/shape.h
AC_DEFUN(AC_X_SHAPE_EXTENSION, [
AC_MSG_CHECKING(for X11 non-rectangular shape extension)
AC_CACHE_VAL(ac_cv_has_shape, [
    ac_cv_has_shape=no
    if test x$no_x = xyes; then
	:
    else
        has_shape_save_LIBS=$LIBS
	LIBS="-lXext $X_EXTRA_LIBS -lX11 $X_PRE_LIBS $LIBS"
	has_shape_save_LDFLAGS=$LDFLAGS
	LDFLAGS="$LDFLAGS $X_LIBS"
	has_shape_save_CFLAGS=$CFLAGS
	CFLAGS="$CFLAGS $X_CFLAGS"
	AC_TRY_LINK([
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
], [
Display *dpy = (Display *)NULL;
int a, b;
XShapeQueryVersion(dpy,&a,&b);
], ac_cv_has_shape=yes)
	CFLAGS="$has_shape_save_CFLAGS"
	LDFLAGS="$has_shape_save_LDFLAGS"
	LIBS="$has_shape_save_LIBS"
    fi
])
AC_MSG_RESULT($ac_cv_has_shape)
])

dnl AC_TRY_WARNINGS(INCLUDES, FUNCTION-BODY,
dnl             ACTION-IF-NO-WARNINGS [, ACTION-IF-WARNINGS-OR-ERROR])
AC_DEFUN(AC_TRY_WARNINGS,
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

dnl Find if make is GNU make.
AC_DEFUN(AC_PROG_GNU_MAKE,
[AC_MSG_CHECKING(whether we are using GNU make)
set dummy ${MAKE-make}; ac_make=[$]2
AC_CACHE_VAL(ac_cv_prog_gnu_make,
[cat > conftestmake <<\EOF
all:
	@echo ' '
EOF
if ${MAKE-make} --version -f conftestmake 2>/dev/null | grep GNU >/dev/null 2>&1; then
  ac_cv_prog_gnu_make=yes
else
  ac_cv_prog_gnu_make=no
fi
rm -f conftestmake])dnl
if test $ac_cv_prog_gnu_make = yes; then
  AC_MSG_RESULT(yes)
  GNU_MAKE="GNU_MAKE=yes"
else
  AC_MSG_RESULT(no)
  GNU_MAKE= 
fi
AC_SUBST([GNU_MAKE])dnl
])

dnl 
dnl Local additions to Autoconf macros.
dnl Copyright (C) 1992, 1994, 1995 Free Software Foundation, Inc.
dnl François Pinard <pinard@iro.umontreal.ca>, 1992.

dnl ## ----------------------------------------- ##
dnl ## ANSIfy the C compiler whenever possible.  ##
dnl ## ----------------------------------------- ##

dnl @defmac AC_PROG_CC_STDC
dnl @maindex PROG_CC_STDC
dnl @ovindex CC
dnl If the C compiler in not in ANSI C mode by default, try to add an option
dnl to output variable @code{CC} to make it so.  This macro tries various
dnl options that select ANSI C on some system or another.  It considers the
dnl compiler to be in ANSI C mode if it defines @code{__STDC__} to 1 and
dnl handles function prototypes correctly.
dnl 
dnl If you use this macro, you should check after calling it whether the C
dnl compiler has been set to accept ANSI C; if not, the shell variable
dnl @code{ac_cv_prog_cc_stdc} is set to @samp{no}.  If you wrote your source
dnl code in ANSI C, you can make an un-ANSIfied copy of it by using the
dnl program @code{ansi2knr}, which comes with Ghostscript.
dnl @end defmac

dnl Unixware 2.1 defines __STDC__ to 1 only when some useful extensions are
dnl turned off. They are on by default and turned off with the option -Xc. 
dnl The consequence is that __STDC__ is defined but e.g. struct sigaction
dnl is not defined. -- Norbert 

dnl Below all tests but the one for HP-UX are removed. They caused more
dnl problems than they soved, sigh. -- Norbert

AC_DEFUN(fp_PROG_CC_STDC,
[AC_MSG_CHECKING(for ${CC-cc} option to accept ANSI C)
AC_CACHE_VAL(ac_cv_prog_cc_stdc,
[ac_cv_prog_cc_stdc=no
ac_save_CFLAGS="$CFLAGS"
dnl Don't try gcc -ansi; that turns off useful extensions and
dnl breaks some systems' header files.
dnl AIX			-qlanglvl=ansi      (removed -- Norbert)
dnl Ultrix and OSF/1	-std1               (removed -- Norbert)
dnl HP-UX		-Aa -D_HPUX_SOURCE
dnl SVR4		-Xc                 (removed -- Norbert)
for ac_arg in "" "-Aa -D_HPUX_SOURCE" 
do
  CFLAGS="$ac_save_CFLAGS $ac_arg"
  AC_TRY_COMPILE(
[#include <signal.h>
#if !defined(__STDC__) || __STDC__ != 1
choke me
#endif	
], [int test (int i, double x);
struct sigaction sa;
struct s1 {int (*f) (int a);};
struct s2 {int (*f) (double a);};],
[ac_cv_prog_cc_stdc="$ac_arg"; break])
done
CFLAGS="$ac_save_CFLAGS"
])
AC_MSG_RESULT($ac_cv_prog_cc_stdc)
case "x$ac_cv_prog_cc_stdc" in
  x|xno) ;;
  *) CC="$CC $ac_cv_prog_cc_stdc" ;;
esac
])

