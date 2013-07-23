
#ifndef MC__TTY_NCURSES_H
#define MC__TTY_NCURSES_H

#ifdef USE_NCURSES
#ifdef HAVE_NCURSES_CURSES_H
#include <ncurses/curses.h>
#elif defined (HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#elif defined (HAVE_NCURSESW_CURSES_H)
#include <ncursesw/curses.h>
#elif defined (HAVE_NCURSES_HCURSES_H) || defined (HAVE_NCURSES_H)
#include <ncurses.h>
#else
#include <curses.h>
#endif
#endif /* USE_NCURSES */

#ifdef USE_NCURSESW
#include <ncursesw/curses.h>
#endif /* USE_NCURSESW */

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/*** inline functions ****************************************************************************/

#endif /* MC_TTY_NCURSES_H */
