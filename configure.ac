dnl
dnl Configure.in file for the Midnight Commander
dnl

AC_INIT([GNU Midnight Commander], [], [mc-devel@gnome.org])
AC_PREREQ(2.52)
AC_CONFIG_SRCDIR(src/main.c)
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE(mc, 4.6.1-pre1a)

AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_CANONICAL_HOST

AC_AIX
AC_MINIX
AC_ISC_POSIX

AC_PROG_CC_STDC

dnl
dnl First try glib 2.x.  If it's not found, use glib 1.2.x.
dnl Keep this check close to the beginning, so that the users
dnl without any glib won't have their time wasted by other checks.
dnl

AC_ARG_WITH(glib12, 
	[  --with-glib12            Force using glib 1.2.x [[no]]])

AC_ARG_WITH([glib_static],
	    [  --with-glib-static       Link glib statically [[no]]])

glib_found=no
if test "x$with_glib12" != "xyes"; then
	PKG_CHECK_MODULES(GLIB, [glib-2.0], [glib_found=yes], [:])
fi

dnl Fall back to glib-1.2, don't use pkgconfig to find it.
if test "x$glib_found" != "xyes" ; then
	dnl This temporary variable is a workaround for a bug in Autoconf-2.53
	glib_path=$PATH:/usr/local/bin

	dnl Klugde for FreeBSD, where glib-config is renamed to glib12-config.
	AC_PATH_PROGS([GLIB_CONFIG], [glib-config glib12-config],,[$glib_path])

	AC_ARG_VAR([GLIB_CONFIG], [Path to glib-config (version 1.2.x only)])
	AM_PATH_GLIB(1.2.6, , [AC_MSG_ERROR([Test for glib failed.
GNU Midnight Commander requires glib 1.2.6 or above.])])
	dnl Save GLIB_CFLAGS and GLIB_LIBS, since the following call to
	dnl AM_PATH_GLIB will overwrite them.
	save_GLIB_CFLAGS="$GLIB_CFLAGS"
	save_GLIB_LIBS="$GLIB_LIBS"
	dnl Check for gmodule.  Store the flags necessary to compile and
	dnl link programs using gmodule functionality in GMODULE_CFLAGS
	dnl and GMODULE_LIBS.
	AM_PATH_GLIB(1.2.6, [gmodule_found=yes], , [gmodule])
	GMODULE_CFLAGS="$GLIB_CFLAGS"
	GMODULE_LIBS="$GLIB_LIBS"
	GLIB_CFLAGS="$save_GLIB_CFLAGS"
	GLIB_LIBS="$save_GLIB_LIBS"
	GLIB_LIBDIR="`$GLIB_CONFIG --exec-prefix`/lib"

	dnl Used in src/glibcompat.c
	AC_CHECK_FUNCS([strlcpy])
else
	PKG_CHECK_MODULES(GMODULE, [gmodule-2.0], [gmodule_found=yes])
	GLIB_LIBDIR="`$PKG_CONFIG --variable=libdir glib-2.0`"
fi

if test "x$gmodule_found" = "xyes" ; then
	dnl Check if the gmodule functionality supported on this system.
	AC_G_MODULE_SUPPORTED
fi

AC_HEADER_MAJOR
AC_C_CONST
AC_SYS_LARGEFILE

AC_PROG_LN_S
AC_CHECK_TOOL(AR, ar, ar)

dnl Only list browsers here that can be run in background (i.e. with `&')
AC_CHECK_PROGS(X11_WWW, [gnome-moz-remote mozilla konqueror opera netscape])

dnl
dnl Ovverriding mmap support.  This has to be before AC_FUNC_MMAP is used.
dnl We use only part of the functionality of mmap, so on AIX,
dnl it's possible to use mmap, even if it doesn't pass the autoconf test.
dnl
AC_ARG_WITH(mmap,
	[  --with-mmap              Use the mmap call [[yes if found]]])
if test x$with_mmap != xno; then
    if test x$with_mmap = x; then
	AC_FUNC_MMAP
    else
	AC_DEFINE(HAVE_MMAP, 1)
    fi
fi

dnl
dnl Internationalization
dnl
MC_ASM_LABELS
AM_GNU_GETTEXT
AM_GNU_GETTEXT_VERSION(0.11.5)

if test "x$USE_INCLUDED_LIBINTL" = xyes; then
    CPPFLAGS="$CPPFLAGS -I\$(top_builddir)/intl -I\$(top_srcdir)/intl"
fi

dnl Determine which help translations we want to install.
ALL_DOC_LINGUAS="es hu it pl ru sr"

DOC_LINGUAS=
if test "x$USE_NLS" = xyes; then
    if test -z "$LINGUAS"; then
	langs="`grep -v '^#' $srcdir/po/LINGUAS`"
    else
	langs="$LINGUAS"
    fi
else
    langs=
fi

for h_lang in $ALL_DOC_LINGUAS; do
    for lang in $langs; do
	if test "$lang" = "$h_lang"; then
	    DOC_LINGUAS="$DOC_LINGUAS $lang"
	    break
	fi
    done
done
AC_SUBST(DOC_LINGUAS)

dnl
dnl OS specific flags.
dnl
case $host_os in
aux*)
    # A/UX
    LIBS="$LIBS -lposix"
    AC_DEFINE(_POSIX_SOURCE)
    ;;
esac


AC_PROG_INSTALL
AC_CHECK_HEADERS([unistd.h string.h memory.h grp.h limits.h malloc.h \
	stdlib.h termios.h utime.h fcntl.h pwd.h sys/statfs.h sys/time.h \
	sys/timeb.h sys/select.h sys/ioctl.h stropts.h arpa/inet.h \
	security/pam_misc.h])

AC_HEADER_TIME
AC_HEADER_SYS_WAIT
AC_HEADER_DIRENT
AC_HEADER_STDC

dnl Missing structure components
AC_CHECK_MEMBERS([struct stat.st_blksize, struct stat.st_rdev])
AC_STRUCT_ST_BLOCKS

dnl
dnl Check availability of some functions 
dnl 

AC_CHECK_FUNCS([atoll cfgetospeed getsid initgroups memcpy memmove memset \
		putenv setreuid setuid statfs strerror strftime \
		sysconf tcgetattr tcsetattr truncate])

dnl S-Lang needs all four functions to be defined to use POSIX signal API
AC_CHECK_FUNCS([sigaction sigemptyset sigprocmask sigaddset], , [slang_signals=no])
if test x$slang_signals != xno; then
    AC_DEFINE(SLANG_POSIX_SIGNALS, 1, [Define to use POSIX signal API in S-Lang])
fi

dnl
dnl getpt is a GNU Extension (glibc 2.1.x)
dnl
AC_CHECK_FUNCS(posix_openpt, , [AC_CHECK_FUNCS(getpt)])
AC_CHECK_FUNCS(grantpt, , [AC_CHECK_LIB(pt, grantpt)])

dnl replacing lstat with statlstat on sco makes it more portable between
dnl sco clones
AC_CHECK_FUNCS(statlstat)

dnl
dnl If running under AIX, AC_AIX does not tell us that
dnl
AC_MSG_CHECKING([for AIX defines])
AC_EGREP_CPP(yes,
[#if defined(AIX) || defined(_AIX) || defined(__aix__) || defined(aix)
yes
#endif
], [
AC_DEFINE(IS_AIX, 1, [Define if compiling for AIX])
AC_MSG_RESULT(yes)
], [AC_MSG_RESULT(no)])

dnl
dnl This is from GNU fileutils, check aclocal.m4 for more information
dnl
AC_GET_FS_INFO

dnl
dnl Missing typedefs and replacements
dnl 

AC_TYPE_MODE_T
AC_CHECK_TYPE(umode_t, int)
AC_CHECK_TYPE(off_t, long)
AC_TYPE_PID_T
AC_TYPE_UID_T
AC_CHECK_TYPE(nlink_t, unsigned int)

dnl This is needed for regex.c only
AC_CHECK_TYPE(uintptr_t,
	      [AC_DEFINE(HAVE_UINTPTR_T, 1,
			 [Define if you have the `uintptr_t' type.])
])

AC_FUNC_ALLOCA
AC_FUNC_STRCOLL

dnl
dnl X11 support.
dnl Used to read keyboard modifiers when running under X11.
dnl

AC_PATH_XTRA
if test "x$no_x" = xyes; then
    textmode_x11_support="no"
else
    CPPFLAGS="$CPPFLAGS $X_CFLAGS"
    if test "x$mc_cv_g_module_supported" = "xyes" ; then
	dnl Replace the contents of GLIB_CFLAGS and GLIB_LIBS with those of
	dnl GMODULE_CFLAGS and GMODULE_LIBS, only if X is available and gmodule
	dnl functionality is supported on the system.  This way, mc will be
	dnl linked against the gmodule library only when it's really required.
	GLIB_CFLAGS="$GMODULE_CFLAGS"
	GLIB_LIBS="$GMODULE_LIBS"
    else
        MCLIBS="$X_LIBS $X_PRE_LIBS -lX11 $X_EXTRA_LIBS"
    fi
    AC_DEFINE(HAVE_TEXTMODE_X11_SUPPORT, 1,
	      [Define to enable getting events from X Window System])
    textmode_x11_support="yes"
fi

dnl
dnl Try to find static libraries for glib and gmodule.
dnl
if test x$with_glib_static = xyes; then
    new_GLIB_LIBS=
    for i in $GLIB_LIBS; do
	case x$i in
	x-lglib*)
	    lib=glib ;;
	x-lgmodule*)
	    lib=gmodule ;;
	*)
	    lib=
	    add="$i" ;;
	esac

	if test -n "$lib"; then
	    lib1=`echo $i | sed 's/^-l//'`
	    if test -f "$GLIB_LIBDIR/lib${lib1}.a"; then
		add="$GLIB_LIBDIR/lib${lib1}.a"
	    else
		if test -f "$GLIB_LIBDIR/lib${lib}.a"; then
		    add="$GLIB_LIBDIR/lib${lib}.a"
		else
		    AC_MSG_ERROR([Cannot find static $lib])
		fi
	    fi
	fi
	new_GLIB_LIBS="$new_GLIB_LIBS $add"
    done
    GLIB_LIBS="$new_GLIB_LIBS"
fi


dnl
dnl Network related functions
dnl

AC_CHECK_LIB(nsl, t_accept)
AC_CHECK_LIB(socket, socket)

have_socket=no
AC_CHECK_FUNCS(socket, have_socket=yes)
if test $have_socket = no; then
  # socket is not in the default libraries.  See if it's in some other.
  for lib in bsd socket inet; do
    AC_CHECK_LIB($lib, socket, [
        LIBS="$LIBS -l$lib"
	have_socket=yes
	AC_DEFINE(HAVE_SOCKET)
	break])
  done
fi

have_gethostbyname=no
AC_CHECK_FUNC(gethostbyname, [have_gethostbyname=yes])
if test $have_gethostbyname = no; then
  # gethostbyname is not in the default libraries.  See if it's in some other.
  for lib in bsd socket inet; do
    AC_CHECK_LIB([$lib], [gethostbyname],
		 [LIBS="$LIBS -l$lib"; have_gethostbyname=yes; break])
  done
fi

dnl
dnl Sequent wants getprocessstats
dnl
AC_CHECK_LIB(seq, get_process_stats, [
	LIBS="$LIBS -lseq"
	AC_DEFINE(HAVE_GET_PROCESS_STATS, 1,
		  [Define if you have function `get_process_stats' and
have to use that instead of gettimeofday])])

MC_VFS_CHECKS

vfs_type="normal"
if test x$use_vfs = xyes; then
	AC_MSG_NOTICE([enabling VFS code])
	vfs_type="Midnight Commander Virtual File System"
fi

dnl
dnl Check for gpm mouse support (Linux only)
dnl
mouse_lib="xterm only"
AC_ARG_WITH(gpm-mouse, 
	[  --with-gpm-mouse         Compile with gpm mouse support (Linux only)
                           [[yes if found]]])

case $host_os in
linux*)
    if test x$with_gpm_mouse != xno; then
	AC_CHECK_LIB(gpm, Gpm_Repeat,
	    [AC_DEFINE(HAVE_LIBGPM, 1,
		       [Define to enable gpm mouse support on Linux])
	    mouse_lib="gpm and xterm"
	    MCLIBS="$MCLIBS -lgpm"],
	    [AC_MSG_WARN([libgpm is missing or older than 0.18])
	])
    fi
    ;;
esac

dnl
dnl Check nroff and the options it supports
dnl
AC_CHECK_PROG(HAVE_nroff, nroff, true, false)

dnl Default values
MANDOC=-man
MAN_FLAGS=

if $HAVE_nroff; then
    AC_MSG_CHECKING([for manual formatting macros])
    AC_CACHE_VAL(mc_cv_mandoc, [
    nroff -mandoc < /dev/null > /dev/null 2>&1
    if test $? = 0; then
	mc_cv_mandoc=-mandoc
    else
	mc_cv_mandoc=-man
    fi
    ])
    MANDOC=$mc_cv_mandoc
    AC_MSG_RESULT([$MANDOC])

    AC_MSG_CHECKING([for option to disable ANSI color in manuals])
    AC_CACHE_VAL(mc_cv_man_nocolor, [
    nroff -c < /dev/null > /dev/null 2>&1
    if test $? = 0; then
	mc_cv_man_nocolor=-c
    else
	mc_cv_man_nocolor=
    fi
    ])
    MAN_FLAGS=$mc_cv_man_nocolor
    AC_MSG_RESULT([${MAN_NOCOLOR-none}])

    AC_MSG_CHECKING([if nroff accepts -Tlatin1 or -Tascii])
    AC_CACHE_VAL(mc_cv_nroff_tascii, [
    mc_cv_nroff_tascii=
    nroff -Tlatin1 < /dev/null > /dev/null 2>&1 /dev/null
    if test $? = 0; then
	mc_cv_nroff_tascii=-Tlatin1
    else
        nroff -Tascii < /dev/null > /dev/null 2>&1 /dev/null
        if test $? = 0; then
	    mc_cv_nroff_tascii=-Tascii
	fi
    fi
    ])
    AC_MSG_RESULT([${mc_cv_nroff_tascii-no}])
    MAN_FLAGS="$MAN_FLAGS $mc_cv_nroff_tascii"
fi

AC_SUBST(MANDOC)
AC_SUBST(MAN_FLAGS)


dnl
dnl Check for -L option to file
dnl
AC_CHECK_PROG(HAVE_FILECMD, file, true, false)
if $HAVE_FILECMD; then
    AC_MSG_CHECKING([for -L option to file command])
    AC_CACHE_VAL(mc_cv_filel, [
    file -L . > /dev/null 2>&1
    if test $? = 0; then
	mc_cv_filel=yes
    else
	mc_cv_filel=no
    fi
    ])
    if test x$mc_cv_filel = xyes; then
	AC_DEFINE(FILE_L, 1, [Define if the file command accepts the -L option])
    fi
    filel=$mc_cv_filel
    AC_MSG_RESULT([$filel])
fi


AC_MSG_CHECKING([for subshell support])
AC_ARG_WITH(subshell,
	[  --with-subshell          Compile in concurrent subshell [[yes]]
  --with-subshell=optional Don't run concurrent shell by default [[no]]],
	[result=no
	if test x$withval = xoptional
	then
		AC_DEFINE(SUBSHELL_OPTIONAL, 1,
			  [Define to make subshell support optional])
		result="optional"
	fi
	if test x$withval = xyes
        then 
		result="yes"
	fi],
	[dnl Default: enable the subshell support
	result="yes"
])
if test "x$result" != xno; then
	AC_DEFINE(HAVE_SUBSHELL_SUPPORT, 1,
		  [Define to enable subshell support])
fi
AC_MSG_RESULT([$result])
subshell="$result"


dnl
dnl Select the screen library.  mcslang is the included S-Lang library.
dnl
AC_ARG_WITH(screen,
	[  --with-screen=LIB        Compile with screen library: slang, mcslang or
                           ncurses [[slang if found, else mcslang]]])

case x$with_screen in
xslang)
	MC_WITH_SLANG(strict)
	;;
xmcslang)
	MC_WITH_MCSLANG
	;;
xncurses)
	MC_WITH_NCURSES
	;;
x)
	MC_WITH_SLANG
	;;
*)
	AC_MSG_ERROR([Value of the screen library is incorrect])
	;;
esac


dnl
dnl Force using termcap.  This option is processed in MC_WITH_MCSLANG.
dnl Report an error if this option is not applicable.
dnl
AC_ARG_WITH(termcap,
	[  --with-termcap           Try using termcap database [[only if no terminfo]]],
	[if test x$with_screen != xmcslang; then
		AC_MSG_ERROR([Option `--with-termcap' only works with `--with-screen=mcslang'])
	fi
])


dnl
dnl Internal editor support.
dnl
AC_ARG_WITH(edit,
        [  --with-edit              Enable internal editor [[yes]]])

if test x$with_edit != xno; then
	AC_DEFINE(USE_INTERNAL_EDIT, 1, [Define to enable internal editor])
	use_edit=yes
	edit_msg="yes"
	AC_MSG_NOTICE([using internal editor])
else
	edit_msg="no"
fi


dnl Check if the OS is supported by the console saver.
cons_saver=""
case $host_os in
linux*)
    cons_saver=yes
esac


dnl Support for background operations
AC_ARG_ENABLE([background],
	      [  --enable-background      Support for background file operations [[yes]]])
if test "x$enable_background" != xno; then
    AC_DEFINE(WITH_BACKGROUND, 1, [Define to enable background file operations])
fi


dnl
dnl User visible support for charset conversion.
dnl
AC_ARG_ENABLE([charset],
	      [  --enable-charset         Support for charset selection and conversion [[no]]])
have_charset=
if test "x$enable_charset" = xyes; then
  if test "x$am_cv_func_iconv" != xyes; then
    AC_MSG_WARN([Cannot enable charset support because iconv function is missing])
  else
    AC_DEFINE(HAVE_CHARSET, 1,
	      [Define to enable charset selection and conversion])
    have_charset=yes
  fi
fi

if test "$GLIBC21" != yes; then
    AC_DEFINE(USE_INCLUDED_REGEX, 1, [Use the regex included here])
fi

if test x"$USE_MAINTAINER_MODE" = x"yes"; then
    if test x"${enable_gcc_warnings+set}" != x"set"; then
        CFLAGS="-Wall $CFLAGS"
    fi
fi

ri_GCC_WARNINGS

AC_SUBST(CFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(LIBS)

dnl Libraries used only when building the mc binary
AC_SUBST(MCLIBS)

dnl Version for the RedHat package, without dashes
RPM_VERSION=`echo $VERSION | sed 's/-//g'`
AC_SUBST(RPM_VERSION)

if test -n "$use_smbfs"; then
  AC_CONFIG_SUBDIRS([vfs/samba])
fi

AM_CONDITIONAL(USE_EDIT, [test -n "$use_edit"])
AM_CONDITIONAL(USE_VFS, [test "x$use_vfs" = xyes])
AM_CONDITIONAL(USE_VFS_NET, [test x"$use_net_code" = xtrue])
AM_CONDITIONAL(USE_UNDEL_FS, [test -n "$use_undelfs"])
AM_CONDITIONAL(USE_SAMBA_FS, [test -n "$use_smbfs"])
AM_CONDITIONAL(USE_MCFS, [test -n "$use_mcfs"])
AM_CONDITIONAL(INCLUDED_SLANG, [test "x$with_screen" = xmcslang])
AM_CONDITIONAL(CHARSET, [test -n "$have_charset"])
AM_CONDITIONAL(CONS_SAVER, [test -n "$cons_saver"])

AC_CONFIG_FILES([
Makefile 
mc.spec
doc/Makefile 
vfs/Makefile
vfs/extfs/Makefile
lib/Makefile
src/Makefile
slang/Makefile 
edit/Makefile 
syntax/Makefile
m4/Makefile

lib/mc.ext

vfs/extfs/a
vfs/extfs/apt
vfs/extfs/audio
vfs/extfs/deb
vfs/extfs/deba
vfs/extfs/debd
vfs/extfs/dpkg
vfs/extfs/iso9660
vfs/extfs/hp48
vfs/extfs/lslR
vfs/extfs/mailfs
vfs/extfs/patchfs
vfs/extfs/rpms
vfs/extfs/uar
vfs/extfs/uarj
vfs/extfs/uha
vfs/extfs/ulha 
vfs/extfs/urar
vfs/extfs/uzip
vfs/extfs/uzoo

doc/mc.1 doc/mcedit.1 doc/mcview.1 doc/mcserv.8
doc/es/mc.1 doc/es/Makefile
doc/hu/mc.1 doc/hu/Makefile
doc/it/mc.1 doc/it/Makefile
doc/pl/mc.1 doc/pl/Makefile
doc/ru/mc.1 doc/ru/Makefile
doc/sr/mc.1 doc/sr/mcserv.8 doc/sr/Makefile

intl/Makefile
po/Makefile.in
])

AC_OUTPUT

echo "
Configuration:

  Source code location:       ${srcdir}
  Compiler:                   ${CC}
  Compiler flags:             ${CFLAGS}
  File system:                ${vfs_type}
                              ${vfs_flags}
  Screen library:             ${screen_msg}
  Mouse support:              ${mouse_lib}
  X11 events support:         ${textmode_x11_support}
  With subshell support:      ${subshell}
  Internal editor:            ${edit_msg}
"
