/* View file module for the Midnight Commander
   Copyright (C) 1994, 1995, 1996 The Free Software Foundation
   Written by: 1994, 1995, 1998 Miguel de Icaza
               1994, 1995 Janne Kukonlehto
               1995 Jakub Jelinek
               1996 Joseph M. Hinkle
	       1997 Norbert Warmuth
	       1998 Pavel Machek

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif
#include <string.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
#   include <sys/mman.h>
#endif
#include <ctype.h>		/* For toupper() */
#include <errno.h>
#include <limits.h>

#include "global.h"
#include "tty.h"
#include "cmd.h"		/* For view_other_cmd */
#include "dlg.h"		/* Needed by widget.h */
#include "widget.h"		/* Needed for buttonbar_new */
#include "color.h"
#include "dialog.h"
#include "mouse.h"
#include "help.h"
#include "key.h"		/* For mi_getch() */
#include "layout.h"
#include "setup.h"
#include "wtools.h"		/* For query_set_sel() */
#include "../vfs/vfs.h"
#include "dir.h"
#include "panel.h"		/* Needed for current_panel and other_panel */
#include "win.h"
#include "main.h"		/* For the externs */
#define WANT_WIDGETS
#include "view.h"

#include "charsets.h"
#include "selcodepage.h"

#ifndef MAP_FILE
#define MAP_FILE 0
#endif

/* Block size for reading files in parts */
#define READ_BLOCK 8192
#define VIEW_PAGE_SIZE 8192

#ifdef IS_AIX
#   define IFAIX(x) case (x):
#else
#   define IFAIX(x)
#endif

#define vwidth (view->widget.cols - (view->have_frame ? 2 : 0))
#define vheight (view->widget.lines - (view->have_frame ? 2 : 0))

/* The growing buffers data types */
typedef struct block_ptr_t {
    unsigned char *data;
} block_ptr_t;

/* A node for building a change list on change_list */
struct hexedit_change_node {
   struct hexedit_change_node *next;
   long                       offset;
   unsigned char              value;
};

enum ViewSide {
    view_side_left,
    view_side_right
};

struct WView {
    Widget widget;

    char *filename;		/* Name of the file */
    char *command;		/* Command used to pipe data in */
    char *localcopy;
    int view_active;
    int have_frame;
    
    unsigned char *data;	/* Memory area for the file to be viewed */

    /* File information */
    int file;			/* File descriptor (for mmap and munmap) */
    FILE *stdfile;		/* Stdio struct for reading file in parts */
    int reading_pipe;		/* Flag: Reading from pipe(use popen/pclose) */
    unsigned long bytes_read;   /* How much of file is read */
    int mmapping;		/* Did we use mmap on the file? */

    /* Display information */
    unsigned long last;         /* Last byte shown */
    unsigned long last_byte;    /* Last byte of file */
    long first;			/* First byte in file */
    long bottom_first;	/* First byte shown when very last page is displayed */
				/* For the case of WINCH we should reset it to -1 */
    unsigned long start_display;/* First char displayed */
    int  start_col;		/* First displayed column, negative */
    unsigned long edit_cursor;  /* HexEdit cursor position in file */
    char hexedit_mode;          /* Hexidecimal editing mode flag */ 
    char nib_shift;             /* A flag for inserting nibbles into bytes */
    enum ViewSide view_side;	/* A flag for the active editing panel */
    int  file_dirty;            /* Number of changes */
    int  start_save;            /* Line start shift between Ascii and Hex */ 
    int  cursor_col;		/* Cursor column */
    int  cursor_row;		/* Cursor row */
    struct hexedit_change_node *change_list;   /* Linked list of changes */

    int dirty;			/* Number of skipped updates */
    int wrap_mode;		/* wrap_mode */
	
    /* Mode variables */
    int hex_mode;		/* Hexadecimal mode flag */
    int bytes_per_line;		/* Number of bytes per line in hex mode */
    int viewer_magic_flag;	/* Selected viewer */
    int viewer_nroff_flag;	/* Do we do nroff style highlighting? */
    
    /* Growing buffers information */
    int growing_buffer;		/* Use the growing buffers? */
    struct block_ptr_t *block_ptr;	/* Pointer to the block pointers */
    int          blocks;	/* The number of blocks in *block_ptr */

    
    /* Search variables */
    int search_start;		/* First character to start searching from */
    int found_len;		/* Length of found string or 0 if none was found */
    char *search_exp;		/* The search expression */
    int  direction;		/* 1= forward; -1 backward */
    void (*last_search)(void *, char *);
                                /* Pointer to the last search command */
    int view_quit;		/* Quit flag */

    int monitor;		/* Monitor file growth (like tail -f) */
    /* Markers */
    int marker;			/* mark to use */
    int marks [10];		/* 10 marks: 0..9 */
    
	
    int  move_dir;		/* return value from widget:  
				 * 0 do nothing
				 * -1 view previous file
				 * 1 view next file
				 */
    struct stat s;		/* stat for file */
};


/* Maxlimit for skipping updates */
int max_dirt_limit =
#ifdef NATIVE_WIN32
    0;
#else
    10;
#endif

extern Hook *idle_hook;

/* If set, show a ruler */
static int ruler = 0;

/* Scrolling is done in pages or line increments */
int mouse_move_pages_viewer = 1;

/* Used to compute the bottom first variable */
int have_fast_cpu = 0;

/* wrap mode default */
int global_wrap_mode = 1;

int default_hex_mode = 0;
static int default_hexedit_mode = 0;
int default_magic_flag = 1;
int default_nroff_flag = 1;
int altered_hex_mode = 0;
int altered_magic_flag = 0;
int altered_nroff_flag = 0;

static const char hex_char[] = "0123456789ABCDEF";

/* Our callback */
static int view_callback (WView *view, int msg, int par);

static int regexp_view_search (WView * view, char *pattern, char *string,
			       int match_type);
static void view_move_forward (WView * view, int i);
static void view_labels (WView * view);
static void set_monitor (WView * view, int set_on);
static void view_update (WView * view, gboolean update_gui);


static void
close_view_file (WView *view)
{
    if (view->file != -1) {
	mc_close (view->file);
	view->file = -1;
    }
}

static void
free_file (WView *view)
{
    int i;

#ifdef HAVE_MMAP
    if (view->mmapping) {
	mc_munmap (view->data, view->s.st_size);
	close_view_file (view);
    } else
#endif				/* HAVE_MMAP */
    {
	if (view->reading_pipe) {
	    /* Check error messages */
	    if (!view->have_frame)
		check_error_pipe ();

	    /* Close pipe */
	    pclose (view->stdfile);
	    view->stdfile = NULL;

	    /* Ignore errors because we don't want to hear about broken pipe */
	    close_error_pipe (-1, NULL);
	} else
	    close_view_file (view);
    }
    /* Block_ptr may be zero if the file was a file with 0 bytes */
    if (view->growing_buffer && view->block_ptr) {
	for (i = 0; i < view->blocks; i++) {
	    g_free (view->block_ptr[i].data);
	}
	g_free (view->block_ptr);
    }
}

/* Valid parameters for second parameter to set_monitor */
enum { off, on };

/* Both views */
static void
view_done (WView *view)
{
    set_monitor (view, off);

    /* alex: release core, used to replace mmap */
    if (!view->mmapping && !view->growing_buffer && view->data != NULL) {
	g_free (view->data);
	view->data = NULL;
    }

    if (view->view_active) {
	if (view->localcopy)
	    mc_ungetlocalcopy (view->filename, view->localcopy, 0);
	free_file (view);
	g_free (view->filename);
	if (view->command)
	    g_free (view->command);
    }
    view->view_active = 0;
    default_hex_mode = view->hex_mode;
    default_nroff_flag = view->viewer_nroff_flag;
    default_magic_flag = view->viewer_magic_flag;
    global_wrap_mode = view->wrap_mode;
}

static void view_hook (void *);

static void
view_destroy (WView *view)
{
    view_done (view);
    if (view->have_frame)
	delete_hook (&select_file_hook, view_hook);
}

static int
get_byte (WView *view, unsigned int byte_index)
{
    int page = byte_index / VIEW_PAGE_SIZE + 1;
    int offset = byte_index % VIEW_PAGE_SIZE;
    int i, n;

    if (view->growing_buffer) {
	if (page > view->blocks) {
	    view->block_ptr = g_realloc (view->block_ptr,
					 sizeof (block_ptr_t) * page);
	    for (i = view->blocks; i < page; i++) {
		char *p = g_malloc (VIEW_PAGE_SIZE);
		view->block_ptr[i].data = p;
		if (!p)
		    return '\n';
		if (view->stdfile != NULL)
		    n = fread (p, 1, VIEW_PAGE_SIZE, view->stdfile);
		else
		    n = mc_read (view->file, p, VIEW_PAGE_SIZE);
/*
 * FIXME: Errors are ignored at this point
 * Also should report preliminary EOF
 */
		if (n != -1)
		    view->bytes_read += n;
		if (view->s.st_size < view->bytes_read) {
		    view->bottom_first = -1;	/* Invalidate cache */
		    view->s.st_size = view->bytes_read;
		    view->last_byte = view->bytes_read;
		    if (view->reading_pipe)
			view->last_byte = view->first + view->bytes_read;
		}
		/* To force loading the next page */
		if (n == VIEW_PAGE_SIZE && view->reading_pipe) {
		    view->last_byte++;
		}
	    }
	    view->blocks = page;
	}
	if (byte_index > view->bytes_read) {
	    return -1;
	} else
	    return view->block_ptr[page - 1].data[offset];
    } else {
	if (byte_index >= view->last_byte)
	    return -1;
	else
	    return view->data[byte_index];
    }
}

static void
enqueue_change (struct hexedit_change_node **head,
		struct hexedit_change_node *node)
{
    struct hexedit_change_node *curr = *head;

    while (curr) {
	if (node->offset < curr->offset) {
	    *head = node;
	    node->next = curr;
	    return;
	}
	head = (struct hexedit_change_node **) curr;
	curr = curr->next;
    }
    *head = node;
    node->next = curr;
}

static void move_right (WView *);

static void
put_editkey (WView *view, unsigned char key)
{
    struct hexedit_change_node *node;
    unsigned char byte_val;

    if (!view->hexedit_mode || view->growing_buffer != 0)
	return;

    /* Has there been a change at this position ? */
    node = view->change_list;
    while (node && (node->offset != view->edit_cursor)) {
	node = node->next;
    }

    if (view->view_side == view_side_left) {
	/* Hex editing */

	if (key >= '0' && key <= '9')
	    key -= '0';
	else if (key >= 'A' && key <= 'F')
	    key -= '7';
	else if (key >= 'a' && key <= 'f')
	    key -= 'W';
	else
	    return;

	if (node)
	    byte_val = node->value;
	else
	    byte_val = get_byte (view, view->edit_cursor);

	if (view->nib_shift == 0) {
	    byte_val = (byte_val & 0x0f) | (key << 4);
	} else {
	    byte_val = (byte_val & 0xf0) | (key);
	}
    } else {
	/* Text editing */
	byte_val = key;
    }
    if (!node) {
	node = (struct hexedit_change_node *)
	    g_new (struct hexedit_change_node, 1);

	if (node) {
#ifndef HAVE_MMAP
	    /* alex@bcs.zaporizhzhe.ua: here we are using file copy
	     * completely loaded into memory, so we can replace bytes in
	     * view->data array to allow changes to be reflected when
	     * user switches back to ascii mode */
	    view->data[view->edit_cursor] = byte_val;
#endif				/* !HAVE_MMAP */
	    node->offset = view->edit_cursor;
	    node->value = byte_val;
	    enqueue_change (&view->change_list, node);
	}
    } else {
	node->value = byte_val;
    }
    view->dirty++;
    view_update (view, TRUE);
    move_right (view);
}

static void
free_change_list (WView *view)
{
    struct hexedit_change_node *n = view->change_list;

    while (n) {
	view->change_list = n->next;
	g_free (n);
	n = view->change_list;
    }
    view->file_dirty = 0;
    view->dirty++;
}

static void
save_edit_changes (WView *view)
{
    struct hexedit_change_node *node = view->change_list;
    int fp;

    do {
	fp = open (view->filename, O_WRONLY);
	if (fp >= 0) {
	    while (node) {
		if (lseek (fp, node->offset, SEEK_SET) == -1 ||
		    write (fp, &node->value, 1) != 1) {
		    close (fp);
		    fp = -1;
		    break;
		}
		node = node->next;
	    }
	    if (fp != -1)
		close (fp);
	}

	if (fp == -1) {
	    fp = query_dialog (_(" Save file "),
			       _(" Cannot save file. "),
			       2, 2, _("&Retry"), _("&Cancel")) - 1;
	}
    } while (fp == -1);

    free_change_list (view);
}

static int
view_ok_to_quit (WView *view)
{
    int r;

    if (!view->change_list)
	return 1;

    r = query_dialog (_("Quit"),
		      _(" File was modified, Save with exit? "), 2, 3,
		      _("Cancel quit"), _("&Yes"), _("&No"));

    switch (r) {
    case 1:
	save_edit_changes (view);
	return 1;
    case 2:
	free_change_list (view);
	return 1;
    default:
	return 0;
    }
}

static char *
set_view_init_error (WView *view, const char *msg)
{
    view->growing_buffer = 0;
    view->reading_pipe = 0;
    view->first = 0;
    view->last_byte = 0;
    if (msg) {
	view->bytes_read = strlen (msg);
	return g_strdup (msg);
    }
    return 0;
}

/* return values: NULL for success, else points to error message */
static char *
init_growing_view (WView *view, char *name, char *filename)
{
    char *err_msg = NULL;

    view->growing_buffer = 1;

    if (name) {
	view->reading_pipe = 1;
	view->s.st_size = 0;

	open_error_pipe ();
	if ((view->stdfile = popen (name, "r")) == NULL) {
	    /* Avoid two messages.  Message from stderr has priority.  */
	    if (!close_error_pipe (view->have_frame ? -1 : 1, view->data))
		err_msg = _(" Cannot spawn child program ");
	    return set_view_init_error (view, err_msg);
	}

	/* First, check if filter produced any output */
	get_byte (view, 0);
	if (view->bytes_read <= 0) {
	    pclose (view->stdfile);
	    view->stdfile = NULL;
	    /* Avoid two messages.  Message from stderr has priority.  */
	    if (!close_error_pipe (view->have_frame ? -1 : 1, view->data))
		err_msg = _("Empty output from child filter");
	    return set_view_init_error (view, err_msg);
	}
    } else {
	view->stdfile = NULL;
	if ((view->file = mc_open (filename, O_RDONLY)) == -1)
	    return set_view_init_error (view, _(" Cannot open file "));
    }
    return NULL;
}

/* Load filename into core */
/* returns:
   -1 on failure.
   if (have_frame), we return success, but data points to a
   error message instead of the file buffer (quick_view feature).
*/
static char *
load_view_file (WView *view, int fd)
{
    view->file = fd;

    if (view->s.st_size == 0) {
	/* Must be one of those nice files that grow (/proc) */
	close_view_file (view);
	return init_growing_view (view, 0, view->filename);
    }
#ifdef HAVE_MMAP
    view->data =
	mc_mmap (0, view->s.st_size, PROT_READ, MAP_FILE | MAP_SHARED,
		 view->file, 0);
    if ((caddr_t) view->data != (caddr_t) - 1) {
	/* mmap worked */
	view->first = 0;
	view->bytes_read = view->s.st_size;
	view->mmapping = 1;
	return NULL;
    }
#endif				/* HAVE_MMAP */

    /* For those OS that dont provide mmap call. Try to load all the
     * file into memory (alex@bcs.zaporizhzhe.ua). Also, mmap can fail
     * for any reason, so we use this as fallback (pavel@ucw.cz) */

    view->data = (unsigned char *) g_malloc (view->s.st_size);
    if (view->data == NULL
	|| mc_lseek (view->file, 0, SEEK_SET) != 0
	|| mc_read (view->file, view->data,
		    view->s.st_size) != view->s.st_size) {
	if (view->data != NULL)
	    g_free (view->data);
	close_view_file (view);
	return init_growing_view (view, 0, view->filename);
    }
    view->first = 0;
    view->bytes_read = view->s.st_size;
    return NULL;
}

/* Return zero on success, -1 on failure */
static int
do_view_init (WView *view, char *_command, const char *_file,
	      int start_line)
{
    char *error = 0;
    int i, type;
    int fd = -1;
    char tmp[BUF_MEDIUM];

    if (view->view_active)
	view_done (view);

    /* Set up the state */
    view->block_ptr = 0;
    view->data = NULL;
    view->growing_buffer = 0;
    view->reading_pipe = 0;
    view->mmapping = 0;
    view->blocks = 0;
    view->block_ptr = 0;
    view->first = view->bytes_read = 0;
    view->last_byte = 0;
    view->filename = g_strdup (_file);
    view->localcopy = 0;
    view->command = 0;
    view->last = view->first + ((LINES - 2) * view->bytes_per_line);

    /* Clear the markers */
    view->marker = 0;
    for (i = 0; i < 10; i++)
	view->marks[i] = 0;

    if (!view->have_frame) {
	view->start_col = 0;
    }

    if (_command && (view->viewer_magic_flag || _file[0] == '\0')) {
	error = init_growing_view (view, _command, view->filename);
    } else if (_file[0]) {
	int cntlflags;

	/* Open the file */
	if ((fd = mc_open (_file, O_RDONLY | O_NONBLOCK)) == -1) {
	    g_snprintf (tmp, sizeof (tmp), _(" Cannot open \"%s\"\n %s "),
			_file, unix_error_string (errno));
	    error = set_view_init_error (view, tmp);
	    goto finish;
	}

	/* Make sure we are working with a regular file */
	if (mc_fstat (fd, &view->s) == -1) {
	    mc_close (fd);
	    g_snprintf (tmp, sizeof (tmp), _(" Cannot stat \"%s\"\n %s "),
			_file, unix_error_string (errno));
	    error = set_view_init_error (view, tmp);
	    goto finish;
	}

	if (!S_ISREG (view->s.st_mode)) {
	    mc_close (fd);
	    g_snprintf (tmp, sizeof (tmp),
			_(" Cannot view: not a regular file "));
	    error = set_view_init_error (view, tmp);
	    goto finish;
	}

	/* We don't need O_NONBLOCK after opening the file, unset it */
	cntlflags = fcntl (fd, F_GETFL, 0);
	if (cntlflags != -1) {
	    cntlflags &= ~O_NONBLOCK;
	    fcntl (fd, F_SETFL, cntlflags);
	}

	type = get_compression_type (fd);

	if (view->viewer_magic_flag && (type != COMPRESSION_NONE)) {
	    g_free (view->filename);
	    view->filename =
		g_strconcat (_file, decompress_extension (type), NULL);
	}

	error = load_view_file (view, fd);
    }

  finish:
    if (error) {
	if (!view->have_frame) {
	    message (1, MSG_ERROR, "%s", error);
	    g_free (error);
	    return -1;
	}
    }

    view->view_active = 1;
    if (_command)
	view->command = g_strdup (_command);
    else
	view->command = 0;
    view->search_start = view->start_display = view->start_save =
	view->first;
    view->found_len = 0;
    view->start_col = 0;
    view->last_search = 0;	/* Start a new search */

    /* Special case: The data points to the error message */
    if (error) {
	view->data = error;
	view->file = -1;
	view->s.st_size = view->bytes_read = strlen (view->data);
    }
    view->last_byte = view->first + view->s.st_size;

    if (start_line > 1 && !error) {
	int saved_wrap_mode = view->wrap_mode;

	view->wrap_mode = 0;
	get_byte (view, 0);
	view_move_forward (view, start_line - 1);
	view->wrap_mode = saved_wrap_mode;
    }
    view->edit_cursor = view->first;
    view->file_dirty = 0;
    view->nib_shift = 0;
    view->view_side = view_side_left;
    view->change_list = NULL;

    return 0;
}

void
view_update_bytes_per_line (WView *view)
{
    int cols;

    if (view->have_frame)
	cols = view->widget.cols - 2;
    else
	cols = view->widget.cols;

    view->bottom_first = -1;
    if (cols < 80)
	view->bytes_per_line = ((cols - 8) / 17) * 4;
    else
	view->bytes_per_line = ((cols - 8) / 18) * 4;

    if (view->bytes_per_line == 0)
	view->bytes_per_line++;	/* To avoid division by 0 */

    view->dirty = max_dirt_limit + 1;	/* To force refresh */
}

/* Both views */
/* Return zero on success, -1 on failure */
int
view_init (WView *view, char *_command, const char *_file, int start_line)
{
    if (!view->view_active || strcmp (_file, view->filename)
	|| altered_magic_flag)
	return do_view_init (view, _command, _file, start_line);
    else
	return 0;
}

static void
view_percent (WView *view, int p, int w, gboolean update_gui)
{
    int percent;

    percent = (view->s.st_size == 0
	       || view->last_byte == view->last) ? 100 : (p >
							  (INT_MAX /
							   100) ? p /
							  (view->s.
							   st_size /
							   100) : p * 100 /
							  view->s.st_size);

#if 0
    percent = view->s.st_size == 0 ? 100 :
	(view->last_byte == view->last ? 100 :
	 (p) * 100 / view->s.st_size);
#endif

    widget_move (view, view->have_frame, w - 5);
    printw ("%3d%%", percent);
}

static void
view_status (WView *view, gboolean update_gui)
{
    static int i18n_adjust = 0;
    static char *file_label;

    int w = view->widget.cols - (view->have_frame * 2);
    int i;

    attrset (SELECTED_COLOR);
    widget_move (view, view->have_frame, view->have_frame);
    hline (' ', w);

    if (!i18n_adjust) {
	file_label = _("File: %s");
	i18n_adjust = strlen (file_label) - 2;
    }

    if (w < i18n_adjust + 6)
	addstr (name_trunc (view->filename ? view->filename :
			    view->command ? view->command : "", w));
    else {
	i = (w > 22 ? 22 : w) - i18n_adjust;
	printw (file_label, name_trunc (view->filename ? view->filename :
					view->command ? view->command : "",
					i));
	if (w > 46) {
	    widget_move (view, view->have_frame, 24 + view->have_frame);
	    if (view->hex_mode)
		printw (_("Offset 0x%08x"), view->edit_cursor);
	    else
		printw (_("Col %d"), -view->start_col);
	}
	if (w > 62) {
	    widget_move (view, view->have_frame, 43 + view->have_frame);
	    printw (_("%s bytes"), size_trunc (view->s.st_size));
	}
	if (w > 70) {
	    printw (" ");
	    if (view->growing_buffer)
		addstr (_("  [grow]"));
	}
	if (w > 26) {
	    view_percent (view,
			  view->hex_mode ? view->edit_cursor : view->
			  start_display,
			  view->widget.cols - view->have_frame + 1,
			  update_gui);
	}
    }
    attrset (SELECTED_COLOR);
}

static inline void
view_display_clean (WView *view, int height, int width)
{
    /* FIXME: Should I use widget_erase only and repaint the box? */
    if (view->have_frame) {
	int i;

	draw_double_box (view->widget.parent, view->widget.y,
			 view->widget.x, view->widget.lines,
			 view->widget.cols);
	for (i = 1; i < height; i++) {
	    widget_move (view, i, 1);
	    printw ("%*s", width - 1, "");
	}
    } else
	widget_erase ((Widget *) view);
}

#define view_add_character(view,c) addch (c)
#define view_add_one_vline()       one_vline()
#define view_add_string(view,s)    addstr (s)
#define view_gotoyx(v,r,c)    widget_move (v,r,c)

#define view_freeze(view)
#define view_thaw(view)

#define STATUS_LINES 1

typedef enum {
    MARK_NORMAL = 0,
    MARK_SELECTED = 1,
    MARK_CURSOR = 2,
    MARK_CHANGED = 3
} mark_t;

/* Shows the file pointed to by *start_display on view_win */
static long
display (WView *view)
{
    const int frame_shift = view->have_frame;
    int col = 0 + frame_shift;
    int row = STATUS_LINES + frame_shift;
    int height, width;
    unsigned long from;
    int c;
    mark_t boldflag = MARK_NORMAL;
    struct hexedit_change_node *curr = view->change_list;

    height = view->widget.lines - frame_shift;
    width = view->widget.cols - frame_shift;
    from = view->start_display;
    attrset (NORMAL_COLOR);

    view_freeze (view);
    view_display_clean (view, height, width);

    /* Optionally, display a ruler */
    if ((!view->hex_mode) && (ruler)) {
	char r_buff[10];
	int cl;

	attrset (MARKED_COLOR);
	for (c = frame_shift; c < width; c++) {
	    cl = c - view->start_col;
	    if (ruler == 1)
		view_gotoyx (view, row, c);
	    else
		view_gotoyx (view, row + height - 2, c);
	    r_buff[0] = '-';
	    if ((cl % 10) == 0)
		r_buff[0] = '|';
	    else if ((cl % 5) == 0)
		r_buff[0] = '*';
	    view_add_character (view, r_buff[0]);
	    if ((cl != 0) && (cl % 10) == 0) {
		g_snprintf (r_buff, sizeof (r_buff), "%03d", cl);
		if (ruler == 1) {
		    widget_move (view, row + 1, c - 1);
		} else {
		    widget_move (view, row + height - 3, c - 1);
		}
		view_add_string (view, r_buff);
	    }
	}
	attrset (NORMAL_COLOR);
	if (ruler == 1)
	    row += 2;
	else
	    height -= 2;
    }

    /* Find the first displayable changed byte */
    while (curr && (curr->offset < from)) {
	curr = curr->next;
    }
    if (view->hex_mode) {
	char hex_buff[10];	/* A temporary buffer for sprintf and mvwaddstr */
	int bytes;		/* Number of bytes already printed on the line */

	/* Start of text column */
	int text_start = width - view->bytes_per_line - 1 + frame_shift;

	for (; row < height && from < view->last_byte; row++) {
	    /* Print the hex offset */
	    attrset (MARKED_COLOR);
	    g_snprintf (hex_buff, sizeof (hex_buff), "%08X",
			(int) (from - view->first));
	    view_gotoyx (view, row, frame_shift);
	    view_add_string (view, hex_buff);
	    attrset (NORMAL_COLOR);

	    /* Hex dump starts from column nine */
	    if (view->have_frame)
		col = 10;
	    else
		col = 9;

	    /* Each hex number is two digits */
	    hex_buff[2] = 0;
	    for (bytes = 0;
		 bytes < view->bytes_per_line && from < view->last_byte;
		 bytes++, from++) {
		/* Display and mark changed bytes */
		if (curr && from == curr->offset) {
		    c = curr->value;
		    curr = curr->next;
		    boldflag = MARK_CHANGED;
		    attrset (VIEW_UNDERLINED_COLOR);
		} else
		    c = (unsigned char) get_byte (view, from);

		if (view->found_len && from >= view->search_start
		    && from < view->search_start + view->found_len) {
		    boldflag = MARK_SELECTED;
		    attrset (MARKED_COLOR);
		}
		/* Display the navigation cursor */
		if (from == view->edit_cursor) {
		    if (view->view_side == view_side_left) {
			view->cursor_row = row;
			view->cursor_col = col;
		    }
		    boldflag = MARK_CURSOR;
		    attrset (view->view_side ==
			     view_side_left ? VIEW_UNDERLINED_COLOR :
			     MARKED_SELECTED_COLOR);
		}

		/* Print a hex number (sprintf is too slow) */
		hex_buff[0] = hex_char[(c >> 4)];
		hex_buff[1] = hex_char[c & 15];
		view_gotoyx (view, row, col);
		view_add_string (view, hex_buff);
		col += 3;
		/* Turn off the cursor or changed byte highlighting here */
		if (boldflag == MARK_CURSOR || boldflag == MARK_CHANGED)
		    attrset (NORMAL_COLOR);
		if ((bytes & 3) == 3 && bytes + 1 < view->bytes_per_line) {
		    /* Turn off the search highlighting */
		    if (boldflag == MARK_SELECTED
			&& from ==
			view->search_start + view->found_len - 1)
			attrset (NORMAL_COLOR);

		    /* Hex numbers are printed in the groups of four */
		    /* Groups are separated by a vline */

		    view_gotoyx (view, row, col - 1);
		    view_add_character (view, ' ');
		    view_gotoyx (view, row, col);
		    if ((view->have_frame && view->widget.cols < 82) ||
			view->widget.cols < 80)
			col += 1;
		    else {
			view_add_one_vline ();
			col += 2;
		    }

		    if (boldflag != MARK_NORMAL
			&& from ==
			view->search_start + view->found_len - 1)
			attrset (MARKED_COLOR);

		}
		if (boldflag != MARK_NORMAL
		    && from < view->search_start + view->found_len - 1
		    && bytes != view->bytes_per_line - 1) {
		    view_gotoyx (view, row, col);
		    view_add_character (view, ' ');
		}

		/* Print the corresponding ascii character */
		view_gotoyx (view, row, text_start + bytes);

		c = convert_to_display_c (c);

		if (!is_printable (c))
		    c = '.';
		switch (boldflag) {
		case MARK_NORMAL:
		    break;
		case MARK_SELECTED:
		    attrset (MARKED_COLOR);
		    break;
		case MARK_CURSOR:
		    if (view->view_side == view_side_right) {
			/* Our side is active */
			view->cursor_col = text_start + bytes;
			view->cursor_row = row;
			attrset (VIEW_UNDERLINED_COLOR);
		    } else {
			/* Other side is active */
			attrset (MARKED_SELECTED_COLOR);
		    }
		    break;
		case MARK_CHANGED:
		    attrset (VIEW_UNDERLINED_COLOR);
		    break;
		}
		view_add_character (view, c);

		if (boldflag != MARK_NORMAL) {
		    boldflag = MARK_NORMAL;
		    attrset (NORMAL_COLOR);
		}
	    }
	}
    } else {
	if (view->growing_buffer && from == view->last_byte)
	    get_byte (view, from);
	for (; row < height && from < view->last_byte; from++) {
	    c = get_byte (view, from);
	    if ((c == '\n') || (col >= width && view->wrap_mode)) {
		col = frame_shift;
		row++;
		if (c == '\n' || row >= height)
		    continue;
	    }
	    if (c == '\r')
		continue;
	    if (c == '\t') {
		col = ((col - frame_shift) / 8) * 8 + 8 + frame_shift;
		continue;
	    }
	    if (view->viewer_nroff_flag && c == '\b') {
		int c_prev;
		int c_next;

		if (from + 1 < view->last_byte
		    && is_printable ((c_next = get_byte (view, from + 1)))
		    && from > view->first
		    && is_printable ((c_prev = get_byte (view, from - 1)))
		    && (c_prev == c_next || c_prev == '_')) {
		    if (col <= frame_shift) {
			/* So it has to be wrap_mode - do not need to check for it */
			if (row == 1 + frame_shift) {
			    from++;
			    continue;	/* There had to be a bold character on the rightmost position
					   of the previous undisplayed line */
			}
			row--;
			col = width;
		    }
		    col--;
		    boldflag = MARK_SELECTED;
		    if (c_prev == '_' && c_next != '_')
			attrset (VIEW_UNDERLINED_COLOR);
		    else
			attrset (MARKED_COLOR);
		    continue;
		}
	    }
	    if (view->found_len && from >= view->search_start
		&& from < view->search_start + view->found_len) {
		boldflag = MARK_SELECTED;
		attrset (SELECTED_COLOR);
	    }
	    if (col >= frame_shift - view->start_col
		&& col < width - view->start_col) {
		view_gotoyx (view, row, col + view->start_col);

		c = convert_to_display_c (c);

		if (!is_printable (c))
		    c = '.';

		view_add_character (view, c);
	    }
	    col++;
	    if (boldflag != MARK_NORMAL) {
		boldflag = MARK_NORMAL;
		attrset (NORMAL_COLOR);
	    }

	    /* Very last thing */
	    if (view->growing_buffer && from + 1 == view->last_byte)
		get_byte (view, from + 1);
	}
    }
    view->last = from;
    view_thaw (view);
    return from;
}

static void
view_place_cursor (WView *view)
{
    int shift;

    if (view->view_side == view_side_left)
	shift = view->nib_shift;
    else
	shift = 0;

    widget_move (&view->widget, view->cursor_row,
		 view->cursor_col + shift);
}

static void
view_update (WView *view, gboolean update_gui)
{
    static int dirt_limit = 1;

    if (view->dirty > dirt_limit) {
	/* Too many updates skipped -> force a update */
	display (view);
	view_status (view, update_gui);
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
	    view_status (view, update_gui);
	    view->dirty = 0;
	    if (dirt_limit > 1)
		dirt_limit--;
	} else {
	    /* We are busy -> skipping full update,
	       only the status line is updated */
	    view_status (view, update_gui);
	}
	/* Here we had a refresh, if fast scrolling does not work
	   restore the refresh, although this should not happen */
    }
}

static inline void
my_define (Dlg_head *h, int idx, char *text, void (*fn) (WView *),
	   WView *view)
{
    define_label_data (h, idx, text, (buttonbarfn) fn, view);
}

/* If the last parameter is nonzero, it means we want get the count of lines
   from current up to the the upto position inclusive */
static long
move_forward2 (WView *view, long current, int lines, long upto)
{
    unsigned long q, p;
    int line;
    int col = 0;

    if (view->hex_mode) {
	p = current + lines * view->bytes_per_line;
	p = (p >= view->last_byte) ? current : p;
	if (lines == 1) {
	    q = view->edit_cursor + view->bytes_per_line;
	    line = q / view->bytes_per_line;
	    col = (view->last_byte - 1) / view->bytes_per_line;
	    view->edit_cursor = (line > col) ? view->edit_cursor : q;
	    view->edit_cursor = (view->edit_cursor < view->last_byte) ?
		view->edit_cursor : view->last_byte - 1;
	    q = current + ((LINES - 2) * view->bytes_per_line);
	    p = (view->edit_cursor < q) ? current : p;
	} else {
	    view->edit_cursor = (view->edit_cursor < p) ?
		p : view->edit_cursor;
	}
	return p;
    } else {
	if (upto) {
	    lines = -1;
	    q = upto;
	} else
	    q = view->last_byte;
	if (get_byte (view, q) != '\n')
	    q++;
	for (line = col = 0, p = current; p < q; p++) {
	    int c;

	    if (lines != -1 && line >= lines)
		return p;

	    c = get_byte (view, p);

	    if (view->wrap_mode) {
		if (c == '\r')
		    continue;	/* This characters is never displayed */
		else if (c == '\t')
		    col =
			((col - view->have_frame) / 8) * 8 + 8 +
			view->have_frame;
		else
		    col++;
		if (view->viewer_nroff_flag && c == '\b') {
		    if (p + 1 < view->last_byte
			&& is_printable (get_byte (view, p + 1))
			&& p > view->first
			&& is_printable (get_byte (view, p - 1)))
			col -= 2;
		} else if (col == vwidth) {
		    /* FIXME: the c in is_printable was a p, that is a bug,
		       I suspect I got that fix from Jakub, same applies
		       for d. */
		    int d = get_byte (view, p + 2);

		    if (p + 2 >= view->last_byte || !is_printable (c) ||
			!view->viewer_nroff_flag
			|| get_byte (view, p + 1) != '\b'
			|| !is_printable (d)) {
			col = 0;

			if (c == '\n' || get_byte (view, p + 1) != '\n')
			    line++;
		    }
		} else if (c == '\n') {
		    line++;
		    col = 0;
		}
	    } else if (c == '\n')
		line++;
	}
	if (upto)
	    return line;
    }
    return current;
}

/* returns the new current pointer */
/* Cause even the forward routine became very complex, we in the wrap_mode
   just find the nearest '\n', use move_forward2(p, 0, q) to get the count
   of lines up to there and then use move_forward2(p, something, 0), which we
   return */
static long
move_backward2 (WView *view, unsigned long current, int lines)
{
    long p, q, pm;
    int line;

    if (!view->hex_mode && current == view->first)
	return current;

    if (view->hex_mode) {
	p = current - lines * view->bytes_per_line;
	p = (p < view->first) ? view->first : p;
	if (lines == 1) {
	    q = view->edit_cursor - view->bytes_per_line;
	    view->edit_cursor = (q < view->first) ? view->edit_cursor : q;
	    p = (view->edit_cursor >= current) ? current : p;
	} else {
	    q = p + ((LINES - 2) * view->bytes_per_line);
	    view->edit_cursor = (view->edit_cursor >= q) ?
		p : view->edit_cursor;
	}
	return p;
    } else {
	if (current == view->last_byte
	    && get_byte (view, current - 1) != '\n')
	    /* There is one virtual '\n' at the end,
	       so that the last line is shown */
	    line = 1;
	else
	    line = 0;
	for (q = p = current - 1; p >= view->first; p--)
	    if (get_byte (view, p) == '\n' || p == view->first) {
		pm = p > view->first ? p + 1 : view->first;
		if (!view->wrap_mode) {
		    if (line == lines)
			return pm;
		    line++;
		} else {
		    line += move_forward2 (view, pm, 0, q);
		    if (line >= lines) {
			if (line == lines)
			    return pm;
			else
			    return move_forward2 (view, pm, line - lines,
						  0);
		    }
		    q = p + 1;
		}
	    }
    }
    return p > view->first ? p : view->first;
}

static void
view_move_backward (WView *view, int i)
{
    view->search_start = view->start_display =
	move_backward2 (view, view->start_display, i);
    view->found_len = 0;
    view->last = view->first + ((LINES - 2) * view->bytes_per_line);
    view->dirty++;
}

static long
get_bottom_first (WView *view, int do_not_cache, int really)
{
    int bottom_first;

    if (!have_fast_cpu && !really)
	return INT_MAX;

    if (!do_not_cache && view->bottom_first != -1)
	return view->bottom_first;

    /* Force loading */
    if (view->growing_buffer) {
	int old_last_byte;

	old_last_byte = -1;
	while (old_last_byte != view->last_byte) {
	    old_last_byte = view->last_byte;
	    get_byte (view, view->last_byte + VIEW_PAGE_SIZE);
	}
    }

    bottom_first = move_backward2 (view, view->last_byte, vheight - 1);

    if (view->hex_mode)
	bottom_first = (bottom_first + view->bytes_per_line - 1)
	    / view->bytes_per_line * view->bytes_per_line;
    view->bottom_first = bottom_first;

    return view->bottom_first;
}

static void
view_move_forward (WView *view, int i)
{
    view->start_display = move_forward2 (view, view->start_display, i, 0);
    if (!view->reading_pipe
	&& view->start_display > get_bottom_first (view, 0, 0))
	view->start_display = view->bottom_first;
    view->search_start = view->start_display;
    view->found_len = 0;
    view->last = view->first + ((LINES - 2) * view->bytes_per_line);
    view->dirty++;
}


static void
move_to_top (WView *view)
{
    view->search_start = view->start_display = view->first;
    view->found_len = 0;
    view->last = view->first + ((LINES - 2) * view->bytes_per_line);
    view->nib_shift = 0;
    view->edit_cursor = view->start_display;
    view->dirty++;
}

static void
move_to_bottom (WView *view)
{
    view->search_start = view->start_display =
	get_bottom_first (view, 0, 1);
    view->found_len = 0;
    view->last = view->first + ((LINES - 2) * view->bytes_per_line);
    view->edit_cursor = (view->edit_cursor < view->start_display) ?
	view->start_display : view->edit_cursor;
    view->dirty++;
}

/* Scroll left/right the view panel functions */
static void
move_right (WView *view)
{
    if (view->wrap_mode && !view->hex_mode)
	return;
    if (view->hex_mode) {
	view->last = view->first + ((LINES - 2) * view->bytes_per_line);

	if (view->hex_mode && view->view_side == view_side_left) {
	    view->nib_shift = 1 - view->nib_shift;
	    if (view->nib_shift == 1)
		return;
	}
	view->edit_cursor = (++view->edit_cursor < view->last_byte) ?
	    view->edit_cursor : view->last_byte - 1;
	if (view->edit_cursor >= view->last) {
	    view->edit_cursor -= view->bytes_per_line;
	    view_move_forward (view, 1);
	}
    } else if (--view->start_col > 0)
	view->start_col = 0;
    view->dirty++;
}

static void
move_left (WView *view)
{
    if (view->wrap_mode && !view->hex_mode)
	return;
    if (view->hex_mode) {
	if (view->hex_mode && view->view_side == view_side_left) {
	    view->nib_shift = 1 - view->nib_shift;
	    if (view->nib_shift == 0)
		return;
	}
	if (view->edit_cursor > view->first)
	    --view->edit_cursor;
	if (view->edit_cursor < view->start_display) {
	    view->edit_cursor += view->bytes_per_line;
	    view_move_backward (view, 1);
	}
    } else if (++view->start_col > 0)
	view->start_col = 0;
    view->dirty++;
}

/* Case insensitive search of text in data */
static int
icase_search_p (WView *view, char *text, char *data, int nothing)
{
    char *q;
    int lng;

    if ((q = _icase_search (text, data, &lng)) != 0) {
	view->found_len = lng;
	view->search_start = q - data - lng;
	return 1;
    }
    return 0;
}

static char *
grow_string_buffer (char *text, int *size)
{
    char *new;

    /* The grow steps */
    *size += 160;
    new = g_realloc (text, *size);
    if (!text) {
	*new = 0;
    }
    return new;
}

static char *
get_line_at (WView *view, unsigned long *p, unsigned long *skipped)
{
    char *buffer = 0;
    int buffer_size = 0;
    int usable_size = 0;
    int ch;
    int direction = view->direction;
    unsigned long pos = *p;
    long i = 0;
    int prev = 0;

    /* skip over all the possible zeros in the file */
    while ((ch = get_byte (view, pos)) == 0) {
	pos += direction;
	i++;
    }
    *skipped = i;

    if (pos) {
	prev = get_byte (view, pos - 1);
	if ((prev == -1) || (prev == '\n'))
	    prev = 0;
    }

    for (i = 0; ch != -1; ch = get_byte (view, pos)) {

	if (i == usable_size) {
	    buffer = grow_string_buffer (buffer, &buffer_size);
	    usable_size = buffer_size - 2;
	}

	pos += direction;
	i++;

	if (ch == '\n' || !ch) {
	    break;
	}
	buffer[i] = ch;
    }
    if (buffer) {
	buffer[0] = prev;
	buffer[i] = 0;

	/* If we are searching backwards, reverse the string */
	if (direction < 0) {
	    g_strreverse (buffer + 1);
	}
    }

    *p = pos;
    return buffer;
}

/** Search status optmizations **/

/* The number of bytes between percent increments */
static int update_steps;

/* Last point where we updated the status */
static long update_activate;

static void
search_update_steps (WView *view)
{
    if (view->s.st_size)
	update_steps = 40000;
    else
	update_steps = view->last_byte / 100;

    /* Do not update the percent display but every 20 ks */
    if (update_steps < 20000)
	update_steps = 20000;
}

static void
search (WView *view, char *text,
	int (*search) (WView *, char *, char *, int))
{
    int w = view->widget.cols - view->have_frame + 1;

    char *s = NULL;		/*  The line we read from the view buffer */
    long p, beginning;
    int found_len, search_start;
    int search_status;
    Dlg_head *d = 0;

    /* Used to keep track of where the line starts, when looking forward */
    /* is the index before transfering the line; the reverse case uses   */
    /* the position returned after the line has been read */
    long forward_line_start;
    long reverse_line_start;
    long t;
    /* Clear interrupt status */
    got_interrupt ();

    if (verbose) {
	d = message (D_INSERT, _("Search"), _("Searching %s"), text);
	mc_refresh ();
    }

    found_len = view->found_len;
    search_start = view->search_start;

    if (view->direction == 1) {
	p = found_len ? search_start + 1 : search_start;
    } else {
	p = (found_len ? search_start : view->last) - 1;
    }
    beginning = p;

    /* Compute the percent steps */
    search_update_steps (view);
    update_activate = 0;

    for (;; g_free (s)) {
	if (p >= update_activate) {
	    update_activate += update_steps;
	    if (verbose) {
		view_percent (view, p, w, TRUE);
		mc_refresh ();
	    }
	    if (got_interrupt ())
		break;
	}
	forward_line_start = p;
	disable_interrupt_key ();
	s = get_line_at (view, &p, &t);
	reverse_line_start = p;
	enable_interrupt_key ();

	if (!s)
	    break;

	search_status = (*search) (view, text, s + 1, match_normal);
	if (search_status < 0) {
	    g_free (s);
	    break;
	}

	if (search_status == 0)
	    continue;

	/* We found the string */

	if (*s && !view->search_start && (search == regexp_view_search)
	    && (*text == '^')) {

	    /* We do not want to match a
	     * ^ regexp when not at the real
	     * beginning of some line
	     */
	    continue;
	}
	/* Record the position used to continue the search */
	if (view->direction == 1)
	    t += forward_line_start;
	else
	    t += reverse_line_start ? reverse_line_start + 3 : 0;
	view->search_start += t;

	if (t != beginning) {
	    if (t > get_bottom_first (view, 0, 0))
		view->start_display = view->bottom_first;
	    else
		view->start_display = t;
	}

	g_free (s);
	break;
    }
    disable_interrupt_key ();
    if (verbose) {
	dlg_run_done (d);
	destroy_dlg (d);
    }
    if (!s) {
	message (0, _("Search"), _(" Search string not found "));
	view->found_len = 0;
    }
}

/* Search buffer (it's size is len) in the complete buffer */
/* returns the position where the block was found or -1 if not found */
static long
block_search (WView *view, char *buffer, int len)
{
    int w = view->widget.cols - view->have_frame + 1;

    char *d = buffer, b;
    unsigned long e;

    /* clear interrupt status */
    got_interrupt ();
    enable_interrupt_key ();
    e = view->found_len ? view->search_start + 1 : view->search_start;

    search_update_steps (view);
    update_activate = 0;

    while (e < view->last_byte) {
	if (e >= update_activate) {
	    update_activate += update_steps;
	    if (verbose) {
		view_percent (view, e, w, TRUE);
		mc_refresh ();
	    }
	    if (got_interrupt ())
		break;
	}
	b = get_byte (view, e++);

	if (*d == b) {
	    d++;
	    if (d - buffer == len) {
		disable_interrupt_key ();
		return e - len;
	    }
	} else {
	    e -= d - buffer;
	    d = buffer;
	}
    }
    disable_interrupt_key ();
    return -1;
}

/*
 * Search in the hex mode.  Supported input:
 * - numbers (oct, dec, hex).  Each of them matches one byte.
 * - strings in double quotes.  Matches exactly without quotes.
 */
static void
hex_search (WView *view, char *text)
{
    char *buffer;		/* Parsed search string */
    char *cur;			/* Current position in it */
    int block_len;		/* Length of the search string */
    long pos;			/* Position of the string in the file */
    int parse_error = 0;

    if (!*text) {
	view->found_len = 0;
	return;
    }

    /* buffer will never be longer that text */
    buffer = g_new (char, strlen (text));
    cur = buffer;

    /* First convert the string to a stream of bytes */
    while (*text) {
	int val;
	int ptr;

	/* Skip leading spaces */
	if (*text == ' ' || *text == '\t') {
	    text++;
	    continue;
	}

	/* %i matches octal, decimal, and hexadecimal numbers */
	if (sscanf (text, "%i%n", &val, &ptr) > 0) {
	    /* Allow signed and unsigned char in the user input */
	    if (val < -128 || val > 255) {
		parse_error = 1;
		break;
	    }

	    *cur++ = (char) val;
	    text += ptr;
	    continue;
	}

	/* Try quoted string, strip quotes */
	if (*text == '"') {
	    char *next_quote;

	    text++;
	    next_quote = strchr (text, '"');
	    if (next_quote) {
		memcpy (cur, text, next_quote - text);
		cur += next_quote - text;
		text = next_quote + 1;
		continue;
	    }
	    /* fall through */
	}

	parse_error = 1;
	break;
    }

    block_len = cur - buffer;

    /* No valid bytes in the user input */
    if (block_len <= 0 || parse_error) {
	message (0, _("Search"), _("Invalid hex search expression"));
	g_free (buffer);
	view->found_len = 0;
	return;
    }

    /* Then start the search */
    pos = block_search (view, buffer, block_len);

    g_free (buffer);

    if (pos == -1) {
	message (0, _("Search"), _(" Search string not found "));
	view->found_len = 0;
	return;
    }

    view->search_start = pos;
    view->found_len = block_len;
    /* Set the edit cursor to the search position, left nibble */
    view->edit_cursor = view->search_start;
    view->nib_shift = 0;

    /* Adjust the file offset */
    view->start_display = (pos & (~(view->bytes_per_line - 1)));
    if (view->start_display > get_bottom_first (view, 0, 0))
	view->start_display = view->bottom_first;
}

static int
regexp_view_search (WView *view, char *pattern, char *string,
		    int match_type)
{
    static regex_t r;
    static char *old_pattern = NULL;
    static int old_type;
    regmatch_t pmatch[1];
    int i, flags = REG_ICASE;

    if (!old_pattern || strcmp (old_pattern, pattern)
	|| old_type != match_type) {
	if (old_pattern) {
	    regfree (&r);
	    g_free (old_pattern);
	    old_pattern = 0;
	}
	for (i = 0; pattern[i] != 0; i++) {
	    if (isupper ((unsigned char) pattern[i])) {
		flags = 0;
		break;
	    }
	}
	flags |= REG_EXTENDED;
	if (regcomp (&r, pattern, flags)) {
	    message (1, MSG_ERROR, _(" Invalid regular expression "));
	    return -1;
	}
	old_pattern = g_strdup (pattern);
	old_type = match_type;
    }
    if (regexec (&r, string, 1, pmatch, 0) != 0)
	return 0;
    view->found_len = pmatch[0].rm_eo - pmatch[0].rm_so;
    view->search_start = pmatch[0].rm_so;
    return 1;
}

static void
do_regexp_search (void *xview, char *regexp)
{
    WView *view = (WView *) xview;

    view->search_exp = regexp;
    search (view, regexp, regexp_view_search);
    /* Had a refresh here */
    view->dirty++;
    view_update (view, TRUE);
}

static void
do_normal_search (void *xview, char *text)
{
    WView *view = (WView *) xview;

    view->search_exp = text;
    if (view->hex_mode)
	hex_search (view, text);
    else
	search (view, text, icase_search_p);
    /* Had a refresh here */
    view->dirty++;
    view_update (view, TRUE);
}

/* Real view only */
static void
view_help_cmd (void)
{
    interactive_display (NULL, "[Internal File Viewer]");
    /*
       view_refresh (0);
     */
}

/* Both views */
static void
toggle_wrap_mode (WView *view)
{
    if (view->hex_mode) {
	if (view->growing_buffer != 0) {
	    return;
	}
	get_bottom_first (view, 1, 1);
	if (view->hexedit_mode) {
	    view->view_side = 1 - view->view_side;
	} else {
	    view->hexedit_mode = 1 - view->hexedit_mode;
	}
	view_labels (view);
	view->dirty++;
	view_update (view, TRUE);
	return;
    }
    view->wrap_mode = 1 - view->wrap_mode;
    get_bottom_first (view, 1, 1);
    if (view->wrap_mode)
	view->start_col = 0;
    else {
	if (have_fast_cpu) {
	    if (view->bottom_first < view->start_display)
		view->search_start = view->start_display =
		    view->bottom_first;
	    view->found_len = 0;
	}
    }
    view_labels (view);
    view->dirty++;
    view_update (view, TRUE);
}

/* Both views */
static void
toggle_hex_mode (WView *view)
{
    view->hex_mode = 1 - view->hex_mode;

    if (view->hex_mode) {
	/* Shift the line start to 0x____0 on entry, restore it for Ascii */
	view->start_save = view->start_display;
	view->start_display -= view->start_display % view->bytes_per_line;
	view->edit_cursor = view->start_display;
	view->widget.options |= W_WANT_CURSOR;
	view->widget.parent->flags |= DLG_WANT_TAB;
    } else {
	view->start_display = view->start_save;
	view->widget.parent->flags &= ~DLG_WANT_TAB;
	view->widget.options &= ~W_WANT_CURSOR;
    }
    altered_hex_mode = 1;
    get_bottom_first (view, 1, 1);
    view_labels (view);
    view->dirty++;
    view_update (view, TRUE);
}

/* Ascii view */
static void
goto_line (WView *view)
{
    char *line, prompt[BUF_SMALL];
    int oldline = 1;
    int saved_wrap_mode = view->wrap_mode;
    unsigned long i;

    view->wrap_mode = 0;
    for (i = view->first; i < view->start_display; i++)
	if (get_byte (view, i) == '\n')
	    oldline++;
    g_snprintf (prompt, sizeof (prompt),
		_(" The current line number is %d.\n"
		  " Enter the new line number:"), oldline);
    line = input_dialog (_(" Goto line "), prompt, "");
    if (line) {
	if (*line) {
	    move_to_top (view);
	    view_move_forward (view, atol (line) - 1);
	}
	g_free (line);
    }
    view->dirty++;
    view->wrap_mode = saved_wrap_mode;
    view_update (view, TRUE);
}

/* Hex view */
static void
goto_addr (WView *view)
{
    char *line, *error, prompt[BUF_SMALL];
    unsigned long addr;

    g_snprintf (prompt, sizeof (prompt),
		_(" The current address is 0x%lx.\n"
		  " Enter the new address:"), view->edit_cursor);
    line = input_dialog (_(" Goto Address "), prompt, "");
    if (line) {
	if (*line) {
	    addr = strtol (line, &error, 0);
	    if ((*error == '\0') && (addr <= view->last_byte)) {
		move_to_top (view);
		view_move_forward (view, addr / view->bytes_per_line);
		view->edit_cursor = addr;
	    }
	}
	g_free (line);
    }
    view->dirty++;
    view_update (view, TRUE);
}

/* Both views */
static void
regexp_search (WView *view, int direction)
{
    char *regexp = "";
    static char *old = 0;

    /* This is really an F6 key handler */
    if (view->hex_mode) {
	/* Save it without a confirmation prompt */
	if (view->change_list)
	    save_edit_changes (view);
	return;
    }

    regexp = old ? old : regexp;
    regexp = input_dialog (_("Search"), _(" Enter regexp:"), regexp);
    if ((!regexp)) {
	return;
    }
    if ((!*regexp)) {
	g_free (regexp);
	return;
    }
    if (old)
	g_free (old);
    old = regexp;
#if 0
    /* Mhm, do we really need to load all the file in the core? */
    if (view->bytes_read < view->last_byte)
	get_byte (view, view->last_byte - 1);	/* Get the whole file in to memory */
#endif
    view->direction = direction;
    do_regexp_search (view, regexp);

    view->last_search = do_regexp_search;
}

static void
regexp_search_cmd (WView *view)
{
    regexp_search (view, 1);
}

/* Both views */
static void
normal_search (WView *view, int direction)
{
    static char *old;
    char *exp;

    convert_to_display (old);

    exp = input_dialog (_("Search"), _(" Enter search string:"), old ? old : "");

    if ((!exp) || (!*exp)) {
	if (exp)
	    g_free (exp);

	convert_from_input (old);
	return;
    }

    if (old)
	g_free (old);
    old = exp;

    convert_from_input (exp);

    view->direction = direction;
    do_normal_search (view, exp);
    view->last_search = do_normal_search;
}

static void
normal_search_cmd (WView *view)
{
    normal_search (view, 1);
}

static void
change_viewer (WView *view)
{
    char *s;
    char *t;


    if (*view->filename) {
	altered_magic_flag = 1;
	view->viewer_magic_flag = !view->viewer_magic_flag;
	s = g_strdup (view->filename);
	if (view->command)
	    t = g_strdup (view->command);
	else
	    t = 0;

	view_done (view);
	view_init (view, t, s, 0);
	g_free (s);
	if (t)
	    g_free (t);
	view_labels (view);
	view->dirty++;
	view_update (view, TRUE);
    }
}

static void
change_nroff (WView *view)
{
    view->viewer_nroff_flag = !view->viewer_nroff_flag;
    altered_nroff_flag = 1;
    view_labels (view);
    view->dirty++;
    view_update (view, TRUE);
}

/* Real view only */
static void
view_quit_cmd (WView *view)
{
    if (view_ok_to_quit (view))
	dlg_stop (view->widget.parent);
}

/* Both views */
static void
view_labels (WView *view)
{
    Dlg_head *h = view->widget.parent;

    define_label (h, 1, _("Help"), view_help_cmd);

    my_define (h, 10, _("Quit"), view_quit_cmd, view);
    my_define (h, 4, view->hex_mode ? _("Ascii") : _("Hex"),
	       toggle_hex_mode, view);
    my_define (h, 5, view->hex_mode ? _("Goto") : _("Line"),
	       view->hex_mode ? goto_addr : goto_line, view);
    my_define (h, 6, view->hex_mode ? _("Save") : _("RxSrch"),
	       regexp_search_cmd, view);

    my_define (h, 2, view->hex_mode ? view->hexedit_mode ?
	       view->view_side ==
	       view_side_left ? _("EdText") : _("EdHex") : view->
	       growing_buffer ? "" : _("Edit") : view->
	       wrap_mode ? _("UnWrap") : _("Wrap"), toggle_wrap_mode,
	       view);

    my_define (h, 7, view->hex_mode ? _("HxSrch") : _("Search"),
	       normal_search_cmd, view);

    my_define (h, 8, view->viewer_magic_flag ? _("Raw") : _("Parse"),
	       change_viewer, view);

    if (!view->have_frame) {
	my_define (h, 9,
		   view->viewer_nroff_flag ? _("Unform") : _("Format"),
		   change_nroff, view);
	my_define (h, 3, _("Quit"), view_quit_cmd, view);
    }

    redraw_labels (h);
}

/* Both views */
static int
check_left_right_keys (WView *view, int c)
{
    if (c == KEY_LEFT)
	move_left (view);
    else if (c == KEY_RIGHT)
	move_right (view);
    else
	return 0;

    return 1;
}

static void
set_monitor (WView *view, int set_on)
{
    int old = view->monitor;

    view->monitor = set_on;

    if (view->monitor) {
	move_to_bottom (view);
	view->bottom_first = -1;
	set_idle_proc (view->widget.parent, 1);
    } else {
	if (old)
	    set_idle_proc (view->widget.parent, 0);
    }
}

static void
continue_search (WView *view)
{
    if (view->last_search) {
	(*view->last_search) (view, view->search_exp);
    } else {
	/* if not... then ask for an expression */
	normal_search (view, 1);
    }
}

/* Both views */
static int
view_handle_key (WView *view, int c)
{
    int prev_monitor = view->monitor;

    set_monitor (view, off);

    c = convert_from_input_c (c);

    if (view->hex_mode) {
	switch (c) {
	case 0x09:		/* Tab key */
	    view->view_side = 1 - view->view_side;
	    view->dirty++;
	    return 1;

	case XCTRL ('a'):	/* Beginning of line */
	    view->edit_cursor -= view->edit_cursor % view->bytes_per_line;
	    view->dirty++;
	    return 1;

	case XCTRL ('b'):	/* Character back */
	    move_left (view);
	    return 1;

	case XCTRL ('e'):	/* End of line */
	    view->edit_cursor -= view->edit_cursor % view->bytes_per_line;
	    view->edit_cursor += view->bytes_per_line - 1;
	    view->dirty++;
	    return 1;

	case XCTRL ('f'):	/* Character forward */
	    move_right (view);
	    return 1;
	}

	/* Trap 0-9,A-F,a-f for left side data entry (hex editing) */
	if (view->view_side == view_side_left) {
	    if ((c >= '0' && c <= '9') ||
		(c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {

		put_editkey (view, c);
		return 1;
	    }
	}

	/* Trap all printable characters for right side data entry */
	/* Also enter the value of the Enter key */
	if (view->view_side == view_side_right) {
	    if (c < 256 && (is_printable (c) || (c == '\n'))) {
		put_editkey (view, c);
		return 1;
	    }
	}
    }

    if (check_left_right_keys (view, c))
	return 1;

    if (check_movement_keys
	(c, 1, vheight, view, (movefn) view_move_backward,
	 (movefn) view_move_forward, (movefn) move_to_top,
	 (movefn) move_to_bottom)) {
	return 1;
    }
    switch (c) {

    case '?':
	regexp_search (view, -1);
	return 1;

    case '/':
	regexp_search (view, 1);
	return 1;

	/* Continue search */
    case XCTRL ('s'):
    case 'n':
    case KEY_F (17):
	continue_search (view);
	return 1;

    case XCTRL ('r'):
	if (view->last_search) {
	    (*view->last_search) (view, view->search_exp);
	} else {
	    normal_search (view, -1);
	}
	return 1;

	/* toggle ruler */
    case ALT ('r'):
	switch (ruler) {
	case 0:
	    ruler = 1;
	    break;
	case 1:
	    ruler = 2;
	    break;
	default:
	    ruler = 0;
	    break;
	}
	view->dirty++;
	return 1;

    case 'h':
	move_left (view);
	return 1;

    case 'j':
    case '\n':
    case 'e':
	view_move_forward (view, 1);
	return 1;

    case 'd':
	view_move_forward (view, vheight / 2);
	return 1;

    case 'u':
	view_move_backward (view, vheight / 2);
	return 1;

    case 'k':
    case 'y':
	view_move_backward (view, 1);
	return 1;

    case 'l':
	move_right (view);
	return 1;

    case ' ':
    case 'f':
	view_move_forward (view, vheight - 1);
	return 1;

    case XCTRL ('o'):
	view_other_cmd ();
	return 1;

	/* Unlike Ctrl-O, run a new shell if the subshell is not running.  */
    case '!':
	exec_shell ();
	return 1;

    case 'F':
	set_monitor (view, on);
	return 1;

    case 'b':
	view_move_backward (view, vheight - 1);
	return 1;

    case KEY_IC:
	view_move_backward (view, 2);
	return 1;

    case KEY_DC:
	view_move_forward (view, 2);
	return 1;

    case 'm':
	view->marks[view->marker] = view->start_display;
	return 1;

    case 'r':
	view->start_display = view->marks[view->marker];
	view->dirty++;
	return 1;

	/*  Use to indicate parent that we want to see the next/previous file */
	/* Only works on full screen mode */
    case XCTRL ('f'):
    case XCTRL ('b'):
	if (!view->have_frame)
	    view->move_dir = c == XCTRL ('f') ? 1 : -1;
	/* fall */

    case 'q':
    case XCTRL ('g'):
    case ESC_CHAR:
	if (view_ok_to_quit (view))
	    view->view_quit = 1;
	return 1;

#ifdef HAVE_CHARSET
    case XCTRL ('t'):
	do_select_codepage ();
	view->dirty++;
	view_update (view, TRUE);
	return 1;
#endif				/* HAVE_CHARSET */

    }
    if (c >= '0' && c <= '9')
	view->marker = c - '0';

    /* Restore the monitor status */
    set_monitor (view, prev_monitor);

    /* Key not used */
    return 0;
}

/* Both views */
static int
view_event (WView *view, Gpm_Event *event, int *result)
{
    *result = MOU_NORMAL;

    /* We are not interested in the release events */
    if (!(event->type & (GPM_DOWN | GPM_DRAG)))
	return 0;

    /* Wheel events */
    if ((event->buttons & GPM_B_UP) && (event->type & GPM_DOWN)) {
	view_move_backward (view, 2);
	return 1;
    }
    if ((event->buttons & GPM_B_DOWN) && (event->type & GPM_DOWN)) {
	view_move_forward (view, 2);
	return 1;
    }

    /* Scrolling left and right */
    if (!view->wrap_mode) {
	if (event->x < view->widget.cols / 4) {
	    move_left (view);
	    goto processed;
	}
	if (event->x > 3 * vwidth / 4) {
	    move_right (view);
	    goto processed;
	}
    }

    /* Scrolling up and down */
    if (event->y < view->widget.lines / 3) {
	if (mouse_move_pages_viewer)
	    view_move_backward (view, view->widget.lines / 2 - 1);
	else
	    view_move_backward (view, 1);
	goto processed;
    } else if (event->y > 2 * vheight / 3) {
	if (mouse_move_pages_viewer)
	    view_move_forward (view, vheight / 2 - 1);
	else
	    view_move_forward (view, 1);
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
    int result;

    if (view_event ((WView *) x, event, &result))
	view_update ((WView *) x, TRUE);
    return result;
}

static void
view_adjust_size (Dlg_head *h)
{
    WView *view;
    WButtonBar *bar;

    /* Look up the viewer and the buttonbar, we assume only two widgets here */
    view = (WView *) find_widget_type (h, (callback_fn) view_callback);
    bar = find_buttonbar (h);
    widget_set_size (&view->widget, 0, 0, LINES - 1, COLS);
    widget_set_size (&bar->widget, LINES - 1, 0, 1, COLS);

    view_update_bytes_per_line (view);
}

/* Callback for the view dialog */
static int
view_dialog_callback (Dlg_head *h, int id, int msg)
{
    switch (msg) {
    case DLG_RESIZE:
	view_adjust_size (h);
	return MSG_HANDLED;
    }
    return default_dlg_callback (h, id, msg);
}

/* Real view only */
int
view (char *_command, const char *_file, int *move_dir_p, int start_line)
{
    int error;
    WView *wview;
    WButtonBar *bar;
    Dlg_head *view_dlg;

    /* Create dialog and widgets, put them on the dialog */
    view_dlg =
	create_dlg (0, 0, LINES, COLS, NULL, view_dialog_callback,
		    "[Internal File Viewer]", NULL, DLG_NONE);

    wview = view_new (0, 0, COLS, LINES - 1, 0);

    bar = buttonbar_new (1);

    add_widget (view_dlg, wview);
    add_widget (view_dlg, bar);

    error = view_init (wview, _command, _file, start_line);
    if (move_dir_p)
	*move_dir_p = 0;

    /* Please note that if you add another widget,
     * you have to modify view_adjust_size to
     * be aware of it
     */
    if (!error) {
	run_dlg (view_dlg);
	if (move_dir_p)
	    *move_dir_p = wview->move_dir;
    }
    destroy_dlg (view_dlg);

    return !error;
}

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
	panel = cpanel;
    else if (get_other_type () == view_listing)
	panel = other_panel;
    else
	return;

    view_init (view, 0, panel->dir.list[panel->selected].fname, 0);
    display (view);
    view_status (view, TRUE);
}

static int
view_callback (WView *view, int msg, int par)
{
    int i;
    Dlg_head *h = view->widget.parent;

    switch (msg) {
    case WIDGET_INIT:
	view_update_bytes_per_line (view);
	if (view->have_frame)
	    add_hook (&select_file_hook, view_hook, view);
	else
	    view_labels (view);
	break;

    case WIDGET_DRAW:
	display (view);
	view_status (view, TRUE);
	break;

    case WIDGET_CURSOR:
	if (view->hex_mode)
	    view_place_cursor (view);
	break;

    case WIDGET_KEY:
	i = view_handle_key ((WView *) view, par);
	if (view->view_quit)
	    dlg_stop (h);
	else {
	    view_update (view, TRUE);
	}
	return i;

    case WIDGET_IDLE:
	/* This event is generated when the user is using the 'F' flag */
	view->bottom_first = -1;
	move_to_bottom (view);
	display (view);
	view_status (view, TRUE);
	sleep (1);
	return 1;

    case WIDGET_FOCUS:
	view_labels (view);
	return 1;

    }
    return default_proc (msg, par);
}

WView *
view_new (int y, int x, int cols, int lines, int is_panel)
{
    WView *view = g_new0 (WView, 1);

    init_widget (&view->widget, y, x, lines, cols,
		 (callback_fn) view_callback,
		 (destroy_fn) view_destroy,
		 (mouse_h) real_view_event, NULL);

    view->hex_mode = default_hex_mode;
    view->hexedit_mode = default_hexedit_mode;
    view->viewer_magic_flag = default_magic_flag;
    view->viewer_nroff_flag = default_nroff_flag;
    view->have_frame = is_panel;
    view->last_byte = -1;
    view->wrap_mode = global_wrap_mode;

    widget_want_cursor (view->widget, 0);

    return view;
}
