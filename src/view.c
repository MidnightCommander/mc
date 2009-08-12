/*
   Internal file viewer for the Midnight Commander

   Copyright (C) 1994, 1995, 1996 The Free Software Foundation

   Written by: 1994, 1995, 1998 Miguel de Icaza
	       1994, 1995 Janne Kukonlehto
	       1995 Jakub Jelinek
	       1996 Joseph M. Hinkle
	       1997 Norbert Warmuth
	       1998 Pavel Machek
	       2004 Roland Illig <roland.illig@gmx.de>
	       2005 Roland Illig <roland.illig@gmx.de>

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file view.c
 *  \brief Source: internal file viewer
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "global.h"

#include "../src/tty/tty.h"
#include "../src/tty/color.h"
#include "../src/tty/key.h"
#include "../src/tty/mouse.h"

#include "cmd.h"		/* For view_other_cmd */
#include "dialog.h"		/* Needed by widget.h */
#include "widget.h"		/* Needed for buttonbar_new */
#include "help.h"
#include "layout.h"
#include "setup.h"
#include "wtools.h"		/* For query_set_sel() */
#include "dir.h"
#include "panel.h"		/* Needed for current_panel and other_panel */
#include "execute.h"
#include "main.h"		/* source_codepage */
#include "view.h"
#include "history.h"		/* MC_HISTORY_SHARED_SEARCH */
#include "charsets.h"
#include "selcodepage.h"
#include "strutil.h"
#include "../src/search/search.h"

/* Block size for reading files in parts */
#define VIEW_PAGE_SIZE		((size_t) 8192)

typedef unsigned char byte;

/* Offset in bytes into a file */
typedef unsigned long offset_type;
#define INVALID_OFFSET ((offset_type) -1)
#define OFFSETTYPE_MAX (~((offset_type) 0))
#define OFFSETTYPE_PRIX		"lX"
#define OFFSETTYPE_PRId		"lu"

/* A width or height on the screen */
typedef unsigned int screen_dimen;

/* A node for building a change list on change_list */
struct hexedit_change_node {
   struct hexedit_change_node *next;
   offset_type                offset;
   byte                       value;
};

/* data sources of the view */
enum view_ds {
    DS_NONE,			/* No data available */
    DS_STDIO_PIPE,		/* Data comes from a pipe using popen/pclose */
    DS_VFS_PIPE,		/* Data comes from a piped-in VFS file */
    DS_FILE,			/* Data comes from a VFS file */
    DS_STRING			/* Data comes from a string in memory */
};

struct area {
    screen_dimen top, left;
    screen_dimen height, width;
};

#define VLF_DISCARD  1
#define VLF_INIT     2
#define VLF_COMPLETE 3

/* basic structure for caching text, it correspond to one line in wrapped text.
 * That makes easier to move in wrap mode. 
 * cache_lines are stored in two linked lists, one for normal mode 
 * and one for nroff mode 
 * cache_line is valid of end is not INVALID_OFFSET
 * last line has set width to (screen_dimen) (-1)*/
/* never access next and previous in cache_line directly, use appropriately function
 * instead */
struct cache_line {
    /* number of line in text, so two cache_line have same number 
     * if they are on same text line (text line is wider than screen) */
    unsigned long number;
    /* previous cache_line in list*/
    struct cache_line *prev;
    /* next cache_line in list*/
    struct cache_line *next;
    /* offset when cache_line start in text, 
     * cache_line.start = cache_line->prev.end */
    offset_type start;
    /* offset when cache_line ends 
     * cache_line.end = cache_line->next.start */
    offset_type end;
    /* how many column take on screen */
    screen_dimen width;
    /* correct ident, if prevoius line ends by tabulator */
    screen_dimen left;
};

/* this structure and view_read_* functions make reading text simpler 
 * view need remeber 4 following charsets: actual, next and two previous */
struct read_info {
    char ch[4][MB_LEN_MAX + 1];
    char *cnxt;
    char *cact;
    char *chi1;
    char *chi2;
    offset_type next;
    offset_type actual;
    int result;
};


struct WView {
    Widget widget;

    char *filename;		/* Name of the file */
    char *command;		/* Command used to pipe data in */

    enum view_ds datasource;	/* Where the displayed data comes from */

    /* stdio pipe data source */
    FILE  *ds_stdio_pipe;	/* Output of a shell command */

    /* vfs pipe data source */
    int    ds_vfs_pipe;		/* Non-seekable vfs file descriptor */

    /* vfs file data source */
    int    ds_file_fd;		/* File with random access */
    off_t  ds_file_filesize;	/* Size of the file */
    off_t  ds_file_offset;	/* Offset of the currently loaded data */
    byte  *ds_file_data;	/* Currently loaded data */
    size_t ds_file_datalen;	/* Number of valid bytes in file_data */
    size_t ds_file_datasize;	/* Number of allocated bytes in file_data */

    /* string data source */
    byte  *ds_string_data;	/* The characters of the string */
    size_t ds_string_len;	/* The length of the string */

    /* Growing buffers information */
    gboolean growbuf_in_use;	/* Use the growing buffers? */
    byte   **growbuf_blockptr;	/* Pointer to the block pointers */
    size_t   growbuf_blocks;	/* The number of blocks in *block_ptr */
    size_t   growbuf_lastindex;	/* Number of bytes in the last page of the
				   growing buffer */
    gboolean growbuf_finished;	/* TRUE when all data has been read. */

    /* Editor modes */
    gboolean hex_mode;		/* Hexview or Hexedit */
    gboolean hexedit_mode;	/* Hexedit */
    gboolean hexview_in_text;	/* Is the hexview cursor in the text area? */
    gboolean text_nroff_mode;	/* Nroff-style highlighting */
    gboolean text_wrap_mode;	/* Wrap text lines to fit them on the screen */
    gboolean magic_mode;	/* Preprocess the file using external programs */

    /* Additional editor state */
    gboolean hexedit_lownibble;	/* Are we editing the last significant nibble? */

    /* Display information */
    screen_dimen dpy_frame_size;/* Size of the frame surrounding the real viewer */
    offset_type dpy_start;	/* Offset of the displayed data */
    offset_type dpy_end;	/* Offset after the displayed data */
    offset_type dpy_text_column;/* Number of skipped columns in non-wrap
				 * text mode */
    offset_type hex_cursor;	/* Hexview cursor position in file */
    screen_dimen cursor_col;	/* Cursor column */
    screen_dimen cursor_row;	/* Cursor row */
    struct hexedit_change_node *change_list;   /* Linked list of changes */
    struct area status_area;	/* Where the status line is displayed */
    struct area ruler_area;	/* Where the ruler is displayed */
    struct area data_area;	/* Where the data is displayed */

    int dirty;			/* Number of skipped updates */
    gboolean dpy_bbar_dirty;	/* Does the button bar need to be updated? */

    /* Mode variables */
    int bytes_per_line;		/* Number of bytes per line in hex mode */

    /* Search variables */
    offset_type search_start;	/* First character to start searching from */
    offset_type search_end;	/* Length of found string or 0 if none was found */

				/* Pointer to the last search command */
    gboolean want_to_quit;	/* Prepare for cleanup ... */

    /* Markers */
    int marker;			/* mark to use */
    offset_type marks [10];	/* 10 marks: 0..9 */

    int  move_dir;		/* return value from widget:
				 * 0 do nothing
				 * -1 view previous file
				 * 1 view next file
				 */

    offset_type update_steps;	/* The number of bytes between percent
				 * increments */
    offset_type update_activate;/* Last point where we updated the status */
    
    /* never access cache_line in view directly, use appropriately function
     * instead */
    /* first cache_line for normal mode */
    struct cache_line *lines;
    /* last cache_line for normal mode, is set when text is read to the end */
    struct cache_line *lines_end;
    /* first cache_line for nroff mode */
    struct cache_line *nroff_lines;
    /* last cache_line for nroff mode, is set when text is read to the end */
    struct cache_line *nroff_lines_end;
    /* cache_line, that is first showed on display (something like cursor)
     * used for both normal adn nroff mode */
    struct cache_line *first_showed_line;
    /* converter for translation of text */
    GIConv converter;

    /* handle of search engine */
    mc_search_t *search;
    gchar *last_search_string;
    mc_search_type_t search_type;
    gboolean search_all_codepages;
    gboolean search_case;
    gboolean search_backwards;

    int search_numNeedSkipChar;
    struct read_info search_onechar_info;
};


/* {{{ Global Variables }}} */

/* Maxlimit for skipping updates */
int max_dirt_limit = 10;

/* If set, show a ruler */
static enum ruler_type {
    RULER_NONE,
    RULER_TOP,
    RULER_BOTTOM
} ruler = RULER_NONE;

/* Scrolling is done in pages or line increments */
int mouse_move_pages_viewer = 1;

/* wrap mode default */
int global_wrap_mode = 1;

int default_hex_mode = 0;
int default_magic_flag = 1;
int default_nroff_flag = 1;
int altered_hex_mode = 0;
int altered_magic_flag = 0;
int altered_nroff_flag = 0;

static const char hex_char[] = "0123456789ABCDEF";

int mcview_remember_file_position = FALSE;

/* {{{ Function Prototypes }}} */

/* Our widget callback */
static cb_ret_t view_callback (Widget *, widget_msg_t, int);

static void view_labels (WView * view);

static void view_init_growbuf (WView *);
static void view_place_cursor (WView *view);
static void display (WView *);
static void view_done (WView *);

/* {{{ Helper Functions }}} */

/* difference or zero */
static inline screen_dimen
dimen_doz (screen_dimen a, screen_dimen b)
{
	return (a >= b) ? a - b : 0;
}

static inline screen_dimen
dimen_min (screen_dimen a, screen_dimen b)
{
    return (a < b) ? a : b;
}

static inline offset_type
offset_doz (offset_type a, offset_type b)
{
	return (a >= b) ? a - b : 0;
}

static inline offset_type
offset_rounddown (offset_type a, offset_type b)
{
	assert (b != 0);
	return a - a % b;
}

/* {{{ Simple Primitive Functions for WView }}} */

static inline gboolean
view_is_in_panel (WView *view)
{
    return (view->dpy_frame_size != 0);
}

static inline int get_byte (WView *view, offset_type offset);
        
/* read on character from text, character is translated into terminal encoding
 * writes result in ch, return how many bytes was read or -1 if end of text */
static int
view_get_char (WView *view, offset_type from, char *ch, int size) 
{
    static char buffer [MB_LEN_MAX + 1];
    static int c;
    static size_t result;
    
    result = 0;
    
    while (result < sizeof (buffer) - 1) {
        c = get_byte (view, from + result);
        if (c == -1) break;
        buffer[result] = (unsigned char) c;
        result++;
        buffer[result] = '\0';
        switch (str_translate_char (view->converter, buffer, result, ch, size)) {
            case ESTR_SUCCESS:
                return (int) result;
            case ESTR_PROBLEM:
                break;
            case ESTR_FAILURE:
                ch[0] = '?';
                ch[1] = '\0';
                return 1;
        }
    }
    return -1;
}


/* set read_info into initial state and read first character to cnxt 
 * return how many bytes was read or -1 if end of text */
static void
view_read_start (WView *view, struct read_info *info, offset_type from)
{
    info->ch[0][0] = '\0';
    info->ch[1][0] = '\0';
    info->ch[2][0] = '\0';
    info->ch[3][0] = '\0';
    
    info->cnxt = info->ch[0];
    info->cact = info->ch[1];
    info->chi1 = info->ch[2];
    info->chi2 = info->ch[3];
    
    info->next = from;
    
    info->result = view_get_char (view, info->next, info->cnxt, MB_LEN_MAX + 1);
}

/* move characters in read_info one forward (chi2 = last previous is forgotten)
 * read additional charsets into cnxt 
 * return how many bytes was read or -1 if end of text */
static void
view_read_continue (WView *view, struct read_info *info)
{
    char *tmp;
    
    tmp = info->chi2;
    info->chi2 = info->chi1;
    info->chi1 = info->cact;
    info->cact = info->cnxt;
    info->cnxt = tmp;
    
    info->actual = info->next;
    info->next+= info->result;
    
    info->result = view_get_char (view, info->next, info->cnxt, MB_LEN_MAX + 1);
}        

#define VRT_NW_NO 0
#define VRT_NW_YES 1
#define VRT_NW_CONTINUE 2

/* test if cact of read_info is newline
 * return VRT_NW_NO, VRT_NW_YES 
 * or VRT_NW_CONTINUE follow after cact ("\r\n", ...) */
static int
view_read_test_new_line (WView *view, struct read_info *info)
{
#define cmp(t1,t2) (strcmp((t1),(t2)) == 0)
    (void) view;
    if (cmp (info->cact, "\n")) return VRT_NW_YES;

    if (cmp (info->cact, "\r")) {
        if (info->result != -1 && (cmp (info->cnxt, "\r") || cmp (info->cnxt, "\n")))
            return VRT_NW_CONTINUE;

        return VRT_NW_YES;
    }
    return VRT_NW_NO;
}
        
/* test if cact of read_info is tabulator
 * return VRT_NW_NO or VRT_NW_YES */     
static int 
view_read_test_tabulator (WView *view, struct read_info *info)
{
#define cmp(t1,t2) (strcmp((t1),(t2)) == 0)
    (void) view;
    return cmp (info->cact, "\t");
}                
        
/* test if read_info.cact is '\b' and charsets in read_info are correct 
 * nroff sequence, return VRT_NW_NO or VRT_NW_YES */
static int
view_read_test_nroff_back (WView *view, struct read_info *info)
{
#define cmp(t1,t2) (strcmp((t1),(t2)) == 0)
    return view->text_nroff_mode && cmp (info->cact, "\b") && 
            (info->result != -1) && str_isprint (info->cnxt) && 
            !cmp (info->chi1, "") && str_isprint (info->chi1) && 
            (cmp (info->chi1, info->cnxt) || cmp (info->chi1, "_") 
                    || (cmp (info->chi1, "+") && cmp (info->cnxt, "o")));
    
}                
        
/* rutine for view_load_cache_line, line is ended and following cache_line is
 * set up for loading 
 * used when end of line has been read in text */
static struct cache_line *
view_lc_create_next_line (struct cache_line *line, offset_type start) 
{
    struct cache_line *result = NULL;
    
    line->end = start;
  
    if (line->next == NULL) {
        result = g_new0 (struct cache_line, 1);
        line->next = result;
        result->prev = line;
    } else result = line->next;
    
    result->start = start;
    result->width = 0;
    result->left = 0;
    result->number = line->number + 1;
    result->end = INVALID_OFFSET;
    
    return result;
}

/* rutine for view_load_cache_line, line is ended and following cache_line is
 * set up for loading
 * used when line has maximum width */
static struct cache_line *
view_lc_create_wrap_line (struct cache_line *line, offset_type start, 
                          screen_dimen left) 
{
    struct cache_line *result = NULL;
    
    line->end = start;
    
    if (line->next == NULL) {
        result = g_new0 (struct cache_line, 1);
        line->next = result;
        result->prev = line;
    } else result = line->next;
    
    result->start = start;
    result->width = 0;
    result->left = left;
    result->number = line->number;
    result->end = INVALID_OFFSET;
    
    return result;
}

/* read charsets fro text and set up correct width, end and start of next line 
 * view->data_area.width must be greater than 0 */
static struct cache_line *
view_load_cache_line (WView *view, struct cache_line *line)
{
    const screen_dimen width = view->data_area.width;
    struct read_info info;
    offset_type nroff_start = 0;
    int nroff_seq = 0;
    gsize w;
    
    line->width = 0;
        
    view_read_start (view, &info, line->start);
    while (info.result != -1) {
        view_read_continue (view, &info);
        
        switch (view_read_test_new_line (view, &info)) {
            case VRT_NW_YES:
                line = view_lc_create_next_line (line, info.next);
                return line;
            case VRT_NW_CONTINUE:
                continue;
        }
        
        if (view_read_test_tabulator (view, &info)) {
            line->width+= 8 - (line->left + line->width) % 8;
            if ((width != 0) && (line->left + line->width >= width)) {
                w = line->left + line->width - width;
                /* if width of screen is very small, tabulator is cut to line 
                 * left of next line is 0 */
                w = (w < width) ? w : 0;
                line->width = width - line->left;
                line = view_lc_create_wrap_line (line, info.next, w);
                return line;
            }
        }
        
        if (view_read_test_nroff_back (view, &info)) {
            w = str_term_width1 (info.chi1);
            line->width-= w;
            nroff_seq = 1;
            continue;
        }
        /* assure, that nroff sequence never start in previous cache_line */
        if (nroff_seq > 0) 
            nroff_seq--;
        else
            nroff_start = info.actual;
        
        w = str_isprint (info.cact) ? str_term_width1 (info.cact) : 1;
        
        if (line->left + line->width + w > width) {
            line = view_lc_create_wrap_line (line, nroff_start, 0);
            return line;
        } else {
            while (info.result != -1 && str_iscombiningmark (info.cnxt)) {
                view_read_continue (view, &info);
            }
        }
        line->width+= w;
        
    }
    
    /* text read to the end, seting lines_end*/
    if (view->text_nroff_mode)
        view->nroff_lines_end = line;
    else
        view->lines_end = line;
    
    line = view_lc_create_next_line (line, info.next);
    line->width = (screen_dimen) (-1);    
    line->end = info.next;
    
    return line;
}        

static void
view_lc_set_param (offset_type *param, offset_type value)
{
    if ((*param) > value) (*param) = value;
}                
        
/* offset to column or column to offset, value that will be maped must be 
 * set to INVALID_OFFSET, it is very analogous to view_load_cache_line
 *  and it is possible to integrate them in one function */
static void
view_map_offset_and_column (WView *view, struct cache_line *line, 
                              offset_type *column, offset_type *offset)
{
    const screen_dimen width = view->data_area.width;
    struct read_info info;
    offset_type nroff_start = 0;
    int nroff_seq = 0;
    gsize w;
    screen_dimen col;

    /* HACK: valgrind screams here.
     * TODO: to figure out WHY valgrind detects uninitialized
     * variables. Maybe, info isn't fully initialized?
     */
    memset (&info, 0, sizeof info);

    col = 0;
    view_read_start (view, &info, line->start);
    while (info.result != -1) {
        view_read_continue (view, &info);
        
        if (*column == INVALID_OFFSET) {
            if (*offset < info.next) {
                (*column) = col + line->left;
                return;
            }
        }
    
        switch (view_read_test_new_line (view, &info)) {
            case VRT_NW_YES:
                view_lc_set_param (offset, info.actual);
                view_lc_set_param (column, line->left + col);
                return;
            case VRT_NW_CONTINUE:
                continue;
        }
        
        if (view_read_test_tabulator (view, &info)) {
            col+= 8 - (line->left + col) % 8;
            if ((width != 0) && (line->left + col >= width)) {
                w = line->left + col - width;
                w = (w < width) ? w : 0;
                col = width - line->left;
                view_lc_set_param (offset, info.actual);
                view_lc_set_param (column, line->left + col);
                return;
            }
        }
        
        if (view_read_test_nroff_back (view, &info)) {
            w = str_term_width1 (info.chi1);
            col-= w;
            nroff_seq = 1;
            continue;
        }
        if (nroff_seq > 0) 
            nroff_seq--;
        else
            nroff_start = info.actual;
        
        w = str_isprint (info.cact) ? str_term_width1 (info.cact) : 1;
        
        if (line->left + col + w > width) {
            view_lc_set_param (offset, nroff_start);
            view_lc_set_param (column, line->left + col);
            return;
        } else {
            while (info.result != -1 && str_iscombiningmark (info.cnxt)) {
                view_read_continue (view, &info);
            }
        }
        
        col+= w;
        
        if (*offset == INVALID_OFFSET) {
            if (*column < col + line->left) {
                (*offset) = nroff_start;
                return;
            }
        }
    }
    
    view_lc_set_param (offset, info.actual);
    view_lc_set_param (column, line->left + col);
}        

/* macro, that iterate cache_line until stop holds, 
 * nf is function to iterate cache_line
 * l is line to iterate and t is temporary line only */
#define view_move_to_stop(l,t,stop,nf) \
while (((t = nf (view, l)) != NULL) && (stop)) l = t;

/* make all cache_lines invalidet */
static void
view_reset_cache_lines (WView *view) 
{
    if (view->lines != NULL) {
        view->lines->end = INVALID_OFFSET;
    }
    view->lines_end = NULL;
    
    if (view->nroff_lines != NULL) {
        view->nroff_lines->end = INVALID_OFFSET;
    }
    view->nroff_lines_end = NULL;
    
    view->first_showed_line = NULL;
}    

#define MAX_UNLOADED_CACHE_LINE 100

/* free some cache_lines, if count of cache_lines is bigger 
 * than MAX_UNLOADED_CACHE_LINE */
static void
view_reduce_cache_lines (WView *view)
{
    struct cache_line *line;
    struct cache_line *next;
    int li;
    
    li = 0;
    line = view->lines;
    while (line != NULL && li < MAX_UNLOADED_CACHE_LINE) {
        line = line->next;
        li++;
    }
    if (line != NULL) line->prev->next = NULL;
    while (line != NULL) {
        next = line->next;
        g_free (line);
        line = next;
    }
    view->lines_end = NULL;
        
    line = view->nroff_lines;
    while (line != NULL && li < MAX_UNLOADED_CACHE_LINE) {
        line = line->next;
        li++;
    }
    if (line != NULL) line->prev->next = NULL;
    while (line != NULL) {
        next = line->next;
        g_free (line);
        line = next;
    }
    view->nroff_lines_end = NULL;
    
    view->first_showed_line = NULL;
}        

/* return first cache_line for actual mode (normal / nroff) */
static struct cache_line *
view_get_first_line (WView *view) 
{
    struct cache_line **first_line;
    
    first_line = (view->text_nroff_mode) ? 
            &(view->nroff_lines) : &(view->lines);
    
    if (*first_line == NULL) {
        (*first_line) = g_new0 (struct cache_line, 1);
        (*first_line)->end = INVALID_OFFSET;
    }
    
    if ((*first_line)->end == INVALID_OFFSET)
        view_load_cache_line (view, *first_line);
    
    return (*first_line);
}

/* return following chahe_line or NULL */
static struct cache_line*
view_get_next_line (WView *view, struct cache_line *line)
{
    struct cache_line *result;
    
    if (line->next != NULL) {
        result = line->next;
        
        if (result->end == INVALID_OFFSET) 
            view_load_cache_line (view, result);
        
        return (result->width != (screen_dimen) (-1)) ? result : NULL;
    }
    return NULL;
}        

/* return last cache_line, it could take same time, because whole read text must
 * be read (only once) */
static struct cache_line *
view_get_last_line (WView *view)
{
    struct cache_line *result;
    struct cache_line *next;
    
    result = (view->text_nroff_mode) ? 
            view->nroff_lines_end : view->nroff_lines_end;
    
    if (result != NULL) return result;
    
    if (view->first_showed_line == NULL)
        view->first_showed_line = view_get_first_line (view);
    
    result = view->first_showed_line;
    next = view_get_next_line (view, result);
    while (next != NULL) {
        result = next;
        next = view_get_next_line (view, result);   
    }
    return result;
}        

/* return previous cache_line or NULL */
static struct cache_line *
view_get_previous_line (WView *view, struct cache_line *line)
{
    (void) view;
    return line->prev;
}

/* return first displayed cache_line */
static struct cache_line *
view_get_first_showed_line (WView *view)
{
    struct cache_line *result;
    
    if (view->first_showed_line == NULL)
        view->first_showed_line = view_get_first_line (view);
    
    result = view->first_showed_line;
    return result;
}        

/* return first cache_line with same number as line */
static struct cache_line *
view_get_start_of_whole_line (WView *view, struct cache_line *line)
{
    struct cache_line *t;
    
    if (line != NULL) {
        view_move_to_stop (line, t, t->number == line->number, view_get_previous_line)
        return line;
    }
    return NULL;
}        

/* return last cache_line with same number as line */
static struct cache_line *
view_get_end_of_whole_line (WView *view, struct cache_line *line)
{
    struct cache_line *t;
    
    if (line != NULL) {
        view_move_to_stop (line, t, t->number == line->number, view_get_next_line)
        return line;
    }
    return NULL;
}        

/* return last cache_line, that has number lesser than line 
 * or NULL */
static struct cache_line *
view_get_previous_whole_line (WView *view, struct cache_line *line)
{
    line = view_get_start_of_whole_line (view, line);
    return view_get_previous_line (view, line);
}           

/* return first cache_line, that has number greater than line 
 * or NULL */
static struct cache_line *
view_get_next_whole_line (WView *view, struct cache_line *line)
{
    line = view_get_end_of_whole_line (view, line);
    return view_get_next_line (view, line);
}           

/* return sum of widths of all cache_lines that has same number as line */
static screen_dimen
view_width_of_whole_line (WView *view, struct cache_line *line) 
{
    struct cache_line *next;
    screen_dimen result = 0;
    
    line = view_get_start_of_whole_line (view, line);
    next = view_get_next_line (view, line);
    while ((next != NULL) && (next->number == line->number)) {
        result+= line->left + line->width;
        line = next;
        next = view_get_next_line (view, line);
    }
    result+= line->left + line->width;
    return result;
}        

/* return sum of widths of cache_lines before line, that has same number as line */
static screen_dimen
view_width_of_whole_line_before (WView *view, struct cache_line *line) 
{
    struct cache_line *next;
    screen_dimen result = 0;
    
    next = view_get_start_of_whole_line (view, line);
    while (next != line) {
        result+= next->left + next->width;
        next = view_get_next_line (view, next);
    }
    return result;
}        

/* map column to offset and cache_line */
static offset_type
view_column_to_offset (WView *view, struct cache_line **line, offset_type column)
{
    struct cache_line *next;
    offset_type result;
    
    *line = view_get_start_of_whole_line (view, *line);
    
    while (column >= (*line)->left + (*line)->width) {
        column-= (*line)->left + (*line)->width;
        result = (*line)->end;
        next = view_get_next_line (view, *line);
        if ((next == NULL) || (next->number != (*line)->number)) break;
        (*line) = next;
    }
/*    if (column < (*line)->left + (*line)->width) { */
        result = INVALID_OFFSET,
        view_map_offset_and_column (view, *line, &column, &result);
/*    } */
    return result;
}

/* map offset to cache_line */
static struct cache_line *
view_offset_to_line (WView *view, offset_type from)
{
    struct cache_line *result;
    struct cache_line *t;
    
    result = view_get_first_line (view);
    
    view_move_to_stop (result, t, result->end <= from, view_get_next_line)
    return result;
}

/* map offset to cache_line, searching starts from line */
static struct cache_line *
view_offset_to_line_from (WView *view, offset_type from, struct cache_line *line)
{
    struct cache_line *result;
    struct cache_line *t;
    
    result = line;
    
    view_move_to_stop (result, t, result->start > from, view_get_previous_line)
    view_move_to_stop (result, t, result->end <= from, view_get_next_line)
            
    return result;
}

/* mam offset to column */
static screen_dimen
view_offset_to_column (WView *view, struct cache_line *line, offset_type from)
{
    offset_type result = INVALID_OFFSET;
    
    view_map_offset_and_column (view, line, &result, &from);
    
    result+= view_width_of_whole_line_before (view, line);
    
    return result;
}        

static void
view_compute_areas (WView *view)
{
    struct area view_area;
    struct cache_line *next;
    screen_dimen height, rest, y;
    screen_dimen old_width;

    old_width = view->data_area.width;

    /* The viewer is surrounded by a frame of size view->dpy_frame_size.
     * Inside that frame, there are: The status line (at the top),
     * the data area and an optional ruler, which is shown above or
     * below the data area. */

    view_area.top = view->dpy_frame_size;
    view_area.left = view->dpy_frame_size;
    view_area.height = dimen_doz(view->widget.lines, 2 * view->dpy_frame_size);
    view_area.width = dimen_doz(view->widget.cols, 2 * view->dpy_frame_size);

    /* Most coordinates of the areas equal those of the whole viewer */
    view->status_area = view_area;
    view->ruler_area = view_area;
    view->data_area = view_area;

    /* Compute the heights of the areas */
    rest = view_area.height;

    height = dimen_min(rest, 1);
    view->status_area.height = height;
    rest -= height;

    height = dimen_min(rest, (ruler == RULER_NONE || view->hex_mode) ? 0 : 2);
    view->ruler_area.height = height;
    rest -= height;

    view->data_area.height = rest;

    /* Compute the position of the areas */
    y = view_area.top;

    view->status_area.top = y;
    y += view->status_area.height;

    if (ruler == RULER_TOP) {
	view->ruler_area.top = y;
	y += view->ruler_area.height;
    }

    view->data_area.top = y;
    y += view->data_area.height;

    if (ruler == RULER_BOTTOM) {
	view->ruler_area.top = y;
	y += view->ruler_area.height;
    }
    
    if (old_width != view->data_area.width) {
        view_reset_cache_lines (view);
        view->first_showed_line = view_get_first_line (view);
        next = view_get_next_line (view, view->first_showed_line);
        while ((next != NULL) && (view->first_showed_line->end <= view->dpy_start)) {
            view->first_showed_line = next;
            next = view_get_next_line (view, view->first_showed_line);
        }
        view->dpy_start = view->first_showed_line->start;
    }
}

static void
view_hexedit_free_change_list (WView *view)
{
    struct hexedit_change_node *curr, *next;

    for (curr = view->change_list; curr != NULL; curr = next) {
	next = curr->next;
	g_free (curr);
    }
    view->change_list = NULL;
    view->dirty++;
}

/* {{{ Growing buffer }}} */

static void
view_init_growbuf (WView *view)
{
    view->growbuf_in_use    = TRUE;
    view->growbuf_blockptr  = NULL;
    view->growbuf_blocks    = 0;
    view->growbuf_lastindex = VIEW_PAGE_SIZE;
    view->growbuf_finished  = FALSE;
}

static void
view_growbuf_free (WView *view)
{
    size_t i;

    assert (view->growbuf_in_use);

    for (i = 0; i < view->growbuf_blocks; i++)
	g_free (view->growbuf_blockptr[i]);
    g_free (view->growbuf_blockptr);
    view->growbuf_blockptr = NULL;
    view->growbuf_in_use = FALSE;
}

static offset_type
view_growbuf_filesize (WView *view)
{
    assert(view->growbuf_in_use);

    if (view->growbuf_blocks == 0)
	return 0;
    else
	return ((offset_type) view->growbuf_blocks - 1) * VIEW_PAGE_SIZE
	       + view->growbuf_lastindex;
}

/* Copies the output from the pipe to the growing buffer, until either
 * the end-of-pipe is reached or the interval [0..ofs) of the growing
 * buffer is completely filled. */
static void
view_growbuf_read_until (WView *view, offset_type ofs)
{
    ssize_t nread;
    byte *p;
    size_t bytesfree;
    gboolean short_read;

    assert (view->growbuf_in_use);

    if (view->growbuf_finished)
	return;

    short_read = FALSE;
    while (view_growbuf_filesize (view) < ofs || short_read) {
	if (view->growbuf_lastindex == VIEW_PAGE_SIZE) {
	    /* Append a new block to the growing buffer */
	    byte *newblock = g_try_malloc (VIEW_PAGE_SIZE);
	    byte **newblocks = g_try_malloc (sizeof (*newblocks) * (view->growbuf_blocks + 1));
	    if (!newblock || !newblocks) {
		g_free (newblock);
		g_free (newblocks);
		return;
	    }
	    memcpy (newblocks, view->growbuf_blockptr, sizeof (*newblocks) * view->growbuf_blocks);
	    g_free (view->growbuf_blockptr);
	    view->growbuf_blockptr = newblocks;
	    view->growbuf_blockptr[view->growbuf_blocks++] = newblock;
	    view->growbuf_lastindex = 0;
	}
	p = view->growbuf_blockptr[view->growbuf_blocks - 1] + view->growbuf_lastindex;
	bytesfree = VIEW_PAGE_SIZE - view->growbuf_lastindex;

	if (view->datasource == DS_STDIO_PIPE) {
	    nread = fread (p, 1, bytesfree, view->ds_stdio_pipe);
	    if (nread == 0) {
		view->growbuf_finished = TRUE;
		(void) pclose (view->ds_stdio_pipe);
		display (view);
		close_error_pipe (D_NORMAL, NULL);
		view->ds_stdio_pipe = NULL;
		return;
	    }
	} else {
	    assert (view->datasource == DS_VFS_PIPE);
	    do {
		nread = mc_read (view->ds_vfs_pipe, p, bytesfree);
	    } while (nread == -1 && errno == EINTR);
	    if (nread == -1 || nread == 0) {
		view->growbuf_finished = TRUE;
		(void) mc_close (view->ds_vfs_pipe);
		view->ds_vfs_pipe = -1;
		return;
	    }
	}
	short_read = ((size_t)nread < bytesfree);
	view->growbuf_lastindex += nread;
    }
}

static int
get_byte_growing_buffer (WView *view, offset_type byte_index)
{
    offset_type pageno    = byte_index / VIEW_PAGE_SIZE;
    offset_type pageindex = byte_index % VIEW_PAGE_SIZE;

    assert (view->growbuf_in_use);

    if ((size_t) pageno != pageno)
	return -1;

    view_growbuf_read_until (view, byte_index + 1);
    if (view->growbuf_blocks == 0)
	return -1;
    if (pageno < view->growbuf_blocks - 1)
	return view->growbuf_blockptr[pageno][pageindex];
    if (pageno == view->growbuf_blocks - 1 && pageindex < view->growbuf_lastindex)
	return view->growbuf_blockptr[pageno][pageindex];
    return -1;
}

/* {{{ Data sources }}} */

/*
    The data source provides the viewer with data from either a file, a
    string or the output of a command. The get_byte() function can be
    used to get the value of a byte at a specific offset. If the offset
    is out of range, -1 is returned. The function get_byte_indexed(a,b)
    returns the byte at the offset a+b, or -1 if a+b is out of range.

    The view_set_byte() function has the effect that later calls to
    get_byte() will return the specified byte for this offset. This
    function is designed only for use by the hexedit component after
    saving its changes. Inspect the source before you want to use it for
    other purposes.

    The view_get_filesize() function returns the current size of the
    data source. If the growing buffer is used, this size may increase
    later on. Use the view_may_still_grow() function when you want to
    know if the size can change later.
 */

static offset_type
view_get_filesize (WView *view)
{
    switch (view->datasource) {
	case DS_NONE:
	    return 0;
	case DS_STDIO_PIPE:
	case DS_VFS_PIPE:
	    return view_growbuf_filesize (view);
	case DS_FILE:
	    return view->ds_file_filesize;
	case DS_STRING:
	    return view->ds_string_len;
	default:
	    assert(!"Unknown datasource type");
	    return 0;
    }
}

static inline gboolean
view_may_still_grow (WView *view)
{
    return (view->growbuf_in_use && !view->growbuf_finished);
}

/* returns TRUE if the idx lies in the half-open interval
 * [offset; offset + size), FALSE otherwise.
 */
static inline gboolean
already_loaded (offset_type offset, offset_type idx, size_t size)
{
    return (offset <= idx && idx - offset < size);
}

static inline void
view_file_load_data (WView *view, offset_type byte_index)
{
    offset_type blockoffset;
    ssize_t res;
    size_t bytes_read;

    assert (view->datasource == DS_FILE);

    if (already_loaded (view->ds_file_offset, byte_index, view->ds_file_datalen))
	return;

    if (byte_index >= view->ds_file_filesize)
	return;

    blockoffset = offset_rounddown (byte_index, view->ds_file_datasize);
    if (mc_lseek (view->ds_file_fd, blockoffset, SEEK_SET) == -1)
	goto error;

    bytes_read = 0;
    while (bytes_read < view->ds_file_datasize) {
	res = mc_read (view->ds_file_fd, view->ds_file_data + bytes_read, view->ds_file_datasize - bytes_read);
	if (res == -1)
	    goto error;
	if (res == 0)
	    break;
	bytes_read += (size_t) res;
    }
    view->ds_file_offset  = blockoffset;
    if (bytes_read > view->ds_file_filesize - view->ds_file_offset) {
	/* the file has grown in the meantime -- stick to the old size */
	view->ds_file_datalen = view->ds_file_filesize - view->ds_file_offset;
    } else {
	view->ds_file_datalen = bytes_read;
    }
    return;

error:
    view->ds_file_datalen = 0;
}

static int
get_byte_none (WView *view, offset_type byte_index)
{
    assert (view->datasource == DS_NONE);
    (void) &view;
    (void) byte_index;
    return -1;
}

static inline int
get_byte_file (WView *view, offset_type byte_index)
{
    assert (view->datasource == DS_FILE);

    view_file_load_data (view, byte_index);
    if (already_loaded(view->ds_file_offset, byte_index, view->ds_file_datalen))
	return view->ds_file_data[byte_index - view->ds_file_offset];
    return -1;
}

static int
get_byte_string (WView *view, offset_type byte_index)
{
    assert (view->datasource == DS_STRING);
    if (byte_index < view->ds_string_len)
	return view->ds_string_data[byte_index];
    return -1;
}

static inline int
get_byte (WView *view, offset_type offset)
{
    switch (view->datasource) {
	case DS_STDIO_PIPE:
	case DS_VFS_PIPE:
	    return get_byte_growing_buffer (view, offset);
	case DS_FILE:
	    return get_byte_file (view, offset);
	case DS_STRING:
	    return get_byte_string (view, offset);
	case DS_NONE:
	    return get_byte_none (view, offset);
    }
    assert(!"Unknown datasource type");
    return -1;
}

static inline int
get_byte_indexed (WView *view, offset_type base, offset_type ofs)
{
    if (base <= OFFSETTYPE_MAX - ofs)
	return get_byte (view, base + ofs);
    return -1;
}

static void
view_set_byte (WView *view, offset_type offset, byte b)
{
    (void) &b;
    assert (offset < view_get_filesize (view));
    assert (view->datasource == DS_FILE);
    view->ds_file_datalen = 0; /* just force reloading */
}

static void
view_set_datasource_none (WView *view)
{
    view->datasource = DS_NONE;
}

static void
view_set_datasource_vfs_pipe (WView *view, int fd)
{
    assert (fd != -1);
    view->datasource = DS_VFS_PIPE;
    view->ds_vfs_pipe = fd;

    view_init_growbuf (view);
}

static void
view_set_datasource_stdio_pipe (WView *view, FILE *fp)
{
    assert (fp != NULL);
    view->datasource = DS_STDIO_PIPE;
    view->ds_stdio_pipe = fp;

    view_init_growbuf (view);
}

static void
view_set_datasource_string (WView *view, const char *s)
{
    view->datasource = DS_STRING;
    view->ds_string_data = (byte *) g_strdup (s);
    view->ds_string_len  = strlen (s);
}

static void
view_set_datasource_file (WView *view, int fd, const struct stat *st)
{
    view->datasource = DS_FILE;
    view->ds_file_fd = fd;
    view->ds_file_filesize = st->st_size;
    view->ds_file_offset = 0;
    view->ds_file_data = g_malloc (4096);
    view->ds_file_datalen = 0;
    view->ds_file_datasize = 4096;
}

static void
view_close_datasource (WView *view)
{
    switch (view->datasource) {
	case DS_NONE:
	    break;
	case DS_STDIO_PIPE:
	    if (view->ds_stdio_pipe != NULL) {
		(void) pclose (view->ds_stdio_pipe);
		display (view);
		close_error_pipe (D_NORMAL, NULL);
		view->ds_stdio_pipe = NULL;
	    }
	    view_growbuf_free (view);
	    break;
	case DS_VFS_PIPE:
	    if (view->ds_vfs_pipe != -1) {
		(void) mc_close (view->ds_vfs_pipe);
		view->ds_vfs_pipe = -1;
	    }
	    view_growbuf_free (view);
	    break;
	case DS_FILE:
	    (void) mc_close (view->ds_file_fd);
	    view->ds_file_fd = -1;
	    g_free (view->ds_file_data);
	    view->ds_file_data = NULL;
	    break;
	case DS_STRING:
	    g_free (view->ds_string_data);
	    view->ds_string_data = NULL;
	    break;
	default:
	    assert (!"Unknown datasource type");
    }
    view->datasource = DS_NONE;
}

/* {{{ Cursor Movement }}} */

/*
   The following variables have to do with the current position and are
   updated by the cursor movement functions.

   In hex view and wrapped text view mode, dpy_start marks the offset of
   the top-left corner on the screen, in non-wrapping text mode it is
   the beginning of the current line.  In hex mode, hex_cursor is the
   offset of the cursor.  In non-wrapping text mode, dpy_text_column is
   the number of columns that are hidden on the left side on the screen.

   In hex mode, dpy_start is updated by the view_fix_cursor_position()
   function in order to keep the other functions simple.  In
   non-wrapping text mode dpy_start and dpy_text_column are normalized
   such that dpy_text_column < view_get_datacolumns().
 */

/* prototypes for functions used by view_moveto_bottom() */
static void view_move_up (WView *, offset_type);
static void view_moveto_bol (WView *);

/* set view->first_showed_line and view->dpy_start 
 * use view->dpy_text_column in nowrap mode */
static void
view_set_first_showed (WView *view, struct cache_line *line)
{
    if (view->text_wrap_mode) {
        view->dpy_start = line->start;
        view->first_showed_line = line;
    } else {
        view->dpy_start = view_column_to_offset (view, &line, view->dpy_text_column);
        view->first_showed_line = line;
    }
    if (view->search_start == view->search_end) {
        view->search_start = view->dpy_start;
        view->search_end = view->dpy_start;
    }
}        

static void
view_scroll_to_cursor (WView *view)
{
    if (view->hex_mode) {
	const offset_type bytes = view->bytes_per_line;
	const offset_type displaysize = view->data_area.height * bytes;
	const offset_type cursor = view->hex_cursor;
	offset_type topleft = view->dpy_start;

	if (topleft + displaysize <= cursor)
	    topleft = offset_rounddown (cursor, bytes)
		    - (displaysize - bytes);
	if (cursor < topleft)
	    topleft = offset_rounddown (cursor, bytes);
	view->dpy_start = topleft;
    } else if (view->text_wrap_mode) {
	view->dpy_text_column = 0;
    } else {
    }
}

static void
view_movement_fixups (WView *view, gboolean reset_search)
{
    view_scroll_to_cursor (view);
    if (reset_search) {
	view->search_start = view->dpy_start;
	view->search_end = view->dpy_start;
    }
    view->dirty++;
}

static void
view_moveto_top (WView *view)
{
    view->dpy_start = 0;
    view->hex_cursor = 0;
    view->first_showed_line = view_get_first_line (view);
    view->dpy_text_column = 0;
    view_movement_fixups (view, TRUE);
}

static void
view_moveto_bottom (WView *view)
{
    offset_type datalines, lines_up, filesize, last_offset;
    struct cache_line *line;

    if (view->growbuf_in_use)
	view_growbuf_read_until (view, OFFSETTYPE_MAX);

    filesize = view_get_filesize (view);
    last_offset = offset_doz(filesize, 1);
    datalines = view->data_area.height;
    lines_up = offset_doz(datalines, 1);

    if (view->hex_mode) {
	view->hex_cursor = filesize;
	view_move_up (view, lines_up);
	view->hex_cursor = last_offset;
    } else {
        line = view_get_last_line (view);
        if (!view->text_wrap_mode)
            line = view_get_start_of_whole_line (view, line);
        view_set_first_showed (view, line);
        view->dpy_text_column = 0;
	view_move_up (view, lines_up);
    }
    view_movement_fixups (view, TRUE);
}

static void
view_moveto_bol (WView *view)
{
    struct cache_line *line;
    
    if (view->hex_mode) {
	view->hex_cursor -= view->hex_cursor % view->bytes_per_line;
    } else if (view->text_wrap_mode) {
	/* do nothing */
    } else {
        line = view_get_first_showed_line (view);
        line = view_get_start_of_whole_line (view, line);
	view->dpy_text_column = 0;
        view_set_first_showed (view, line);
    }
    view_movement_fixups (view, TRUE);
}

static void
view_moveto_eol (WView *view)
{
    const screen_dimen width = view->data_area.width;
    struct cache_line *line;
    screen_dimen w;
    
    if (view->hex_mode) {
	offset_type filesize, bol;

	bol = offset_rounddown (view->hex_cursor, view->bytes_per_line);
	if (get_byte_indexed (view, bol, view->bytes_per_line - 1) != -1) {
	    view->hex_cursor = bol + view->bytes_per_line - 1;
	} else {
	    filesize = view_get_filesize (view);
	    view->hex_cursor = offset_doz(filesize, 1);
	}
    } else if (view->text_wrap_mode) {
	/* nothing to do */
    } else {
        line = view_get_first_showed_line (view);
        line = view_get_start_of_whole_line (view, line);
        w = view_width_of_whole_line (view, line);
        if (w > width) {
            view->dpy_text_column = w - width; 
        } else {
/*            if (w + width <= view->dpy_text_column) {*/
                view->dpy_text_column = 0;
/*            }*/
        }
        view_set_first_showed (view, line);
    }
    view_movement_fixups (view, FALSE);
}

static void
view_moveto_offset (WView *view, offset_type offset)
{
    struct cache_line *line;
    
    if (view->hex_mode) {
	view->hex_cursor = offset;
	view->dpy_start = offset - offset % view->bytes_per_line;
    } else {
        line = view_offset_to_line (view, offset);
	view->dpy_start = (view->text_wrap_mode) ? line->start : offset;
        view->first_showed_line = line;
        view->dpy_text_column = (view->text_wrap_mode) ? 
                0 : view_offset_to_column (view, line, offset);
    }
    view_movement_fixups (view, TRUE);
}

static void
view_moveto (WView *view, offset_type row, offset_type col)
{
    struct cache_line *act;
    struct cache_line *t;

    act = view_get_first_line (view);
    view_move_to_stop (act, t, (off_t)act->number != row, view_get_next_line)

    view->dpy_text_column = (view->text_wrap_mode) ? 0 : col;
    view->dpy_start = view_column_to_offset (view, &act, col);
    view->dpy_start = (view->text_wrap_mode) ? act->start : view->dpy_start;
    view->first_showed_line = act;

    if (view->hex_mode)
        view_moveto_offset (view, view->dpy_start);
}

/* extendet view_move_to_stop, now has counter, too */
#define view_count_to_stop(l,t,i,stop,nf)\
while (((t = nf(view, l)) != NULL) && (stop)) {\
    l = t;i++;}

static void
view_move_up (WView *view, offset_type lines)
{
    struct cache_line *line, *t;
    off_t li;
    
    if (view->hex_mode) {
	offset_type bytes = lines * view->bytes_per_line;
	if (view->hex_cursor >= bytes) {
	    view->hex_cursor -= bytes;
	    if (view->hex_cursor < view->dpy_start)
		view->dpy_start = offset_doz (view->dpy_start, bytes);
	} else {
	    view->hex_cursor %= view->bytes_per_line;
	}
    } else if (view->text_wrap_mode) {
        line = view_get_first_showed_line (view);
        li = 0;
        view_count_to_stop (line, t, li, (li < lines), view_get_previous_line)
        view_set_first_showed (view, line);
    } else {
        line = view_get_first_showed_line (view);
        li = 0;
        view_count_to_stop (line, t, li, (li < lines), view_get_previous_whole_line)
        view_set_first_showed (view, line);
    }
    view_movement_fixups (view, (lines != 1));
}
/*
static void
return_up (struct cache_line * (*ne) (WView *, struct cache_line *),
               struct cache_line * (*pr) (WView *, struct cache_line *),
               off_t *li, struct cache_line **t, struct cache_line **line,
               WView *view)
{
    *li = 0;
    *t = *line;
    while ((*t != NULL) && (*li < view->data_area.height)) {
        (*li)++;
        *t = ne (view, *t);
    }
    *li = view->data_area.height - *li;
    *t = pr (view, *line);
    while ((*t != NULL) && (*li > 0)) {
        *line = *t;
        *t = pr (view, *line);
        (*li)--;
    }
}
*/
static void
view_move_down (WView *view, offset_type lines)
{
    struct cache_line *line;
    struct cache_line *t;
    off_t li;


    if (view->hex_mode) {
	offset_type i, limit, last_byte;

	last_byte = view_get_filesize (view);
	if (last_byte >= (offset_type) view->bytes_per_line)
	    limit = last_byte - view->bytes_per_line;
	else
	    limit = 0;
	for (i = 0; i < lines && view->hex_cursor < limit; i++) {
	    view->hex_cursor += view->bytes_per_line;
	    if (lines != 1)
		view->dpy_start += view->bytes_per_line;
	}

    } else if (view->text_wrap_mode) {
        line = view_get_first_showed_line (view);
        li = 0;
        view_count_to_stop (line, t, li, (off_t)li < lines, view_get_next_line)
        
      /*  return_up (view_get_next_line, view_get_previous_line); */
        view_set_first_showed (view, line);
    } else {
        line = view_get_first_showed_line (view);
        li = 0;
        view_count_to_stop (line, t, li, li < lines, view_get_next_whole_line)

     /*   return_up (view_get_next_whole_line, view_get_previous_whole_line); */
        view_set_first_showed (view, line);
    }
    view_movement_fixups (view, (lines != 1));
}

static void
view_move_left (WView *view, offset_type columns)
{
    struct cache_line *line;
    
    if (view->hex_mode) {
	assert (columns == 1);
	if (view->hexview_in_text || !view->hexedit_lownibble) {
	    if (view->hex_cursor > 0)
		view->hex_cursor--;
	}
	if (!view->hexview_in_text)
	    view->hexedit_lownibble = !view->hexedit_lownibble;
    } else if (view->text_wrap_mode) {
	/* nothing to do */
    } else {
	if (view->dpy_text_column >= columns)
	    view->dpy_text_column-= columns;
	else
	    view->dpy_text_column = 0;
        
        line = view_get_first_showed_line (view);
        view_set_first_showed (view, line);
    }
    view_movement_fixups (view, FALSE);
}

static void
view_move_right (WView *view, offset_type columns)
{
    struct cache_line *line;
    
    if (view->hex_mode) {
	assert (columns == 1);
	if (view->hexview_in_text || view->hexedit_lownibble) {
	    if (get_byte_indexed (view, view->hex_cursor, 1) != -1)
		view->hex_cursor++;
	}
	if (!view->hexview_in_text)
	    view->hexedit_lownibble = !view->hexedit_lownibble;
    } else if (view->text_wrap_mode) {
	/* nothing to do */
    } else {
	view->dpy_text_column += columns;
        line = view_get_first_showed_line (view);
        view_set_first_showed (view, line);
    }
    view_movement_fixups (view, FALSE);
}

/* {{{ Toggling of viewer modes }}} */

static void
view_toggle_hex_mode (WView *view)
{
    struct cache_line *line;
    
    view->hex_mode = !view->hex_mode;
    view->search_type = (view->hex_mode)?MC_SEARCH_T_HEX:MC_SEARCH_T_NORMAL;

    if (view->hex_mode) {
	view->hex_cursor = view->dpy_start;
	view->dpy_start =
	    offset_rounddown (view->dpy_start, view->bytes_per_line);
	view->widget.options |= W_WANT_CURSOR;
    } else {
        line = view_offset_to_line (view, view->hex_cursor);
        view->dpy_text_column = (view->text_wrap_mode) ? 0 :
                view_offset_to_column (view, line, view->hex_cursor);
        view_set_first_showed (view, line);
	view->widget.options &= ~W_WANT_CURSOR;
    }
    altered_hex_mode = 1;
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

static void
view_toggle_hexedit_mode (WView *view)
{
    view->hexedit_mode = !view->hexedit_mode;
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

static void
view_toggle_wrap_mode (WView *view)
{
    struct cache_line *line;
    
    view->text_wrap_mode = !view->text_wrap_mode;
    if (view->text_wrap_mode) {
        view->dpy_text_column = 0;
        view->dpy_start = view_get_first_showed_line (view)->start;
    } else {
        line = view_get_first_showed_line (view);
        view->dpy_text_column = view_width_of_whole_line_before (view, line);
    }
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

static void
view_toggle_nroff_mode (WView *view)
{
    struct cache_line *line;
    struct cache_line *next;
    
    view->text_nroff_mode = !view->text_nroff_mode;
    altered_nroff_flag = 1;
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;

    line = view_get_first_line (view);
    view_move_to_stop (line, next, line->end <= view->dpy_start, view_get_next_line)
    
    view_set_first_showed (view, line);
}

static void
view_toggle_magic_mode (WView *view)
{
    char *filename, *command;

    altered_magic_flag = 1;
    view->magic_mode = !view->magic_mode;
    filename = g_strdup (view->filename);
    command = g_strdup (view->command);

    view_done (view);
    view_load (view, command, filename, 0);
    g_free (filename);
    g_free (command);
    view->dpy_bbar_dirty = TRUE;
    view->dirty++;
}

/* {{{ Miscellaneous functions }}} */

static void
view_done (WView *view)
{
    /* Save current file position */
    if (mcview_remember_file_position && view->filename != NULL) {
        struct cache_line *line;
	char *canon_fname;
	offset_type row, col;

	canon_fname = vfs_canon (view->filename);
        line = view_get_first_showed_line (view);
        row = line->number + 1;
        col = view_offset_to_column (view, line, view->dpy_start);
        
	save_file_position (canon_fname, row, col);
	g_free (canon_fname);
    }

    /* Write back the global viewer mode */
    default_hex_mode = view->hex_mode;
    default_nroff_flag = view->text_nroff_mode;
    default_magic_flag = view->magic_mode;
    global_wrap_mode = view->text_wrap_mode;


    /* view->widget needs no destructor */

    g_free (view->filename), view->filename = NULL;
    g_free (view->command), view->command = NULL;

    view_close_datasource (view);
    /* the growing buffer is freed with the datasource */

    view_hexedit_free_change_list (view);

    g_free(view->last_search_string);
    view->last_search_string = NULL;

    /* Free memory used by the viewer */
    view_reduce_cache_lines (view);
    if (view->converter != str_cnv_from_term) str_close_conv (view->converter);
    
}

static void
view_show_error (WView *view, const char *msg)
{
    view_close_datasource (view);
    if (view_is_in_panel (view)) {
	view_set_datasource_string (view, msg);
    } else {
	message (D_ERROR, MSG_ERROR, "%s", msg);
    }
}

static gboolean
view_load_command_output (WView *view, const char *command)
{
    FILE *fp;

    view_close_datasource (view);

    open_error_pipe ();
    if ((fp = popen (command, "r")) == NULL) {
	/* Avoid two messages.  Message from stderr has priority.  */
	display (view);
	if (!close_error_pipe (view_is_in_panel (view) ? -1 : D_ERROR, NULL))
	    view_show_error (view, _(" Cannot spawn child process "));
	return FALSE;
    }

    /* First, check if filter produced any output */
    view_set_datasource_stdio_pipe (view, fp);
    if (get_byte (view, 0) == -1) {
	view_close_datasource (view);

	/* Avoid two messages.  Message from stderr has priority.  */
	display (view);
	if (!close_error_pipe (view_is_in_panel (view) ? -1 : D_ERROR, NULL))
	    view_show_error (view, _("Empty output from child filter"));
	return FALSE;
    } else {
	/*
	 * At least something was read correctly. Close stderr and let
	 * program die if it will try to write something there.
	 *
	 * Ideally stderr should be read asynchronously to prevent programs
	 * from blocking (poll/select multiplexor).
	 */
	close_error_pipe (D_NORMAL, NULL);
    }
    return TRUE;
}

gboolean
view_load (WView *view, const char *command, const char *file,
	   int start_line)
{
    int i, type;
    int fd = -1;
    char tmp[BUF_MEDIUM];
    const char *enc;
    char *canon_fname;
    struct stat st;
    gboolean retval = FALSE;

    assert (view->bytes_per_line != 0);
    view_done (view);

    
    /* Set up the state */
    view_set_datasource_none (view);
    view->filename = g_strdup (file);
    view->command = 0;

    /* Clear the markers */
    view->marker = 0;
    for (i = 0; i < 10; i++)
	view->marks[i] = 0;

    if (!view_is_in_panel (view)) {
	view->dpy_text_column = 0;
    }

    if (command && (view->magic_mode || file == NULL || file[0] == '\0')) {
	retval = view_load_command_output (view, command);
    } else if (file != NULL && file[0] != '\0') {
	/* Open the file */
	if ((fd = mc_open (file, O_RDONLY | O_NONBLOCK)) == -1) {
	    g_snprintf (tmp, sizeof (tmp), _(" Cannot open \"%s\"\n %s "),
			file, unix_error_string (errno));
	    view_show_error (view, tmp);
            g_free (view->filename);
            view->filename = NULL;
	    goto finish;
	}

	/* Make sure we are working with a regular file */
	if (mc_fstat (fd, &st) == -1) {
	    mc_close (fd);
	    g_snprintf (tmp, sizeof (tmp), _(" Cannot stat \"%s\"\n %s "),
			file, unix_error_string (errno));
	    view_show_error (view, tmp);
            g_free (view->filename);
            view->filename = NULL;
	    goto finish;
	}

	if (!S_ISREG (st.st_mode)) {
	    mc_close (fd);
	    view_show_error (view, _(" Cannot view: not a regular file "));
            g_free (view->filename);
            view->filename = NULL;
	    goto finish;
	}

	if (st.st_size == 0 || mc_lseek (fd, 0, SEEK_SET) == -1) {
	    /* Must be one of those nice files that grow (/proc) */
	    view_set_datasource_vfs_pipe (view, fd);
	} else {
	    type = get_compression_type (fd);

	    if (view->magic_mode && (type != COMPRESSION_NONE)) {
		g_free (view->filename);
		view->filename = g_strconcat (file, decompress_extension (type), (char *) NULL);
	    }
	    view_set_datasource_file (view, fd, &st);
	}
	retval = TRUE;
    }

  finish:
    view->command = g_strdup (command);
    view->dpy_start = 0;
    view->search_start = 0;
    view->search_end = 0;
    view->dpy_text_column = 0;

    view->converter = str_cnv_from_term;
    /* try detect encoding from path */
    if (view->filename != NULL) {
        canon_fname = vfs_canon (view->filename);
        enc = vfs_get_encoding (canon_fname);
        if (enc != NULL) {
            view->converter = str_crt_conv_from (enc);
            if (view->converter == INVALID_CONV)
		view->converter = str_cnv_from_term;
        }
        g_free (canon_fname);
    }
    
    view_compute_areas (view);
    view_reset_cache_lines (view);
    view->first_showed_line = view_get_first_line (view);
    
    assert (view->bytes_per_line != 0);
    if (mcview_remember_file_position && view->filename != NULL && start_line == 0) {
	long line, col;

	canon_fname = vfs_canon (view->filename);
	load_file_position (canon_fname, &line, &col);
	g_free (canon_fname);
	view_moveto (view, offset_doz(line, 1), col);
    } else if (start_line > 0) {
	view_moveto (view, start_line - 1, 0);
    }

    view->hexedit_lownibble = FALSE;
    view->hexview_in_text = FALSE;
    view->change_list = NULL;

    return retval;
}

/* {{{ Display management }}} */

static void
view_update_bytes_per_line (WView *view)
{
    const screen_dimen cols = view->data_area.width;
    int bytes;

    if (cols < 8 + 17)
	bytes = 4;
    else
	bytes = 4 * ((cols - 8) / ((cols < 80) ? 17 : 18));
    assert(bytes != 0);

    view->bytes_per_line = bytes;
    view->dirty = max_dirt_limit + 1;	/* To force refresh */
}

static void
view_percent (WView *view, offset_type p)
{
    const screen_dimen top = view->status_area.top;
    const screen_dimen right = view->status_area.left + view->status_area.width;
    const screen_dimen height = view->status_area.height;
    int percent;
    offset_type filesize;

    if (height < 1 || right < 4)
	return;
    if (view_may_still_grow (view))
	return;
    filesize = view_get_filesize (view);

    if (filesize == 0 || view->dpy_end == filesize)
	percent = 100;
    else if (p > (INT_MAX / 100))
	percent = p / (filesize / 100);
    else
	percent = p * 100 / filesize;

    widget_move (view, top, right - 4);
    tty_printf ("%3d%%", percent);
}

static void
view_display_status (WView *view)
{
    const screen_dimen top = view->status_area.top;
    const screen_dimen left = view->status_area.left;
    const screen_dimen width = view->status_area.width;
    const screen_dimen height = view->status_area.height;
    const char *file_label, *file_name;
    screen_dimen file_label_width;
    int i;

    if (height < 1)
	return;

    tty_setcolor (SELECTED_COLOR);
    tty_draw_hline (view->widget.y + top, view->widget.x + left, ' ', width);

    file_label = _("File: %s");
    file_label_width = str_term_width1 (file_label) - 2;
    file_name = view->filename ? view->filename
	: view->command ? view->command
	: "";

    if (width < file_label_width + 6)
	tty_print_string (str_fit_to_term (file_name, width, J_LEFT_FIT));
    else {
	i = (width > 22 ? 22 : width) - file_label_width;
        tty_printf (file_label, str_fit_to_term (file_name, i, J_LEFT_FIT));

	if (width > 46) {
	    widget_move (view, top, left + 24);
	    /* FIXME: the format strings need to be changed when offset_type changes */
	    if (view->hex_mode)
		tty_printf (_("Offset 0x%08lx"), (unsigned long) view->hex_cursor);
	    else {
		screen_dimen row, col;
                struct cache_line *line;
                
                line = view_get_first_showed_line (view);
                row = line->number + 1;
                
                col = (view->text_wrap_mode) ?
                    view_width_of_whole_line_before (view, line) :
                    view->dpy_text_column;
                col++;
                
		tty_printf (_("Line %lu Col %lu"),
		    (unsigned long) row, (unsigned long) col);
	    }
	}
	if (width > 62) {
	    offset_type filesize;
	    filesize = view_get_filesize (view);
	    widget_move (view, top, left + 43);
	    if (!view_may_still_grow (view)) {
		tty_printf (_("%s bytes"), size_trunc (filesize));
	    } else {
		tty_printf (_(">= %s bytes"), size_trunc (filesize));
	    }
	}
	if (width > 26) {
	    view_percent (view, view->hex_mode
		? view->hex_cursor
		: view->dpy_end);
	}
    }
    tty_setcolor (SELECTED_COLOR);
}

static inline void
view_display_clean (WView *view)
{
    tty_setcolor (NORMAL_COLOR);
    widget_erase ((Widget *) view);
    if (view->dpy_frame_size != 0) {
	draw_box (view->widget.parent, view->widget.y,
		    view->widget.x, view->widget.lines,
		    view->widget.cols);
    }
}

typedef enum {
    MARK_NORMAL,
    MARK_SELECTED,
    MARK_CURSOR,
    MARK_CHANGED
} mark_t;

static inline int
view_count_backspaces (WView *view, off_t offset)
{
    int backspaces = 0;
    while (offset >= 2 * backspaces
	   && get_byte (view, offset - 2 * backspaces) == '\b')
	backspaces++;
    return backspaces;
}

static void
view_display_ruler (WView *view)
{
    static const char ruler_chars[] = "|----*----";
    const screen_dimen top = view->ruler_area.top;
    const screen_dimen left = view->ruler_area.left;
    const screen_dimen width = view->ruler_area.width;
    const screen_dimen height = view->ruler_area.height;
    const screen_dimen line_row = (ruler == RULER_TOP) ? 0 : 1;
    const screen_dimen nums_row = (ruler == RULER_TOP) ? 1 : 0;

    char r_buff[10];
    offset_type cl;
    screen_dimen c;

    if (ruler == RULER_NONE || height < 1)
	return;

    tty_setcolor (MARKED_COLOR);
    for (c = 0; c < width; c++) {
	cl = view->dpy_text_column + c;
	if (line_row < height) {
	    widget_move (view, top + line_row, left + c);
	    tty_print_char (ruler_chars[cl % 10]);
	}

	if ((cl != 0) && (cl % 10) == 0) {
	    g_snprintf (r_buff, sizeof (r_buff), "%"OFFSETTYPE_PRId, cl);
	    if (nums_row < height) {
		widget_move (view, top + nums_row, left + c - 1);
		tty_print_string (r_buff);
	    }
	}
    }
    tty_setcolor (NORMAL_COLOR);
}

static void
view_display_hex (WView *view)
{
    const screen_dimen top = view->data_area.top;
    const screen_dimen left = view->data_area.left;
    const screen_dimen height = view->data_area.height;
    const screen_dimen width = view->data_area.width;
    const int ngroups = view->bytes_per_line / 4;
    const screen_dimen text_start =
	8 + 13 * ngroups + ((width < 80) ? 0 : (ngroups - 1 + 1));
    /* 8 characters are used for the file offset, and every hex group
     * takes 13 characters. On ``big'' screens, the groups are separated
     * by an extra vertical line, and there is an extra space before the
     * text column.
     */

    screen_dimen row, col;
    offset_type from;
    int c;
    mark_t boldflag = MARK_NORMAL;
    struct hexedit_change_node *curr = view->change_list;
    size_t i;

    char hex_buff[10];	/* A temporary buffer for sprintf */
    int bytes;		/* Number of bytes already printed on the line */

    view_display_clean (view);

    /* Find the first displayable changed byte */
    from = view->dpy_start;
    while (curr && (curr->offset < from)) {
	curr = curr->next;
    }

    for (row = 0; get_byte (view, from) != -1 && row < height; row++) {
	col = 0;

	/* Print the hex offset */
	g_snprintf (hex_buff, sizeof (hex_buff), "%08"OFFSETTYPE_PRIX" ", from);
	widget_move (view, top + row, left);
	tty_setcolor (MARKED_COLOR);
	for (i = 0; col < width && hex_buff[i] != '\0'; i++) {
	    tty_print_char (hex_buff[i]);
	    col++;
	}
	tty_setcolor (NORMAL_COLOR);

	for (bytes = 0; bytes < view->bytes_per_line; bytes++, from++) {

	    if ((c = get_byte (view, from)) == -1)
		break;

	    /* Save the cursor position for view_place_cursor() */
	    if (from == view->hex_cursor && !view->hexview_in_text) {
		view->cursor_row = row;
		view->cursor_col = col;
	    }

	    /* Determine the state of the current byte */
	    boldflag =
		  (from == view->hex_cursor) ? MARK_CURSOR
		: (curr != NULL && from == curr->offset) ? MARK_CHANGED
		: (view->search_start <= from &&
		   from < view->search_end) ? MARK_SELECTED
		: MARK_NORMAL;

	    /* Determine the value of the current byte */
	    if (curr != NULL && from == curr->offset) {
		c = curr->value;
		curr = curr->next;
	    }

	    /* Select the color for the hex number */
	    tty_setcolor (
		boldflag == MARK_NORMAL ? NORMAL_COLOR :
		boldflag == MARK_SELECTED ? MARKED_COLOR :
		boldflag == MARK_CHANGED ? VIEW_UNDERLINED_COLOR :
		/* boldflag == MARK_CURSOR */
		view->hexview_in_text ? MARKED_SELECTED_COLOR :
		VIEW_UNDERLINED_COLOR);

	    /* Print the hex number */
	    widget_move (view, top + row, left + col);
	    if (col < width) {
		tty_print_char (hex_char[c / 16]);
		col += 1;
	    }
	    if (col < width) {
		tty_print_char (hex_char[c % 16]);
		col += 1;
	    }

	    /* Print the separator */
	    tty_setcolor (NORMAL_COLOR);
	    if (bytes != view->bytes_per_line - 1) {
	    	if (col < width) {
		    tty_print_char (' ');
		    col += 1;
		}

		/* After every four bytes, print a group separator */
		if (bytes % 4 == 3) {
		    if (view->data_area.width >= 80 && col < width) {
			tty_print_one_vline ();
			col += 1;
		    }
		    if (col < width) {
			tty_print_char (' ');
			col += 1;
		    }
		}
	    }

	    /* Select the color for the character; this differs from the
	     * hex color when boldflag == MARK_CURSOR */
	    tty_setcolor (
		boldflag == MARK_NORMAL ? NORMAL_COLOR :
		boldflag == MARK_SELECTED ? MARKED_COLOR :
		boldflag == MARK_CHANGED ? VIEW_UNDERLINED_COLOR :
		/* boldflag == MARK_CURSOR */
		view->hexview_in_text ? VIEW_UNDERLINED_COLOR :
		MARKED_SELECTED_COLOR);

	    c = convert_to_display_c (c);
	    if (!g_ascii_isprint (c))
		c = '.';

	    /* Print corresponding character on the text side */
	    if (text_start + bytes < width) {
		widget_move (view, top + row, left + text_start + bytes);
		tty_print_char (c);
	    }

	    /* Save the cursor position for view_place_cursor() */
	    if (from == view->hex_cursor && view->hexview_in_text) {
		view->cursor_row = row;
		view->cursor_col = text_start + bytes;
	    }
	}
    }

    /* Be polite to the other functions */
    tty_setcolor (NORMAL_COLOR);

    view_place_cursor (view);
    view->dpy_end = from;
}

static void
view_display_text (WView * view)
{
    #define cmp(t1,t2) (strcmp((t1),(t2)) == 0)
    
    const screen_dimen left = view->data_area.left;
    const screen_dimen top = view->data_area.top;
    const screen_dimen width = view->data_area.width;
    const screen_dimen height = view->data_area.height;
    struct read_info info;
    offset_type row, col;
    int w;
    struct cache_line *line_act;
    struct cache_line *line_nxt;

    view_display_clean (view);
    view_display_ruler (view);

    tty_setcolor (NORMAL_COLOR);

    widget_move (view, top, left);
    
    line_act = view_get_first_showed_line (view);
    
    row = 0;
    /* set col correct value */
    col = (view->text_wrap_mode) ? 0 : view_width_of_whole_line_before (view, line_act);
    col+= line_act->left;
    
    view_read_start (view, &info, line_act->start);
    while ((info.result != -1) && (row < height)) {
        /* real detection of new line */
        if (info.next >= line_act->end) {
            line_nxt = view_get_next_line (view, line_act);
            if (line_nxt == NULL)
		break;
            if (view->text_wrap_mode || (line_act->number != line_nxt->number)){
                row++;
                col = line_nxt->left;
            }
            line_act = line_nxt;
	    continue;
	}

        view_read_continue (view, &info);
        if (view_read_test_nroff_back (view, &info)) {
            w = str_term_width1 (info.chi1);
            col -= w;
            if (col >= view->dpy_text_column
                && col + w - view->dpy_text_column <= width)
		tty_draw_hline (view->widget.y + top + row,
				view->widget.x + left + (col - view->dpy_text_column),
				' ', w);

            if (cmp (info.chi1, "_") && (!cmp (info.cnxt, "_") || !cmp (info.chi2, "\b")))
		    tty_setcolor (VIEW_UNDERLINED_COLOR);
		else
		    tty_setcolor (MARKED_COLOR);
		continue;
	    }

        if (view_read_test_new_line (view, &info)) 
		continue;


        if (view_read_test_tabulator (view, &info)) {
	    col+= (8 - (col % 8));
	    continue;
	}

	if (view->search_start <= info.actual
	 && info.actual < view->search_end) {
	    tty_setcolor (SELECTED_COLOR);
	}

        w = str_isprint (info.cact) ? str_term_width1 (info.cact) : 1;
	
	if (col >= view->dpy_text_column
	    && col + w - view->dpy_text_column <= width) {
	    widget_move (view, top + row, left + (col - view->dpy_text_column));

            if (!str_iscombiningmark (info.cnxt)) {
                if (str_isprint (info.cact)) {
                    tty_print_string (info.cact);
                } else {
                    tty_print_char ('.');
	}
            } else {
                GString *comb = g_string_new ("");
                if (str_isprint (info.cact)) {
                    g_string_append(comb,info.cact);
                } else {
                    g_string_append(comb,".");
                }
                while (str_iscombiningmark (info.cnxt)) {
                    view_read_continue (view, &info);
                    g_string_append(comb,info.cact);
                }
                tty_print_string (comb->str);
                g_string_free (comb, TRUE);
            }
	} else {
            while (str_iscombiningmark (info.cnxt)) {
                view_read_continue (view, &info);
            }
	}
        col+= w;

	tty_setcolor (NORMAL_COLOR);
    }
    view->dpy_end = info.next;
}

/* Displays as much data from view->dpy_start as fits on the screen */
static void
display (WView *view)
{
    view_compute_areas (view);
    if (view->hex_mode) {
	view_display_hex (view);
    } else {
	view_display_text (view);
    }
    view_display_status (view);
}

static void
view_place_cursor (WView *view)
{
    const screen_dimen top = view->data_area.top;
    const screen_dimen left = view->data_area.left;
    screen_dimen col;

    col = view->cursor_col;
    if (!view->hexview_in_text && view->hexedit_lownibble)
	col++;
    widget_move (&view->widget, top + view->cursor_row, left + col);
}

static void
view_update (WView *view)
{
    static int dirt_limit = 1;

    if (view->dpy_bbar_dirty) {
	view->dpy_bbar_dirty = FALSE;
	view_labels (view);
	buttonbar_redraw (view->widget.parent);
    }

    if (view->dirty > dirt_limit) {
	/* Too many updates skipped -> force a update */
	display (view);
	view->dirty = 0;
	/* Raise the update skipping limit */
	dirt_limit++;
	if (dirt_limit > max_dirt_limit)
	    dirt_limit = max_dirt_limit;
    }
    if (view->dirty) {
	if (is_idle ()) {
	    /* We have time to update the screen properly */
	    display (view);
	    view->dirty = 0;
	    if (dirt_limit > 1)
		dirt_limit--;
	} else {
	    /* We are busy -> skipping full update,
	       only the status line is updated */
	    view_display_status (view);
	}
	/* Here we had a refresh, if fast scrolling does not work
	   restore the refresh, although this should not happen */
    }
}

/* {{{ Hex editor }}} */

static void
enqueue_change (struct hexedit_change_node **head,
		struct hexedit_change_node *node)
{
    /* chnode always either points to the head of the list or
     * to one of the ->next fields in the list. The value at
     * this location will be overwritten with the new node.   */
    struct hexedit_change_node **chnode = head;

    while (*chnode != NULL && (*chnode)->offset < node->offset)
	chnode = &((*chnode)->next);

    node->next = *chnode;
    *chnode = node;
}

static cb_ret_t
view_handle_editkey (WView *view, int key)
{
    struct hexedit_change_node *node;
    byte byte_val;

    /* Has there been a change at this position? */
    node = view->change_list;
    while (node && (node->offset != view->hex_cursor))
	node = node->next;

    if (!view->hexview_in_text) {
	/* Hex editing */
	unsigned int hexvalue = 0;

	if (key >= '0' && key <= '9')
	    hexvalue =  0 + (key - '0');
	else if (key >= 'A' && key <= 'F')
	    hexvalue = 10 + (key - 'A');
	else if (key >= 'a' && key <= 'f')
	    hexvalue = 10 + (key - 'a');
	else
	    return MSG_NOT_HANDLED;

	if (node)
	    byte_val = node->value;
	else
	    byte_val = get_byte (view, view->hex_cursor);

	if (view->hexedit_lownibble) {
	    byte_val = (byte_val & 0xf0) | (hexvalue);
	} else {
	    byte_val = (byte_val & 0x0f) | (hexvalue << 4);
	}
    } else {
	/* Text editing */
	if (key < 256 && (is_printable (key) || (key == '\n')))
	    byte_val = key;
	else
	    return MSG_NOT_HANDLED;
    }
    if (!node) {
	node = g_new (struct hexedit_change_node, 1);
	node->offset = view->hex_cursor;
	node->value = byte_val;
	enqueue_change (&view->change_list, node);
    } else {
	node->value = byte_val;
    }
    view->dirty++;
    view_update (view);
    view_move_right (view, 1);
    return MSG_HANDLED;
}

static gboolean
view_hexedit_save_changes (WView *view)
{
    struct hexedit_change_node *curr, *next;
    int fp, answer;
    char *text, *error;

    if (view->change_list == NULL)
	return TRUE;

  retry_save:
    assert (view->filename != NULL);
    fp = mc_open (view->filename, O_WRONLY);
    if (fp == -1)
	goto save_error;

    for (curr = view->change_list; curr != NULL; curr = next) {
	next = curr->next;

	if (mc_lseek (fp, curr->offset, SEEK_SET) == -1
	    || mc_write (fp, &(curr->value), 1) != 1)
	    goto save_error;

	/* delete the saved item from the change list */
	view->change_list = next;
	view->dirty++;
	view_set_byte (view, curr->offset, curr->value);
	g_free (curr);
    }

    if (mc_close (fp) == -1) {
	error = g_strdup (strerror (errno));
        message (D_ERROR, _(" Save file "), _(
                " Error while closing the file: \n %s \n"
	      " Data may have been written or not. "), error);
	g_free (error);
    }
    view_update (view);
    return TRUE;

  save_error:
    error = g_strdup (strerror (errno));
    text = g_strdup_printf (_(" Cannot save file: \n %s "), error);
    g_free (error);
    (void) mc_close (fp);

    answer = query_dialog (_(" Save file "), text, D_ERROR,
	2, _("&Retry"), _("&Cancel"));
    g_free (text);

    if (answer == 0)
	goto retry_save;
    return FALSE;
}

/* {{{ Miscellaneous functions }}} */

static gboolean
view_ok_to_quit (WView *view)
{
    int r;

    if (view->change_list == NULL)
	return TRUE;

    r = query_dialog (_("Quit"),
		      _(" File was modified, Save with exit? "), D_NORMAL, 3,
		      _("&Cancel quit"), _("&Yes"), _("&No"));

    switch (r) {
    case 1:
	return view_hexedit_save_changes (view);
    case 2:
	view_hexedit_free_change_list (view);
	return TRUE;
    default:
	return FALSE;
    }
}

static inline void
my_define (Dlg_head *h, int idx, const char *text, void (*fn) (WView *),
	   WView *view)
{
    buttonbar_set_label_data (h, idx, text, (buttonbarfn) fn, view);
}

/* {{{ Searching }}} */


/* map search result positions to offsets in text */
void
view_matchs_to_offsets (WView *view, offset_type start, offset_type end,
                        size_t match_start, size_t match_end,
                        offset_type *search_start, offset_type *search_end) 
{
    struct read_info info;
    size_t c = 0;

    (*search_start) = INVALID_OFFSET;
    (*search_end) = INVALID_OFFSET;

    view_read_start (view, &info, start);

    while ((info.result != -1) && (info.next < end)) {
        view_read_continue (view, &info);

        if (view_read_test_nroff_back (view, &info)) {
            c-= 1;
            continue;
	}
        if ((c == match_start) && (*search_start == INVALID_OFFSET)) 
            *search_start = info.actual;
        if (c == match_end) (*search_end) = info.actual;
        c+= !str_iscombiningmark (info.cact) || (c == 0);
    }

    if ((c == match_start) && (*search_start == INVALID_OFFSET)) *search_start = info.next;
    if (c == match_end) (*search_end) = info.next;
}        

/* we have set view->search_start and view->search_end and must set 
 * view->dpy_text_column, view->first_showed_line and view->dpy_start
 * try to displaye maximum of match */
void
view_moveto_match (WView *view)
{
    const screen_dimen height = view->data_area.height;
    const screen_dimen height3 = height / 3;
    const screen_dimen width = view->data_area.width;
    struct cache_line *line;
    struct cache_line *line_end, *line_start;
    struct cache_line *t;
    int start_off = -1;
    int end_off = -1;
    int off = 0; 

    line = view_get_first_showed_line (view);
    if (view->text_wrap_mode) {
        if (line->start > view->search_start) {
            if (line->start <= view->search_start && line->end > view->search_start)
                start_off = 0;
            if (line->start <= view->search_end && line->end >= view->search_end)
                end_off = 0;
            t = view_get_previous_line (view, line);
            while ((t != NULL) && ((start_off == -1) || (end_off == -1))) {
                line = t;
                t = view_get_previous_line (view, line);
                off++;
                if (line->start <= view->search_start && line->end > view->search_start)
                    start_off = off;
                if (line->start <= view->search_end && line->end >= view->search_end)
                    end_off = off;
            }

            line = view_get_first_showed_line (view);

            off = ((off_t)(start_off - end_off) < (off_t)(height - height3)) 
                ? (int)(start_off + height3)
                : (int)end_off;
            for (;off >= 0 && line->start > 0; off--) 
                line = view_get_previous_line (view, line);
        } else {
            /* start_off, end_off - how many cache_lines far are 
             * view->search_start, end from line */
            if (line->start <= view->search_start && line->end > view->search_start)
                start_off = 0;
            if (line->start <= view->search_end && line->end >= view->search_end)
                end_off = 0;
            t = view_get_next_line (view, line);
            while ((t != NULL) && ((start_off == -1) || (end_off == -1))) {
                line = t;
                t = view_get_next_line (view, line);
                off++;
                if (line->start <= view->search_start && line->end > view->search_start)
                    start_off = off;
                if (line->start <= view->search_end && line->end >= view->search_end)
                    end_off = off;
            }

            line = view_get_first_showed_line (view);
            /* if view->search_end is farther then screen heigth */
            if ((off_t)end_off >= height) {
                off = ((off_t)(end_off - start_off) < (off_t)(height - height3))
                    ? (int) (end_off - height + height3)
                    : (int) start_off;

                for (;off >= 0; off--) 
                    line = view_get_next_line (view, line);
            }
        }
    } else {
        /* first part similar like in wrap mode,only wokrs with whole lines */
        line = view_get_first_showed_line (view);
        line = view_get_start_of_whole_line (view, line);
        if (line->start > view->search_start) {
            line_start = view_get_start_of_whole_line (view, line);
            if (line_start->start <= view->search_start && line->end > view->search_start)
                start_off = 0;
            if (line_start->start <= view->search_end && line->end >= view->search_end)
                end_off = 0;
            t = view_get_previous_whole_line (view, line_start);
            while ((t != NULL) && ((start_off == -1) || (end_off == -1))) {
                line = t;
                line_start = view_get_start_of_whole_line (view, line);
                t = view_get_previous_whole_line (view, line_start);
                off++;
                if (line_start->start <= view->search_start && line->end > view->search_start)
                    start_off = off;
                if (line_start->start <= view->search_end && line->end >= view->search_end)
                    end_off = off;
            }
            
            line = view_get_first_showed_line (view);
            line = view_get_start_of_whole_line (view, line);
            off = ((off_t)(start_off - end_off) < (off_t)(height - height3))
                ? (int)(start_off + height3)
                : (int)end_off;
            for (;off >= 0 && line->start > 0; off--) {
                line = view_get_previous_whole_line (view, line);
                line = view_get_start_of_whole_line (view, line);
            }
        } else {
            line_end = view_get_end_of_whole_line (view, line);
            if (line->start <= view->search_start && line_end->end > view->search_start)
                start_off = 0;
            if (line->start <= view->search_end && line_end->end >= view->search_end)
                end_off = 0;
            t = view_get_next_whole_line (view, line_end);
            while ((t != NULL) && ((start_off == -1) || (end_off == -1))) {
                line = t;
                line_end = view_get_end_of_whole_line (view, line);
                t = view_get_next_whole_line (view, line_end);
                off++;
                if (line->start <= view->search_start && line_end->end > view->search_start)
                    start_off = off;
                if (line->start <= view->search_end && line_end->end >= view->search_end)
                    end_off = off;
            }

            line = view_get_first_showed_line (view);
            line = view_get_start_of_whole_line (view, line);
            if ((off_t)end_off >= height) {
                off = ((off_t)(end_off - start_off) < (off_t)(height - height3))
                    ? (int)(end_off - height + height3)
                    : (int)start_off;

                for (;off >= 0; off--) 
                    line = view_get_next_whole_line (view, line);
            }
        }
        /*now line point to begin of line, that we want show*/
        
        t = view_offset_to_line_from (view, view->search_start, line);
        start_off = view_offset_to_column (view, t, view->search_start);
        t = view_offset_to_line_from (view, view->search_end, line);
        end_off = view_offset_to_column (view, t, view->search_end);
        
        if ((off_t)(end_off - start_off) > width) end_off = start_off + width;
        if (view->dpy_text_column > (off_t)start_off) {
            view->dpy_text_column = start_off;
        } else {
            if (view->dpy_text_column + width < (off_t)end_off) {
                view->dpy_text_column = end_off - width;
	}
	}
    }

    view_set_first_showed (view, line);
}

static void
search_update_steps (WView *view)
{
    offset_type filesize = view_get_filesize (view);
    if (filesize == 0)
	view->update_steps = 40000;
    else /* viewing a data stream, not a file */
	view->update_steps = filesize / 100;

    /* Do not update the percent display but every 20 ks */
    if (view->update_steps < 20000)
	view->update_steps = 20000;
}

/* {{{ User-definable commands }}} */

/*
    The functions in this section can be bound to hotkeys. They are all
    of the same type (taking a pointer to WView as parameter and
    returning void). TODO: In the not-too-distant future, these commands
    will become fully configurable, like they already are in the
    internal editor. By convention, all the function names end in
    "_cmd".
 */

static void
view_help_cmd (void)
{
    interactive_display (NULL, "[Internal File Viewer]");
}

/* Toggle between hexview and hexedit mode */
static void
view_toggle_hexedit_mode_cmd (WView *view)
{
    view_toggle_hexedit_mode (view);
    view_update (view);
}

/* Toggle between wrapped and unwrapped view */
static void
view_toggle_wrap_mode_cmd (WView *view)
{
    view_toggle_wrap_mode (view);
    view_update (view);
}

/* Toggle between hex view and text view */
static void
view_toggle_hex_mode_cmd (WView *view)
{
    view_toggle_hex_mode (view);
    view_update (view);
}

static void
view_moveto_line_cmd (WView *view)
{
    char *answer, *answer_end, prompt[BUF_SMALL];
    struct cache_line *line;
    offset_type row;

    line = view_get_first_showed_line (view);
    row = line->number + 1;

    g_snprintf (prompt, sizeof (prompt),
		_(" The current line number is %lu.\n"
		  " Enter the new line number:"), line->number);
    answer = input_dialog (_(" Goto line "), prompt, MC_HISTORY_VIEW_GOTO_LINE, "");
    if (answer != NULL && answer[0] != '\0') {
	errno = 0;
	row = strtoul (answer, &answer_end, 10);
	if (*answer_end == '\0' && errno == 0 && row >= 1)
	    view_moveto (view, row - 1, 0);
    }
    g_free (answer);
    view->dirty++;
    view_update (view);
}

static void
view_moveto_addr_cmd (WView *view)
{
    char *line, *error, prompt[BUF_SMALL];
    offset_type addr;

    g_snprintf (prompt, sizeof (prompt),
		_(" The current address is 0x%08"OFFSETTYPE_PRIX".\n"
		  " Enter the new address:"), view->hex_cursor);

    line = input_dialog (_(" Goto Address "), prompt, MC_HISTORY_VIEW_GOTO_ADDR, "");
    if (line != NULL) {
	if (*line != '\0') {
	    addr = strtoul (line, &error, 0);
	    if ((*error == '\0') && get_byte (view, addr) != -1) {
		view_moveto_offset (view, addr);
	    } else {
		message (D_ERROR, _("Warning"), _(" Invalid address "));
	    }
	}
	g_free (line);
    }
    view->dirty++;
    view_update (view);
}

static void
view_hexedit_save_changes_cmd (WView *view)
{
    (void) view_hexedit_save_changes (view);
}

static int
view__get_nroff_real_len(WView *view, offset_type start, offset_type length)
{
    offset_type loop1 = 0;
    int nroff_seq = 0;
    struct read_info info;

    view_read_start (view, &info, start);
    while((loop1 < length ) && (info.result != -1)) {
        view_read_continue (view, &info);
        if (view_read_test_nroff_back (view, &info)) {

            if (cmp (info.chi1, "_") && (!cmp (info.cnxt, "_") || !cmp (info.chi2, "\b")))
            {
                view_read_continue (view, &info);
                view_read_continue (view, &info);
                view_read_continue (view, &info);
                view_read_continue (view, &info);
                nroff_seq+=4;
            } else {
                view_read_continue (view, &info);
                view_read_continue (view, &info);
                nroff_seq+=2;
            }
        }
        if (*info.cact == '_' && *info.cnxt == 0x8)
        {
            view_read_continue (view, &info);
            view_read_continue (view, &info);
            nroff_seq+=2;
        }
        loop1++;
    }
    return nroff_seq;
}

static int
view_search_update_cmd_callback(const void *user_data, gsize char_offset)
{
    WView *view = (WView *) user_data;

    if (char_offset >= view->update_activate) {
        view->update_activate += view->update_steps;
        if (verbose) {
            view_percent (view, char_offset);
            tty_refresh ();
        }
        if (tty_got_interrupt ())
            return MC_SEARCH_CB_ABORT;
    }
    /* may be in future return from this callback will change current position
    * in searching block. Now this just constant return value.
    */
    return 1;
}

static int
view_search_cmd_callback(const void *user_data, gsize char_offset)
{
    int byte;
    WView *view = (WView *) user_data;

    byte = get_byte (view, char_offset);
    if (byte == -1)
        return MC_SEARCH_CB_ABORT;
    view_read_continue (view, &view->search_onechar_info);

    if (view->search_numNeedSkipChar) {
        view->search_numNeedSkipChar--;
        if (view->search_numNeedSkipChar){
            return byte;
        }
        return MC_SEARCH_CB_SKIP;
    }

    if (view_read_test_nroff_back (view, &view->search_onechar_info)) {
        if (
            cmp (view->search_onechar_info.chi1, "_") &&
            (!cmp (view->search_onechar_info.cnxt, "_") || !cmp (view->search_onechar_info.chi2, "\b"))
        )
            view->search_numNeedSkipChar = 2;
        else
            view->search_numNeedSkipChar = 1;

        return MC_SEARCH_CB_SKIP;
    }
    if (byte == '_' && *view->search_onechar_info.cnxt == 0x8)
    {
        view->search_numNeedSkipChar = 1;
        return MC_SEARCH_CB_SKIP;
    }

    return byte;
}

static gboolean
view_find (WView *view, gsize search_start, gsize *len)
{
    gsize search_end;

    view->search_numNeedSkipChar = 0;

    if (view->search_backwards) {
        search_end = view_get_filesize (view);
        while ((int) search_start >= 0) {
            if (search_end > search_start + view->search->original_len && mc_search_is_fixed_search_str(view->search))
                search_end = search_start + view->search->original_len;

            view_read_start (view, &view->search_onechar_info, search_start);

            if ( mc_search_run(view->search, (void *) view, search_start, search_end, len)
                && view->search->normal_offset == search_start )
                return TRUE;

            search_start--;
        }
        view->search->error_str = g_strdup(_(" Search string not found "));
        return FALSE;
    }
    view_read_start (view, &view->search_onechar_info, search_start);
    return mc_search_run(view->search, (void *) view, search_start, view_get_filesize (view), len);
}

static void
do_search (WView *view)
{
    offset_type search_start;
    gboolean isFound = FALSE;

    Dlg_head *d = NULL;

    size_t match_len;

    if (verbose) {
        d = create_message (D_NORMAL, _("Search"), _("Searching %s"), view->last_search_string);
        tty_refresh ();
    }

    /*for avoid infinite search loop we need to increase or decrease start offset of search */

    if (view->search_start)
    {
        search_start = (view->search_backwards) ? -2 : 2;
        search_start = view->search_start + search_start +
            view__get_nroff_real_len(view, view->search_start, 2) * search_start;
    }
    else
    {
        search_start = view->search_start;
    }

    if (view->search_backwards && (int) search_start < 0 )
        search_start = 0;

    /* Compute the percent steps */
    search_update_steps (view);
    view->update_activate = 0;

    tty_enable_interrupt_key ();

    do
    {
        if (view_find(view, search_start, &match_len))
        {
            view->search_start =  view->search->normal_offset +
                view__get_nroff_real_len(view,
                                    view->search->start_buffer,
                                    view->search->normal_offset - view->search->start_buffer);

            view->search_end = view->search_start + match_len +
                view__get_nroff_real_len(view, view->search_start, match_len + 1);

            if (view->hex_mode){
                view->hex_cursor = view->search_start;
                view->hexedit_lownibble = FALSE;
                view->dpy_start = view->search_start - view->search_start % view->bytes_per_line;
                view->dpy_end = view->search_end - view->search_end % view->bytes_per_line;
            }

            if (verbose) {
                dlg_run_done (d);
                destroy_dlg (d);
                d = create_message (D_NORMAL, _("Search"), _("Seeking to search result"));
                tty_refresh ();
            }

            view_moveto_match (view);
            isFound = TRUE;
            break;
        }
    } while (view_may_still_grow(view));

    if (!isFound){
        if (view->search->error_str)
            message (D_NORMAL, _("Search"), "%s", view->search->error_str);
    }

    view->dirty++;
    view_update (view);

    tty_disable_interrupt_key ();

    if (verbose) {
        dlg_run_done (d);
        destroy_dlg (d);
    }

}

/* Both views */
static void
view_search_cmd (WView *view)
{
    enum {
        SEARCH_DLG_MIN_HEIGHT = 10,
        SEARCH_DLG_HEIGHT_SUPPLY = 3,
        SEARCH_DLG_WIDTH = 58
    };

    char *defval = g_strdup (view->last_search_string != NULL ? view->last_search_string : "");
    char *exp = NULL;
#ifdef HAVE_CHARSET
    GString *tmp;
#endif

    int ttype_of_search = (int) view->search_type;
    int tall_codepages = (int) view->search_all_codepages;
    int tsearch_case = (int) view->search_case;
    int tsearch_backwards = (int) view->search_backwards;

    gchar **list_of_types = mc_search_get_types_strings_array();
    int SEARCH_DLG_HEIGHT = SEARCH_DLG_MIN_HEIGHT + g_strv_length (list_of_types) - SEARCH_DLG_HEIGHT_SUPPLY;

    QuickWidget quick_widgets[] = {

	{quick_button, 6, 10, SEARCH_DLG_HEIGHT - 3, SEARCH_DLG_HEIGHT, N_("&Cancel"), 0,
	 B_CANCEL, 0, 0, NULL, NULL, NULL},

	{quick_button, 2, 10, SEARCH_DLG_HEIGHT - 3, SEARCH_DLG_HEIGHT , N_("&OK"), 0, B_ENTER,
	 0, 0, NULL, NULL, NULL},

#ifdef HAVE_CHARSET
        {quick_checkbox, SEARCH_DLG_WIDTH/2 + 3, SEARCH_DLG_WIDTH, 6, SEARCH_DLG_HEIGHT, N_("All charsets"), 0, 0,
         &tall_codepages, 0, NULL, NULL, NULL},
#endif

	{quick_checkbox, SEARCH_DLG_WIDTH/2 + 3, SEARCH_DLG_WIDTH, 5, SEARCH_DLG_HEIGHT,
	 N_("&Backwards"), 0, 0, &tsearch_backwards, 0, NULL, NULL, NULL},

        {quick_checkbox, SEARCH_DLG_WIDTH/2 + 3, SEARCH_DLG_WIDTH, 4, SEARCH_DLG_HEIGHT, N_("case &Sensitive"), 0, 0,
         &tsearch_case, 0, NULL, NULL, NULL},

        {quick_radio, 3, SEARCH_DLG_WIDTH, 4, SEARCH_DLG_HEIGHT, 0, g_strv_length (list_of_types), ttype_of_search,
         (void *) &ttype_of_search, const_cast (char **, list_of_types), NULL, NULL, NULL},


	{quick_input, 3, SEARCH_DLG_WIDTH, 3, SEARCH_DLG_HEIGHT, defval, 52, 0,
	 0, &exp, MC_HISTORY_SHARED_SEARCH, NULL, NULL},

	{quick_label, 2, SEARCH_DLG_WIDTH, 2, SEARCH_DLG_HEIGHT,
	 N_(" Enter search string:"), 0, 0,  0, 0, 0, NULL, NULL},

	NULL_QuickWidget
    };

    QuickDialog Quick_input = {
	SEARCH_DLG_WIDTH, SEARCH_DLG_HEIGHT, -1, 0, N_("Search"),
	"[Input Line Keys]", quick_widgets, 0
    };

    convert_to_display (defval);


    if (quick_dialog (&Quick_input) == B_CANCEL)
	goto cleanup;

    view->search_backwards = tsearch_backwards;
    view->search_type = (mc_search_type_t) ttype_of_search;

    view->search_all_codepages = (gboolean) tall_codepages;
    view->search_case = (gboolean) tsearch_case;

    if (exp == NULL || exp[0] == '\0')
	goto cleanup;

#ifdef HAVE_CHARSET
    tmp = str_convert_to_input (exp);

    if (tmp)
    {
        g_free(exp);
        exp = g_string_free (tmp, FALSE);
    }
#endif

    g_free (view->last_search_string);
    view->last_search_string = exp;
    exp = NULL;

    if (view->search)
        mc_search_free(view->search);

    view->search = mc_search_new(view->last_search_string, -1);
    if (! view->search)
        goto cleanup;

    view->search->search_type = view->search_type;
    view->search->is_all_charsets = view->search_all_codepages;
    view->search->is_case_sentitive = view->search_case;
    view->search->search_fn = view_search_cmd_callback;
    view->search->update_fn = view_search_update_cmd_callback;

    do_search (view);

cleanup:
    g_free (exp);
    g_free (defval);
}

static void
view_toggle_magic_mode_cmd (WView *view)
{
    view_toggle_magic_mode (view);
    view_update (view);
}

static void
view_toggle_nroff_mode_cmd (WView *view)
{
    view_toggle_nroff_mode (view);
    view_update (view);
}

static void
view_quit_cmd (WView *view)
{
    if (view_ok_to_quit (view))
	dlg_stop (view->widget.parent);
}

/* {{{ Miscellaneous functions }}} */

/* Define labels and handlers for functional keys */
static void
view_labels (WView *view)
{
    const char *text;
    Dlg_head *h = view->widget.parent;

    buttonbar_set_label (h, 1, Q_("ButtonBar|Help"), view_help_cmd);

    my_define (h, 10, Q_("ButtonBar|Quit"), view_quit_cmd, view);
    text = view->hex_mode ? Q_("ButtonBar|Ascii") : Q_("ButtonBar|Hex");
    my_define (h, 4, text, view_toggle_hex_mode_cmd, view);
    text = view->hex_mode ? Q_("ButtonBar|Goto"): Q_("ButtonBar|Line");
    my_define (h, 5, text,
	view->hex_mode ? view_moveto_addr_cmd : view_moveto_line_cmd, view);

    if (view->hex_mode) {
	if (view->hexedit_mode) {
	    my_define (h, 2, Q_("ButtonBar|View"),
		view_toggle_hexedit_mode_cmd, view);
	} else if (view->datasource == DS_FILE) {
	    my_define (h, 2, Q_("ButtonBar|Edit"),
		view_toggle_hexedit_mode_cmd, view);
	} else {
	    buttonbar_clear_label (h, 2);
	}
	my_define (h, 6, Q_("ButtonBar|Save"),
	    view_hexedit_save_changes_cmd, view);
    } else {
        text = view->text_wrap_mode ? Q_("ButtonBar|UnWrap") : Q_("ButtonBar|Wrap");
	my_define (h, 2, text, view_toggle_wrap_mode_cmd, view);
    }

    text = view->hex_mode ? Q_("ButtonBar|HxSrch") : Q_("ButtonBar|Search");
    my_define (h, 7, text, view_search_cmd, view);
    text = view->magic_mode ? Q_("ButtonBar|Raw") : Q_("ButtonBar|Parse");
    my_define (h, 8, text, view_toggle_magic_mode_cmd, view);

    /* don't override the key to access the main menu */
    if (!view_is_in_panel (view)) {
        text = view->text_nroff_mode ? Q_("ButtonBar|Unform") : Q_("ButtonBar|Format");
	my_define (h, 9, text, view_toggle_nroff_mode_cmd, view);
	my_define (h, 3, Q_("ButtonBar|Quit"), view_quit_cmd, view);
    }
}

/* {{{ Event handling }}} */

/* Check for left and right arrows, possibly with modifiers */
static cb_ret_t
check_left_right_keys (WView *view, int c)
{
    if (c == KEY_LEFT) {
	view_move_left (view, 1);
	return MSG_HANDLED;
    }

    if (c == KEY_RIGHT) {
	view_move_right (view, 1);
	return MSG_HANDLED;
    }

    /* Ctrl with arrows moves by 10 postions in the unwrap mode */
    if (view->hex_mode || view->text_wrap_mode)
	return MSG_NOT_HANDLED;

    if (c == (KEY_M_CTRL | KEY_LEFT)) {
	if (view->dpy_text_column >= 10)
	    view->dpy_text_column -= 10;
	else
	    view->dpy_text_column = 0;
	view->dirty++;
	return MSG_HANDLED;
    }

    if (c == (KEY_M_CTRL | KEY_RIGHT)) {
	if (view->dpy_text_column <= OFFSETTYPE_MAX - 10)
	    view->dpy_text_column += 10;
	else
	    view->dpy_text_column = OFFSETTYPE_MAX;
	view->dirty++;
	return MSG_HANDLED;
    }

    return MSG_NOT_HANDLED;
}

/* {{{ User-definable commands }}} */

static void
view_continue_search_cmd (WView *view)
{
    if (view->last_search_string!=NULL) {
        do_search(view);
    } else {
	/* if not... then ask for an expression */
	view_search_cmd (view);
    }
}

static void
view_toggle_ruler_cmd (WView *view)
{
    static const enum ruler_type next[3] = {
	RULER_TOP,
	RULER_BOTTOM,
	RULER_NONE
    };

    assert ((size_t) ruler < 3);
    ruler = next[(size_t) ruler];
    view->dirty++;
}

/* {{{ Event handling }}} */

static void view_cmk_move_up (void *w, int n) {
    view_move_up ((WView *) w, n);
}
static void view_cmk_move_down (void *w, int n) {
    view_move_down ((WView *) w, n);
}
static void view_cmk_moveto_top (void *w, int n) {
    (void) &n;
    view_moveto_top ((WView *) w);
}
static void view_cmk_moveto_bottom (void *w, int n) {
    (void) &n;
    view_moveto_bottom ((WView *) w);
}

static void
view_select_encoding (WView *view) 
{
#ifdef HAVE_CHARSET
    const char *enc = NULL;

    if (!do_select_codepage ())
	return;

    enc = get_codepage_id (source_codepage);
    if (enc != NULL) {
	GIConv conv;
	struct cache_line *line;

        conv = str_crt_conv_from (enc);
        if (conv != INVALID_CONV) {
            if (view->converter != str_cnv_from_term)
		str_close_conv (view->converter);
            view->converter = conv;
            view_reset_cache_lines (view);
            line = view_offset_to_line (view, view->dpy_start);
            view_set_first_showed (view, line);
        }
    }
#endif
}


/* Both views */
static cb_ret_t
view_handle_key (WView *view, int c)
{
    c = convert_from_input_c (c);

    if (view->hex_mode) {
	switch (c) {
	case '\t':
	    view->hexview_in_text = !view->hexview_in_text;
	    view->dirty++;
	    return MSG_HANDLED;

	case XCTRL ('a'):
	    view_moveto_bol (view);
	    view->dirty++;
	    return MSG_HANDLED;

	case XCTRL ('b'):
	    view_move_left (view, 1);
	    return MSG_HANDLED;

	case XCTRL ('e'):
	    view_moveto_eol (view);
	    return MSG_HANDLED;

	case XCTRL ('f'):
	    view_move_right (view, 1);
	    return MSG_HANDLED;
	}

	if (view->hexedit_mode
	    && view_handle_editkey (view, c) == MSG_HANDLED)
	    return MSG_HANDLED;
    }

    if (check_left_right_keys (view, c))
	return MSG_HANDLED;

    if (check_movement_keys (c, view->data_area.height + 1, view,
	view_cmk_move_up, view_cmk_move_down,
	view_cmk_moveto_top, view_cmk_moveto_bottom))
	return MSG_HANDLED;

    switch (c) {

    case '?':
    case '/':
	view->search_type = MC_SEARCH_T_REGEX;
	view_search_cmd(view);
	return MSG_HANDLED;
    break;
	/* Continue search */
    case XCTRL ('r'):
    case XCTRL ('s'):
    case 'n':
    case KEY_F (17):
	view_continue_search_cmd (view);
	return MSG_HANDLED;

	/* toggle ruler */
    case ALT ('r'):
	view_toggle_ruler_cmd (view);
	return MSG_HANDLED;

    case 'h':
	view_move_left (view, 1);
	return MSG_HANDLED;

    case 'j':
    case '\n':
    case 'e':
	view_move_down (view, 1);
	return MSG_HANDLED;

    case 'd':
	view_move_down (view, (view->data_area.height + 1) / 2);
	return MSG_HANDLED;

    case 'u':
	view_move_up (view, (view->data_area.height + 1) / 2);
	return MSG_HANDLED;

    case 'k':
    case 'y':
	view_move_up (view, 1);
	return MSG_HANDLED;

    case 'l':
	view_move_right (view, 1);
	return MSG_HANDLED;

    case ' ':
    case 'f':
	view_move_down (view, view->data_area.height);
	return MSG_HANDLED;

    case XCTRL ('o'):
	view_other_cmd ();
	return MSG_HANDLED;

	/* Unlike Ctrl-O, run a new shell if the subshell is not running.  */
    case '!':
	exec_shell ();
	return MSG_HANDLED;

    case 'b':
	view_move_up (view, view->data_area.height);
	return MSG_HANDLED;

    case KEY_IC:
	view_move_up (view, 2);
	return MSG_HANDLED;

    case KEY_DC:
	view_move_down (view, 2);
	return MSG_HANDLED;

    case 'm':
	view->marks[view->marker] = view->dpy_start;
	return MSG_HANDLED;

    case 'r':
	view->dpy_start = view->marks[view->marker];
	view->dirty++;
	return MSG_HANDLED;

	/*  Use to indicate parent that we want to see the next/previous file */
	/* Does not work in panel mode */
    case XCTRL ('f'):
    case XCTRL ('b'):
	if (!view_is_in_panel (view))
	    view->move_dir = c == XCTRL ('f') ? 1 : -1;
	/* FALLTHROUGH */
    case 'q':
    case XCTRL ('g'):
    case ESC_CHAR:
	if (view_ok_to_quit (view))
	    view->want_to_quit = TRUE;
	return MSG_HANDLED;

    case XCTRL ('t'):
	view_select_encoding (view);
	view->dirty++;
	view_update (view);
	return MSG_HANDLED;

#ifdef MC_ENABLE_DEBUGGING_CODE
    case 't': /* mnemonic: "test" */
	view_ccache_dump (view);
	return MSG_HANDLED;
#endif
    }
    if (c >= '0' && c <= '9')
	view->marker = c - '0';

    /* Key not used */
    return MSG_NOT_HANDLED;
}

/* Both views */
static int
view_event (WView *view, Gpm_Event *event, int *result)
{
    screen_dimen y, x;

    *result = MOU_NORMAL;

    /* We are not interested in the release events */
    if (!(event->type & (GPM_DOWN | GPM_DRAG)))
	return 0;

    /* Wheel events */
    if ((event->buttons & GPM_B_UP) && (event->type & GPM_DOWN)) {
	view_move_up (view, 2);
	return 1;
    }
    if ((event->buttons & GPM_B_DOWN) && (event->type & GPM_DOWN)) {
	view_move_down (view, 2);
	return 1;
    }

    x = event->x;
    y = event->y;

    /* Scrolling left and right */
    if (!view->text_wrap_mode) {
	if (x < view->data_area.width * 1/4) {
	    view_move_left (view, 1);
	    goto processed;
	} else if (x < view->data_area.width * 3/4) {
	    /* ignore the click */
	} else {
	    view_move_right (view, 1);
	    goto processed;
	}
    }

    /* Scrolling up and down */
    if (y < view->data_area.top + view->data_area.height * 1/3) {
	if (mouse_move_pages_viewer)
	    view_move_up (view, view->data_area.height / 2);
	else
	    view_move_up (view, 1);
	goto processed;
    } else if (y < view->data_area.top + view->data_area.height * 2/3) {
	/* ignore the click */
    } else {
	if (mouse_move_pages_viewer)
	    view_move_down (view, view->data_area.height / 2);
	else
	    view_move_down (view, 1);
	goto processed;
    }

    return 0;

  processed:
    *result = MOU_REPEAT;
    return 1;
}

/* Real view only */
static int
real_view_event (Gpm_Event *event, void *x)
{
    WView *view = (WView *) x;
    int result;

    if (view_event (view, event, &result))
	view_update (view);
    return result;
}

static void
view_adjust_size (Dlg_head *h)
{
    WView *view;
    WButtonBar *bar;

    /* Look up the viewer and the buttonbar, we assume only two widgets here */
    view = (WView *) find_widget_type (h, view_callback);
    bar = find_buttonbar (h);
    widget_set_size (&view->widget, 0, 0, LINES - 1, COLS);
    widget_set_size ((Widget *) bar, LINES - 1, 0, 1, COLS);

    view_compute_areas (view);
    view_update_bytes_per_line (view);
}

/* Callback for the view dialog */
static cb_ret_t
view_dialog_callback (Dlg_head *h, dlg_msg_t msg, int parm)
{
    switch (msg) {
    case DLG_RESIZE:
	view_adjust_size (h);
	return MSG_HANDLED;

    default:
	return default_dlg_callback (h, msg, parm);
    }
}

/* {{{ External interface }}} */

/* Real view only */
int
mc_internal_viewer (const char *command, const char *file,
	int *move_dir_p, int start_line)
{
    gboolean succeeded;
    WView *wview;
    WButtonBar *bar;
    Dlg_head *view_dlg;

    /* Create dialog and widgets, put them on the dialog */
    view_dlg =
	create_dlg (0, 0, LINES, COLS, NULL, view_dialog_callback,
		    "[Internal File Viewer]", NULL, DLG_WANT_TAB);

    wview = view_new (0, 0, COLS, LINES - 1, 0);

    bar = buttonbar_new (1);

    add_widget (view_dlg, bar);
    add_widget (view_dlg, wview);

    succeeded = view_load (wview, command, file, start_line);
    if (succeeded) {
	run_dlg (view_dlg);
	if (move_dir_p)
	    *move_dir_p = wview->move_dir;
    } else {
	if (move_dir_p)
	    *move_dir_p = 0;
    }
    destroy_dlg (view_dlg);

    return succeeded;
}

/* {{{ Miscellaneous functions }}} */

static void
view_hook (void *v)
{
    WView *view = (WView *) v;
    WPanel *panel;

    /* If the user is busy typing, wait until he finishes to update the
       screen */
    if (!is_idle ()) {
	if (!hook_present (idle_hook, view_hook))
	    add_hook (&idle_hook, view_hook, v);
	return;
    }

    delete_hook (&idle_hook, view_hook);

    if (get_current_type () == view_listing)
	panel = current_panel;
    else if (get_other_type () == view_listing)
	panel = other_panel;
    else
	return;

    view_load (view, 0, panel->dir.list[panel->selected].fname, 0);
    display (view);
}

/* {{{ Event handling }}} */

static cb_ret_t
view_callback (Widget *w, widget_msg_t msg, int parm)
{
    WView *view = (WView *) w;
    cb_ret_t i;
    Dlg_head *h = view->widget.parent;

    view_compute_areas (view);
    view_update_bytes_per_line (view);

    switch (msg) {
    case WIDGET_INIT:
	if (view_is_in_panel (view))
	    add_hook (&select_file_hook, view_hook, view);
	else
	    view->dpy_bbar_dirty = TRUE;
	return MSG_HANDLED;

    case WIDGET_DRAW:
	display (view);
	return MSG_HANDLED;

    case WIDGET_CURSOR:
	if (view->hex_mode)
	    view_place_cursor (view);
	return MSG_HANDLED;

    case WIDGET_KEY:
	i = view_handle_key ((WView *) view, parm);
	if (view->want_to_quit && !view_is_in_panel (view))
	    dlg_stop (h);
	else {
	    view_update (view);
	}
	return i;

    case WIDGET_FOCUS:
	view->dpy_bbar_dirty = TRUE;
	view_update (view);
	return MSG_HANDLED;

    case WIDGET_DESTROY:
	view_done (view);
	if (view_is_in_panel (view))
	    delete_hook (&select_file_hook, view_hook);
	return MSG_HANDLED;

    default:
	return default_proc (msg, parm);
    }
}

/* {{{ External interface }}} */

WView *
view_new (int y, int x, int cols, int lines, int is_panel)
{
    WView *view = g_new0 (WView, 1);
    size_t i;

    init_widget (&view->widget, y, x, lines, cols,
		 view_callback,
		 real_view_event);

    view->filename          = NULL;
    view->command           = NULL;

    view_set_datasource_none (view);

    view->growbuf_in_use    = FALSE;
    /* leave the other growbuf fields uninitialized */

    view->hex_mode = FALSE;
    view->hexedit_mode = FALSE;
    view->hexview_in_text = FALSE;
    view->text_nroff_mode = FALSE;
    view->text_wrap_mode = FALSE;
    view->magic_mode = FALSE;

    view->hexedit_lownibble = FALSE;

    view->dpy_frame_size    = is_panel ? 1 : 0;
    view->dpy_start = 0;
    view->dpy_text_column   = 0;
    view->dpy_end = 0;
    view->hex_cursor        = 0;
    view->cursor_col        = 0;
    view->cursor_row        = 0;
    view->change_list       = NULL;
    view->converter         = str_cnv_from_term;

    /* {status,ruler,data}_area are left uninitialized */

    view->dirty             = 0;
    view->dpy_bbar_dirty = TRUE;
    view->bytes_per_line    = 1;

    view->search_start      = 0;
    view->search_end        = 0;

    view->want_to_quit      = FALSE;
    view->marker            = 0;
    for (i = 0; i < sizeof(view->marks) / sizeof(view->marks[0]); i++)
	view->marks[i] = 0;

    view->move_dir          = 0;
    view->update_steps      = 0;
    view->update_activate   = 0;

    if (default_hex_mode)
	view_toggle_hex_mode (view);
    if (default_nroff_flag)
	view_toggle_nroff_mode (view);
    if (global_wrap_mode)
	view_toggle_wrap_mode (view);
    if (default_magic_flag)
	view_toggle_magic_mode (view);

    return view;
}
