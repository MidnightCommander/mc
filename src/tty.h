#ifndef MC_TTY_H
#define MC_TTY_H

/* This file takes care of loading ncurses or slang */

int got_interrupt (void);

#ifdef HAVE_SLANG
#   include "myslang.h"
#else
#   define enable_interrupt_key()
#   define disable_interrupt_key()
#   define acs()
#   define noacs()
#   define one_vline() addch (slow_terminal ? ' ' : ACS_VLINE)
#   define one_hline() addch (slow_terminal ? ' ' : ACS_HLINE)
#endif

#ifdef USE_NCURSES
#    ifdef HAVE_NCURSES_CURSES_H
#        include <ncurses/curses.h>
#    elif HAVE_NCURSES_H
#        include <ncurses.h>
#    else
#        include <curses.h>
#    endif
#endif /* USE_NCURSES */

#define KEY_KP_ADD	4001
#define KEY_KP_SUBTRACT	4002
#define KEY_KP_MULTIPLY	4003

void mc_refresh (void);

/* print a string left-aligned, adjusted to exactly LEN characters */
static inline void printwstr (const char *s, int len)
{
    printw("%-*.*s", len, len, s);
}

#endif
