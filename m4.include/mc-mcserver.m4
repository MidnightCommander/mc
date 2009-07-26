dnl MC_MCSERVER_CHECKS
dnl    Check how mcserver should check passwords.
dnl    Possible methods are PAM, pwdauth and crypt.
dnl    The later works with both /etc/shadow and /etc/passwd.
dnl    If PAM is found, other methods are not checked.

AC_DEFUN([MC_MCSERVER_CHECKS], [

    dnl
    dnl mcfs support
    dnl
    AC_ARG_ENABLE([mcserver],
        [  --enable-mcserver              Support mc-specific networking file system server [[no]]],
        [if test "x$enableval" != "xno"; then
            AC_DEFINE(ENABLE_MCSERVER, 1, [Define to enable mc-specific networking file system server])
            AC_MC_VFS_ADDNAME([mcfs])
            enable_mcserver=yes
        fi]
      )

    if test x"$enable_mcserver" = "xyes"; then
        AC_MC_VFS_MCFS_SET

        dnl Check if PAM can be used for mcserv
        AC_CHECK_LIB(dl, dlopen, [LIB_DL="-ldl"])
        AC_CHECK_LIB(pam, pam_start, [
            AC_DEFINE(HAVE_PAM, 1,
                  [Define if PAM (Pluggable Authentication Modules) is available])
            MCSERVLIBS="-lpam $LIB_DL"
            mcserv_pam=yes], [], [$LIB_DL])

        dnl Check for crypt() - needed for both /etc/shadow and /etc/passwd.
        if test x"$mcserv_pam" = x; then

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
    fi
    AC_SUBST(MCSERVLIBS)
])
