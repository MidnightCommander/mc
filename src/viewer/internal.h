#ifndef MC__VIEWER_INTERNAL_H
#define MC__VIEWER_INTERNAL_H

#include <limits.h>             /* CHAR_BIT */
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

#include "lib/global.h"

#include "lib/search.h"
#include "lib/widget.h"
#include "lib/vfs/vfs.h"        /* vfs_path_t */
#include "lib/util.h"           /* mc_pipe_t */

#include "src/keymap.h"         /* global_keymap_t */
#include "src/filemanager/dir.h"        /* dir_list */

#include "mcviewer.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define OFF_T_BITWIDTH ((unsigned int) (sizeof (off_t) * CHAR_BIT - 1))
#define OFFSETTYPE_MAX (((off_t) 1 << (OFF_T_BITWIDTH - 1)) - 1)

typedef unsigned char byte;

/*** enums ***************************************************************************************/

/* data sources of the view */
enum view_ds
{
    DS_NONE,                    /* No data available */
    DS_STDIO_PIPE,              /* Data comes from a pipe using popen/pclose */
    DS_VFS_PIPE,                /* Data comes from a piped-in VFS file */
    DS_FILE,                    /* Data comes from a VFS file */
    DS_STRING                   /* Data comes from a string in memory */
};

enum ccache_type
{
    CCACHE_OFFSET,
    CCACHE_LINECOL
};

typedef enum
{
    NROFF_TYPE_NONE = 0,
    NROFF_TYPE_BOLD = 1,
    NROFF_TYPE_UNDERLINE = 2
} nroff_type_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/* A node for building a change list on change_list */
struct hexedit_change_node
{
    struct hexedit_change_node *next;
    off_t offset;
    byte value;
};

/* A cache entry for mapping offsets into line/column pairs and vice versa.
 * cc_offset, cc_line, and cc_column are the 0-based values of the offset,
 * line and column of that cache entry. cc_nroff_column is the column
 * corresponding to cc_offset in nroff mode.
 */
typedef struct
{
    off_t cc_offset;
    off_t cc_line;
    off_t cc_column;
    off_t cc_nroff_column;
} coord_cache_entry_t;

/* TODO: find a better name. This is not actually a "state machine",
 * but a "state machine's state", but that sounds silly.
 * Could be parser_state, formatter_state... */
typedef struct
{
    off_t offset;               /* The file offset at which this is the state. */
    off_t unwrapped_column;     /* Columns if the paragraph wasn't wrapped, */
    /* used for positioning TABs in wrapped lines */
    gboolean nroff_underscore_is_underlined;    /* whether _\b_ is underlined rather than bold */
    gboolean print_lonely_combining;    /* whether lonely combining marks are printed on a dotted circle */
} mcview_state_machine_t;

struct mcview_nroff_struct;

struct WView
{
    Widget widget;

    vfs_path_t *filename_vpath; /* Name of the file */
    vfs_path_t *workdir_vpath;  /* Name of the working directory */
    char *command;              /* Command used to pipe data in */

    enum view_ds datasource;    /* Where the displayed data comes from */

    /* stdio pipe data source */
    mc_pipe_t *ds_stdio_pipe;   /* Output of a shell command */
    gboolean pipe_first_err_msg;        /* Show only 1st message from stderr */

    /* vfs pipe data source */
    int ds_vfs_pipe;            /* Non-seekable vfs file descriptor */

    /* vfs file data source */
    int ds_file_fd;             /* File with random access */
    off_t ds_file_filesize;     /* Size of the file */
    off_t ds_file_offset;       /* Offset of the currently loaded data */
    byte *ds_file_data;         /* Currently loaded data */
    size_t ds_file_datalen;     /* Number of valid bytes in file_data */
    size_t ds_file_datasize;    /* Number of allocated bytes in file_data */

    /* string data source */
    byte *ds_string_data;       /* The characters of the string */
    size_t ds_string_len;       /* The length of the string */

    /* Growing buffers information */
    gboolean growbuf_in_use;    /* Use the growing buffers? */
    GPtrArray *growbuf_blockptr;        /* Pointer to the block pointers */
    size_t growbuf_lastindex;   /* Number of bytes in the last page of the
                                   growing buffer */
    gboolean growbuf_finished;  /* TRUE when all data has been read. */

    mcview_mode_flags_t mode_flags;

    /* Hex editor modes */
    gboolean hexedit_mode;      /* Hexview or Hexedit */
    const global_keymap_t *hex_keymap;
    gboolean hexview_in_text;   /* Is the hexview cursor in the text area? */
    int bytes_per_line;         /* Number of bytes per line in hex mode */
    off_t hex_cursor;           /* Hexview cursor position in file */
    gboolean hexedit_lownibble; /* Are we editing the last significant nibble? */
    gboolean locked;            /* We hold lock on current file */

#ifdef HAVE_CHARSET
    gboolean utf8;              /* It's multibyte file codeset */
#endif

    GPtrArray *coord_cache;     /* Cache for mapping offsets to cursor positions */

    /* Display information */
    int dpy_frame_size;         /* Size of the frame surrounding the real viewer */
    off_t dpy_start;            /* Offset of the displayed data (start of the paragraph in non-hex mode) */
    off_t dpy_end;              /* Offset after the displayed data */
    off_t dpy_paragraph_skip_lines;     /* Extra lines to skip in wrap mode */
    mcview_state_machine_t dpy_state_top;       /* Parser-formatter state at the topmost visible line in wrap mode */
    mcview_state_machine_t dpy_state_bottom;    /* Parser-formatter state after the bottomvisible line in wrap mode */
    gboolean dpy_wrap_dirty;    /* dpy_state_top needs to be recomputed */
    off_t dpy_text_column;      /* Number of skipped columns in non-wrap
                                 * text mode */
    int cursor_col;             /* Cursor column */
    int cursor_row;             /* Cursor row */
    struct hexedit_change_node *change_list;    /* Linked list of changes */
    WRect status_area;          /* Where the status line is displayed */
    WRect ruler_area;           /* Where the ruler is displayed */
    WRect data_area;            /* Where the data is displayed */

    ssize_t force_max;          /* Force a max offset, or -1 */

    int dirty;                  /* Number of skipped updates */
    gboolean dpy_bbar_dirty;    /* Does the button bar need to be updated? */


    /* handle of search engine */
    mc_search_t *search;
    gchar *last_search_string;
    struct mcview_nroff_struct *search_nroff_seq;
    off_t search_start;         /* First character to start searching from */
    off_t search_end;           /* Length of found string or 0 if none was found */
    int search_numNeedSkipChar;

    /* Markers */
    int marker;                 /* mark to use */
    off_t marks[10];            /* 10 marks: 0..9 */

    off_t update_steps;         /* The number of bytes between percent increments */
    off_t update_activate;      /* Last point where we updated the status */

    /* converter for translation of text */
    GIConv converter;

    GArray *saved_bookmarks;

    dir_list *dir;              /* List of current directory files
                                 * to handle CK_FileNext and CK_FilePrev commands */
    int *dir_idx;               /* Index of current file in dir structure.
                                 * Pointer is used here as reference to WPanel::dir::count */
    vfs_path_t *ext_script;     /* Temporary script file created by regex_command_for() */
};

typedef struct mcview_nroff_struct
{
    WView *view;
    off_t index;
    int char_length;
    int current_char;
    nroff_type_t type;
    nroff_type_t prev_type;
} mcview_nroff_t;

typedef struct mcview_search_options_t
{
    mc_search_type_t type;
    gboolean case_sens;
    gboolean backwards;
    gboolean whole_words;
    gboolean all_codepages;
} mcview_search_options_t;

/*** global variables defined in .c file *********************************************************/

extern mcview_search_options_t mcview_search_options;

/*** declarations of public functions ************************************************************/

/* actions_cmd.c:  */
cb_ret_t mcview_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data);
cb_ret_t mcview_dialog_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm,
                                 void *data);

/* ascii.c: */
void mcview_display_text (WView * view);
void mcview_state_machine_init (mcview_state_machine_t *, off_t);
void mcview_ascii_move_down (WView * view, off_t lines);
void mcview_ascii_move_up (WView * view, off_t lines);
void mcview_ascii_moveto_bol (WView * view);
void mcview_ascii_moveto_eol (WView * view);

#ifdef MC_ENABLE_DEBUGGING_CODE
void mcview_ccache_dump (WView * view);
#endif

void mcview_ccache_lookup (WView * view, coord_cache_entry_t * coord, enum ccache_type lookup_what);

/* datasource.c: */
void mcview_set_datasource_none (WView * view);
off_t mcview_get_filesize (WView * view);
void mcview_update_filesize (WView * view);
char *mcview_get_ptr_file (WView * view, off_t byte_index);
char *mcview_get_ptr_string (WView * view, off_t byte_index);
gboolean mcview_get_utf (WView * view, off_t byte_index, int *ch, int *ch_len);
gboolean mcview_get_byte_string (WView * view, off_t byte_index, int *retval);
gboolean mcview_get_byte_none (WView * view, off_t byte_index, int *retval);
void mcview_set_byte (WView * view, off_t offset, byte b);
void mcview_file_load_data (WView * view, off_t byte_index);
void mcview_close_datasource (WView * view);
void mcview_set_datasource_file (WView * view, int fd, const struct stat *st);
gboolean mcview_load_command_output (WView * view, const char *command);
void mcview_set_datasource_vfs_pipe (WView * view, int fd);
void mcview_set_datasource_string (WView * view, const char *s);

/* dialog.c: */
gboolean mcview_dialog_search (WView * view);
gboolean mcview_dialog_goto (WView * view, off_t * offset);

/* display.c: */
void mcview_update (WView * view);
void mcview_display (WView * view);
void mcview_compute_areas (WView * view);
void mcview_update_bytes_per_line (WView * view);
void mcview_display_toggle_ruler (WView * view);
void mcview_display_clean (WView * view);
void mcview_display_ruler (WView * view);

/* growbuf.c: */
void mcview_growbuf_init (WView * view);
void mcview_growbuf_done (WView * view);
void mcview_growbuf_free (WView * view);
off_t mcview_growbuf_filesize (WView * view);
void mcview_growbuf_read_until (WView * view, off_t ofs);
gboolean mcview_get_byte_growing_buffer (WView * view, off_t byte_index, int *retval);
char *mcview_get_ptr_growing_buffer (WView * view, off_t byte_index);

/* hex.c: */
void mcview_display_hex (WView * view);
gboolean mcview_hexedit_save_changes (WView * view);
void mcview_toggle_hexedit_mode (WView * view);
void mcview_hexedit_free_change_list (WView * view);
void mcview_enqueue_change (struct hexedit_change_node **head, struct hexedit_change_node *node);

/* lib.c: */
void mcview_toggle_magic_mode (WView * view);
void mcview_toggle_wrap_mode (WView * view);
void mcview_toggle_nroff_mode (WView * view);
void mcview_toggle_hex_mode (WView * view);
void mcview_init (WView * view);
void mcview_done (WView * view);
#ifdef HAVE_CHARSET
void mcview_select_encoding (WView * view);
void mcview_set_codeset (WView * view);
#endif
void mcview_show_error (WView * view, const char *error);
off_t mcview_bol (WView * view, off_t current, off_t limit);
off_t mcview_eol (WView * view, off_t current);
char *mcview_get_title (const WDialog * h, size_t len);
int mcview_calc_percent (WView * view, off_t p);

/* move.c */
void mcview_move_up (WView * view, off_t lines);
void mcview_move_down (WView * view, off_t lines);
void mcview_move_left (WView * view, off_t columns);
void mcview_move_right (WView * view, off_t columns);
void mcview_moveto_top (WView * view);
void mcview_moveto_bottom (WView * view);
void mcview_moveto_bol (WView * view);
void mcview_moveto_eol (WView * view);
void mcview_moveto_offset (WView * view, off_t offset);
void mcview_moveto (WView * view, off_t, off_t col);
void mcview_coord_to_offset (WView * view, off_t * ret_offset, off_t line, off_t column);
void mcview_offset_to_coord (WView * view, off_t * ret_line, off_t * ret_column, off_t offset);
void mcview_place_cursor (WView * view);
void mcview_moveto_match (WView * view);

/* nroff.c: */
int mcview__get_nroff_real_len (WView * view, off_t start, off_t length);
mcview_nroff_t *mcview_nroff_seq_new_num (WView * view, off_t lc_index);
mcview_nroff_t *mcview_nroff_seq_new (WView * view);
void mcview_nroff_seq_free (mcview_nroff_t ** nroff);
nroff_type_t mcview_nroff_seq_info (mcview_nroff_t * nroff);
int mcview_nroff_seq_next (mcview_nroff_t * nroff);
int mcview_nroff_seq_prev (mcview_nroff_t * nroff);

/* search.c: */
gboolean mcview_search_init (WView * view);
void mcview_search_deinit (WView * view);
mc_search_cbret_t mcview_search_cmd_callback (const void *user_data, gsize char_offset,
                                              int *current_char);
mc_search_cbret_t mcview_search_update_cmd_callback (const void *user_data, gsize char_offset);
void mcview_do_search (WView * view, off_t want_search_start);

/* --------------------------------------------------------------------------------------------- */
/*** inline functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static inline off_t
mcview_offset_rounddown (off_t a, off_t b)
{
    g_assert (b != 0);
    return a - a % b;
}

/* --------------------------------------------------------------------------------------------- */

/* {{{ Simple Primitive Functions for WView }}} */
static inline gboolean
mcview_is_in_panel (WView *view)
{
    return (view->dpy_frame_size != 0);
}

/* --------------------------------------------------------------------------------------------- */

static inline gboolean
mcview_may_still_grow (WView *view)
{
    return (view->growbuf_in_use && !view->growbuf_finished);
}

/* --------------------------------------------------------------------------------------------- */

/* returns TRUE if the idx lies in the half-open interval
 * [offset; offset + size), FALSE otherwise.
 */
static inline gboolean
mcview_already_loaded (off_t offset, off_t idx, size_t size)
{
    return (offset <= idx && idx - offset < (off_t) size);
}

/* --------------------------------------------------------------------------------------------- */

static inline gboolean
mcview_get_byte_file (WView *view, off_t byte_index, int *retval)
{
    g_assert (view->datasource == DS_FILE);

    mcview_file_load_data (view, byte_index);
    if (mcview_already_loaded (view->ds_file_offset, byte_index, view->ds_file_datalen))
    {
        if (retval)
            *retval = view->ds_file_data[byte_index - view->ds_file_offset];
        return TRUE;
    }
    if (retval)
        *retval = -1;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static inline gboolean
mcview_get_byte (WView *view, off_t offset, int *retval)
{
    switch (view->datasource)
    {
    case DS_STDIO_PIPE:
    case DS_VFS_PIPE:
        return mcview_get_byte_growing_buffer (view, offset, retval);
    case DS_FILE:
        return mcview_get_byte_file (view, offset, retval);
    case DS_STRING:
        return mcview_get_byte_string (view, offset, retval);
    case DS_NONE:
        return mcview_get_byte_none (view, offset, retval);
    default:
        return FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static inline gboolean
mcview_get_byte_indexed (WView *view, off_t base, off_t ofs, int *retval)
{
    if (base <= OFFSETTYPE_MAX - ofs)
        return mcview_get_byte (view, base + ofs, retval);

    if (retval != NULL)
        *retval = -1;

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static inline int
mcview_count_backspaces (WView *view, off_t offset)
{
    int backspaces = 0;
    int c;

    while (offset >= 2 * backspaces && mcview_get_byte (view, offset - 2 * backspaces, &c)
           && c == '\b')
        backspaces++;

    return backspaces;
}

/* --------------------------------------------------------------------------------------------- */

static inline gboolean
mcview_is_nroff_sequence (WView *view, off_t offset)
{
    int c0, c1, c2;

    /* The following commands are ordered to speed up the calculation. */

    if (!mcview_get_byte_indexed (view, offset, 1, &c1) || c1 != '\b')
        return FALSE;

    if (!mcview_get_byte_indexed (view, offset, 0, &c0) || !g_ascii_isprint (c0))
        return FALSE;

    if (!mcview_get_byte_indexed (view, offset, 2, &c2) || !g_ascii_isprint (c2))
        return FALSE;

    return (c0 == c2 || c0 == '_' || (c0 == '+' && c2 == 'o'));
}

/* --------------------------------------------------------------------------------------------- */

static inline void
mcview_growbuf_read_all_data (WView *view)
{
    mcview_growbuf_read_until (view, OFFSETTYPE_MAX);
}

/* --------------------------------------------------------------------------------------------- */

#endif /* MC__VIEWER_INTERNAL_H */
