/* Color setup
   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007 Free Software Foundation, Inc.
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/** \file color.c
 *  \brief Source: color setup
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "tty.h"
#include "setup.h"		/* For the externs */
#include "color.h"

/* Set to force black and white display at program startup */
int   disable_colors = 0;

/* Set if we are actually using colors */
int use_colors = 0;

/* Color styles for normal and error dialogs */
int dialog_colors [4];
int alarm_colors [4];

#define ELEMENTS(arr) ( sizeof(arr) / sizeof((arr)[0]) )

#ifdef HAVE_SLANG
#   define color_map_fg(n) color_map[n].fg
#   define color_map_bg(n) color_map[n].bg
#else
#   define color_map_fg(n) (color_map[n].fg & COLOR_WHITE)
#   define color_map_bg(n) (color_map[n].bg & COLOR_WHITE)
#endif

struct colorpair {
    const char *name;		/* Name of the entry */
    CTYPE fg;			/* foreground color */
    CTYPE bg;			/* background color */
};

static struct colorpair color_map [] = {
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
    { "menuhot=",    0, 0 },    /* Color for menu hotkeys */
    { "menuhotsel=", 0, 0 },    /* Menu hotkeys/selected entry */
    
    { "helpnormal=", 0, 0 },    /* Help normal */          /* 16 */
    { "helpitalic=", 0, 0 },    /* Italic in help */
    { "helpbold=",   0, 0 },    /* Bold in help */
    { "helplink=",   0, 0 },    /* Not selected hyperlink */
    { "helpslink=",  0, 0 },    /* Selected hyperlink */
    
    { "gauge=",      0, 0 },    /* Color of the progress bar (percentage) *//* 21 */
    { "input=",      0, 0 },
 
    /* Per file types colors */
    { "directory=",  0, 0 },                               /*  23 */
    { "executable=", 0, 0 },
    { "link=",       0, 0 },  /* symbolic link (neither stale nor link to directory) */
    { "stalelink=",  0, 0 },  /* stale symbolic link */
    { "device=",     0, 0 },
    { "special=",    0, 0 }, /* sockets, fifo */
    { "core=",       0, 0 }, /* core files */              /* 29 */

    { 0,             0, 0 }, /* not usable (DEFAULT_COLOR_INDEX) *//* 30 */
    { 0,             0, 0 }, /* unused */
    { 0,             0, 0 }, /* not usable (A_REVERSE) */
    { 0,             0, 0 }, /* not usable (A_REVERSE_BOLD) */

/* editor colors start at 34 */
    { "editnormal=",     0, 0 },	/* normal */       /* 34 */
    { "editbold=",       0, 0 },	/* search->found */
    { "editmarked=",     0, 0 },	/* marked/selected */
    { "editwhitespace=", 0, 0 },	/* whitespace */
    { "editlinestate=", 0, 0 },	/* line number bar*/

/* error dialog colors start at 39 */
    { "errdhotnormal=",  0, 0 }, /* Error dialog normal/hot */ /* 39 */
    { "errdhotfocus=",   0, 0 }, /* Error dialog focused/hot */
};

struct color_table_s {
    const char *name;
    int  value;
};


static struct color_table_s const color_table [] = {
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

#ifdef HAVE_SLANG
#   define color_value(i) color_table [i].name
#   define color_name(i)  color_table [i].name
#else
#   define color_value(i) color_table [i].value
#   define color_name(i)  color_table [i].name
#endif

static void get_color (const char *cpp, CTYPE *colp)
{
    size_t i;
    
    for (i = 0; i < ELEMENTS(color_table); i++){
	if (strcmp (cpp, color_name (i)) == 0){
	    *colp = color_value (i);
            return;
	}
    }
}

static void get_two_colors (char **cpp, struct colorpair *colorpairp)
{
    char *p = *cpp;
    int state;

    state = 0;
    
    for (; *p; p++){
	if (*p == ':'){
	    *p = 0;
	    get_color (*cpp, state ? &colorpairp->bg : &colorpairp->fg);
	    *p = ':';
	    *cpp = p + 1;
	    return;
	}

	if (*p == ','){
	    state = 1;
	    *p = 0;
	    get_color (*cpp, &colorpairp->fg);
	    *p = ',';
	    *cpp = p + 1;
	}
    }
    get_color (*cpp, state ? &colorpairp->bg : &colorpairp->fg);
}

static void configure_colors_string (const char *the_color_string)
{
    char *color_string, *p;
    size_t i;
    int found;

    if (!the_color_string)
	return;

    p = color_string = g_strdup (the_color_string);
    while (color_string && *color_string){
	while (*color_string == ' ' || *color_string == '\t')
	    color_string++;

	found = 0;
	for (i = 0; i < ELEMENTS(color_map); i++){
	    int klen;

            if (!color_map [i].name)
                continue;
            klen = strlen (color_map [i].name);

	    if (strncmp (color_string, color_map [i].name, klen) == 0){
		color_string += klen;
		get_two_colors (&color_string, &color_map [i]);
		found = 1;
	    }
	}
	if (!found){
		while (*color_string && *color_string != ':')
			color_string++;
		if (*color_string)
			color_string++;
	}
    }
   g_free (p);
}

static void configure_colors (void)
{
    gchar *default_colors = g_strconcat (
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
	,
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
	"errdhotfocus=yellow,lightgray", NULL);

    extern char *command_line_colors;

    configure_colors_string (default_colors);
    configure_colors_string (setup_color_string);
    configure_colors_string (term_color_string);
    configure_colors_string (getenv ("MC_COLOR_TABLE"));
    configure_colors_string (command_line_colors);
    g_free(default_colors);
}

#ifndef HAVE_SLANG
#define MAX_PAIRS 64
int attr_pairs [MAX_PAIRS];
#endif

static void
load_dialog_colors (void)
{
    dialog_colors [0] = COLOR_NORMAL;
    dialog_colors [1] = COLOR_FOCUS;
    dialog_colors [2] = COLOR_HOT_NORMAL;
    dialog_colors [3] = COLOR_HOT_FOCUS;

    alarm_colors [0] = ERROR_COLOR;
    alarm_colors [1] = REVERSE_COLOR;
    alarm_colors [2] = ERROR_HOT_NORMAL;
    alarm_colors [3] = ERROR_HOT_FOCUS;
}

void init_colors (void)
{
    size_t i;
    
    int hascolors;

    /* FIXME: if S-Lang is used, this function must be called regardless
       of whether we are interested in its result */
    hascolors = has_colors ();

    if (!disable_colors && hascolors){
	use_colors = 1;
    }

    if (use_colors){
	start_color ();
#ifndef HAVE_SLANG
	use_default_colors ();
#endif /* !HAVE_SLANG */
	configure_colors ();

#ifndef HAVE_SLANG
	if (ELEMENTS (color_map) > MAX_PAIRS){
	    /* This message should only be seen by the developers */
	    fprintf (stderr,
		     "Too many defined colors, resize MAX_PAIRS on color.c");
	    exit (1);
	}
#endif /* !HAVE_SLANG */

	if (use_colors) {
#ifdef HAVE_SLANG
	    /*
	     * We are relying on undocumented feature of
	     * S-Lang to make COLOR_PAIR(DEFAULT_COLOR_INDEX)
	     * the default fg/bg of the terminal.
	     * Hopefully, future versions of S-Lang will
	     * document this feature.
	     */
	    SLtt_set_color (DEFAULT_COLOR_INDEX, NULL, (char*)"default", (char*)"default");
#else
	    /* Use default terminal colors */
	    mc_init_pair (DEFAULT_COLOR_INDEX, -1, -1);
#endif /* !HAVE_SLANG */
	}

	for (i = 0; i < ELEMENTS (color_map); i++){
            if (!color_map [i].name)
                continue;

	    mc_init_pair (i+1, color_map_fg(i), color_map_bg(i));

#ifndef HAVE_SLANG
	    /*
	     * ncurses doesn't remember bold attribute in the color pairs,
	     * so we should keep track of it in a separate array.
	     */
	     attr_pairs [i+1] = color_map [i].fg & A_BOLD;
#endif /* !HAVE_SLANG */
	}
    }
    load_dialog_colors ();
}

/* Functions necessary to implement syntax highlighting  */

static int max_index = 0;

static int
alloc_color_pair (CTYPE foreground, CTYPE background)
{
    mc_init_pair (++max_index, foreground, background);
    return max_index;
}

static struct colors_avail {
    struct colors_avail *next;
    char *fg, *bg;
    int index;
} c = { 0, 0, 0, 0 };

#ifdef HAVE_SLANG
void
mc_init_pair (int index, CTYPE foreground, CTYPE background)
{
    if (!background)
	background = "default";

    if (!foreground)
	foreground = "default";

    SLtt_set_color (index, (char*)"", (char *) foreground, (char *) background);
    if (index > max_index)
	max_index = index;
}

int
try_alloc_color_pair (const char *fg, const char *bg)
{
    struct colors_avail *p = &c;

    c.index = EDITOR_NORMAL_COLOR_INDEX;
    for (;;) {
	if (((fg && p->fg) ? !strcmp (fg, p->fg) : fg == p->fg) != 0
	    && ((bg && p->bg) ? !strcmp (bg, p->bg) : bg == p->bg) != 0)
	    return p->index;
	if (!p->next)
	    break;
	p = p->next;
    }
    p->next = g_new (struct colors_avail, 1);
    p = p->next;
    p->next = 0;
    p->fg = fg ? g_strdup (fg) : 0;
    p->bg = bg ? g_strdup (bg) : 0;
    if (!fg)
        /* Index in color_map array = COLOR_INDEX - 1 */
	fg = color_map[EDITOR_NORMAL_COLOR_INDEX - 1].fg;
    if (!bg)
	bg = color_map[EDITOR_NORMAL_COLOR_INDEX - 1].bg;
    p->index = alloc_color_pair (fg, bg);
    return p->index;
}

#else /* !HAVE_SLANG */
void
mc_init_pair (int index, CTYPE foreground, CTYPE background)
{
    init_pair (index, foreground, background == 0 ? -1 : background);
    if (index > max_index)
	max_index = index;
}

int
try_alloc_color_pair (const char *fg, const char *bg)
{
    int fg_index, bg_index;
    int bold_attr;
    struct colors_avail *p = &c;

    c.index = EDITOR_NORMAL_COLOR_INDEX;
    for (;;) {
	if (((fg && p->fg) ? !strcmp (fg, p->fg) : fg == p->fg) != 0
	    && ((bg && p->bg) ? !strcmp (bg, p->bg) : bg == p->bg) != 0)
	    return p->index;
	if (!p->next)
	    break;
	p = p->next;
    }
    p->next = g_new (struct colors_avail, 1);
    p = p->next;
    p->next = 0;
    p->fg = fg ? g_strdup (fg) : 0;
    p->bg = bg ? g_strdup (bg) : 0;
    if (!fg)
        /* Index in color_map array = COLOR_INDEX - 1 */
	fg_index = color_map[EDITOR_NORMAL_COLOR_INDEX - 1].fg;
    else
	get_color (fg, &fg_index);

    if (!bg)
	bg_index = color_map[EDITOR_NORMAL_COLOR_INDEX - 1].bg;
    else
	get_color (bg, &bg_index);

    bold_attr = fg_index & A_BOLD;
    fg_index = fg_index & COLOR_WHITE;
    bg_index = bg_index & COLOR_WHITE;

    p->index = alloc_color_pair (fg_index, bg_index);
    attr_pairs [p->index] = bold_attr;
    return p->index;
}
#endif /* !HAVE_SLANG */

void
done_colors (void)
{
    struct colors_avail *p, *next;

    for (p = c.next; p; p = next) {
	next = p->next;
	g_free (p->fg);
	g_free (p->bg);
	g_free (p);
    }
    c.next = NULL;
}

