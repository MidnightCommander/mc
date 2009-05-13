
/** \file color-internal.c
 *  \brief Source: Internal stuff of color setup
 */

#include <config.h>

#include <sys/types.h>				/* size_t */

#include "../../src/tty/color.h"		/* colors and attributes */
#include "../../src/tty/color-internal.h"

gboolean disable_colors = FALSE;
gboolean force_colors = FALSE;	/* unused with NCurses */

struct color_table_s const color_table [] = {
    { "black",         COLOR_BLACK   },
    { "gray",          COLOR_BLACK   | A_BOLD },
    { "red",           COLOR_RED     },
    { "brightred",     COLOR_RED     | A_BOLD },
    { "green",         COLOR_GREEN   },
    { "brightgreen",   COLOR_GREEN   | A_BOLD },
    { "brown",         COLOR_YELLOW  },
    { "yellow",        COLOR_YELLOW  | A_BOLD },
    { "blue",          COLOR_BLUE    },
    { "brightblue",    COLOR_BLUE    | A_BOLD },
    { "magenta",       COLOR_MAGENTA },
    { "brightmagenta", COLOR_MAGENTA | A_BOLD },
    { "cyan",          COLOR_CYAN    },
    { "brightcyan",    COLOR_CYAN    | A_BOLD },
    { "lightgray",     COLOR_WHITE },
    { "white",         COLOR_WHITE   | A_BOLD },
    { "default",       0 } /* default color of the terminal */
};

struct colorpair color_map [] = {
    { "normal=",     0, 0 },	/* normal */               /*  1 */
    { "selected=",   0, 0 },	/* selected */
    { "marked=",     0, 0 },	/* marked */
    { "markselect=", 0, 0 },	/* marked/selected */
    { "errors=",     0, 0 },	/* errors */
    { "menu=",       0, 0 },	/* menu entry */
    { "reverse=",    0, 0 },	/* reverse */

    /* Dialog colors */
    { "dnormal=",    0, 0 },	/* Dialog normal */        /*  8 */
    { "dfocus=",     0, 0 },	/* Dialog focused */
    { "dhotnormal=", 0, 0 },	/* Dialog normal/hot */
    { "dhotfocus=",  0, 0 },	/* Dialog focused/hot */

    { "viewunderline=", 0, 0 },	/* _\b? sequence in view, underline in editor */
    { "menusel=",    0, 0 },	/* Menu selected color */  /* 13 */
    { "menuhot=",    0, 0 },	/* Color for menu hotkeys */
    { "menuhotsel=", 0, 0 },	/* Menu hotkeys/selected entry */

    { "helpnormal=", 0, 0 },	/* Help normal */          /* 16 */
    { "helpitalic=", 0, 0 },	/* Italic in help */
    { "helpbold=",   0, 0 },	/* Bold in help */
    { "helplink=",   0, 0 },	/* Not selected hyperlink */
    { "helpslink=",  0, 0 },	/* Selected hyperlink */

    { "gauge=",      0, 0 },	/* Color of the progress bar (percentage) *//* 21 */
    { "input=",      0, 0 },

    /* Per file types colors */
    { "directory=",  0, 0 },                               /*  23 */
    { "executable=", 0, 0 },
    { "link=",       0, 0 },	/* symbolic link (neither stale nor link to directory) */
    { "stalelink=",  0, 0 },	/* stale symbolic link */
    { "device=",     0, 0 },
    { "special=",    0, 0 },	/* sockets, fifo */
    { "core=",       0, 0 },	/* core files */              /* 29 */

    { 0,             0, 0 },	/* not usable (DEFAULT_COLOR_INDEX) *//* 30 */
    { 0,             0, 0 },	/* unused */
    { 0,             0, 0 },	/* not usable (A_REVERSE) */
    { 0,             0, 0 },	/* not usable (A_REVERSE_BOLD) */

    /* editor colors start at 34 */
    { "editnormal=",     0, 0 },	/* normal */       /* 34 */
    { "editbold=",       0, 0 },	/* search->found */
    { "editmarked=",     0, 0 },	/* marked/selected */
    { "editwhitespace=", 0, 0 },	/* whitespace */

    /* error dialog colors start at 38 */
    { "errdhotnormal=",  0, 0 },	/* Error dialog normal/hot */ /* 38 */
    { "errdhotfocus=",   0, 0 },	/* Error dialog focused/hot */
};

size_t
color_table_len (void)
{
    return sizeof (color_table)/sizeof(color_table [0]);
}

size_t
color_map_len (void)
{
    return sizeof (color_map)/sizeof(color_map [0]);
}
