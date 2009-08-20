#ifndef MC_VIEWER_INTERNAL_H
#define MC_VIEWER_INTERNAL_H

#include "../src/widget.h"
#include "../src/search/search.h"

/*** typedefs(not structures) and defined constants ********************/

typedef unsigned char byte;

/* A width or height on the screen */
typedef unsigned int screen_dimen;

/* Offset in bytes into a file */
typedef unsigned long offset_type;

#define OFFSETTYPE_PRIX "lX"
#define OFFSETTYPE_PRId "lu"

/*** enums *************************************************************/

/* data sources of the view */
enum view_ds {
    DS_NONE,                    /* No data available */
    DS_STDIO_PIPE,              /* Data comes from a pipe using popen/pclose */
    DS_VFS_PIPE,                /* Data comes from a piped-in VFS file */
    DS_FILE,                    /* Data comes from a VFS file */
    DS_STRING                   /* Data comes from a string in memory */
};


/* Offset in bytes into a file */
typedef enum {
    INVALID_OFFSET = ((off_t) - 1),
    OFFSETTYPE_MAX = ((off_t) 1 << (sizeof (off_t) * 8 - 1) - 1)
}
mcview_offset_t;

enum ccache_type {
    CCACHE_OFFSET,
    CCACHE_LINECOL
};

typedef enum {
    NROFF_TYPE_NONE = 0,
    NROFF_TYPE_BOLD = 1,
    NROFF_TYPE_UNDERLINE = 2
} nroff_type_t;

/*** structures declarations (and typedefs of structures)***************/

/* A node for building a change list on change_list */
struct hexedit_change_node {
    struct hexedit_change_node *next;
    off_t offset;
    byte value;
};

struct area {
    screen_dimen top, left;
    screen_dimen height, width;
};


/* A cache entry for mapping offsets into line/column pairs and vice versa.
 * cc_offset, cc_line, and cc_column are the 0-based values of the offset,
 * line and column of that cache entry. cc_nroff_column is the column
 * corresponding to cc_offset in nroff mode.
 */
struct coord_cache_entry {
    off_t cc_offset;
    off_t cc_line;
    off_t cc_column;
    off_t cc_nroff_column;
};

struct mcview_nroff_struct;

typedef struct mcview_struct {
    Widget widget;

    char *filename;             /* Name of the file */
    char *command;              /* Command used to pipe data in */

    enum view_ds datasource;    /* Where the displayed data comes from */

    /* stdio pipe data source */
    FILE *ds_stdio_pipe;        /* Output of a shell command */

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
    byte **growbuf_blockptr;    /* Pointer to the block pointers */
    size_t growbuf_blocks;      /* The number of blocks in *block_ptr */
    size_t growbuf_lastindex;   /* Number of bytes in the last page of the
                                   growing buffer */
    gboolean growbuf_finished;  /* TRUE when all data has been read. */

    /* Editor modes */
    gboolean hex_mode;          /* Hexview or Hexedit */
    gboolean hexedit_mode;      /* Hexedit */
    gboolean hexview_in_text;   /* Is the hexview cursor in the text area? */
    gboolean text_nroff_mode;   /* Nroff-style highlighting */
    gboolean text_wrap_mode;    /* Wrap text lines to fit them on the screen */
    gboolean magic_mode;        /* Preprocess the file using external programs */
    gboolean utf8;              /* It's multibyte file codeset */

    /* Additional editor state */
    gboolean hexedit_lownibble; /* Are we editing the last significant nibble? */
    GArray *coord_cache;        /* Cache for mapping offsets to cursor positions */

    /* Display information */
    screen_dimen dpy_frame_size;        /* Size of the frame surrounding the real viewer */
    offset_type dpy_start;      /* Offset of the displayed data */
    offset_type dpy_end;        /* Offset after the displayed data */
    offset_type dpy_text_column;        /* Number of skipped columns in non-wrap
                                         * text mode */
    offset_type hex_cursor;     /* Hexview cursor position in file */
    screen_dimen cursor_col;    /* Cursor column */
    screen_dimen cursor_row;    /* Cursor row */
    struct hexedit_change_node *change_list;    /* Linked list of changes */
    struct area status_area;    /* Where the status line is displayed */
    struct area ruler_area;     /* Where the ruler is displayed */
    struct area data_area;      /* Where the data is displayed */

    int dirty;                  /* Number of skipped updates */
    gboolean dpy_bbar_dirty;    /* Does the button bar need to be updated? */

    /* Mode variables */
    int bytes_per_line;         /* Number of bytes per line in hex mode */

    /* Search variables */
    offset_type search_start;   /* First character to start searching from */
    offset_type search_end;     /* Length of found string or 0 if none was found */

    /* Pointer to the last search command */
    gboolean want_to_quit;      /* Prepare for cleanup ... */

    /* Markers */
    int marker;                 /* mark to use */
    offset_type marks[10];      /* 10 marks: 0..9 */

    int move_dir;               /* return value from widget:
                                 * 0 do nothing
                                 * -1 view previous file
                                 * 1 view next file
                                 */

    offset_type update_steps;   /* The number of bytes between percent
                                 * increments */
    offset_type update_activate;        /* Last point where we updated the status */

    /* converter for translation of text */
    GIConv converter;

    /* handle of search engine */
    mc_search_t *search;
    gchar *last_search_string;
    mc_search_type_t search_type;
    gboolean search_all_codepages;
    gboolean search_case;
    gboolean search_backwards;
    struct mcview_nroff_struct *search_nroff_seq;

    int search_numNeedSkipChar;
} mcview_t;

typedef struct mcview_nroff_struct{
    mcview_t *view;
    off_t index;
    int char_width;
    int current_char;
    nroff_type_t type;
    nroff_type_t prev_type;
} mcview_nroff_t;

/*** global variables defined in .c file *******************************/

/*** declarations of public functions **********************************/

/* actions_cmd.c: callbacks  */
cb_ret_t mcview_callback (Widget *, widget_msg_t, int);
cb_ret_t mcview_dialog_callback (Dlg_head *, dlg_msg_t, int);
void mcview_help_cmd (void);
void mcview_quit_cmd (mcview_t *);
void mcview_toggle_hex_mode_cmd (mcview_t *);
void mcview_moveto_line_cmd (mcview_t *);
void mcview_moveto_addr_cmd (mcview_t *);
void mcview_toggle_hexedit_mode_cmd (mcview_t *);
void mcview_hexedit_save_changes_cmd (mcview_t *);
void mcview_toggle_wrap_mode_cmd (mcview_t *);
void mcview_search_cmd (mcview_t *);
void mcview_toggle_magic_mode_cmd (mcview_t *);
void mcview_toggle_nroff_mode_cmd (mcview_t *);
void mcview_toggle_ruler_cmd (mcview_t *);


/* coord_cache.c: */
gboolean mcview_coord_cache_entry_less (const struct coord_cache_entry *,
                                        const struct coord_cache_entry *, enum ccache_type,
                                        gboolean);
#ifdef MC_ENABLE_DEBUGGING_CODE
void mcview_ccache_dump (mcview_t *);
#endif


/* datasource.c: */
void mcview_set_datasource_none (mcview_t *);
off_t mcview_get_filesize (mcview_t *);
char *mcview_get_ptr_file (mcview_t *, off_t);
char *mcview_get_ptr_string (mcview_t *, off_t);
int mcview_get_utf (mcview_t *, off_t, int *, gboolean *);
int mcview_get_byte_string (mcview_t *, off_t);
int mcview_get_byte_none (mcview_t *, off_t);
void mcview_set_byte (mcview_t *, off_t, byte);
void mcview_file_load_data (mcview_t *, off_t);
void mcview_close_datasource (mcview_t *);
void mcview_set_datasource_file (mcview_t *, int, const struct stat *);
gboolean mcview_load_command_output (mcview_t *, const char *);
void mcview_set_datasource_vfs_pipe (mcview_t *, int);
void mcview_set_datasource_string (mcview_t *, const char *);

/* dialog.c: */
gboolean mcview_dialog_search (mcview_t *);

/* display.c: */
void mcview_update (mcview_t *);
void mcview_display (mcview_t *);
void mcview_compute_areas (mcview_t *);
void mcview_update_bytes_per_line (mcview_t *);
void mcview_display_toggle_ruler (mcview_t *);
void mcview_display_clean (mcview_t *);
void mcview_display_ruler (mcview_t *);
void mcview_adjust_size (Dlg_head *);
void mcview_percent (mcview_t *, off_t);

/* growbuf.c: */
void mcview_growbuf_init (mcview_t *);
void mcview_growbuf_free (mcview_t *);
off_t mcview_growbuf_filesize (mcview_t *);
void mcview_growbuf_read_until (mcview_t *, off_t);
int mcview_get_byte_growing_buffer (mcview_t *, off_t);
char *mcview_get_ptr_growing_buffer (mcview_t *, off_t);

/* hex.c: */
void mcview_display_hex (mcview_t *);
gboolean mcview_hexedit_save_changes (mcview_t *);
void mcview_toggle_hexedit_mode (mcview_t *);
void mcview_hexedit_free_change_list (mcview_t *);
void mcview_enqueue_change (struct hexedit_change_node **, struct hexedit_change_node *);

/* lib.c: */
void mcview_toggle_magic_mode (mcview_t *);
void mcview_toggle_wrap_mode (mcview_t *);
void mcview_toggle_nroff_mode (mcview_t *);
void mcview_toggle_hex_mode (mcview_t *);
gboolean mcview_ok_to_quit (mcview_t *);
void mcview_done (mcview_t *);
void mcview_select_encoding (mcview_t *);
void mcview_show_error (mcview_t *, const char *);

/* move.c */
void mcview_move_up (mcview_t *, off_t);
void mcview_move_down (mcview_t *, off_t);
void mcview_move_left (mcview_t *, off_t);
void mcview_move_right (mcview_t *, off_t);
void mcview_scroll_to_cursor (mcview_t *);
void mcview_moveto_top (mcview_t *);
void mcview_moveto_bottom (mcview_t *);
void mcview_moveto_bol (mcview_t *);
void mcview_moveto_eol (mcview_t *);
void mcview_moveto_offset (mcview_t *, off_t);
void mcview_moveto (mcview_t *, off_t, off_t);
void mcview_coord_to_offset (mcview_t *, off_t *, off_t, off_t);
void mcview_offset_to_coord (mcview_t *, off_t *, off_t *, off_t);
void mcview_place_cursor (mcview_t *);
void mcview_moveto_match (mcview_t *);

/* nroff.c: */
void mcview_display_nroff (mcview_t *);
int mcview__get_nroff_real_len (mcview_t *, off_t, off_t);

mcview_nroff_t *mcview_nroff_seq_new_num (mcview_t * view, off_t);
mcview_nroff_t *mcview_nroff_seq_new (mcview_t * view);
void mcview_nroff_seq_free (mcview_nroff_t **);
nroff_type_t mcview_nroff_seq_info (mcview_nroff_t *);
int mcview_nroff_seq_next (mcview_nroff_t *);

/* plain.c: */
void mcview_display_text (mcview_t *);

/* search.c: */
int mcview_search_cmd_callback (const void *user_data, gsize char_offset);
int mcview_search_update_cmd_callback (const void *, gsize);
void mcview_do_search (mcview_t *);


/*** inline functions ****************************************************************************/

#include "../src/viewer/inlines.h"

#endif
