dnl @synopsis mc_ENCODING
dnl
dnl Clarify encoding names in mc.charset
dnl
dnl @author Slava Zanko <slavazanko@gmail.com>
dnl @modified Yury V. Zaytsev <yury@shurup.com>
dnl @modified Andrew Borodin <aborodin@vmail.ru>
dnl @version 2025-03-22
dnl @license GPL
dnl @copyright Free Software Foundation, Inc.

AC_DEFUN([mc_ENCODING],[
    AC_CHECK_HEADERS([gnu/libc-version.h])

    dnl Solaris has different name of Windows 1251 encoding
    case $host_os in
        solaris*)
            ENCODING_CP1251="ANSI-1251"
            ;;
        *)
            ENCODING_CP1251="CP1251"
            ;;
    esac

    if test "x$ac_cv_header_gnu_libc_version_h" != "xno"; then
        ENCODING_CP866="IBM866"
        ENCODING_ISO8859="ISO-8859"
    else
        ENCODING_CP866="CP866"
        ENCODING_ISO8859="ISO8859"
    fi

    AC_SUBST(ENCODING_CP1251)
    AC_SUBST(ENCODING_CP866)
    AC_SUBST(ENCODING_ISO8859)
])
