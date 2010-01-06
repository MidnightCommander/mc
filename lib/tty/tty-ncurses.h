
#ifndef MC_TTY_NCURSES_H
#define MC_TTY_NCURSES_H

#ifdef USE_NCURSES
#    ifdef HAVE_NCURSES_CURSES_H
#        include <ncurses/curses.h>
#    elif HAVE_NCURSESW_CURSES_H
#        include <ncursesw/curses.h>
#    elif HAVE_NCURSES_H
#        include <ncurses.h>
#    else
#        include <curses.h>
#    endif
#endif /* USE_NCURSES */

#ifdef USE_NCURSESW
#   include <ncursesw/curses.h>
#endif /* USE_NCURSESW */

#endif /* MC_TTY_NCURSES_H */
