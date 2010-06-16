m4_include([m4.include/mc-with-screen-ncurses.m4])
m4_include([m4.include/mc-with-screen-slang.m4])

dnl
dnl Select the screen library.
dnl

AC_DEFUN([MC_WITH_SCREEN], [

    AC_ARG_WITH(screen,
        [  --with-screen=LIB        Compile with screen library: slang or
                           ncurses [[slang if found]]])

    case x$with_screen in
    xslang)
        MC_WITH_SLANG(strict)
        ;;
    xncurses)
        MC_WITH_NCURSES
        ;;
    xncursesw)
        MC_WITH_NCURSESW
        ;;
    x)
        MC_WITH_SLANG
        ;;
    *)
        AC_MSG_ERROR([Value of the screen library is incorrect])
        ;;
    esac
])
