dnl Process this file with autoconf to produce a configure script.
AC_INIT(include/includes.h)
AC_PREREQ(2.52)
AC_CONFIG_HEADER(include/config.h)
AC_CONFIG_AUX_DIR(../../config)
# we want to be compatibe with older versions of Samba
AC_PREFIX_DEFAULT(/usr/local/samba)
AC_SYS_LARGEFILE

dnl Unique-to-Samba variables we'll be playing with.
AC_SUBST(SHELL)
AC_SUBST(MPROGS)
AC_SUBST(LDSHFLAGS)
AC_SUBST(HOST_OS)
AC_SUBST(WRAP)
AC_SUBST(WRAP32)

# compile with optimization and without debugging by default
CFLAGS=${CFLAGS-"-O"}

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_AWK
AC_CHECK_TOOL(AR, ar, ar)

dnl Check if C compiler understands -c and -o at the same time
AC_PROG_CC_C_O
if eval "test \"`echo '$ac_cv_prog_cc_'${ac_cc}_c_o`\" = no"; then
      BROKEN_CC=
else
      BROKEN_CC=#
fi
AC_SUBST(BROKEN_CC)

AC_CANONICAL_HOST
AC_VALIDATE_CACHE_SYSTEM_TYPE
SAMBA_MAINTAINER_MODE

AC_INLINE
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_HEADER_TIME
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(arpa/inet.h sys/fcntl.h sys/select.h fcntl.h sys/time.h sys/unistd.h)
AC_CHECK_HEADERS(unistd.h utime.h grp.h sys/id.h limits.h memory.h net/if.h)
AC_CHECK_HEADERS(compat.h rpc/rpc.h rpcsvc/nis.h rpcsvc/yp_prot.h rpcsvc/ypclnt.h)
AC_CHECK_HEADERS(sys/param.h ctype.h sys/wait.h sys/resource.h sys/ioctl.h sys/mode.h)
AC_CHECK_HEADERS(sys/mman.h sys/filio.h sys/priv.h string.h strings.h stdlib.h sys/socket.h)
AC_CHECK_HEADERS(sys/mount.h sys/vfs.h sys/fs/s5param.h sys/filsys.h termios.h termio.h)
AC_CHECK_HEADERS(sys/termio.h sys/statfs.h sys/dustat.h sys/statvfs.h stdarg.h sys/sockio.h)
AC_CHECK_HEADERS(netinet/tcp.h netinet/in_systm.h netinet/in_ip.h)
AC_CHECK_HEADERS(sys/security.h security/pam_appl.h)
AC_CHECK_HEADERS(stropts.h poll.h syscall.h sys/syscall.h)
AC_CHECK_HEADERS(sys/acl.h sys/cdefs.h glob.h)

AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(off_t)
AC_CHECK_SIZEOF(ino_t)

AC_C_CONST
AC_C_INLINE
AC_C_BIGENDIAN
AC_C_CHAR_UNSIGNED

AC_TYPE_SIGNAL
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_PID_T
AC_CHECK_TYPE(ino_t,unsigned)
AC_CHECK_TYPE(loff_t,off_t)
AC_CHECK_TYPE(offset_t,off_t)
AC_CHECK_TYPE(ssize_t, int)

# we need libdl for PAM and the new VFS code
AC_CHECK_LIB(dl,main)

AC_CACHE_CHECK([for errno in errno.h],samba_cv_errno, [
    AC_TRY_COMPILE([#include <errno.h>],[int i = errno],
	samba_cv_errno=yes,samba_cv_have_errno=no)])
if test x"$samba_cv_errno" = x"yes"; then
   AC_DEFINE(HAVE_ERRNO_DECL, 1, [Define if errno is declared])
fi

# stupid glibc has the functions but no declaration. grrrr.
AC_CACHE_CHECK([for crypt declaration],samba_cv_have_crypt_decl,[
    AC_TRY_COMPILE([#include <unistd.h>],[int i = (int)crypt],
        samba_cv_have_crypt_decl=yes,samba_cv_have_crypt_decl=no)])
if test x"$samba_cv_have_crypt_decl" = x"yes"; then
    AC_DEFINE(HAVE_CRYPT_DECL, 1, [Define if crypt() is declared])
fi

AC_FUNC_MEMCMP

###############################################
# test for where we get crypt() from
AC_CHECK_FUNCS(crypt)
if test x"$ac_cv_func_crypt" = x"no"; then
    AC_CHECK_LIB(crypt, crypt, [LIBS="$LIBS -lcrypt";
	AC_DEFINE(HAVE_CRYPT)])
fi


# The following test taken from the cvs sources
# If we can't find connect, try looking in -lsocket, -lnsl, and -linet.
# The Irix 5 libc.so has connect and gethostbyname, but Irix 5 also has
# libsocket.so which has a bad implementation of gethostbyname (it
# only looks in /etc/hosts), so we only look for -lsocket if we need
# it.
AC_CHECK_FUNCS(connect)
if test x"$ac_cv_func_connect" = x"no"; then
    case "$LIBS" in
    *-lnsl*) ;;
    *) AC_CHECK_LIB(nsl_s, printf) ;;
    esac
    case "$LIBS" in
    *-lnsl*) ;;
    *) AC_CHECK_LIB(nsl, printf) ;;
    esac
    case "$LIBS" in
    *-lsocket*) ;;
    *) AC_CHECK_LIB(socket, connect) ;;
    esac
    case "$LIBS" in
    *-linet*) ;;
    *) AC_CHECK_LIB(inet, connect) ;;
    esac
    dnl We can't just call AC_CHECK_FUNCS(connect) here, because the value
    dnl has been cached.
    if test x"$ac_cv_lib_socket_connect" = x"yes" || 
       test x"$ac_cv_lib_inet_connect" = x"yes"; then
        # ac_cv_func_connect=yes
        # don't!  it would cause AC_CHECK_FUNC to succeed next time configure is run
        AC_DEFINE(HAVE_CONNECT)
    fi
fi

AC_CHECK_FUNCS(waitpid getcwd strtoul strerror chown chmod)
AC_CHECK_FUNCS(fstat strchr utime utimes getrlimit fsync bzero memset)
AC_CHECK_FUNCS(memmove vsnprintf snprintf setsid glob pipe crypt16 getauthuid)
AC_CHECK_FUNCS(sigprocmask sigblock sigaction innetgr setnetgrent getnetgrent endnetgrent)
AC_CHECK_FUNCS(initgroups select rdchk getgrnam pathconf)
AC_CHECK_FUNCS(setpriv setgidx setuidx setgroups mktime rename ftruncate)
AC_CHECK_FUNCS(grantpt dup2 yp_get_default_domain getpwanam)
AC_CHECK_FUNCS(srandom random srand rand setenv usleep strcasecmp)

AC_CHECK_FUNCS(getdents)
AC_CHECK_FUNCS(llseek)

#
# If no strcasecmp, check for it in some known places
# It is in -lresolv on ReliantUNIX and UnixWare
#   -lresolve *must* follow -lnsl for name resolution to work properly
#

if test x$ac_cv_func_strcasecmp = xno ; then
	AC_CHECK_LIB(resolv,strcasecmp,[LIBS="$LIBS -lresolv"]
		AC_DEFINE(HAVE_STRCASECMP))
fi

#
# Check for the functions putprpwnam, set_auth_parameters,
# getspnam, bigcrypt and getprpwnam in -lsec and -lsecurity
# Needed for OSF1 and HPUX.
#

AC_LIBTESTFUNC(security, putprpwnam)
AC_LIBTESTFUNC(sec, putprpwnam)

AC_LIBTESTFUNC(security, set_auth_parameters)
AC_LIBTESTFUNC(sec, set_auth_parameters)

AC_LIBTESTFUNC(security, getspnam)
AC_LIBTESTFUNC(sec, getspnam)

AC_LIBTESTFUNC(security, bigcrypt)
AC_LIBTESTFUNC(sec, bigcrypt)

AC_LIBTESTFUNC(security, getprpwnam)
AC_LIBTESTFUNC(sec, getprpwnam)

# this bit needs to be modified for each OS that is suported by
# smbwrapper. You need to specify how to created a shared library and
# how to compile C code to produce PIC object files

# these are the defaults, good for lots of systems
HOST_OS="$host_os"
LDSHFLAGS="-shared"

# and these are for particular systems
case "$host_os" in
		*linux*)   AC_DEFINE(LINUX, 1, [Define on Linux]);;
		*solaris*) AC_DEFINE(SUNOS5, 1, [Define on SunOS 5 (Solaris)])
					LDSHFLAGS="-G"
		;;
		*sunos*) AC_DEFINE(SUNOS4, 1, [Define on SunOS 4])
					LDSHFLAGS=""
		;;
		*bsd*) LDSHFLAGS="-shared -Bshareable"
		;;
		*irix*) AC_DEFINE(IRIX, 1, [Define on IRIX])
			case "$host_os" in
			*irix6*) AC_DEFINE(IRIX6, 1, [Define on IRIX 6])
			;;
			esac
			ATTEMPT_WRAP32_BUILD=yes
		;;
		*aix*) AC_DEFINE(AIX, 1, [Define on AIX]);;
		*hpux*) AC_DEFINE(HPUX, 1, [Define on HP-UX]);;
		*qnx*) AC_DEFINE(QNX, 1, [Define on QNX]);;
		*osf*) AC_DEFINE(OSF1, 1, [Define on OSF1]);;
		*sco*) AC_DEFINE(SCO, 1, [Define on SCO]);;
		*next2*) AC_DEFINE(NEXT2, 1, [Define on NeXT 2]);;
		*dgux*) AC_CHECK_PROG( ROFF, groff, [groff -etpsR -Tascii -man]);;
		*sysv4*)
			case "$host" in
				*-univel-*)     if [ test "$GCC" != yes ]; then
									AC_DEFINE(HAVE_MEMSET)
								fi
								LDSHFLAGS="-G"
				;;
			esac
			;;
		*sysv5*)
			if [ test "$GCC" != yes ]; then
				AC_DEFINE(HAVE_MEMSET)
			fi
			LDSHFLAGS="-G"
			;;
esac

################

AC_CACHE_CHECK([for long long],samba_cv_have_longlong,[
AC_TRY_RUN([#include <stdio.h>
main() { long long x = 1000000; x *= x; exit(((x/1000000) == 1000000)? 0: 1); }],
samba_cv_have_longlong=yes,samba_cv_have_longlong=no,samba_cv_have_longlong=cross)])
if test x"$samba_cv_have_longlong" = x"yes"; then
    AC_DEFINE(HAVE_LONGLONG, 1, [Define if long long is usable])
fi

AC_CACHE_CHECK([for sin_len in sock],samba_cv_HAVE_SOCK_SIN_LEN,[
AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>],
[struct sockaddr_in sock; sock.sin_len = sizeof(sock);],
samba_cv_HAVE_SOCK_SIN_LEN=yes,samba_cv_HAVE_SOCK_SIN_LEN=no)])
if test x"$samba_cv_HAVE_SOCK_SIN_LEN" = x"yes"; then
    AC_DEFINE(HAVE_SOCK_SIN_LEN, 1, [Define if struct sockaddr_in has sin_len field])
fi

AC_CACHE_CHECK([for __FILE__ macro],samba_cv_HAVE_FILE_MACRO,[
AC_TRY_COMPILE([#include <stdio.h>], [printf("%s\n", __FILE__);],
samba_cv_HAVE_FILE_MACRO=yes,samba_cv_HAVE_FILE_MACRO=no)])
if test x"$samba_cv_HAVE_FILE_MACRO" = x"yes"; then
    AC_DEFINE(HAVE_FILE_MACRO, 1, [Define if __FILE__ macro is supported])
fi

AC_CACHE_CHECK([for __FUNCTION__ macro],samba_cv_HAVE_FUNCTION_MACRO,[
AC_TRY_COMPILE([#include <stdio.h>], [printf("%s\n", __FUNCTION__);],
samba_cv_HAVE_FUNCTION_MACRO=yes,samba_cv_HAVE_FUNCTION_MACRO=no)])
if test x"$samba_cv_HAVE_FUNCTION_MACRO" = x"yes"; then
    AC_DEFINE(HAVE_FUNCTION_MACRO, 1, [Define if __FUNCTION__ macro is supported])
fi

AC_CACHE_CHECK([if gettimeofday takes tz argument],samba_cv_HAVE_GETTIMEOFDAY_TZ,[
AC_TRY_RUN([
#include <sys/time.h>
#include <unistd.h>
main() { struct timeval tv; exit(gettimeofday(&tv, NULL));}],
           samba_cv_HAVE_GETTIMEOFDAY_TZ=yes,samba_cv_HAVE_GETTIMEOFDAY_TZ=no,samba_cv_HAVE_GETTIMEOFDAY_TZ=cross)])
if test x"$samba_cv_HAVE_GETTIMEOFDAY_TZ" = x"yes"; then
    AC_DEFINE(HAVE_GETTIMEOFDAY_TZ, 1, [Define if gettimeofday takes tz argument])
fi


AC_CACHE_CHECK([for broken readdir],samba_cv_HAVE_BROKEN_READDIR,[
AC_TRY_RUN([#include <sys/types.h>
#include <dirent.h>
main() { struct dirent *di; DIR *d = opendir("."); di = readdir(d);
if (di && di->d_name[-2] == '.' && di->d_name[-1] == 0 &&
di->d_name[0] == 0) exit(0); exit(1);} ],
samba_cv_HAVE_BROKEN_READDIR=yes,samba_cv_HAVE_BROKEN_READDIR=no,samba_cv_HAVE_BROKEN_READDIR=cross)])
if test x"$samba_cv_HAVE_BROKEN_READDIR" = x"yes"; then
    AC_DEFINE(HAVE_BROKEN_READDIR, 1, [Define if readdir() is broken])
fi

AC_CACHE_CHECK([for kernel oplock type definitions],samba_cv_HAVE_KERNEL_OPLOCKS,[
AC_TRY_COMPILE([#include <sys/types.h>
#include <fcntl.h>],
[oplock_stat_t t; t.os_state = OP_REVOKE; t.os_dev = 1; t.os_ino = 1;],
samba_cv_HAVE_KERNEL_OPLOCKS=yes,samba_cv_HAVE_KERNEL_OPLOCKS=no)])
if test x"$samba_cv_HAVE_KERNEL_OPLOCKS" = x"yes"; then
    AC_DEFINE(HAVE_KERNEL_OPLOCKS, 1, [Define to use kernel oplock capabilities])
fi

#
# Check for int16, uint16, int32 and uint32 in rpc/types.h included from rpc/rpc.h
# This is *really* broken but some systems (DEC OSF1) do this.... JRA.
#

AC_CACHE_CHECK([for int16 typedef included by rpc/rpc.h],samba_cv_HAVE_INT16_FROM_RPC_RPC_H,[
AC_TRY_COMPILE([#include <sys/types.h>
#if defined(HAVE_RPC_RPC_H)
#include <rpc/rpc.h>
#endif],
[int16 testvar;],
samba_cv_HAVE_INT16_FROM_RPC_RPC_H=yes,samba_cv_HAVE_INT16_FROM_RPC_RPC_H=no)])
if test x"$samba_cv_HAVE_INT16_FROM_RPC_RPC_H" = x"yes"; then
    AC_DEFINE(HAVE_INT16_FROM_RPC_RPC_H, 1, [Define if rpc/rpc.h defines int16])
fi

AC_CACHE_CHECK([for uint16 typedef included by rpc/rpc.h],samba_cv_HAVE_UINT16_FROM_RPC_RPC_H,[
AC_TRY_COMPILE([#include <sys/types.h>
#if defined(HAVE_RPC_RPC_H)
#include <rpc/rpc.h>
#endif],
[uint16 testvar;],
samba_cv_HAVE_UINT16_FROM_RPC_RPC_H=yes,samba_cv_HAVE_UINT16_FROM_RPC_RPC_H=no)])
if test x"$samba_cv_HAVE_UINT16_FROM_RPC_RPC_H" = x"yes"; then
    AC_DEFINE(HAVE_UINT16_FROM_RPC_RPC_H, 1, [Define if rpc/rpc.h defines uint16])
fi

AC_CACHE_CHECK([for int32 typedef included by rpc/rpc.h],samba_cv_HAVE_INT32_FROM_RPC_RPC_H,[
AC_TRY_COMPILE([#include <sys/types.h>
#if defined(HAVE_RPC_RPC_H)
#include <rpc/rpc.h>
#endif],
[int32 testvar;],
samba_cv_HAVE_INT32_FROM_RPC_RPC_H=yes,samba_cv_HAVE_INT32_FROM_RPC_RPC_H=no)])
if test x"$samba_cv_HAVE_INT32_FROM_RPC_RPC_H" = x"yes"; then
    AC_DEFINE(HAVE_INT32_FROM_RPC_RPC_H, 1, [Define if rpc/rpc.h defines int32])
fi

AC_CACHE_CHECK([for uint32 typedef included by rpc/rpc.h],samba_cv_HAVE_UINT32_FROM_RPC_RPC_H,[
AC_TRY_COMPILE([#include <sys/types.h>
#if defined(HAVE_RPC_RPC_H)
#include <rpc/rpc.h>
#endif],
[uint32 testvar;],
samba_cv_HAVE_UINT32_FROM_RPC_RPC_H=yes,samba_cv_HAVE_UINT32_FROM_RPC_RPC_H=no)])
if test x"$samba_cv_HAVE_UINT32_FROM_RPC_RPC_H" = x"yes"; then
    AC_DEFINE(HAVE_UINT32_FROM_RPC_RPC_H, 1, [Define if rpc/rpc.h defines uint32])
fi

dnl
dnl Some systems (SCO) have a problem including
dnl <prot.h> and <rpc/rpc.h> due to AUTH_ERROR being defined
dnl as a #define in <prot.h> and as part of an enum
dnl in <rpc/rpc.h>.
dnl

AC_CACHE_CHECK([for conflicting AUTH_ERROR define in rpc/rpc.h],samba_cv_HAVE_RPC_AUTH_ERROR_CONFLICT,[
AC_TRY_COMPILE([#include <sys/types.h>
#ifdef HAVE_SYS_SECURITY_H
#include <sys/security.h>
#include <prot.h>
#endif  /* HAVE_SYS_SECURITY_H */
#if defined(HAVE_RPC_RPC_H)
#include <rpc/rpc.h>
#endif],
[int testvar;],
samba_cv_HAVE_RPC_AUTH_ERROR_CONFLICT=no,samba_cv_HAVE_RPC_AUTH_ERROR_CONFLICT=yes)])
if test x"$samba_cv_HAVE_RPC_AUTH_ERROR_CONFLICT" = x"yes"; then
    AC_DEFINE(HAVE_RPC_AUTH_ERROR_CONFLICT, 1, [Define if rpc/rpc.h defines AUTH_ERROR])
fi

netmask=no;
AC_CACHE_CHECK([for netmask ifconf],samba_cv_HAVE_NETMASK_IFCONF,[
AC_TRY_RUN([
#define HAVE_NETMASK_IFCONF 1
#define AUTOCONF 1
#include "confdefs.h"
#include "${srcdir-.}/lib/netmask.c"],
           samba_cv_HAVE_NETMASK_IFCONF=yes,samba_cv_HAVE_NETMASK_IFCONF=no,samba_cv_HAVE_NETMASK_IFCONF=cross)])
if test x"$samba_cv_HAVE_NETMASK_IFCONF" = x"yes"; then
    netmask=yes;AC_DEFINE(HAVE_NETMASK_IFCONF, 1, [Define to use IFCONF style netmask])
fi

if test $netmask = no; then
AC_CACHE_CHECK([for netmask ifreq],samba_cv_HAVE_NETMASK_IFREQ,[
AC_TRY_RUN([
#define HAVE_NETMASK_IFREQ 1
#define AUTOCONF 1
#include "confdefs.h"
#include "${srcdir-.}/lib/netmask.c"],
           samba_cv_HAVE_NETMASK_IFREQ=yes,samba_cv_HAVE_NETMASK_IFREQ=no,samba_cv_HAVE_NETMASK_IFREQ=cross)])
if test x"$samba_cv_HAVE_NETMASK_IFREQ" = x"yes"; then
    netmask=yes;AC_DEFINE(HAVE_NETMASK_IFREQ, 1, [Define to use IFREQ style netmask])
fi
fi

if test $netmask = no; then
AC_CACHE_CHECK([for netmask AIX],samba_cv_HAVE_NETMASK_AIX,[
AC_TRY_RUN([
#define HAVE_NETMASK_AIX 1
#define AUTOCONF 1
#include "confdefs.h"
#include "${srcdir-.}/lib/netmask.c"],
           samba_cv_HAVE_NETMASK_AIX=yes,samba_cv_HAVE_NETMASK_AIX=no,samba_cv_HAVE_NETMASK_AIX=cross)])
if test x"$samba_cv_HAVE_NETMASK_AIX" = x"yes"; then
    netmask=yes;AC_DEFINE(HAVE_NETMASK_AIX, 1, [Define to use AIX style netmask])
fi
fi


AC_CACHE_CHECK([for broken nisplus include files],samba_cv_BROKEN_NISPLUS_INCLUDE_FILES,[
AC_TRY_COMPILE([#include <sys/acl.h>
#if defined(HAVE_RPCSVC_NIS_H)
#include <rpcsvc/nis.h>
#endif],
[return 0;],
samba_cv_BROKEN_NISPLUS_INCLUDE_FILES=no,samba_cv_BROKEN_NISPLUS_INCLUDE_FILES=yes)])
if test x"$samba_cv_BROKEN_NISPLUS_INCLUDE_FILES" = x"yes"; then
    AC_DEFINE(BROKEN_NISPLUS_INCLUDE_FILES, 1,
	      [Define if sys/acl.h and rpcsvc/nis.h cannot be included together])
fi


#################################################
# check for a LDAP password database
AC_MSG_CHECKING([whether to use LDAP password database])
AC_ARG_WITH(ldap,
[  --with-ldap     Include LDAP support
  --without-ldap  Don't include LDAP support (default)],
[ case "$withval" in
  yes)
    AC_MSG_RESULT(yes)
    AC_DEFINE(WITH_LDAP, 1, [Define to enable LDAP support])
    ;;
  *)
    AC_MSG_RESULT(no)
    ;;
  esac ],
  AC_MSG_RESULT(no)
)

#################################################
# check for a NISPLUS password database
AC_MSG_CHECKING([whether to use NISPLUS password database])
AC_ARG_WITH(nisplus,
[  --with-nisplus     Include NISPLUS password database support
  --without-nisplus  Don't include NISPLUS password database support (default)],
[ case "$withval" in
  yes)
    AC_MSG_RESULT(yes)
    AC_DEFINE(WITH_NISPLUS, 1, [Define to enable NIS+ support])
    ;;
  *)
    AC_MSG_RESULT(no)
    ;;
  esac ],
  AC_MSG_RESULT(no)
)

#################################################
# check for a NISPLUS_HOME support 
AC_MSG_CHECKING([whether to use NISPLUS_HOME])
AC_ARG_WITH(nisplus-home,
[  --with-nisplus-home     Include NISPLUS_HOME support
  --without-nisplus-home  Don't include NISPLUS_HOME support (default)],
[ case "$withval" in
  yes)
    AC_MSG_RESULT(yes)
    AC_DEFINE(WITH_NISPLUS_HOME, 1, [Define to support NISPLUS_HOME variable])
    ;;
  *)
    AC_MSG_RESULT(no)
    ;;
  esac ],
  AC_MSG_RESULT(no)
)

#################################################
# check for the secure socket layer
AC_MSG_CHECKING([whether to use SSL])
AC_ARG_WITH(ssl,
[  --with-ssl     Include SSL support
  --without-ssl  Don't include SSL support (default)
  --with-sslinc=DIR Where the SSL includes are (defaults to /usr/local/ssl)],
[ case "$withval" in
  yes)
    AC_MSG_RESULT(yes)
    AC_DEFINE(WITH_SSL, 1, [Define to enable SSL support])
    withval="/usr/local/ssl"     # default

    if test "${with_sslinc+set}" = set; then

	withval="$with_sslinc"
	case "$withval" in
	yes|no)
	  AC_MSG_WARN([--with-sslinc called without argument - will use default])
	  CFLAGS="-I/usr/local/ssl/include $CFLAGS"
	  LIBS="-lssl -lcrypto $LIBS"
	  LDFLAGS="=L/usr/local/ssl/lib $LDFLAGS"
	;;
        * )
          CFLAGS="-I${withval}/include $CFLAGS"
	  LIBS="-lssl -l crypto $LIBS"
  	  LDFLAGS="-L${withval}/lib $LDFLAGS"
	;;
	esac

    else

	CFLAGS="-I/usr/local/ssl/include $CFLAGS"   
	LIBS="-lssl -lcrypto $LIBS"
	LDFLAGS="-L/usr/local/ssl/lib $LDFLAGS"

    fi

    if test ! -d ${withval}; then
      AC_MSG_ERROR([called with --with-ssl, but ssl base directory ${withval} does not exist or is not a directory. Aborting config])
    fi 

    CFLAGS="-DHAVE_CRYPT_DECL $CFLAGS"  # Damn, SSLeay defines its own

    ;;
  *)
    AC_MSG_RESULT(no)
    ;;
  esac ],
  AC_MSG_RESULT(no)
)


#################################################
# set configuration directory location
configdir="\$(LIBDIR)"
AC_ARG_WITH(configdir,
[  --with-configdir=DIR    Where to put configuration files (\$libdir)],
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


#################################################
# set codepage directory location
codepagedir="\$(LIBDIR)/codepages"
AC_ARG_WITH(codepagedir,
[  --with-codepagedir=DIR  Where to put codepage files (\$libdir/codepages)],
[ case "$withval" in
  yes|no)
  #
  # Just in case anybody does it
  #
    AC_MSG_WARN([--with-codepagedir called without argument - will use default])
  ;;
  * )
    codepagedir="$withval"
    ;;
  esac])

AC_MSG_CHECKING([configure summary])
AC_TRY_RUN([
#include <stdio.h>

main()
{
#if !(defined(HAVE_NETMASK_IFCONF) || defined(HAVE_NETMASK_IFREQ) || defined(HAVE_NETMASK_AIX))
    printf("WARNING: No automated netmask determination - use an interfaces line\n");
#endif

#if !((defined(HAVE_RANDOM) || defined(HAVE_RAND)) && (defined(HAVE_SRANDOM) || defined(HAVE_SRAND)))
    printf("ERROR: No random or srandom routine!\n");
    exit(1);
#endif

    exit(0);
}
],
           AC_MSG_RESULT(OK);,
	   AC_MSG_RESULT(failure)
	   AC_MSG_ERROR([Aborting config]),:)

builddir=`pwd`
AC_SUBST(builddir)
AC_SUBST(configdir)
AC_SUBST(codepagedir)

AC_OUTPUT([include/stamp-h Makefile])
