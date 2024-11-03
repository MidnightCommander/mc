dnl @synopsis mc_CHECK_CFLAGS
dnl
dnl Check flags supported by C compiler
dnl
dnl @author Slava Zanko <slavazanko@gmail.com>
dnl @modified by Andrew Borodin <aborodin@vmail.ru>
dnl @modified by Yury V. Zaytsev <yury@shurup.com>
dnl @version 2024-11-03
dnl @license GPL
dnl @copyright Free Software Foundation, Inc.

AC_DEFUN([mc_CHECK_CFLAGS],[
    AC_LANG_PUSH(C)

    mc_configured_cflags=""

    dnl Sorted -f options:
    case "$CC" in
    gcc*)
        AX_APPEND_COMPILE_FLAGS([-fdiagnostics-show-option], [mc_configured_cflags])
        ;;
    *)
        ;;
    esac

    AX_APPEND_COMPILE_FLAGS([-Wall], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wextra], [mc_configured_cflags], [$EXTRA_OPTION])

    dnl Enable support for C standard features up to (and including) C99 and pedantic warnings
    AX_APPEND_COMPILE_FLAGS([-Wattributes], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wimplicit-function-declaration], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wimplicit-int], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wno-declaration-after-statement], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wno-long-long], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wno-vla], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wincompatible-pointer-types], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wint-conversion], [mc_configured_cflags], [$EXTRA_OPTION])

    dnl Sorted -W options not included in -Wall and -Wextra
    AX_APPEND_COMPILE_FLAGS([-Wbad-function-cast], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wconditional-uninitialized], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wfloat-conversion], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wfloat-equal], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wformat-security], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wformat-signedness], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wimplicit-fallthrough], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wmissing-declarations], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wmissing-format-attribute], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wmissing-prototypes], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wmissing-variable-declarations], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wnested-externs], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wpointer-arith], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wredundant-decls], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wshadow], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wstrict-prototypes], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wswitch-default], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wundef], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wunreachable-code], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wunused-result], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wwrite-strings], [mc_configured_cflags], [$EXTRA_OPTION])

    dnl Explicitly disabled warnings

    dnl This flags casts like (GCompareDataFunc) with missing parameter
    AX_APPEND_COMPILE_FLAGS([-Wno-cast-function-type], [mc_configured_cflags], [$EXTRA_OPTION])

    dnl https://github.com/llvm/llvm-project/issues/20574
    AX_APPEND_COMPILE_FLAGS([-Wno-assign-enum], [mc_configured_cflags], [$EXTRA_OPTION])

    AC_LANG_POP()
])
