dnl
dnl Configure.in file for the Midnight Commander
dnl

AC_PREREQ(2.60)
AC_INIT([GNU Midnight Commander], [], [mc-devel@gnome.org])
m4_pattern_forbid(MC_)
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_SRCDIR(src/main.c)
AC_CONFIG_HEADERS(config.h)
MC_VERSION
AM_INIT_AUTOMAKE(mc, ${VERSION} )
dnl Enable silent rules by default (if yes)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AM_MAINTAINER_MODE

AC_CANONICAL_HOST

AC_USE_SYSTEM_EXTENSIONS


dnl ############################################################################
dnl Check for compiler
dnl ############################################################################

AC_PROG_CC_STDC
AM_PROG_CC_C_O

MC_CHECK_CFLAGS

CFLAGS_OPTS=""
if test "x$CFLAGS" = "x"; then
    CFLAGS_OPTS=" -O2 "
fi
if test x$USE_MAINTAINER_MODE = xyes; then
    CFLAGS_OPTS="-g3 -O -ggdb"
    AC_DEFINE(USE_MAINTAINER_MODE, 1, [Use maintainer mode])
fi

AC_ARG_ENABLE([werror],
    AS_HELP_STRING([--enable-werror], [Handle all compiler warnings as errors]))
if test "x$enable_werror" = xyes; then
    MC_CHECK_ONE_CFLAG([-Werror])
fi

AC_PROG_LIBTOOL


dnl ############################################################################
dnl Check for programs
dnl ############################################################################

PKG_PROG_PKG_CONFIG
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PATH_PROG([PERL], [perl], [/usr/bin/perl])
AC_PATH_PROG([PYTHON], [python], [/usr/bin/python])
AC_PATH_PROG([RUBY], [ruby], [/usr/bin/ruby])

dnl Check nroff and the options it supports
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

dnl Check for -L option to file
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

dnl Only list browsers here that can be run in background (i.e. with `&')
AC_CHECK_PROGS(X11_WWW, [gnome-moz-remote mozilla firefox konqueror opera])


dnl ############################################################################
dnl Check for other tools
dnl ############################################################################

AC_CHECK_TOOL(AR, ar, ar)
MC_UNIT_TESTS


dnl ############################################################################
dnl Check for main libraies
dnl ############################################################################

AC_CHECK_GLIB
AC_G_MODULE_SUPPORTED
MC_WITH_SCREEN
MC_CHECK_SEARCH_TYPE
dnl X11 support. Used to read keyboard modifiers when running under X11.
MC_WITH_X


dnl ############################################################################
dnl Check for header files
dnl ############################################################################

AC_CHECK_HEADERS([unistd.h string.h memory.h limits.h malloc.h \
	utime.h fcntl.h sys/statfs.h sys/vfs.h sys/time.h \
	sys/select.h sys/ioctl.h stropts.h arpa/inet.h \
	sys/socket.h sys/sysmacros.h sys/types.h sys/mkdev.h])
AC_HEADER_MAJOR
AC_HEADER_TIME
AC_HEADER_DIRENT
AC_HEADER_ASSERT


dnl ############################################################################
dnl Check for types
dnl ############################################################################

AC_C_CONST
dnl Check largefile before type sizeof checks
AC_SYS_LARGEFILE

AC_CHECK_SIZEOF(long)
AC_TYPE_UINTMAX_T
AC_CHECK_SIZEOF(uintmax_t)
AC_TYPE_OFF_T
AC_CHECK_SIZEOF(off_t)
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_UID_T

AC_STRUCT_ST_BLOCKS
AC_CHECK_MEMBERS([struct stat.st_blksize, struct stat.st_rdev])

AH_TEMPLATE([sig_atomic_t],
            [/* Define to `int' if <signal.h> doesn't define.])
AH_TEMPLATE([SIG_ATOMIC_VOLATILE_T],
            [Some systems declare sig_atomic_t as volatile, some others -- no.
             This define will have value `sig_atomic_t' or
             `volatile sig_atomic_t' accordingly.])

AC_MSG_CHECKING(for sig_atomic_t in signal.h)
AC_EGREP_HEADER(sig_atomic_t,signal.h,
  [
    ac_cv_type_sig_atomic_t=yes;
    AC_EGREP_HEADER(volatile.*sig_atomic_t,
                    signal.h,
                    [
                        is_sig_atomic_t_volatile=yes;
                        AC_MSG_RESULT([yes, volatile])
                    ],
                    [
                        is_sig_atomic_t_volatile=no;
                        AC_MSG_RESULT([yes, non volatile])
                    ])
  ],
  [
    AC_MSG_RESULT(no)
    AC_CHECK_TYPE(sig_atomic_t, int)
    is_sig_atomic_t_volatile=no
  ])
if test $is_sig_atomic_t_volatile = 'yes'
then
    AC_DEFINE(SIG_ATOMIC_VOLATILE_T, sig_atomic_t)
else
    AC_DEFINE(SIG_ATOMIC_VOLATILE_T, [volatile sig_atomic_t])
fi


dnl ############################################################################
dnl Check for functions
dnl ############################################################################

AC_CHECK_FUNCS([\
	isascii \
	statfs sysconf \
	tcgetattr tcsetattr \
	ftruncate \
	strverscmp \
	strncasecmp \
	realpath
])
AC_FUNC_STRCOLL

dnl getpt is a GNU Extension (glibc 2.1.x)
AC_CHECK_FUNCS(posix_openpt, , [AC_CHECK_FUNCS(getpt)])
AC_CHECK_FUNCS(grantpt, , [AC_CHECK_LIB(pt, grantpt)])

dnl replacing lstat with statlstat on sco makes it more portable between
dnl sco clones
AC_CHECK_FUNCS(statlstat)

dnl Ovverriding mmap support.  This has to be before AC_FUNC_MMAP is used.
dnl We use only part of the functionality of mmap, so on AIX,
dnl it's possible to use mmap, even if it doesn't pass the autoconf test.
AC_ARG_WITH([mmap],
	AS_HELP_STRING([--with-mmap], [Use the mmap call @<:@yes if found@:>@]))
if test x$with_mmap != xno; then
    if test x$with_mmap = x; then
	AC_FUNC_MMAP
    else
	AC_DEFINE(HAVE_MMAP, 1)
    fi
fi

dnl Sequent wants getprocessstats
AC_CHECK_LIB(seq, get_process_stats, [
	LIBS="$LIBS -lseq"
	AC_DEFINE(HAVE_GET_PROCESS_STATS, 1,
		  [Define if you have function `get_process_stats' and
have to use that instead of gettimeofday])])

mc_AC_GET_FS_INFO


dnl ############################################################################
dnl Internationalization
dnl ############################################################################

AM_GNU_GETTEXT(no-libtool, need-ngettext)
AM_GNU_GETTEXT_VERSION(0.14.3)

if test "x$USE_INCLUDED_LIBINTL" = xyes; then
    CPPFLAGS="$CPPFLAGS -I\$(top_builddir)/intl -I\$(top_srcdir)/intl"
fi

dnl User visible support for charset conversion.
AC_ARG_ENABLE([charset],
    AS_HELP_STRING([--enable-charset], [Support for charset selection and conversion @<:@yes@:>@]))
have_charset=
charset_msg="no"
if test "x$enable_charset" != "xno"; then
    AC_DEFINE(HAVE_CHARSET, 1, [Define to enable charset selection and conversion])
    have_charset=yes
    charset_msg="yes"
fi


dnl ############################################################################
dnl OS specific flags
dnl ############################################################################

AC_ISC_POSIX

case $host_os in
aux*)
    # A/UX
    LIBS="$LIBS -lposix"
    AC_DEFINE(_POSIX_SOURCE)
    ;;
esac

dnl If running under AIX, AC_AIX does not tell us that
AC_MSG_CHECKING([for AIX defines])
AC_EGREP_CPP([yes],
    [
        #if defined(AIX) || defined(_AIX) || defined(__aix__) || defined(aix)
        yes
        #endif
    ],
    [
        AC_DEFINE(IS_AIX, 1, [Define if compiling for AIX])
        AC_MSG_RESULT(yes)
    ],
    [
        AC_MSG_RESULT(no)
    ])

dnl Check if the OS is supported by the console saver.
cons_saver=""
case $host_os in
linux*)
    cons_saver=yes
esac

dnl Check for gpm mouse support (Linux only)
mouse_lib="xterm only"
AC_ARG_WITH([gpm-mouse],
    AS_HELP_STRING([--with-gpm-mouse], [Compile with gpm mouse support (Linux only) @<:@yes if found@:>@]))

case $host_os in
linux*)
    if test x$with_gpm_mouse != xno; then
	AC_CHECK_LIB(gpm, Gpm_Repeat,
	    [AC_DEFINE(HAVE_LIBGPM, 1,
		       [Define to enable gpm mouse support on Linux])
	    mouse_lib="gpm and xterm"
	    MCLIBS="$MCLIBS -lgpm"],
            if test "x$with_gpm_mouse" = "xyes"; then
                [AC_MSG_ERROR([libgpm is missing or older than 0.18])]
            else
                [AC_MSG_WARN([libgpm is missing or older than 0.18])]
            fi
	)
    fi
    ;;
esac


dnl ############################################################################
dnl libmc
dnl ############################################################################

LIBMC_VERSION="0.0.1"
LIBMC_RELEASE="1"
AC_SUBST(LIBMC_VERSION)
AC_SUBST(LIBMC_RELEASE)

AC_ARG_ENABLE([mclib],
        [AS_HELP_STRING([--enable-mclib], [Compile shared library libmc.so @<:@no@:>@])],
        [
            if test "x$enableval" = "xno" ; then
                enable_mclib=no
            else
                if test "x$enable_shared" = "xno" ; then
                    AC_MSG_WARN([Build of shared library is disabled. Specify --enable-shared first])
                    enable_mclib=no
                else
                    enable_mclib=yes
                fi
            fi
        ],
        [enable_mclib=no])

AM_CONDITIONAL([ENABLE_MCLIB], [test x$enable_mclib = xyes])


dnl ############################################################################
dnl Where config files should be placed
dnl ############################################################################

AC_ARG_WITH([homedir],
    AS_HELP_STRING([--with-homedir],
                   [Choose any place of user settings relative to home dir, or XDG for respect XDG standards @<:@XDG@:>@]),
    [
	if test "x$withval" = "xXDG"; then
	    with_homedir=XDG
	else
	    with_homedir=$withval
	fi
    ],
    [with_homedir=XDG])


if test x$with_homedir = xXDG; then
    AC_DEFINE(MC_HOMEDIR_XDG, 1,
	[Define to enable XDG standard support])
else
    AC_DEFINE_UNQUOTED([MC_USERCONF_DIR], ["$with_homedir"], [Where configs will be placed relative to $HOME])
fi


dnl ############################################################################
dnl MC options
dnl ############################################################################

MC_WITH_EDIT

dnl Diff viewer support.
AC_ARG_WITH([diff_viewer],
    AS_HELP_STRING([--with-diff-viewer], [Compile with diff viewer @<:@yes@:>@]))

if test x$with_diff_viewer != xno; then
	AC_DEFINE(USE_DIFF_VIEW, 1, [Define to enable diff viewer])
	use_diff=yes
	diff_msg="yes"
	AC_MSG_NOTICE([using diff viewer])
else
	diff_msg="no"
fi

MC_SUBSHELL
MC_BACKGROUND
AC_MC_VFS_CHECKS

dnl ############################################################################
dnl Directories
dnl ############################################################################

dnl ${prefix} and ${exec_prefix} are undefined here if --prefix is not used in command line
dnl Let define ${prefix} and ${exec_prefix}
test "x$prefix" = "xNONE" && prefix="$ac_default_prefix"
test "x$exec_prefix" = "xNONE" && exec_prefix="${prefix}"

if test x${libexecdir} = x'${exec_prefix}/libexec'; then
    EXTHELPERSDIR=${prefix}/libexec/${PACKAGE}/ext.d
elif test x${libexecdir} = x'${exec_prefix}/lib'; then
    EXTHELPERSDIR=${prefix}/lib/${PACKAGE}/ext.d
else
    EXTHELPERSDIR=${libexecdir}/${PACKAGE}/ext.d
fi
AC_SUBST(EXTHELPERSDIR)


dnl ############################################################################
dnl Documentation
dnl ############################################################################

MAN_DATE="$(LC_ALL=C date "+%B %Y")"
AC_SUBST(MAN_DATE)

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

DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN(mc,doxygen.cfg,devel)


dnl ############################################################################
dnl Configure results
dnl ############################################################################

CFLAGS="$mc_configured_cflags $CFLAGS_OPTS $CFLAGS"

AC_SUBST(MCLIBS)
AC_SUBST(CFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(LIBS)

AM_CONDITIONAL(USE_NLS, [test x"$USE_NLS" = xyes])
AM_CONDITIONAL(USE_MAINTAINER_MODE, [test x"$USE_MAINTAINER_MODE" = xyes])
AM_CONDITIONAL(USE_SCREEN_SLANG, [test x"$with_screen" = xslang])
AM_CONDITIONAL(USE_EDIT, [test x"$use_edit" = xyes ])
AM_CONDITIONAL(USE_ASPELL, [test x"$enable_aspell" = xyes ])
AM_CONDITIONAL(USE_DIFF, [test -n "$use_diff"])
AM_CONDITIONAL(CHARSET, [test -n "$have_charset"])
AM_CONDITIONAL(CONS_SAVER, [test -n "$cons_saver"])

AC_DEFINE_UNQUOTED([MC_CONFIGURE_ARGS], ["$ac_configure_args"], [MC configure arguments])

AC_CONFIG_FILES(
[
src/man2hlp/man2hlp
],
[
chmod +x src/man2hlp/man2hlp
])

AC_CONFIG_FILES([
Makefile

contrib/Makefile
contrib/dist/Makefile
contrib/dist/gentoo/Makefile
contrib/dist/redhat/Makefile
contrib/dist/redhat/mc.spec
contrib/dist/pkginfo
contrib/dist/prototype

misc/Makefile
misc/skins/Makefile
misc/ext.d/Makefile
misc/ext.d/doc.sh
misc/ext.d/misc.sh
misc/ext.d/text.sh
misc/ext.d/web.sh
misc/macros.d/Makefile
misc/mc.ext

src/Makefile
src/consaver/Makefile
src/editor/Makefile
src/man2hlp/Makefile
src/viewer/Makefile
src/diffviewer/Makefile
src/filemanager/Makefile

src/vfs/Makefile

src/vfs/cpio/Makefile

src/vfs/extfs/Makefile
src/vfs/extfs/helpers/Makefile
src/vfs/extfs/helpers/a+
src/vfs/extfs/helpers/apt+
src/vfs/extfs/helpers/audio
src/vfs/extfs/helpers/deb
src/vfs/extfs/helpers/deba
src/vfs/extfs/helpers/debd
src/vfs/extfs/helpers/dpkg+
src/vfs/extfs/helpers/iso9660
src/vfs/extfs/helpers/hp48+
src/vfs/extfs/helpers/lslR
src/vfs/extfs/helpers/mailfs
src/vfs/extfs/helpers/patchfs
src/vfs/extfs/helpers/rpms+
src/vfs/extfs/helpers/s3+
src/vfs/extfs/helpers/uace
src/vfs/extfs/helpers/ualz
src/vfs/extfs/helpers/uar
src/vfs/extfs/helpers/uarc
src/vfs/extfs/helpers/uarj
src/vfs/extfs/helpers/uc1541
src/vfs/extfs/helpers/ucab
src/vfs/extfs/helpers/uha
src/vfs/extfs/helpers/ulha
src/vfs/extfs/helpers/ulib
src/vfs/extfs/helpers/urar
src/vfs/extfs/helpers/uzip
src/vfs/extfs/helpers/uzoo

src/vfs/fish/Makefile
src/vfs/fish/helpers/Makefile

src/vfs/ftpfs/Makefile

src/vfs/sftpfs/Makefile

src/vfs/local/Makefile

src/vfs/sfs/Makefile

src/vfs/smbfs/Makefile

src/vfs/tar/Makefile

src/vfs/undelfs/Makefile

lib/Makefile
lib/event/Makefile
lib/filehighlight/Makefile
lib/mcconfig/Makefile
lib/search/Makefile
lib/skin/Makefile
lib/strutil/Makefile
lib/tty/Makefile

lib/vfs/Makefile

lib/widget/Makefile

misc/syntax/Makefile
misc/syntax/Syntax

doc/Makefile
doc/hints/Makefile
doc/man/Makefile
doc/man/es/Makefile
doc/man/hu/Makefile
doc/man/it/Makefile
doc/man/pl/Makefile
doc/man/ru/Makefile
doc/man/sr/Makefile

doc/hlp/Makefile
doc/hlp/es/Makefile
doc/hlp/hu/Makefile
doc/hlp/it/Makefile
doc/hlp/pl/Makefile
doc/hlp/ru/Makefile
doc/hlp/sr/Makefile

intl/Makefile
po/Makefile.in
])

if test x$enable_tests != xno; then
    AC_CONFIG_FILES([
tests/Makefile
tests/lib/Makefile
tests/lib/mcconfig/Makefile
tests/lib/search/Makefile
tests/lib/strutil/Makefile
tests/lib/vfs/Makefile
tests/lib/widget/Makefile
tests/src/Makefile
tests/src/filemanager/Makefile
])
fi

AC_OUTPUT

AC_MSG_NOTICE([

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
  With background operations: ${enable_background}
  Internal editor:            ${edit_msg}
  Diff viewer:                ${diff_msg}
  Support for charset:        ${charset_msg}
  Search type:                ${SEARCH_TYPE}
])
