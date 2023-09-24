dnl
dnl Configure.in file for the Midnight Commander
dnl

dnl 2.64 is required at least for m4_esyscmd_s()
AC_PREREQ(2.64)

AC_INIT([GNU Midnight Commander], m4_esyscmd_s([./version.sh .]),
        [https://www.midnight-commander.org/wiki/NewTicket], [mc],
        [https://www.midnight-commander.org/])

m4_pattern_forbid(MC_)
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_SRCDIR(src/main.c)
AC_CONFIG_HEADERS(config.h)

dnl Apply "no-define" to avoid defining the VERSION macro in config.h
AM_INIT_AUTOMAKE([no-define])
dnl PACKAGE macro isn't defined if "no-define" is applied
AC_DEFINE([PACKAGE], ["mc"], [Name of package])

dnl Enable silent rules by default (if yes)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

mc_VERSION

AM_MAINTAINER_MODE

AC_CANONICAL_HOST

AC_USE_SYSTEM_EXTENSIONS


dnl ############################################################################
dnl Check for compiler
dnl ############################################################################

AC_PROG_CC
AM_PROG_CC_C_O

mc_CHECK_CFLAGS

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
    AX_APPEND_COMPILE_FLAGS([-Werror], [mc_configured_cflags])
fi

dnl Compiler can generate warnings for unrecognized flags added to CFLAGS
dnl which causes attribute checks to fail
ax_gcc_func_attribute_save_flags=$[]_AC_LANG_PREFIX[]FLAGS
_AC_LANG_PREFIX[]FLAGS=
AX_GCC_FUNC_ATTRIBUTE([fallthrough])
_AC_LANG_PREFIX[]FLAGS=$ax_gcc_func_attribute_save_flags
unset ax_gcc_func_attribute_save_flags

LT_INIT


dnl ############################################################################
dnl Check for programs
dnl ############################################################################

AC_PROG_SED
PKG_PROG_PKG_CONFIG
AC_PROG_INSTALL
AC_PROG_LN_S
dnl See also the "OS specific stuff" section below.

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

dnl Check for -z, -b, -L, and -S options to file
AC_CHECK_PROG(HAVE_FILECMD, file, true, false)
if $HAVE_FILECMD; then
    dnl Don't use the file command if it doesn't accept the -z option
    AC_MSG_CHECKING([for -z option to file command])
    AC_CACHE_VAL(mc_cv_file_z, [
        file -z . > /dev/null 2>&1
        if test $? = 0; then
            mc_cv_file_z=yes
        else
            mc_cv_file_z=no
        fi
    ])
    AC_MSG_RESULT([$mc_cv_file_z])

    if test x$mc_cv_file_z = xyes; then
        AC_DEFINE(USE_FILE_CMD, 1, [Define if the file command accepts the -z option])
    else
        AC_MSG_WARN([The file command doesn't accept the -z option and will not be used])
    fi

    if test x$mc_cv_file_z = xyes; then
        dnl file is used; check -b, -L and -S options

        AC_MSG_CHECKING([for -b option to file command])
        AC_CACHE_VAL(mc_cv_file_b, [
            file -b . > /dev/null 2>&1
            if test $? = 0; then
                mc_cv_file_b=yes
            else
                mc_cv_file_b=no
            fi
        ])
        AC_MSG_RESULT([$mc_cv_file_b])

        if test x$mc_cv_file_b = xyes; then
            AC_DEFINE(FILE_B, "-b ", [Define if the file command accepts the -b option])
        fi

        AC_MSG_CHECKING([for -L option to file command])
        AC_CACHE_VAL(mc_cv_file_L, [
            file -L . > /dev/null 2>&1
            if test $? = 0; then
                mc_cv_file_L=yes
            else
                mc_cv_file_L=no
            fi
        ])
        AC_MSG_RESULT([$mc_cv_file_L])

        if test x$mc_cv_file_L = xyes; then
            AC_DEFINE(FILE_L, "-L ", [Define if the file command accepts the -L option])
        else
            AC_DEFINE(FILE_L, "", [Define if the file command accepts the -L option])
        fi

        dnl The file command accepts the -S option since 5.33
        AC_MSG_CHECKING([for -S option to file command])
        AC_CACHE_VAL(mc_cv_file_S, [
            file -S . > /dev/null 2>&1
            if test $? = 0; then
                mc_cv_file_S=yes
            else
                mc_cv_file_S=no
            fi
        ])
        AC_MSG_RESULT([$mc_cv_file_S])

        if test x$mc_cv_file_S = xyes; then
            AC_DEFINE(FILE_S, "-S ", [Define if file command accepts the -S option])
        else
            AC_DEFINE(FILE_S, "", [Define if file command accepts the -S option])
        fi
    fi
fi

dnl Only list browsers here that can be run in background (i.e. with `&')
AC_CHECK_PROGS(X11_WWW, [gnome-moz-remote mozilla firefox konqueror opera])


dnl ############################################################################
dnl Check for other tools
dnl ############################################################################

AC_CHECK_TOOL(AR, ar, ar)
mc_UNIT_TESTS


dnl ############################################################################
dnl Check for main libraries
dnl ############################################################################

mc_CHECK_GLIB
mc_G_MODULE_SUPPORTED
mc_WITH_SCREEN
mc_CHECK_SEARCH_TYPE
dnl X11 support. Used to read keyboard modifiers when running under X11.
mc_WITH_X


dnl ############################################################################
dnl Check for header files
dnl ############################################################################

AC_CHECK_HEADERS([string.h memory.h limits.h malloc.h \
	utime.h sys/statfs.h sys/vfs.h \
	sys/select.h sys/ioctl.h stropts.h arpa/inet.h \
	sys/socket.h])
dnl This macro is redefined in m4.include/gnulib/sys_types_h.m4
dnl   to work around a buggy version in autoconf <= 2.69.
AC_HEADER_MAJOR


dnl ############################################################################
dnl Check for types
dnl ############################################################################

dnl Check largefile before type sizeof checks
AC_SYS_LARGEFILE

AC_CHECK_SIZEOF(long)
AC_TYPE_UINTMAX_T
AC_CHECK_SIZEOF(uintmax_t)
AC_TYPE_OFF_T
AC_CHECK_SIZEOF(off_t)
AC_TYPE_MODE_T
gl_PROMOTED_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_UID_T
AC_CHECK_TYPE([major_t], [], [AC_DEFINE([major_t], [int], [Type of major device numbers.])])
AC_CHECK_TYPE([minor_t], [], [AC_DEFINE([minor_t], [int], [Type of minor device numbers.])])

AC_STRUCT_ST_BLOCKS
AC_CHECK_MEMBERS([struct stat.st_blksize, struct stat.st_rdev, struct stat.st_mtim])
gl_STAT_SIZE

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
	strverscmp \
	strncasecmp \
	realpath
])

dnl getpt is a GNU Extension (glibc 2.1.x)
AC_CHECK_FUNCS(posix_openpt, , [AC_CHECK_FUNCS(getpt)])
AC_CHECK_FUNCS(grantpt, , [AC_CHECK_LIB(pt, grantpt)])

dnl replacing lstat with statlstat on sco makes it more portable between
dnl sco clones
AC_CHECK_FUNCS(statlstat)

dnl Overriding mmap support.  This has to be before AC_FUNC_MMAP is used.
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

mc_GET_FS_INFO


dnl ############################################################################
dnl Internationalization
dnl ############################################################################

AC_CHECK_FUNCS([setlocale])

AM_GNU_GETTEXT([external], [need-ngettext])
AM_GNU_GETTEXT_VERSION([0.18.2])

mc_I18N

dnl ############################################################################
dnl OS specific stuff
dnl ############################################################################

case $host_os in
*os400)
    AC_PATH_PROG([PERL], [perl], [/QOpenSys/pkgs/bin/perl])
    AC_PATH_PROG([PERL_FOR_BUILD], [perl], [/QOpenSys/pkgs/bin/perl])
    AC_PATH_PROG([PYTHON], [python], [/QOpenSys/pkgs/bin/python2])
    AC_PATH_PROG([RUBY], [ruby], [/QOpenSys/pkgs/bin/ruby])
    ;;
*)
    AC_PATH_PROG([PERL], [perl], [/usr/bin/perl])
    AC_PATH_PROG([PERL_FOR_BUILD], [perl], [/usr/bin/perl])
    AC_PATH_PROG([PYTHON], [python], [/usr/bin/python])
    AC_PATH_PROG([RUBY], [ruby], [/usr/bin/ruby])
esac

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

dnl utimensat is supported since glibc 2.6 and specified in POSIX.1-2008
dnl utimensat() causes different timespec structures to cause failures on IBM i and AIX
case $host_os in
*os400 | aix*)
    ;;
*)
    AC_CHECK_FUNCS([utimensat])
    ;;
esac

case $host_os in
*os400)
    AC_DEFINE([PTY_ZEROREAD], [1], [read(1) can return 0 for a non-closed fd])
esac

dnl Check linux/fs.h for FICLONE to support BTRFS's file clone operation
case $host_os in
linux*)
    AC_CHECK_HEADERS([linux/fs.h])
esac

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
dnl MC options
dnl ############################################################################

mc_ASSERT

mc_WITH_INTERNAL_EDIT

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

mc_SUBSHELL
mc_BACKGROUND
mc_EXT2FS_ATTR
mc_VFS_CHECKS

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
AM_CONDITIONAL(USE_INTERNAL_EDIT, [test x"$use_internal_edit" = xyes ])
AM_CONDITIONAL(USE_ASPELL, [test x"$enable_aspell" = xyes ])
AM_CONDITIONAL(USE_DIFF, [test -n "$use_diff"])
AM_CONDITIONAL(CHARSET, [test -n "$have_charset"])
AM_CONDITIONAL(CONS_SAVER, [test -n "$cons_saver"])
dnl Clarify do we really need GModule
AM_CONDITIONAL([HAVE_GMODULE], [test -n "$g_module_supported" && \
                                test x"$textmode_x11_support" = x"yes" -o x"$enable_aspell" = x"yes"])

AC_ARG_ENABLE([configure-args],
    AS_HELP_STRING([--enable-configure-args], [Embed ./configure arguments into binaries]))
if test "x$enable_configure_args" != xno; then
    AC_DEFINE([ENABLE_CONFIGURE_ARGS], 1, [Define to enable showing configure arguments in help])
    AC_DEFINE_UNQUOTED([MC_CONFIGURE_ARGS], ["$ac_configure_args"], [MC configure arguments])
fi


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

misc/Makefile
misc/mc.charsets
misc/mc.menu
misc/mcedit.menu
misc/skins/Makefile
misc/ext.d/Makefile
misc/ext.d/doc.sh
misc/ext.d/misc.sh
misc/ext.d/text.sh
misc/ext.d/web.sh
misc/macros.d/Makefile
misc/mc.ext.ini

src/Makefile
src/consaver/Makefile
src/editor/Makefile
src/man2hlp/Makefile
src/subshell/Makefile
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
src/vfs/extfs/helpers/ucab
src/vfs/extfs/helpers/uha
src/vfs/extfs/helpers/ulha
src/vfs/extfs/helpers/ulib
src/vfs/extfs/helpers/unar
src/vfs/extfs/helpers/urar
src/vfs/extfs/helpers/uwim
src/vfs/extfs/helpers/uzip
src/vfs/extfs/helpers/uzoo

src/vfs/shell/Makefile
src/vfs/shell/helpers/Makefile

src/vfs/ftpfs/Makefile

src/vfs/sftpfs/Makefile

src/vfs/local/Makefile

src/vfs/sfs/Makefile

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

doc/Makefile

doc/hints/Makefile
doc/hints/l10n/Makefile

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

po/Makefile.in
])

dnl https://stackoverflow.com/questions/30897170/ac-subst-does-not-expand-variable/30932102#30932102
AC_CONFIG_FILES(
[misc/syntax/Syntax], [${SED-sed} -e "s%\${prefix}%$PREFIX%" misc/syntax/Syntax > misc/syntax/Syntax.tmp && \
  mv -f misc/syntax/Syntax.tmp misc/syntax/Syntax], [export PREFIX=$prefix]
)

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
tests/src/editor/Makefile
tests/src/editor/test-data.txt
tests/src/vfs/Makefile
tests/src/vfs/extfs/Makefile
tests/src/vfs/extfs/helpers-list/Makefile
tests/src/vfs/extfs/helpers-list/data/config.sh
tests/src/vfs/extfs/helpers-list/misc/Makefile
tests/src/vfs/ftpfs/Makefile
])

AC_OUTPUT

AC_MSG_NOTICE([

Configuration:

  Source code location:           ${srcdir}
  Compiler:                       ${CC}
  Compiler flags:                 ${CFLAGS}
  Assertions:                     ${enable_assert}
  Unit tests:                     ${tests_msg}
  File system:                    ${vfs_type}
                                  ${vfs_flags}
  Screen library:                 ${screen_msg}
  Mouse support:                  ${mouse_lib}
  X11 events support:             ${textmode_x11_support}
  With subshell support:          ${subshell}
  With background operations:     ${enable_background}
  With ext2fs attributes support: ${ext2fs_attr_msg}
  Internal editor:                ${edit_msg}
  Diff viewer:                    ${diff_msg}
  Support for charset:            ${charset_msg}
  Search type:                    ${SEARCH_TYPE}
])

dnl option checking is disable by default due to AC_CONFIG_SUBDIRS
dnl we enable it back for top-level ./configure
if test -n "$ac_unrecognized_opts"; then
  case $enable_option_checking in
    fatal) AC_MSG_ERROR([unrecognized options: $ac_unrecognized_opts]) ;;
    *)     AC_MSG_WARN( [unrecognized options: $ac_unrecognized_opts]) ;;
  esac
fi
