#ifndef MC_EDIT_WIDGET_H
#define MC_EDIT_WIDGET_H

#include "../src/dialog.h"		/* Widget */
#include "../src/search/search.h"		/* mc_search_t */
#include "edit.h"

#define MAX_MACRO_LENGTH 1024
#define N_LINE_CACHES 32

#define BOOK_MARK_COLOR ((25 << 8) | 5)
#define BOOK_MARK_FOUND_COLOR ((26 << 8) | 4)

struct _book_mark {
    int line;			/* line number */
    int c;			/* color */
    struct _book_mark *next;
    struct _book_mark *prev;
};

struct syntax_rule {
    unsigned short keyword;
    unsigned char end;
    unsigned char context;
    unsigned char _context;
    unsigned char border;
};

typedef struct edit_key_map_type {
    long key;
    long command;
} edit_key_map_type;

struct WEdit {
    Widget widget;

    int num_widget_lines;
    int num_widget_columns;

    char *filename;		/* Name of the file */
    char *dir;			/* NULL if filename is absolute */

    /* dynamic buffers and cursor position for editor: */
    long curs1;			/* position of the cursor from the beginning of the file. */
    long curs2;			/* position from the end of the file */
    unsigned char *buffers1[MAXBUFF + 1];	/* all data up to curs1 */
    unsigned char *buffers2[MAXBUFF + 1];	/* all data from end of file down to curs2 */

    /* UTF8 */
    char charbuf[4 + 1];
    int charpoint;
    /* search variables */
    mc_search_t *search;

    unsigned int search_type;
    int replace_mode;
    int replace_backwards;
    int replace_case;
    int all_codepages;

    long search_start;		/* First character to start searching from */
    int found_len;		/* Length of found string or 0 if none was found */
    long found_start;		/* the found word from a search - start position */

    /* display information */
    long last_byte;		/* Last byte of file */
    long start_display;		/* First char displayed */
    long start_col;		/* First displayed column, negative */
    long max_column;		/* The maximum cursor position ever reached used to calc hori scroll bar */
    long curs_row;		/* row position of cursor on the screen */
    long curs_col;		/* column position on screen */
    int force;			/* how much of the screen do we redraw? */
    unsigned int overwrite:1;	/* Overwrite on type mode (as opposed to insert) */
    unsigned int modified:1;	/* File has been modified and needs saving */
    unsigned int loading_done:1;/* File has been loaded into the editor */
    unsigned int locked:1;	/* We hold lock on current file */
    unsigned int screen_modified:1; /* File has been changed since the last screen draw */
    unsigned int delete_file:1;	/* New file, needs to be deleted unless modified */
    unsigned int highlight:1;	/* There is a selected block */
    unsigned int utf8:1;	/* It's multibyte file codeset */
    long prev_col;		/* recent column position of the cursor - used when moving
				   up or down past lines that are shorter than the current line */
    long curs_line;		/* line number of the cursor. */
    long start_line;		/* line number of the top of the page */

    /* file info */
    long total_lines;		/* total lines in the file */
    long mark1;			/* position of highlight start */
    long mark2;			/* position of highlight end */
    int column1;		/* position of column highlight start */
    int column2;		/* position of column highlight end */
    long bracket;		/* position of a matching bracket */

    /* cache speedup for line lookups */
    int caches_valid;
    int line_numbers[N_LINE_CACHES];
    long line_offsets[N_LINE_CACHES];

    struct _book_mark *book_mark;

    /* undo stack and pointers */
    unsigned long stack_pointer;
    long *undo_stack;
    unsigned long stack_size;
    unsigned long stack_size_mask;
    unsigned long stack_bottom;
    unsigned int stack_disable:1; /* If not 0, don't save events in the undo stack */

    struct stat stat1;		/* Result of mc_fstat() on the file */
    unsigned int skip_detach_prompt:1;	/* Do not prompt whether to detach a file anymore */

    /* syntax higlighting */
    struct _syntax_marker *syntax_marker;
    struct context_rule **rules;
    long last_get_rule;
    struct syntax_rule rule;
    char *syntax_type;		/* description of syntax highlighting type being used */
    GTree *defines;		/* List of defines */

    /* macro stuff */
    int macro_i;		/* index to macro[], -1 if not recording a macro */
    int macro_depth;		/* depth of the macro recursion */
    struct macro macro[MAX_MACRO_LENGTH];

    /* user map stuff */
    const edit_key_map_type *user_map;
    const edit_key_map_type *ext_map;

    int extmod;

    char *labels[10];

};

#endif
