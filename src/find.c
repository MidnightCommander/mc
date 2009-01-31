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

#include <config.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <mhl/string.h>

#include "global.h"
#include "tty.h"
#include "win.h"
#include "color.h"
#include "setup.h"
#include "find.h"

/* Dialog manager and widgets */
#include "dialog.h"
#include "widget.h"

#include "dir.h"
#include "panel.h"		/* current_panel */
#include "main.h"		/* do_cd, try_to_select */
#include "wtools.h"
#include "cmd.h"		/* view_file_at_line */
#include "boxes.h"
#include "key.h"

/* Size of the find parameters window */
#define FIND_Y 15
static int FIND_X = 50;

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
    FIND_CONT,
    FIND_SUSPEND,
    FIND_ABORT
} FindProgressStatus;

/* List of directories to be ignored, separated by ':' */
char *find_ignore_dirs = 0;

static WInput *in_start;	/* Start path */
static WInput *in_name;		/* Pattern to search */
static WInput *in_with;		/* Text inside filename */
static WCheck *case_sense;	/* "case sensitive" checkbox */
static WCheck *find_regex_cbox;	/* [x] find regular expression */

static int running = 0;		/* nice flag */
static char *find_pattern;	/* Pattern to search */
static char *content_pattern;	/* pattern to search inside files; if
				   find_regex_flag is true, it contains the
				   regex pattern, else the search string. */
static int count;		/* Number of files displayed */
static int matches;		/* Number of matches */
static int is_start;		/* Status of the start/stop toggle button */
static char *old_dir;

/* Where did we stop */
static int resuming;
static int last_line;
static int last_pos;

static Dlg_head *find_dlg;	/* The dialog */

static WButton *stop_button;	/* pointer to the stop button */
static WLabel *status_label;	/* Finished, Searching etc. */
static WListbox *find_list;	/* Listbox with the file list */

/* This keeps track of the directory stack */
typedef struct dir_stack {
    char *name;
    struct dir_stack *prev;
} dir_stack ;

static dir_stack *dir_stack_base = 0;

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

static inline char * add_to_list (const char *text, void *data) {
    return listbox_add_item (find_list, 0, 0, text, data);
}
static inline void stop_idle (void *data) {
    set_idle_proc (data, 0);
}
static inline void status_update (const char *text) {
    label_set_text (status_label, text);
}
static void get_list_info (char **file, char **dir) {
    listbox_get_current (find_list, file, dir);
}

/* FIXME: r should be local variable */
static regex_t *r; /* Pointer to compiled content_pattern */
 
static int case_sensitive = 1;
static gboolean find_regex_flag = TRUE;
static int find_recursively = 1;

/*
 * Callback for the parameter dialog.
 * Validate regex, prevent closing the dialog if it's invalid.
 */
static cb_ret_t
find_parm_callback (struct Dlg_head *h, dlg_msg_t msg, int parm)
{
    int flags;

    switch (msg) {
    case DLG_VALIDATE:
	if ((h->ret_value != B_ENTER) || !in_with->buffer[0]
	    || !(find_regex_cbox->state & C_BOOL))
	    return MSG_HANDLED;

	flags = REG_EXTENDED | REG_NOSUB;

	if (!(case_sense->state & C_BOOL))
	    flags |= REG_ICASE;

	if (regcomp (r, in_with->buffer, flags)) {
	    message (D_ERROR, MSG_ERROR, _("  Malformed regular expression  "));
	    dlg_select_widget (in_with);
	    h->running = 1;	/* Don't stop the dialog */
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
    char *temp_dir;
    static const char *case_label = N_("case &Sensitive");
    static const char *recurs_label = N_("&Find recursively");

    WCheck *recursively_cbox;

    static char *in_contents = NULL;
    static char *in_start_dir = NULL;
    static char *in_start_name = NULL;

    static const char *labs[] =
	{ N_("Start at:"), N_("Filename:"), N_("Content: ") };
    static const char *buts[] = { N_("&OK"), N_("&Tree"), N_("&Cancel") };
    static int ilen = 30, istart = 14;
    static int b0 = 3, b1 = 16, b2 = 36;

#ifdef ENABLE_NLS
    static int i18n_flag = 0;

    if (!i18n_flag) {
	register int i = sizeof (labs) / sizeof (labs[0]);
	int l1, maxlen = 0;

	while (i--) {
	    l1 = strlen (labs[i] = _(labs[i]));
	    if (l1 > maxlen)
		maxlen = l1;
	}
	i = maxlen + ilen + 7;
	if (i > FIND_X)
	    FIND_X = i;

	for (i = sizeof (buts) / sizeof (buts[0]), l1 = 0; i--;) {
	    l1 += strlen (buts[i] = _(buts[i]));
	}
	l1 += 21;
	if (l1 > FIND_X)
	    FIND_X = l1;

	ilen = FIND_X - 7 - maxlen;	/* for the case of very long buttons :) */
	istart = FIND_X - 3 - ilen;

	b1 = b0 + strlen (buts[0]) + 7;
	b2 = FIND_X - (strlen (buts[2]) + 6);

	i18n_flag = 1;
	case_label = _(case_label);
	recurs_label = _(recurs_label);
    }
#endif				/* ENABLE_NLS */

  find_par_start:
    if (!in_start_dir)
	in_start_dir = g_strdup (".");
    if (!in_start_name)
	in_start_name = g_strdup (easy_patterns ? "*" : ".");
    if (!in_contents)
	in_contents = g_strdup ("");

    find_dlg =
	create_dlg (0, 0, FIND_Y, FIND_X, dialog_colors,
		    find_parm_callback, "[Find File]", _("Find File"),
		    DLG_CENTER | DLG_REVERSE);

    add_widget (find_dlg,
		button_new (12, b2, B_CANCEL, NORMAL_BUTTON, buts[2], 0));
    add_widget (find_dlg,
		button_new (12, b1, B_TREE, NORMAL_BUTTON, buts[1], 0));
    add_widget (find_dlg,
		button_new (12, b0, B_ENTER, DEFPUSH_BUTTON, buts[0], 0));

    recursively_cbox =
	check_new (6, istart, find_recursively, recurs_label);
 
    find_regex_cbox = check_new (10, istart, find_regex_flag, _("&Regular expression"));
    add_widget (find_dlg, find_regex_cbox);

    case_sense = check_new (9, istart, case_sensitive, case_label);
    add_widget (find_dlg, case_sense);

    in_with =
	input_new (8, istart, INPUT_COLOR, ilen, in_contents, "content");
    add_widget (find_dlg, in_with);

    add_widget (find_dlg, recursively_cbox);
    in_name =
	input_new (5, istart, INPUT_COLOR, ilen, in_start_name, "name");
    add_widget (find_dlg, in_name);

    in_start =
	input_new (3, istart, INPUT_COLOR, ilen, in_start_dir, "start");
    add_widget (find_dlg, in_start);

    add_widget (find_dlg, label_new (8, 3, labs[2]));
    add_widget (find_dlg, label_new (5, 3, labs[1]));
    add_widget (find_dlg, label_new (3, 3, labs[0]));

    dlg_select_widget (in_name);

    run_dlg (find_dlg);

    switch (find_dlg->ret_value) {
    case B_CANCEL:
	return_value = 0;
	break;

    case B_TREE:
	temp_dir = g_strdup (in_start->buffer);
	case_sensitive = case_sense->state & C_BOOL;
	find_regex_flag = find_regex_cbox->state & C_BOOL;
 	find_recursively = recursively_cbox->state & C_BOOL;
	destroy_dlg (find_dlg);
	g_free (in_start_dir);
	if (strcmp (temp_dir, ".") == 0) {
	    g_free (temp_dir);
	    temp_dir = g_strdup (current_panel->cwd);
	}
	in_start_dir = tree_box (temp_dir);
	if (in_start_dir)
	    g_free (temp_dir);
	else
	    in_start_dir = temp_dir;
	/* Warning: Dreadful goto */
	goto find_par_start;
	break;

    default:
	g_free (in_contents);
	if (in_with->buffer[0]) {
	    *content = g_strdup (in_with->buffer);
	    in_contents = g_strdup (*content);
	} else {
	    *content = in_contents = NULL;
	    r = 0;
	}

	case_sensitive = case_sense->state & C_BOOL;
	find_regex_flag = find_regex_cbox->state & C_BOOL;
 	find_recursively = recursively_cbox->state & C_BOOL;
	return_value = 1;
	*start_dir = g_strdup (in_start->buffer);
	*pattern = g_strdup (in_name->buffer);

	g_free (in_start_dir);
	in_start_dir = g_strdup (*start_dir);
	g_free (in_start_name);
	in_start_name = g_strdup (*pattern);
    }

    destroy_dlg (find_dlg);

    return return_value;
}

static void
push_directory (const char *dir)
{
    dir_stack *new;

    new = g_new (dir_stack, 1);
    new->name = mhl_str_dir_plus_file (dir, NULL);
    new->prev = dir_stack_base;
    dir_stack_base = new;
}

static char*
pop_directory (void)
{
    char *name; 
    dir_stack *next;

    if (dir_stack_base){
	name = dir_stack_base->name;
	next = dir_stack_base->prev;
	g_free (dir_stack_base);
	dir_stack_base = next;
	return name;
    } else
	return 0;
}

static void
insert_file (const char *dir, const char *file)
{
    char *tmp_name;
    static char *dirname;

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
    
    tmp_name = g_strconcat ("    ", file, (char *) NULL);
    add_to_list (tmp_name, dirname);
    g_free (tmp_name);
}

static void
find_add_match (Dlg_head *h, const char *dir, const char *file)
{
    int p = ++matches & 7;

    (void) h;

    insert_file (dir, file);

    /* Scroll nicely */
    if (!p)
	listbox_select_last (find_list, 1);
    else
	listbox_select_last (find_list, 0);
	/* Updates the current listing */
	send_message (&find_list->widget, WIDGET_DRAW, 0);
	if (p == 7)
	    mc_refresh ();
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
get_line_at (int file_fd, char *buf, int *pos, int *n_read, int buf_size,
	     int *has_newline)
{
    char *buffer = 0;
    int buffer_size = 0;
    char ch = 0;
    int i = 0;

    for (;;) {
	if (*pos >= *n_read) {
	    *pos = 0;
	    if ((*n_read = mc_read (file_fd, buf, buf_size)) <= 0)
		break;
	}

	ch = buf[(*pos)++];
	if (ch == 0) {
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

    *has_newline = ch ? 1 : 0;

    if (buffer) {
	buffer[i] = 0;
    }

    return buffer;
}

static FindProgressStatus
check_find_events(Dlg_head *h)
{
    Gpm_Event event;
    int c;
     
    c = get_event (&event, h->mouse_status == MOU_REPEAT, 0);
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
 * Search the global (FIXME) regexp compiled content_pattern string in the
 * DIRECTORY/FILE.  It will add the found entries to the find listbox.
 * 
 * returns 0 if do_search should look for another file
 *         1 if do_search should exit and proceed to the event handler
 */
static int
search_content (Dlg_head *h, const char *directory, const char *filename)
{
    struct stat s;
    char buffer [BUF_SMALL];
    char *fname;
    int file_fd;
    int ret_val = 0;

    fname = mhl_str_dir_plus_file (directory, filename);

    if (mc_stat (fname, &s) != 0 || !S_ISREG (s.st_mode)){
	g_free (fname);
	return 0;
    }

    file_fd = mc_open (fname, O_RDONLY);
    g_free (fname);

    if (file_fd == -1)
	return 0;

    g_snprintf (buffer, sizeof (buffer), _("Grepping in %s"), name_trunc (filename, FIND2_X_USE));

    status_update (buffer);
    mc_refresh ();

    enable_interrupt_key ();
    got_interrupt ();

    {
	int line = 1;
	int pos = 0;
	int n_read = 0;
	int has_newline;
	char *p;
	int found = 0;
	typedef const char * (*search_fn) (const char *, const char *);
	search_fn search_func;

	if (resuming) {
	    /* We've been previously suspended, start from the previous position */
	    resuming = 0;
	    line = last_line;
	    pos = last_pos;
	}

	search_func = (case_sensitive) ? cstrstr : cstrcasestr;
	
	while ((p = get_line_at (file_fd, buffer, &pos, &n_read, sizeof (buffer), &has_newline)) && (ret_val == 0)){
	    if (found == 0){	/* Search in binary line once */
	    	if (find_regex_flag) {
		if (regexec (r, p, 1, 0, 0) == 0){
		    g_free (p);
		    p = g_strdup_printf ("%d:%s", line, filename);
		    find_add_match (h, directory, p);
		    found = 1;
		}
	    	} else {
	    	    if (search_func (p, content_pattern) != NULL) {
	    	    	char *match = g_strdup_printf("%d:%s", line, filename);
			find_add_match (h, directory, match);
			found = TRUE;
	    	    }
	    	}
	    }
	    if (has_newline){
		line++;
		found = 0;
	    }
	    g_free (p);
 
 	    if ((line & 0xff) == 0) {
		FindProgressStatus res;
		res = check_find_events(h);
		switch (res) {
		case FIND_ABORT:
		    stop_idle (h);
		    ret_val = 1;
		    break;
		case FIND_SUSPEND:
		    resuming = 1;
		    last_line = line;
		    last_pos = pos;
		    ret_val = 1;
		    break;
		default:
		    break;
		}
	    }
 
	}
    }
    disable_interrupt_key ();
    mc_close (file_fd);
    return ret_val;
}

static int
do_search (struct Dlg_head *h)
{
    static struct dirent *dp   = 0;
    static DIR  *dirp = 0;
    static char *directory;
    struct stat tmp_stat;
    static int pos;
    static int subdirs_left = 0;

    if (!h) { /* someone forces me to close dirp */
	if (dirp) {
	    mc_closedir (dirp);
	    dirp = 0;
	}
	g_free (directory);
	directory = NULL;
        dp = 0;
	return 1;
    }
 do_search_begin:
    while (!dp){
	
	if (dirp){
	    mc_closedir (dirp);
	    dirp = 0;
	}
	
	while (!dirp){
	    char *tmp;

	    attrset (REVERSE_COLOR);
	    while (1) {
		tmp = pop_directory ();
		if (!tmp){
		    running = 0;
		    status_update (_("Finished"));
		    stop_idle (h);
		    return 0;
		}
		if (find_ignore_dirs){
                    int found;
		    char *temp_dir = g_strconcat (":", tmp, ":", (char *) NULL);

                    found = strstr (find_ignore_dirs, temp_dir) != 0;
                    g_free (temp_dir);
		    if (found)
			g_free (tmp);
		    else
			break;
		} else
		    break;
	    } 

	    g_free (directory);
	    directory = tmp;

	    if (verbose){
		char buffer [BUF_SMALL];

		g_snprintf (buffer, sizeof (buffer), _("Searching %s"), 
			    name_trunc (directory, FIND2_X_USE));
		status_update (buffer);
	    }
	    /* mc_stat should not be called after mc_opendir
	       because vfs_s_opendir modifies the st_nlink
	    */
	    if (!mc_stat (directory, &tmp_stat))
		subdirs_left = tmp_stat.st_nlink - 2;
	    else
		subdirs_left = 0;
	    /* Commented out as unnecessary
	       if (subdirs_left < 0)
	       subdirs_left = MAXINT;
	    */
	    dirp = mc_opendir (directory);
	}   /* while (!dirp) */
	dp = mc_readdir (dirp);
    }	/* while (!dp) */

    if (strcmp (dp->d_name, ".") == 0 ||
	strcmp (dp->d_name, "..") == 0){
	dp = mc_readdir (dirp);
	return 1;
    }

    if (subdirs_left && find_recursively && directory) { /* Can directory be NULL ? */
	char *tmp_name = mhl_str_dir_plus_file (directory, dp->d_name);
	if (!mc_lstat (tmp_name, &tmp_stat)
	    && S_ISDIR (tmp_stat.st_mode)) {
	    push_directory (tmp_name);
	    subdirs_left--;
	}
	g_free (tmp_name);
    }

    if (regexp_match (find_pattern, dp->d_name, match_file)){
	if (content_pattern) {
	    if (search_content (h, directory, dp->d_name)) {
		return 1;
	    }
	} else 
	    find_add_match (h, directory, dp->d_name);
    }
    
    dp = mc_readdir (dirp);

    /* Displays the nice dot */
    count++;
    if (!(count & 31)){
	/* For nice updating */
	const char *rotating_dash = "|/-\\";

	if (verbose){
	    pos = (pos + 1) % 4;
	    attrset (DLG_NORMALC (h));
	    dlg_move (h, FIND2_Y-6, FIND2_X - 4);
	    addch (rotating_dash [pos]);
	    mc_refresh ();
	}
    } else
	goto do_search_begin;
    return 1;
}

static void
init_find_vars (void)
{
    char *dir;
    
    g_free (old_dir);
    old_dir = 0;
    count = 0;
    matches = 0;

    /* Remove all the items in the stack */
    while ((dir = pop_directory ()) != NULL)
	g_free (dir);
}

static char *
make_fullname (const char *dirname, const char *filename)
{

    if (strcmp(dirname, ".") == 0 || strcmp(dirname, "."PATH_SEP_STR) == 0)
	return g_strdup (filename);
    if (strncmp(dirname, "."PATH_SEP_STR, 2) == 0)
	return mhl_str_dir_plus_file (dirname + 2, filename);
    return mhl_str_dir_plus_file (dirname, filename);
}

static void
find_do_view_edit (int unparsed_view, int edit, char *dir, char *file)
{
    char *fullname;
    const char *filename;
    int line;
    
    if (content_pattern){
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

static int
view_edit_currently_selected_file (int unparsed_view, int edit)
{
    WLEntry *entry = find_list->current;
    char *dir;

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
    button_set_text (stop_button, fbuts [is_start].text);

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
    static int i18n_flag = 0;
    if (!i18n_flag) {
	register int i = sizeof (fbuts) / sizeof (fbuts[0]);
	while (i--)
	    fbuts[i].len = strlen (fbuts[i].text = _(fbuts[i].text)) + 3;
	fbuts[2].len += 2;	/* DEFPUSH_BUTTON */
	i18n_flag = 1;
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

	FIND2_X = COLS - 16;

	/* Check, if both button rows fit within FIND2_X */
	if (l1 + 9 > FIND2_X)
	    FIND2_X = l1 + 9;
	if (l2 + 8 > FIND2_X)
	    FIND2_X = l2 + 8;

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

    status_label = label_new (FIND2_Y - 6, 4, _("Searching"));
    add_widget (find_dlg, status_label);

    find_list =
	listbox_new (2, 2, FIND2_X - 4, FIND2_Y - 9, 0);
    add_widget (find_dlg, find_list);
}

static int
run_process (void)
{
    resuming = 0;
    set_idle_proc (find_dlg, 1);
    run_dlg (find_dlg);
    return find_dlg->ret_value;
}

static void
kill_gui (void)
{
    set_idle_proc (find_dlg, 0);
    destroy_dlg (find_dlg);
}

static int
find_file (char *start_dir, char *pattern, char *content, char **dirname,
	   char **filename)
{
    int return_value = 0;
    char *dir;
    char *dir_tmp, *file_tmp;

    setup_gui ();

    /* FIXME: Need to cleanup this, this ought to be passed non-globaly */
    find_pattern = pattern;
    content_pattern = content;

    init_find_vars ();
    push_directory (start_dir);

    return_value = run_process ();

    /* Remove all the items in the stack */
    while ((dir = pop_directory ()) != NULL)
	g_free (dir);

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
	char *name;

	for (i = 0; entry && i < find_list->count;
	     entry = entry->next, i++) {
	    const char *filename;

	    if (!entry->text || !entry->data)
		continue;

	    if (content_pattern)
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
	    if (content_pattern && next_free > 0) {
		if (strcmp (list->list[next_free - 1].fname, name) == 0) {
		    g_free (name);
		    continue;
		}
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
	    next_free++;
	    if (!(next_free & 15))
		rotate_dash ();
	}
	if (next_free) {
	    current_panel->count = next_free;
	    current_panel->is_panelized = 1;
	    /* Done by panel_clean_dir a few lines above 
	       current_panel->dirs_marked = 0;
	       current_panel->marked = 0;
	       current_panel->total = 0;
	       current_panel->top_file = 0;
	       current_panel->selected = 0; */

	    if (start_dir[0] == PATH_SEP) {
		strcpy (current_panel->cwd, PATH_SEP_STR);
		chdir (PATH_SEP_STR);
	    }
	}
    }

    kill_gui ();
    do_search (0);		/* force do_search to release resources */
    g_free (old_dir);
    old_dir = 0;

    return return_value;
}

void
do_find (void)
{
    char *start_dir = NULL, *pattern = NULL, *content = NULL;
    char *filename, *dirname;
    int  v, dir_and_file_set;
    regex_t rx; /* Compiled content_pattern to search inside files */

    for (r = &rx; find_parameters (&start_dir, &pattern, &content); r = &rx){

	dirname = filename = NULL;
	is_start = 0;
	v = find_file (start_dir, pattern, content, &dirname, &filename);
	g_free (start_dir);
	g_free (pattern);
	if (find_regex_flag && r)
	    regfree (r);
	
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

