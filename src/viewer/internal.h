#ifndef MC__VIEWER_INTERNAL_H
#define MC__VIEWER_INTERNAL_H

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

#include "lib/global.h"

#include "lib/search.h"
#include "lib/widget.h"
#include "lib/vfs/vfs.h"        /* vfs_path_t */

#include "src/keybind-defaults.h"       /* global_keymap_t */
#include "src/filemanager/dir.h"        /* dir_list */

/*** typedefs(not structures) and defined constants **********************************************/

typedef unsigned char byte;

/* A width or height on the screen */
typedef unsigned int screen_dimen;

extern const off_t INVALID_OFFSET;
extern const off_t OFFSETTYPE_MAX;

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

struct area
{
    screen_dimen top, left;
    screen_dimen height, width;
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

typedef struct
{
    size_t size;
    size_t capacity;
    coord_cache_entry_t **cache;
} coord_cache_t;

struct mcview_nroff_struct;

typedef struct mcview_struct
{
    Widget widget;

    vfs_path_t *filename_vpath; /* Name of the file */
    vfs_path_t *workdir_vpath;  /* Name of the working directory */
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
    GPtrArray *growbuf_blockptr;        /* Pointer to the block pointers */
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
    gboolean hexedit_lownibble; /* Are we editing the last significant nibble? */
    gboolean locked;            /* We hold lock on current file */

    gboolean utf8;              /* It's multibyte file codeset */

    coord_cache_t *coord_cache; /* Cache for mapping offsets to cursor positions */

    /* Display information */
    screen_dimen dpy_frame_size;        /* Size of the frame surrounding the real viewer */
    off_t dpy_start;            /* Offset of the displayed data */
    off_t dpy_end;              /* Offset after the displayed data */
    off_t dpy_text_column;      /* Number of skipped columns in non-wrap
                                 * text mode */
    off_t hex_cursor;           /* Hexview cursor position in file */
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
    off_t search_start;         /* First character to start searching from */
    off_t search_end;           /* Length of found string or 0 if none was found */

    /* Markers */
    int marker;                 /* mark to use */
    off_t marks[10];            /* 10 marks: 0..9 */

    off_t update_steps;         /* The number of bytes between percent
                                 * increments */
    off_t update_activate;      /* Last point where we updated the status */

    /* converter for translation of text */
    GIConv converter;

    /* handle of search engine */
    mc_search_t *search;
    gchar *last_search_string;
    struct mcview_nroff_struct *search_nroff_seq;

    int search_numNeedSkipChar;

    GArray *saved_bookmarks;

    dir_list *dir;              /* List of current directory files
                                 * to handle CK_FileNext and CK_FilePrev commands */
    int *dir_count;             /* Number of files in dir structure.
                                 * Pointer is used here as reference to WPanel::count */
    int *dir_idx;               /* Index of current file in dir structure.
                                 * Pointer is used here as reference to WPanel::count */
} mcview_t;

typedef struct mcview_nroff_struct
{
    mcview_t *view;
    off_t index;
    int char_width;
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

/* coord_cache.c: */
coord_cache_t *coord_cache_new (void);
void coord_cache_free (coord_cache_t * cache);

#ifdef MC_ENABLE_DEBUGGING_CODE
void mcview_ccache_dump (mcview_t * view);
#endif

void mcview_ccache_lookup (mcview_t * view, coord_cache_entry_t * coord,
                           enum ccache_type lookup_what);

/* datasource.c: */
void mcview_set_datasource_none (mcview_t *);
off_t mcview_get_filesize (mcview_t *);
void mcview_update_filesize (mcview_t * view);
char *mcview_get_ptr_file (mcview_t *, off_t);
char *mcview_get_ptr_string (mcview_t *, off_t);
int mcview_get_utf (mcview_t *, off_t, int *, gboolean *);
gboolean mcview_get_byte_string (mcview_t *, off_t, int *);
gboolean mcview_get_byte_none (mcview_t *, off_t, int *);
void mcview_set_byte (mcview_t *, off_t, byte);
void mcview_file_load_data (mcview_t *, off_t);
void mcview_close_datasource (mcview_t *);
void mcview_set_datasource_file (mcview_t *, int, const struct stat *);
gboolean mcview_load_command_output (mcview_t *, const char *);
void mcview_set_datasource_vfs_pipe (mcview_t *, int);
void mcview_set_datasource_string (mcview_t *, const char *);

/* dialog.c: */
gboolean mcview_dialog_search (mcview_t * view);
gboolean mcview_dialog_goto (mcview_t * view, off_t * offset);

/* display.c: */
void mcview_update (mcview_t * view);
void mcview_display (mcview_t * view);
void mcview_compute_areas (mcview_t * view);
void mcview_update_bytes_per_line (mcview_t * view);
void mcview_display_toggle_ruler (mcview_t * view);
void mcview_display_clean (mcview_t * view);
void mcview_display_ruler (mcview_t * view);
void mcview_percent (mcview_t * view, off_t p);

/* growbuf.c: */
void mcview_growbuf_init (mcview_t * view);
void mcview_growbuf_free (mcview_t * view);
off_t mcview_growbuf_filesize (mcview_t * view);
void mcview_growbuf_read_until (mcview_t * view, off_t p);
gboolean mcview_get_byte_growing_buffer (mcview_t * view, off_t p, int *);
char *mcview_get_ptr_growing_buffer (mcview_t * view, off_t p);

/* hex.c: */
void mcview_display_hex (mcview_t * view);
gboolean mcview_hexedit_save_changes (mcview_t * view);
void mcview_toggle_hexedit_mode (mcview_t * view);
void mcview_hexedit_free_change_list (mcview_t * view);
void mcview_enqueue_change (struct hexedit_change_node **, struct hexedit_change_node *);

/* lib.c: */
void mcview_toggle_magic_mode (mcview_t * view);
void mcview_toggle_wrap_mode (mcview_t * view);
void mcview_toggle_nroff_mode (mcview_t * view);
void mcview_toggle_hex_mode (mcview_t * view);
gboolean mcview_ok_to_quit (mcview_t * view);
void mcview_init (mcview_t * view);
void mcview_done (mcview_t * view);
void mcview_select_encoding (mcview_t * view);
void mcview_set_codeset (mcview_t * view);
void mcview_show_error (mcview_t * view, const char *error);
off_t mcview_bol (mcview_t * view, off_t current, off_t limit);
off_t mcview_eol (mcview_t * view, off_t current, off_t limit);
char *mcview_get_title (const WDialog * h, size_t len);

/* move.c */
void mcview_move_up (mcview_t *, off_t);
void mcview_move_down (mcview_t *, off_t);
void mcview_move_left (mcview_t *, off_t);
void mcview_move_right (mcview_t *, off_t);
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
void mcview_display_nroff (mcview_t * view);
int mcview__get_nroff_real_len (mcview_t * view, off_t, off_t p);

mcview_nroff_t *mcview_nroff_seq_new_num (mcview_t * view, off_t p);
mcview_nroff_t *mcview_nroff_seq_new (mcview_t * view);
void mcview_nroff_seq_free (mcview_nroff_t **);
nroff_type_t mcview_nroff_seq_info (mcview_nroff_t *);
int mcview_nroff_seq_next (mcview_nroff_t *);
int mcview_nroff_seq_prev (mcview_nroff_t *);


/* plain.c: */
void mcview_display_text (mcview_t *);

/* search.c: */
mc_search_cbret_t mcview_search_cmd_callback (const void *user_data, gsize char_offset,
                                              int *current_char);
int mcview_search_update_cmd_callback (const void *, gsize);
void mcview_do_search (mcview_t * view);

/*** inline functions ****************************************************************************/

#include "inlines.h"

#endif /* MC__VIEWER_INTERNAL_H */
