#ifndef __NCURSES
#define __NCURSES

#ifdef USE_BSD_CURSES

/* This is only to let people that don't want to install ncurses */
/* run this nice program; they get what they deserve.            */

    /* Ultrix has a better curses: cursesX */
    #ifdef ultrix
	#include <cursesX.h>
    #else
	#include <curses.h>
    #endif

    #ifndef ACS_VLINE
	#define ACS_VLINE '|'
    #endif

    #ifndef ACS_HLINE
	#define ACS_HLINE '-'
    #endif

    #ifndef ACS_ULCORNER
	#define ACS_ULCORNER '+'
    #endif

    #ifndef ACS_LLCORNER
	#define ACS_LLCORNER '+'
    #endif

    #ifndef ACS_URCORNER
	#define ACS_URCORNER '+'
    #endif

    #ifndef ACS_LRCORNER
	#define ACS_LRCORNER '+'
    #endif

    #ifndef ACS_LTEE
	#define ACS_LTEE '+'
    #endif

    #ifndef KEY_BACKSPACE
        #define KEY_BACKSPACE 0
    #endif

    #ifndef KEY_END
	#define KEY_END 0
    #endif

    enum {
	COLOR_BLACK, COLOR_RED,     COLOR_GREEN, COLOR_YELLOW,
	COLOR_BLUE,  COLOR_MAGENTA, COLOR_CYAN,  COLOR_WHITE
    };

    int has_colors (void);
    int init_pair (int, int, int);
    #define ACS_MAP(x) '*'
    #define COLOR_PAIR(x) 1

    #define xgetch x_getch
    #define wtouchln(win,b,c,d) touchwin(win)
    #define untouchwin(win)
    #define derwin(win,x,y,z,w) win
    #define wscrl(win,n)

#else  /* if not USE_BSD_CURSES then ...*/

/* Use this file only under System V */
#if defined(USE_SYSV_CURSES)
    #include <curses.h>
    #ifdef INCLUDE_TERM
        #include <term.h>
        /* Ugly hack to avoid name space pollution */
        #undef cols
        #undef lines
        #undef buttons

        #define TERM_INCLUDED 1
    #endif

    #if defined(sparc) || defined(__sgi) || defined(_SGI_SOURCE)
        /* We are dealing with Solaris or SGI buggy curses :-) */
        #define BUGGY_CURSES 1
    #endif

    #if defined(mips) && defined(sgi)
        /* GNU C compiler, buggy sgi */
        #define BUGGY_CURSES 1
    #endif
				 
#else
    /* This is required since ncurses 1.8.6 and newer changed the name of */
    /* the include files (argh!) */
    #ifdef RENAMED_NCURSES
        #include <curses.h>

        #ifdef INCLUDE_TERM
            #include <term.h>
            #define TERM_INCLUDED 1
        #endif
    #else
        #error The ncurses.h file should only be included under SystemV.
        #error BSD systems and Linux require the real ncurses package
    #endif
#endif

#endif /* USE_BSD_CURSES */
#endif /* __NCURSES_H */
