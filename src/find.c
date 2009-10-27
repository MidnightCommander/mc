/* Find file command for the Midnight Commander
   Copyright (C) 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2006, 2007 Free Software Foundation, Inc.
   Written 1995 by Miguel de Icaza

   Complete rewrote.
   
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

/** \file find.c
 *  \brief Source: Find file command
 */

#include <config.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "global.h"

#include "../src/tty/tty.h"
#include "../src/skin/skin.h"
#include "../src/tty/key.h"

#include "../src/search/search.h"

#include "setup.h"
#include "find.h"
#include "strutil.h"
#include "dialog.h"
#include "widget.h"
#include "dir.h"
#include "panel.h"		/* current_panel */
#include "main.h"		/* do_cd, try_to_select */
#include "wtools.h"
#include "cmd.h"		/* view_file_at_line */
#include "boxes.h"
#include "history.h"		/* MC_HISTORY_SHARED_SEARCH */
#include "layout.h"		/* mc_refresh() */

/* Size of the find parameters window */
#if HAVE_CHARSET
static int FIND_Y = 16;
#else
static int FIND_Y = 15;
#endif
static int FIND_X = 68;

/* Size of the find window */
#define FIND2_Y (LINES - 4)

static int FIND2_X = 64;
#define FIND2_X_USE (FIND2_X - 20)

/* A couple of extra messages we need */
enum {
    B_STOP = B_USER + 1,
    B_AGAIN,
    B_PANELIZE,
    B_TREE,
    B_VIEW
};

typedef enum {
    FIND_CONT = 0,
    FIND_SUSPEND,
    FIND_ABORT
} FindProgressStatus;

/* List of directories to be ignored, separated by ':' */
char *find_ignore_dirs = NULL;

/* static variables to remember find parameters */
static WInput *in_start;			/* Start path */
static WInput *in_name;				/* Filename */
static WInput *in_with;				/* Text inside filename */
static WCheck *file_case_sens_cbox;		/* "case sensitive" checkbox */
static WCheck *file_pattern_cbox;		/* File name is glob or regexp */
static WCheck *recursively_cbox;
static WCheck *skip_hidden_cbox;
static WCheck *content_case_sens_cbox;		/* "case sensitive" checkbox */
static WCheck *content_regexp_cbox;		/* "find regular expression" checkbox */
static WCheck *content_first_hit_cbox;		/* "First hit" checkbox" */
static WCheck *content_whole_words_cbox;	/* "whole words" checkbox */
#ifdef HAVE_CHARSET
static WCheck *file_all_charsets_cbox;
static WCheck *content_all_charsets_cbox;
#endif

static gboolean running = FALSE;	/* nice flag */
static char *find_pattern = NULL;	/* Pattern to search */
static char *content_pattern = NULL;	/* pattern to search inside files; if
					   content_regexp_flag is true, it contains the
					   regex pattern, else the search string. */
static unsigned long matches;		/* Number of matches */
static gboolean is_start = FALSE;	/* Status of the start/stop toggle button */
static char *old_dir = NULL;

/* Where did we stop */
static int resuming;
static int last_line;
static int last_pos;

static Dlg_head *find_dlg;	/* The dialog */
static WButton *stop_button;	/* pointer to the stop button */
static WLabel *status_label;	/* Finished, Searching etc. */
static WLabel *found_num_label;	/* Number of found items */
static WListbox *find_list;	/* Listbox with the file list */

/* This keeps track of the directory stack */
#if GLIB_CHECK_VERSION (2, 14, 0)
static GQueue dir_queue = G_QUEUE_INIT;
#else
typedef struct dir_stack {
    char *name;
    struct dir_stack *prev;
} dir_stack;

static dir_stack *dir_stack_base = 0;
#endif				 /* GLIB_CHECK_VERSION */

static struct {
	const char* text;
	int len;	/* length including space and brackets */
	int x;
} fbuts [] = {
	{ N_("&Suspend"),   11, 29 },
	{ N_("Con&tinue"),  12, 29 },
	{ N_("&Chdir"),     11, 3  },
	{ N_("&Again"),     9,  17 },
	{ N_("&Quit"),      8,  43 },
	{ N_("Pane&lize"),  12, 3  },
	{ N_("&View - F3"), 13, 20 },
	{ N_("&Edit - F4"), 13, 38 }
};

static const char *in_contents = NULL;
static char *in_start_dir = INPUT_LAST_TEXT;

static mc_search_t *search_file_handle = NULL;
static gboolean skip_hidden_flag = FALSE;
static gboolean file_pattern_flag = TRUE;
static gboolean file_all_charsets_flag = FALSE;
static gboolean file_case_sens_flag = TRUE;
static gboolean find_recurs_flag = TRUE;

static mc_search_t *search_content_handle = NULL;
static gboolean content_regexp_flag = FALSE;
static gboolean content_all_charsets_flag = FALSE;
static gboolean content_case_sens_flag = TRUE;
static gboolean content_first_hit_flag = FALSE;
static gboolean content_whole_words = FALSE;

static inline char *
add_to_list (const char *text, void *data)
{
    return listbox_add_item (find_list, LISTBOX_APPEND_AT_END, 0, text, data);
}

static inline void
stop_idle (void *data)
{
    set_idle_proc (data, 0);
}

static inline void
status_update (const char *text)
{
    label_set_text (status_label, text);
}

static void
found_num_update (void)
{
    char buffer [BUF_TINY];
    g_snprintf (buffer, sizeof (buffer), _("Found: %ld"), matches);
    label_set_text (found_num_label, buffer);
}

static void
get_list_info (char **file, char **dir)
{
    listbox_get_current (find_list, file, dir);
}

/* check regular expression */
gboolean
find_check_regexp (const char *r)
{
    mc_search_t *search;
    gboolean regexp_ok = FALSE;

    search = mc_search_new (r, -1);

    if (search != NULL) {
	search->search_type = MC_SEARCH_T_REGEX;
	regexp_ok = mc_search_prepare (search);
	mc_search_free (search);
    }

    return regexp_ok;
}

/*
 * Callback for the parameter dialog.
 * Validate regex, prevent closing the dialog if it's invalid.
 */
static cb_ret_t
find_parm_callback (struct Dlg_head *h, dlg_msg_t msg, int parm)
{
    switch (msg) {
    case DLG_VALIDATE:
	if (h->ret_value != B_ENTER)
	    return MSG_HANDLED;

	/* check filename regexp */
	if (!(file_pattern_cbox->state & C_BOOL)
	    && (in_name->buffer[0] != '\0')
	    && !find_check_regexp (in_name->buffer)) {
		message (D_ERROR, MSG_ERROR, _(" Malformed regular expression "));
		dlg_select_widget (in_name);
		h->running = 1;		/* Don't stop the dialog */
		return MSG_HANDLED;
	}

	/* check content regexp */
	if ((content_regexp_cbox->state & C_BOOL)
	    && (in_with->buffer[0] != '\0')
	    && !find_check_regexp (in_with->buffer)) {
		message (D_ERROR, MSG_ERROR, _(" Malformed regular expression "));
		dlg_select_widget (in_with);
		h->running = 1;		/* Don't stop the dialog */
		return MSG_HANDLED;
	}

	return MSG_HANDLED;

    default:
	return default_dlg_callback (h, msg, parm);
    }
}

/*
 * find_parameters: gets information from the user
 *
 * If the return value is true, then the following holds:
 *
 * START_DIR and PATTERN are pointers to char * and upon return they
 * contain the information provided by the user.
 *
 * CONTENT holds a strdup of the contents specified by the user if he
 * asked for them or 0 if not (note, this is different from the
 * behavior for the other two parameters.
 *
 */
static int
find_parameters (char **start_dir, char **pattern, char **content)
{
    int return_value;
    char *temp_dir = NULL;

    /* file name */
    const char *file_case_label = N_("Cas&e sensitive");
    const char *file_pattern_label = N_("&Using shell patterns");
    const char *file_recurs_label = N_("&Find recursively");
    const char *file_skip_hidden_label = N_("S&kip hidden");
    const char *file_all_charsets_label = N_("&All charsets");

    /* file content */
    const char *content_case_label = N_("Case sens&itive");
    const char *content_regexp_label = N_("Re&gular expression");
    const char *content_first_hit_label = N_("Fir&st hit");
    const char *content_whole_words_label = N_("&Whole words");
    const char *content_all_charsets_label = N_("All cha&rsets");

    const char *buts[] = { N_("&OK"), N_("&Cancel"), N_("&Tree") };

    int b0, b1, b2;

#ifdef ENABLE_NLS
    {
	int i = sizeof (buts) / sizeof (buts[0]);
	while (i-- != 0)
	    buts[i] = _(buts[i]);

	file_case_label = _(file_case_label);
	file_pattern_label = _(file_pattern_label);
	file_recurs_label = _(file_recurs_label);
	file_skip_hidden_label = _(file_skip_hidden_label);
	file_all_charsets_label = _(file_all_charsets_label);
	content_case_label = _(content_case_label);
	content_regexp_label = _(content_regexp_label);
	content_first_hit_label = _(content_first_hit_label);
	content_whole_words_label = _(content_whole_words_label);
	content_all_charsets_label = _(content_all_charsets_label);
    }
#endif				/* ENABLE_NLS */

    b0 = str_term_width1 (buts[0]) + 6; /* default button */
    b1 = str_term_width1 (buts[1]) + 4;
    b2 = str_term_width1 (buts[2]) + 4;

  find_par_start:
    if (in_contents == NULL)
	in_contents = INPUT_LAST_TEXT;

    if (in_start_dir == NULL)
	in_start_dir = g_strdup (".");

    find_dlg =
	create_dlg (0, 0, FIND_Y, FIND_X, dialog_colors,
		    find_parm_callback, "[Find File]", _("Find File"),
		    DLG_CENTER | DLG_REVERSE);

    add_widget (find_dlg,
		button_new (FIND_Y - 3, FIND_X * 3/4 - b1/2, B_CANCEL, NORMAL_BUTTON, buts[1], 0));
    add_widget (find_dlg,
		button_new (FIND_Y - 3, FIND_X/4 - b0/2, B_ENTER, DEFPUSH_BUTTON, buts[0], 0));

#ifdef HAVE_CHARSET
    content_all_charsets_cbox = check_new (11, FIND_X / 2 + 1,
		content_all_charsets_flag, content_all_charsets_label);
    add_widget (find_dlg, content_all_charsets_cbox);
#endif

    content_whole_words_cbox = check_new (10, FIND_X / 2 + 1, content_whole_words, content_whole_words_label);
    add_widget (find_dlg, content_whole_words_cbox);

    content_first_hit_cbox = check_new (9, FIND_X / 2 + 1, content_first_hit_flag, content_first_hit_label);
    add_widget (find_dlg, content_first_hit_cbox);

    content_regexp_cbox = check_new (8, FIND_X / 2 + 1, content_regexp_flag, content_regexp_label);
    add_widget (find_dlg, content_regexp_cbox);

    content_case_sens_cbox = check_new (7, FIND_X / 2 + 1, content_case_sens_flag, content_case_label);
    add_widget (find_dlg, content_case_sens_cbox);

#ifdef HAVE_CHARSET
    file_all_charsets_cbox = check_new (11, 3,
		file_all_charsets_flag, file_all_charsets_label);
    add_widget (find_dlg, file_all_charsets_cbox);
#endif

    skip_hidden_cbox = check_new (10, 3, skip_hidden_flag, file_skip_hidden_label);
    add_widget (find_dlg, skip_hidden_cbox);

    recursively_cbox = check_new (9, 3, find_recurs_flag, file_recurs_label);
    add_widget (find_dlg, recursively_cbox);

    file_pattern_cbox = check_new (8, 3, file_pattern_flag, file_pattern_label);
    add_widget (find_dlg, file_pattern_cbox);

    file_case_sens_cbox = check_new (7, 3, file_case_sens_flag, file_case_label);
    add_widget (find_dlg, file_case_sens_cbox);

    in_with = input_new (6, FIND_X / 2 + 1, INPUT_COLOR, FIND_X / 2 - 4, in_contents,
                         MC_HISTORY_SHARED_SEARCH, INPUT_COMPLETE_DEFAULT);
    add_widget (find_dlg, in_with);
    add_widget (find_dlg, label_new (5, FIND_X / 2 + 1, _("Content:")));

    in_name = input_new (6, 3, INPUT_COLOR, FIND_X / 2 - 4, INPUT_LAST_TEXT, "name",
                         INPUT_COMPLETE_DEFAULT);
    add_widget (find_dlg, in_name);
    add_widget (find_dlg, label_new (5, 3, _("File name:")));

    add_widget (find_dlg,
		button_new (3, FIND_X - b2 - 2, B_TREE, NORMAL_BUTTON, buts[2], 0));

    in_start = input_new (3, 3, INPUT_COLOR, FIND_X - b2 - 6, in_start_dir, "start",
                          INPUT_COMPLETE_DEFAULT);
    add_widget (find_dlg, in_start);
    add_widget (find_dlg, label_new (2, 3, _("Start at:")));

    dlg_select_widget (in_name);

    switch (run_dlg (find_dlg)) {
    case B_CANCEL:
	return_value = 0;
	break;

    case B_TREE:
#ifdef HAVE_CHARSET
	file_all_charsets_flag = file_all_charsets_cbox->state & C_BOOL;
	content_all_charsets_flag = content_all_charsets_cbox->state & C_BOOL;
#endif
	content_case_sens_flag = content_case_sens_cbox->state & C_BOOL;
	content_regexp_flag = content_regexp_cbox->state & C_BOOL;
	content_first_hit_flag = content_first_hit_cbox->state & C_BOOL;
	content_whole_words = content_whole_words_cbox->state & C_BOOL;
	file_pattern_flag = file_pattern_cbox->state & C_BOOL;
	file_case_sens_flag = file_case_sens_cbox->state & C_BOOL;
	find_recurs_flag = recursively_cbox->state & C_BOOL;
	skip_hidden_flag = skip_hidden_cbox->state & C_BOOL;
	destroy_dlg (find_dlg);
	if (in_start_dir != INPUT_LAST_TEXT)
	    g_free (in_start_dir);
	temp_dir = g_strdup (in_start->buffer);
	if ((temp_dir[0] == '.') && (temp_dir[1] == '\0')) {
	    g_free (temp_dir);
	    temp_dir = g_strdup (current_panel->cwd);
	}
	in_start_dir = tree_box (temp_dir);
	if (in_start_dir != NULL)
	    g_free (temp_dir);
	else
	    in_start_dir = temp_dir;
	/* Warning: Dreadful goto */
	goto find_par_start;
	break;

    default:
#ifdef HAVE_CHARSET
	file_all_charsets_flag = file_all_charsets_cbox->state & C_BOOL;
	content_all_charsets_flag = content_all_charsets_cbox->state & C_BOOL;
#endif
	content_case_sens_flag = content_case_sens_cbox->state & C_BOOL;
	content_regexp_flag = content_regexp_cbox->state & C_BOOL;
	content_first_hit_flag = content_first_hit_cbox->state & C_BOOL;
	content_whole_words = content_whole_words_cbox->state & C_BOOL;
	find_recurs_flag = recursively_cbox->state & C_BOOL;
	file_pattern_flag = file_pattern_cbox->state & C_BOOL;
	file_case_sens_flag = file_case_sens_cbox->state & C_BOOL;
	skip_hidden_flag = skip_hidden_cbox->state & C_BOOL;

	/* keep empty Content field */
	/* if not empty, fill from history */
	*content = NULL;
	in_contents =  "";
	if (in_with->buffer[0] != '\0') {
	    *content = g_strdup (in_with->buffer);
	    in_contents = INPUT_LAST_TEXT;
	}

	*start_dir = g_strdup ((in_start->buffer[0] != '\0') ? in_start->buffer : ".");
	*pattern = g_strdup (in_name->buffer);
	if (in_start_dir != INPUT_LAST_TEXT)
	    g_free (in_start_dir);
	in_start_dir = g_strdup (*start_dir);
	return_value = 1;
    }

    destroy_dlg (find_dlg);

    return return_value;
}

#if GLIB_CHECK_VERSION (2, 14, 0)

static inline void
push_directory (const char *dir)
{
    g_queue_push_head (&dir_queue, (void *) dir);
}

static inline char *
pop_directory (void)
{
    return (char *) g_queue_pop_tail (&dir_queue);
}

/* Remove all the items in the stack */
static void
clear_stack (void)
{
    g_queue_foreach (&dir_queue, (GFunc) g_free, NULL);
    g_queue_clear (&dir_queue);
}

#else				/* GLIB_CHAECK_VERSION */

static void
push_directory (const char *dir)
{
    dir_stack *new;

    new = g_new (dir_stack, 1);
    new->name = str_unconst (dir);
    new->prev = dir_stack_base;
    dir_stack_base = new;
}

static char *
pop_directory (void)
{
    char *name = NULL;

    if (dir_stack_base != NULL) {
	dir_stack *next;
	name = dir_stack_base->name;
	next = dir_stack_base->prev;
	g_free (dir_stack_base);
	dir_stack_base = next;
    }

    return name;
}

/* Remove all the items in the stack */
static void
clear_stack (void)
{
    char *dir = NULL;
    while ((dir = pop_directory ()) != NULL)
	g_free (dir);
}

#endif				/* GLIB_CHAECK_VERSION */

static void
insert_file (const char *dir, const char *file)
{
    char *tmp_name = NULL;
    static char *dirname = NULL;

    while (dir [0] == PATH_SEP && dir [1] == PATH_SEP)
	dir++;

    if (old_dir){
	if (strcmp (old_dir, dir)){
	    g_free (old_dir);
	    old_dir = g_strdup (dir);
	    dirname = add_to_list (dir, NULL);
	}
    } else {
	old_dir = g_strdup (dir);
	dirname = add_to_list (dir, NULL);
    }

    tmp_name = g_strdup_printf ("    %s", file);
    add_to_list (tmp_name, dirname);
    g_free (tmp_name);
}

static void
find_add_match (const char *dir, const char *file)
{
    insert_file (dir, file);

    /* Don't scroll */
    if (matches == 0)
	listbox_select_by_number (find_list, 0);
    send_message (&find_list->widget, WIDGET_DRAW, 0);

    matches++;
    found_num_update ();
}

/*
 * get_line_at:
 *
 * Returns malloced null-terminated line from file file_fd.
 * Input is buffered in buf_size long buffer.
 * Current pos in buf is stored in pos.
 * n_read - number of read chars.
 * has_newline - is there newline ?
 */
static char *
get_line_at (int file_fd, char *buf, int buf_size, int *pos, int *n_read,
	     gboolean *has_newline)
{
    char *buffer = NULL;
    int buffer_size = 0;
    char ch = 0;
    int i = 0;

    for (;;) {
	if (*pos >= *n_read) {
	    *pos = 0;
	    *n_read = mc_read (file_fd, buf, buf_size);
	    if (*n_read <= 0)
		break;
	}

	ch = buf[(*pos)++];
	if (ch == '\0') {
	    /* skip possible leading zero(s) */
	    if (i == 0)
		continue;
	    else
		break;
	}

	if (i >= buffer_size - 1) {
	    buffer = g_realloc (buffer, buffer_size += 80);
	}
	/* Strip newline */
	if (ch == '\n')
	    break;

	buffer[i++] = ch;
    }

    *has_newline = (ch != '\0');

    if (buffer != NULL)
	buffer[i] = '\0';

    return buffer;
}

static FindProgressStatus
check_find_events(Dlg_head *h)
{
    Gpm_Event event;
    int c;

    event.x = -1;
    c = tty_get_event (&event, h->mouse_status == MOU_REPEAT, FALSE);
    if (c != EV_NONE) {
 	dlg_process_event (h, c, &event);
 	if (h->ret_value == B_ENTER
 	    || h->ret_value == B_CANCEL
 	    || h->ret_value == B_AGAIN
 	    || h->ret_value == B_PANELIZE) {
 	    /* dialog terminated */
 	    return FIND_ABORT;
 	}
 	if (!(h->flags & DLG_WANT_IDLE)) {
 	    /* searching suspended */
 	    return FIND_SUSPEND;
 	}
    }

    return FIND_CONT;
}

/*
 * search_content:
 *
 * Search the content_pattern string in the DIRECTORY/FILE.
 * It will add the found entries to the find listbox.
 *
 * returns FALSE if do_search should look for another file
 *         TRUE if do_search should exit and proceed to the event handler
 */
static gboolean
search_content (Dlg_head *h, const char *directory, const char *filename)
{
    struct stat s;
    char buffer [BUF_4K];
    char *fname = NULL;
    int file_fd;
    gboolean ret_val = FALSE;

    fname = concat_dir_and_file (directory, filename);

    if (mc_stat (fname, &s) != 0 || !S_ISREG (s.st_mode)){
	g_free (fname);
	return 0;
    }

    file_fd = mc_open (fname, O_RDONLY);
    g_free (fname);

    if (file_fd == -1)
	return 0;

    g_snprintf (buffer, sizeof (buffer), _("Grepping in %s"), str_trunc (filename, FIND2_X_USE));

    status_update (buffer);
    mc_refresh ();

    tty_enable_interrupt_key ();
    tty_got_interrupt ();

    {
	int line = 1;
	int pos = 0;
	int n_read = 0;
	gboolean has_newline;
	char *p = NULL;
	gboolean found = FALSE;
	gsize found_len;
	char result [BUF_MEDIUM];

	if (resuming) {
	    /* We've been previously suspended, start from the previous position */
	    resuming = 0;
	    line = last_line;
	    pos = last_pos;
	}
	while (!ret_val
		&& (p = get_line_at (file_fd, buffer, sizeof (buffer),
					&pos, &n_read, &has_newline)) != NULL) {
	    if (!found		/* Search in binary line once */
		    && mc_search_run (search_content_handle,
					(const void *) p, 0, strlen (p), &found_len)) {
		g_snprintf (result, sizeof (result), "%d:%s", line, filename);
		find_add_match (directory, result);
		found = TRUE;
	    }
	    g_free (p);

	    if (found && content_first_hit_flag)
		break;

	    if (has_newline) {
		found = FALSE;
		line++;
	    }

	    if ((line & 0xff) == 0) {
		FindProgressStatus res;
		res = check_find_events(h);
		switch (res) {
		case FIND_ABORT:
		    stop_idle (h);
		    ret_val = TRUE;
		    break;
		case FIND_SUSPEND:
		    resuming = 1;
		    last_line = line;
		    last_pos = pos;
		    ret_val = TRUE;
		    break;
		default:
		    break;
		}
	    }

	}
    }
    tty_disable_interrupt_key ();
    mc_close (file_fd);
    return ret_val;
}

static int
do_search (struct Dlg_head *h)
{
    static struct dirent *dp = NULL;
    static DIR  *dirp = NULL;
    static char *directory = NULL;
    struct stat tmp_stat;
    static int pos = 0;
    static int subdirs_left = 0;
    gsize bytes_found;
    unsigned long count;			/* Number of files displayed */

    if (!h) { /* someone forces me to close dirp */
	if (dirp) {
	    mc_closedir (dirp);
	    dirp = NULL;
	}
	g_free (directory);
	directory = NULL;
        dp = NULL;
	return 1;
    }

    search_content_handle = mc_search_new(content_pattern, -1);
    if (search_content_handle) {
        search_content_handle->search_type = (content_regexp_flag) ? MC_SEARCH_T_REGEX : MC_SEARCH_T_NORMAL;
        search_content_handle->is_case_sentitive = content_case_sens_flag;
        search_content_handle->whole_words = content_whole_words;
        search_content_handle->is_all_charsets = content_all_charsets_flag;
    }
    search_file_handle = mc_search_new(find_pattern, -1);
    search_file_handle->search_type = (file_pattern_flag) ?  MC_SEARCH_T_GLOB : MC_SEARCH_T_REGEX;
    search_file_handle->is_case_sentitive = file_case_sens_flag;
    search_file_handle->is_all_charsets = file_all_charsets_flag;
    search_file_handle->is_entire_line = file_pattern_flag;

    count = 0;

 do_search_begin:
    while (!dp){
	if (dirp){
	    mc_closedir (dirp);
	    dirp = 0;
	}
	
	while (!dirp){
	    char *tmp = NULL;

	    tty_setcolor (REVERSE_COLOR);
	    while (1) {
		char *temp_dir = NULL;
		gboolean found;

		tmp = pop_directory ();
		if (tmp == NULL) {
		    running = FALSE;
		    status_update (_("Finished"));
		    stop_idle (h);
		    mc_search_free (search_file_handle);
		    search_file_handle = NULL;
		    mc_search_free (search_content_handle);
		    search_content_handle = NULL;
		    return 0;
		}

		if ((find_ignore_dirs == NULL) || (find_ignore_dirs[0] == '\0'))
		    break;

		temp_dir = g_strdup_printf (":%s:", tmp);
		found = strstr (find_ignore_dirs, temp_dir) != 0;
		g_free (temp_dir);
		
		if (!found)
		    break;

		g_free (tmp);
	    }

	    g_free (directory);
	    directory = tmp;

	    if (verbose){
		char buffer [BUF_SMALL];

		g_snprintf (buffer, sizeof (buffer), _("Searching %s"),
			    str_trunc (directory, FIND2_X_USE));
		status_update (buffer);
	    }
	    /* mc_stat should not be called after mc_opendir
	       because vfs_s_opendir modifies the st_nlink
	    */
	    if (!mc_stat (directory, &tmp_stat))
		subdirs_left = tmp_stat.st_nlink - 2;
	    else
		subdirs_left = 0;

	    dirp = mc_opendir (directory);
	}   /* while (!dirp) */

        /* skip invalid filenames */
        while ((dp = mc_readdir (dirp)) != NULL
		 && !str_is_valid_string (dp->d_name))
            ;
    }	/* while (!dp) */

    if (strcmp (dp->d_name, ".") == 0 ||
	strcmp (dp->d_name, "..") == 0){
	dp = mc_readdir (dirp);
        /* skip invalid filenames */
        while (dp != NULL && !str_is_valid_string (dp->d_name))
            dp = mc_readdir (dirp);

	mc_search_free(search_file_handle);
	search_file_handle = NULL;
	mc_search_free(search_content_handle);
	search_content_handle = NULL;
	return 1;
    }

    if (!(skip_hidden_flag && (dp->d_name[0] == '.'))) {
	gboolean search_ok;

        if ((subdirs_left != 0) && find_recurs_flag
		&& (directory != NULL)) { /* Can directory be NULL ? */
            char *tmp_name = concat_dir_and_file (directory, dp->d_name);
            if (!mc_lstat (tmp_name, &tmp_stat)
                && S_ISDIR (tmp_stat.st_mode)) {
                push_directory (tmp_name);
                subdirs_left--;
            } else
                g_free (tmp_name);
        }

	search_ok = mc_search_run (search_file_handle, dp->d_name,
				    0, strlen (dp->d_name), &bytes_found);

        if (search_ok) {
            if (content_pattern == NULL)
                find_add_match (directory, dp->d_name);
            else if (search_content (h, directory, dp->d_name)) {
                mc_search_free(search_file_handle);
                search_file_handle = NULL;
                mc_search_free(search_content_handle);
                search_content_handle = NULL;
                return 1;
            }
        }
    }

    /* skip invalid filenames */
    while ((dp = mc_readdir (dirp)) != NULL
	    && !str_is_valid_string (dp->d_name))
        ;

    /* Displays the nice dot */
    count++;
    if (!(count & 31)){
	/* For nice updating */
	const char rotating_dash[] = "|/-\\";

	if (verbose){
	    pos = (pos + 1) % 4;
	    tty_setcolor (DLG_NORMALC (h));
	    dlg_move (h, FIND2_Y - 7, FIND2_X - 4);
	    tty_print_char (rotating_dash [pos]);
	    mc_refresh ();
	}
    } else
	goto do_search_begin;

    mc_search_free (search_file_handle);
    search_file_handle = NULL;
    mc_search_free (search_content_handle);
    search_content_handle = NULL;
    return 1;
}

static void
init_find_vars (void)
{
    g_free (old_dir);
    old_dir = NULL;
    matches = 0;

    /* Remove all the items in the stack */
    clear_stack ();
}

static char *
make_fullname (const char *dirname, const char *filename)
{

    if (strcmp(dirname, ".") == 0 || strcmp(dirname, "."PATH_SEP_STR) == 0)
	return g_strdup (filename);
    if (strncmp(dirname, "."PATH_SEP_STR, 2) == 0)
	return concat_dir_and_file (dirname + 2, filename);
    return concat_dir_and_file (dirname, filename);
}

static void
find_do_view_edit (int unparsed_view, int edit, char *dir, char *file)
{
    char *fullname = NULL;
    const char *filename = NULL;
    int line;

    if (content_pattern != NULL) {
	filename = strchr (file + 4, ':') + 1;
	line = atoi (file + 4);
    } else {
	filename = file + 4;
	line = 0;
    }

    fullname = make_fullname (dir, filename);
    if (edit)
	do_edit_at_line (fullname, line);
    else
        view_file_at_line (fullname, unparsed_view, use_internal_view, line);
    g_free (fullname);
}

static cb_ret_t
view_edit_currently_selected_file (int unparsed_view, int edit)
{
    WLEntry *entry = find_list->current;
    char *dir = NULL;

    if (!entry)
        return MSG_NOT_HANDLED;

    dir = entry->data;

    if (!entry->text || !dir)
	return MSG_NOT_HANDLED;

    find_do_view_edit (unparsed_view, edit, dir, entry->text);
    return MSG_HANDLED;
}

static cb_ret_t
find_callback (struct Dlg_head *h, dlg_msg_t msg, int parm)
{
    switch (msg) {
    case DLG_KEY:
	if (parm == KEY_F (3) || parm == KEY_F (13)) {
	    int unparsed_view = (parm == KEY_F (13));
	    return view_edit_currently_selected_file (unparsed_view, 0);
	}
	if (parm == KEY_F (4)) {
	    return view_edit_currently_selected_file (0, 1);
	}
	return MSG_NOT_HANDLED;

    case DLG_IDLE:
	do_search (h);
	return MSG_HANDLED;

    default:
	return default_dlg_callback (h, msg, parm);
    }
}

/* Handles the Stop/Start button in the find window */
static int
start_stop (int button)
{
    (void) button;

    running = is_start;
    set_idle_proc (find_dlg, running);
    is_start = !is_start;

    status_update (is_start ? _("Stopped") : _("Searching"));
    button_set_text (stop_button, fbuts [is_start ? 1 : 0].text);

    return 0;
}

/* Handle view command, when invoked as a button */
static int
find_do_view_file (int button)
{
    (void) button;

    view_edit_currently_selected_file (0, 0);
    return 0;
}

/* Handle edit command, when invoked as a button */
static int
find_do_edit_file (int button)
{
    (void) button;

    view_edit_currently_selected_file (0, 1);
    return 0;
}

static void
setup_gui (void)
{
#ifdef ENABLE_NLS
    static gboolean i18n_flag = FALSE;

    if (!i18n_flag) {
	int i = sizeof (fbuts) / sizeof (fbuts[0]);
	while (i-- != 0) {
            fbuts[i].text = _(fbuts[i].text);
            fbuts[i].len = str_term_width1 (fbuts[i].text) + 3;
	}

	fbuts[2].len += 2;	/* DEFPUSH_BUTTON */
	i18n_flag = TRUE;
    }
#endif				/* ENABLE_NLS */

    /*
     * Dynamically place buttons centered within current window size
     */
    {
	int l0 = max (fbuts[0].len, fbuts[1].len);
	int l1 = fbuts[2].len + fbuts[3].len + l0 + fbuts[4].len;
	int l2 = fbuts[5].len + fbuts[6].len + fbuts[7].len;
	int r1, r2;

	/* Check, if both button rows fit within FIND2_X */
	FIND2_X = max (l1 + 9, COLS - 16);
	FIND2_X = max (l2 + 8, FIND2_X);

	/* compute amount of space between buttons for each row */
	r1 = (FIND2_X - 4 - l1) % 5;
	l1 = (FIND2_X - 4 - l1) / 5;
	r2 = (FIND2_X - 4 - l2) % 4;
	l2 = (FIND2_X - 4 - l2) / 4;

	/* ...and finally, place buttons */
	fbuts[2].x = 2 + r1 / 2 + l1;
	fbuts[3].x = fbuts[2].x + fbuts[2].len + l1;
	fbuts[0].x = fbuts[3].x + fbuts[3].len + l1;
	fbuts[4].x = fbuts[0].x + l0 + l1;
	fbuts[5].x = 2 + r2 / 2 + l2;
	fbuts[6].x = fbuts[5].x + fbuts[5].len + l2;
	fbuts[7].x = fbuts[6].x + fbuts[6].len + l2;
    }

    find_dlg =
	create_dlg (0, 0, FIND2_Y, FIND2_X, dialog_colors, find_callback,
		    "[Find File]", _("Find File"), DLG_CENTER | DLG_REVERSE);

    add_widget (find_dlg,
		button_new (FIND2_Y - 3, fbuts[7].x, B_VIEW, NORMAL_BUTTON,
			    fbuts[7].text, find_do_edit_file));
    add_widget (find_dlg,
		button_new (FIND2_Y - 3, fbuts[6].x, B_VIEW, NORMAL_BUTTON,
			    fbuts[6].text, find_do_view_file));
    add_widget (find_dlg,
		button_new (FIND2_Y - 3, fbuts[5].x, B_PANELIZE,
			    NORMAL_BUTTON, fbuts[5].text, 0));

    add_widget (find_dlg,
		button_new (FIND2_Y - 4, fbuts[4].x, B_CANCEL,
			    NORMAL_BUTTON, fbuts[4].text, 0));
    stop_button =
	button_new (FIND2_Y - 4, fbuts[0].x, B_STOP, NORMAL_BUTTON,
		    fbuts[0].text, start_stop);
    add_widget (find_dlg, stop_button);
    add_widget (find_dlg,
		button_new (FIND2_Y - 4, fbuts[3].x, B_AGAIN,
			    NORMAL_BUTTON, fbuts[3].text, 0));
    add_widget (find_dlg,
		button_new (FIND2_Y - 4, fbuts[2].x, B_ENTER,
			    DEFPUSH_BUTTON, fbuts[2].text, 0));

    status_label = label_new (FIND2_Y - 7, 4, _("Searching"));
    add_widget (find_dlg, status_label);

    found_num_label = label_new (FIND2_Y - 6, 4, "");
    add_widget (find_dlg, found_num_label);

    find_list =
	listbox_new (2, 2, FIND2_Y - 10, FIND2_X - 4, NULL);
    add_widget (find_dlg, find_list);
}

static int
run_process (void)
{
    resuming = 0;
    set_idle_proc (find_dlg, 1);
    return run_dlg (find_dlg);
}

static void
kill_gui (void)
{
    set_idle_proc (find_dlg, 0);
    destroy_dlg (find_dlg);
}

static int
find_file (const char *start_dir, const char *pattern, const char *content,
	    char **dirname, char **filename)
{
    int return_value = 0;
    char *dir_tmp = NULL, *file_tmp = NULL;

    setup_gui ();

    /* FIXME: Need to cleanup this, this ought to be passed non-globaly */
    find_pattern = str_unconst (pattern);
    content_pattern = (content != NULL && str_is_valid_string (content)) 
            ? g_strdup(content)
            : NULL;

    init_find_vars ();
    push_directory (start_dir);

    return_value = run_process ();

    /* Remove all the items in the stack */
    clear_stack ();

    get_list_info (&file_tmp, &dir_tmp);

    if (dir_tmp)
	*dirname = g_strdup (dir_tmp);
    if (file_tmp)
	*filename = g_strdup (file_tmp);

    if (return_value == B_PANELIZE && *filename) {
	int status, link_to_dir, stale_link;
	int next_free = 0;
	int i;
	struct stat st;
	WLEntry *entry = find_list->list;
	dir_list *list = &current_panel->dir;
	char *name = NULL;

	for (i = 0; entry != NULL && i < find_list->count;
			entry = entry->next, i++) {
	    const char *filename = NULL;

	    if (!entry->text || !entry->data)
		continue;

	    if (content_pattern != NULL)
		filename = strchr (entry->text + 4, ':') + 1;
	    else
		filename = entry->text + 4;

	    name = make_fullname (entry->data, filename);
	    status =
		handle_path (list, name, &st, next_free, &link_to_dir,
			     &stale_link);
	    if (status == 0) {
		g_free (name);
		continue;
	    }
	    if (status == -1) {
		g_free (name);
		break;
	    }

	    /* don't add files more than once to the panel */
	    if (content_pattern != NULL && next_free > 0
		&& strcmp (list->list[next_free - 1].fname, name) == 0) {
		g_free (name);
		continue;
	    }

	    if (!next_free)	/* first turn i.e clean old list */
		panel_clean_dir (current_panel);
	    list->list[next_free].fnamelen = strlen (name);
	    list->list[next_free].fname = name;
	    list->list[next_free].f.marked = 0;
	    list->list[next_free].f.link_to_dir = link_to_dir;
	    list->list[next_free].f.stale_link = stale_link;
	    list->list[next_free].f.dir_size_computed = 0;
	    list->list[next_free].st = st;
            list->list[next_free].sort_key = NULL;
            list->list[next_free].second_sort_key = NULL;
	    next_free++;
	    if (!(next_free & 15))
		rotate_dash ();
	}

	if (next_free) {
	    current_panel->count = next_free;
	    current_panel->is_panelized = 1;

	    if (start_dir[0] == PATH_SEP) {
		strcpy (current_panel->cwd, PATH_SEP_STR);
		chdir (PATH_SEP_STR);
	    }
	}
    }

    g_free (content_pattern);
    kill_gui ();
    do_search (NULL);		/* force do_search to release resources */
    g_free (old_dir);
    old_dir = NULL;

    return return_value;
}

void
do_find (void)
{
    char *start_dir = NULL, *pattern = NULL, *content = NULL;
    char *filename = NULL, *dirname = NULL;
    int  v;
    gboolean dir_and_file_set;

    while (find_parameters (&start_dir, &pattern, &content)){
	if (pattern [0] == '\0')
	    break; /* nothing search*/

	dirname = filename = NULL;
	is_start = FALSE;
	v = find_file (start_dir, pattern, content, &dirname, &filename);
	g_free (pattern);

	if (v == B_ENTER){
	    if (dirname || filename){
		if (dirname){
		    do_cd (dirname, cd_exact);
		    if (filename)
			try_to_select (current_panel, filename + (content ? 
			   (strchr (filename + 4, ':') - filename + 1) : 4) );
		} else if (filename)
		    do_cd (filename, cd_exact);
		select_item (current_panel);
	    }
	    g_free (dirname);
	    g_free (filename);
	    break;
	}

	g_free (content);
	dir_and_file_set = dirname && filename;
	g_free (dirname);
	g_free (filename);

	if (v == B_CANCEL)
	    break;
	
	if (v == B_PANELIZE){
	    if (dir_and_file_set){
	        try_to_select (current_panel, NULL);
		panel_re_sort (current_panel);
	        try_to_select (current_panel, NULL);
	    }
	    break;
	}
    }
}
