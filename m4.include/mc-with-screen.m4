m4_include([m4.include/mc-with-screen-ncurses.m4])
m4_include([m4.include/mc-with-screen-slang.m4])

dnl
dnl Select the screen library.
dnl

AC_DEFUN([mc_WITH_SCREEN], [

    AC_ARG_WITH([screen],
        AS_HELP_STRING([--with-screen=@<:@LIB@:>@],
                       [Compile with screen library: slang or ncurses @<:@slang if found@:>@]))

    case x$with_screen in
    x | xslang)
        mc_WITH_SLANG
        ;;
    xncurses)
        mc_WITH_NCURSES
        ;;
    *)
        AC_MSG_ERROR([Value of the screen library is incorrect])
        ;;
    esac
])
