dnl @synopsis mc_CHECK_CFLAGS
dnl
dnl Check flags supported by C compiler
dnl
dnl @author Slava Zanko <slavazanko@gmail.com>
dnl @modified by Andrew Borodin <aborodin@vmail.ru>
dnl @version 2021-09-19
dnl @license GPL
dnl @copyright Free Software Foundation, Inc.

AC_DEFUN([mc_CHECK_CFLAGS],[
    AC_LANG_PUSH(C)

    mc_configured_cflags=""

    dnl Sorted -f options:
    case "$CC" in
    gcc*)
        AX_APPEND_COMPILE_FLAGS([-fdiagnostics-show-option], [mc_configured_cflags])
dnl        AX_APPEND_COMPILE_FLAGS([-fno-stack-protector], [mc_configured_cflags])
        ;;
    *)
        ;;
    esac

    dnl Sorted -W options:
    AX_APPEND_COMPILE_FLAGS([-Wassign-enum], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wbad-function-cast], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wcomment], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wconditional-uninitialized], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wdeclaration-after-statement], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wfloat-conversion], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wfloat-equal], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wformat], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wformat-security], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wformat-signedness], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wimplicit], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wimplicit-fallthrough], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wignored-qualifiers], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wlogical-not-parentheses], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wmaybe-uninitialized], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wmissing-braces], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wmissing-declarations], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wmissing-field-initializers], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wmissing-format-attribute], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wmissing-parameter-type], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wmissing-prototypes], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wmissing-variable-declarations], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wnested-externs], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wno-long-long], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wno-unreachable-code], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wparentheses], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wpointer-arith], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wpointer-sign], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wredundant-decls], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wreturn-type], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wsequence-point], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wshadow], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wsign-compare], [mc_configured_cflags], [$EXTRA_OPTION])
dnl    AX_APPEND_COMPILE_FLAGS([-Wstrict-aliasing], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wstrict-prototypes], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wswitch], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wswitch-default], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wtype-limits], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wundef], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wuninitialized], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wunreachable-code], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wunused-but-set-variable], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wunused-function], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wunused-label], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wunused-parameter], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wunused-result], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wunused-value], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wunused-variable], [mc_configured_cflags], [$EXTRA_OPTION])
    AX_APPEND_COMPILE_FLAGS([-Wwrite-strings], [mc_configured_cflags], [$EXTRA_OPTION])

    AC_LANG_POP()
])
