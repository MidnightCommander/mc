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

/*** enums *************************************************************/

/* data sources of the view */
enum view_ds {
    DS_NONE,			/* No data available */
    DS_STDIO_PIPE,		/* Data comes from a pipe using popen/pclose */
    DS_VFS_PIPE,		/* Data comes from a piped-in VFS file */
    DS_FILE,			/* Data comes from a VFS file */
    DS_STRING			/* Data comes from a string in memory */
};

/*** structures declarations (and typedefs of structures)***************/

struct area {
    screen_dimen top, left;
    screen_dimen height, width;
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
    GArray *coord_cache;	/* Cache for mapping offsets to cursor positions */

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
};

/*** global variables defined in .c file *******************************/

/*** declarations of public functions **********************************/

#endif
