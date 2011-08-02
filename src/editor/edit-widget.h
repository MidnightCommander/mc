/** \file
 *  \brief Header: editor widget WEdit
 */

#ifndef MC__EDIT_WIDGET_H
#define MC__EDIT_WIDGET_H

#include "lib/search.h"         /* mc_search_t */
#include "lib/widget.h"         /* Widget */

#include "edit-impl.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define N_LINE_CACHES 32

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

struct _book_mark
{
    int line;                   /* line number */
    int c;                      /* color */
    struct _book_mark *next;
    struct _book_mark *prev;
};

struct syntax_rule
{
    unsigned short keyword;
    unsigned char end;
    unsigned char context;
    unsigned char _context;
    unsigned char border;
};

struct WEdit
{
    Widget widget;

    char *filename;             /* Name of the file */
    char *dir;                  /* NULL if filename is absolute */

    /* dynamic buffers and cursor position for editor: */
    long curs1;                 /* position of the cursor from the beginning of the file. */
    long curs2;                 /* position from the end of the file */
    unsigned char *buffers1[MAXBUFF + 1];       /* all data up to curs1 */
    unsigned char *buffers2[MAXBUFF + 1];       /* all data from end of file down to curs2 */

    /* UTF8 */
    char charbuf[4 + 1];
    int charpoint;

    /* search handler */
    mc_search_t *search;
    int replace_mode;

    char *last_search_string;   /* String that have been searched */
    long search_start;          /* First character to start searching from */
    int found_len;              /* Length of found string or 0 if none was found */
    long found_start;           /* the found word from a search - start position */

    /* display information */
    long last_byte;             /* Last byte of file */
    long start_display;         /* First char displayed */
    long start_col;             /* First displayed column, negative */
    long max_column;            /* The maximum cursor position ever reached used to calc hori scroll bar */
    long curs_row;              /* row position of cursor on the screen */
    long curs_col;              /* column position on screen */
    long over_col;              /* pos after '\n' */
    int force;                  /* how much of the screen do we redraw? */
    unsigned int overwrite:1;   /* Overwrite on type mode (as opposed to insert) */
    unsigned int modified:1;    /* File has been modified and needs saving */
    unsigned int loading_done:1;        /* File has been loaded into the editor */
    unsigned int locked:1;      /* We hold lock on current file */
    unsigned int delete_file:1; /* New file, needs to be deleted unless modified */
    unsigned int highlight:1;   /* There is a selected block */
    unsigned int column_highlight:1;
    unsigned int utf8:1;        /* It's multibyte file codeset */
    long prev_col;              /* recent column position of the cursor - used when moving
                                   up or down past lines that are shorter than the current line */
    long curs_line;             /* line number of the cursor. */
    long start_line;            /* line number of the top of the page */

    /* file info */
    long total_lines;           /* total lines in the file */
    long mark1;                 /* position of highlight start */
    long mark2;                 /* position of highlight end */
    long end_mark_curs;         /* position of cursor after end of highlighting */
    int column1;                /* position of column highlight start */
    int column2;                /* position of column highlight end */
    long bracket;               /* position of a matching bracket */

    /* cache speedup for line lookups */
    int caches_valid;
    int line_numbers[N_LINE_CACHES];
    long line_offsets[N_LINE_CACHES];

    struct _book_mark *book_mark;
    GArray *serialized_bookmarks;

    /* undo stack and pointers */
    unsigned long undo_stack_pointer;
    long *undo_stack;
    unsigned long undo_stack_size;
    unsigned long undo_stack_size_mask;
    unsigned long undo_stack_bottom;
    unsigned int undo_stack_disable:1;       /* If not 0, don't save events in the undo stack */

    unsigned long redo_stack_pointer;
    long *redo_stack;
    unsigned long redo_stack_size;
    unsigned long redo_stack_size_mask;
    unsigned long redo_stack_bottom;
    unsigned int redo_stack_reset:1;         /* If 1, need clear redo stack */

    struct stat stat1;          /* Result of mc_fstat() on the file */
    unsigned int skip_detach_prompt:1;  /* Do not prompt whether to detach a file anymore */

    /* syntax higlighting */
    struct _syntax_marker *syntax_marker;
    struct context_rule **rules;
    long last_get_rule;
    struct syntax_rule rule;
    char *syntax_type;          /* description of syntax highlighting type being used */
    GTree *defines;             /* List of defines */
    gboolean is_case_insensitive;       /* selects language case sensitivity */

    /* user map stuff */
    GIConv converter;

    /* line break */
    LineBreaks lb;
    gboolean extmod;

    char *labels[10];

};

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/*** inline functions ****************************************************************************/
#endif /* MC__EDIT_WIDGET_H */
