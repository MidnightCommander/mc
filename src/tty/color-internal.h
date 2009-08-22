
/** \file color-internal.h
 *  \brief Header: Internal stuff of color setup
 */

#ifndef MC_COLOR_INTERNAL_H
#define MC_COLOR_INTERNAL_H

#include <sys/types.h>			/* size_t */

#include "../../src/global.h"

#ifdef HAVE_SLANG
#   include "../../src/tty/tty-slang.h"
#else
#   include "../../src/tty/tty-ncurses.h"
#endif			/* HAVE_SLANG */

extern gboolean use_colors;

struct color_table_s {
    const char *name;
    int  value;
};

extern const struct color_table_s color_table [];

#ifdef HAVE_SLANG
#   define CTYPE const char *
#else
#   define CTYPE int
#endif			/* HAVE_SLANG */

struct colorpair {
    const char *name;		/* Name of the entry */
    CTYPE fg;			/* foreground color */
    CTYPE bg;			/* background color */
};

extern struct colorpair color_map [];

#ifdef HAVE_SLANG
#   define color_value(i) color_table [i].name
#   define color_name(i)  color_table [i].name

#   define color_map_fg(n) color_map [n].fg
#   define color_map_bg(n) color_map [n].bg
#else
#   define color_value(i) color_table [i].value
#   define color_name(i)  color_table [i].name

#   define color_map_fg(n) (color_map [n].fg & COLOR_WHITE)
#   define color_map_bg(n) (color_map [n].bg & COLOR_WHITE)
#endif			/* HAVE_SLANG */

static const char default_colors[] = {
	"normal=lightgray,blue:"
	"selected=black,cyan:"
	"marked=yellow,blue:"
	"markselect=yellow,cyan:"
	"errors=white,red:"
	"menu=white,cyan:"
	"reverse=black,lightgray:"
	"dnormal=black,lightgray:"
	"dfocus=black,cyan:"
	"dhotnormal=blue,lightgray:"
	"dhotfocus=blue,cyan:"
	"viewunderline=brightred,blue:"
	"menuhot=yellow,cyan:"
	"menusel=white,black:"
	"menuhotsel=yellow,black:"
	"helpnormal=black,lightgray:"
	"helpitalic=red,lightgray:"
	"helpbold=blue,lightgray:"
	"helplink=black,cyan:"
	"helpslink=yellow,blue:"
	"gauge=white,black:"
	"input=black,cyan:"
	"directory=white,blue:"
	"executable=brightgreen,blue:"
	"link=lightgray,blue:"
	"stalelink=brightred,blue:"
	"device=brightmagenta,blue:"
	"core=red,blue:"
	"special=black,blue:"
	"editnormal=lightgray,blue:"
	"editbold=yellow,blue:"
	"editmarked=black,cyan:"
	"editwhitespace=brightblue,blue:"
	"editlinestate=white,cyan:"
	"errdhotnormal=yellow,red:"
	"errdhotfocus=yellow,lightgray"
};

struct colors_avail {
    struct colors_avail *next;
    char *fg, *bg;
    int index;
};

extern struct colors_avail c;
extern int max_index;

size_t color_table_len (void);
size_t color_map_len (void);

void configure_colors (void);
void get_color (const char *cpp, CTYPE *colp);
int alloc_color_pair (CTYPE foreground, CTYPE background);

#endif				/* MC_COLOR_INTERNAL_H */
