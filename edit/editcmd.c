/* editor high level editing commands.

   Copyright (C) 1996, 1997 the Free Software Foundation

   Authors: 1996, 1997 Paul Sheer

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

*/

/* #define PIPE_BLOCKS_SO_READ_BYTE_BY_BYTE */

#include <config.h>
#include <ctype.h>

#include "edit.h"
#include "editlock.h"
#include "editcmddef.h"
#include "edit-widget.h"

#include "src/color.h"		/* dialog_colors */
#include "src/tty.h"		/* LINES */
#include "src/widget.h"		/* listbox_new() */
#include "src/main.h"		/* mc_home */
#include "src/help.h"		/* interactive_display() */
#include "src/key.h"		/* XCTRL */
#include "src/dialog.h"		/* do_refresh() */
#include "src/wtools.h"		/* message() */
#include "src/charsets.h"

#define edit_get_load_file(f,h) input_dialog (h, _(" Enter file name: "), f)
#define edit_get_save_file(f,h) input_dialog (h, _(" Enter file name: "), f)

/* globals: */

/* search and replace: */
int replace_scanf = 0;
int replace_regexp = 0;
int replace_all = 0;
int replace_prompt = 1;
int replace_whole = 0;
int replace_case = 0;
int replace_backwards = 0;
int search_create_bookmark = 0;

/* queries on a save */
int edit_confirm_save = 1;

#define NUM_REPL_ARGS 64
#define MAX_REPL_LEN 1024

static inline int my_lower_case (int c)
{
    return tolower(c & 0xFF);
}

char *strcasechr (const unsigned char *s, int c)
{
    for (c = my_lower_case (c); my_lower_case ((int) *s) != c; ++s)
	if (*s == '\0')
	    return 0;
    return (char *) s;
}

#ifndef HAVE_MEMMOVE
/* for Christophe */
static void *memmove (void *dest, const void *src, size_t n)
{
    char *t, *s;

    if (dest <= src) {
	t = (char *) dest;
	s = (char *) src;
	while (n--)
	    *t++ = *s++;
    } else {
	t = (char *) dest + n;
	s = (char *) src + n;
	while (n--)
	    *--t = *--s;
    }
    return dest;
}
#endif /* !HAVE_MEMMOVE */

/* #define itoa MY_itoa  <---- this line is now in edit.h */
static char *
MY_itoa (int i)
{
    static char t[14];
    char *s = t + 13;
    int j = i;
    *s-- = 0;
    do {
	*s-- = i % 10 + '0';
    } while ((i = i / 10));
    if (j < 0)
	*s-- = '-';
    return ++s;
}

/* Temporary strings */
static char *stacked[16];

/*
   This joins strings end on end and allocates memory for the result.
   The result is later automatically free'd and must not be free'd
   by the caller.
 */
char *catstrs (const char *first,...)
{
    static int i = 0;
    va_list ap;
    int len;
    char *data;

    if (!first)
	return 0;

    len = strlen (first);
    va_start (ap, first);

    while ((data = va_arg (ap, char *)) != 0)
	 len += strlen (data);

    len++;

    i = (i + 1) % 16;
    g_free (stacked[i]);

    stacked[i] = g_malloc (len);
    va_end (ap);
    va_start (ap, first);
    strcpy (stacked[i], first);
    while ((data = va_arg (ap, char *)) != 0)
	 strcat (stacked[i], data);
    va_end (ap);

    return stacked[i];
}

/* Free temporary strings */
void freestrs(void)
{
    int i;

    for (i = 0; i < sizeof(stacked) / sizeof(stacked[0]); i++) {
	g_free (stacked[i]);
	stacked[i] = NULL;
    }
}

void edit_help_cmd (WEdit * edit)
{
    interactive_display (NULL, "[Internal File Editor]");
    edit->force |= REDRAW_COMPLETELY;
}

void edit_refresh_cmd (WEdit * edit)
{
#ifndef HAVE_SLANG
    clr_scr();
    do_refresh();
#else
    {
	int color;
	edit_get_syntax_color (edit, -1, &color);
    }
    touchwin(stdscr);
#endif /* !HAVE_SLANG */
    mc_refresh();
    doupdate();
}

/*  If 0 (quick save) then  a) create/truncate <filename> file,
			    b) save to <filename>;
    if 1 (safe save) then   a) save to <tempnam>,
			    b) rename <tempnam> to <filename>;
    if 2 (do backups) then  a) save to <tempnam>,
			    b) rename <filename> to <filename.backup_ext>,
			    c) rename <tempnam> to <filename>. */

/* returns 0 on error */
int
edit_save_file (WEdit *edit, const char *filename)
{
    char *p;
    long filelen = 0;
    char *savename = 0;
    int this_save_mode, fd;

    if (!filename)
	return 0;
    if (!*filename)
	return 0;

    if ((fd = mc_open (filename, O_WRONLY | O_BINARY)) == -1) {
	/*
	 * The file does not exists yet, so no safe save or
	 * backup are necessary.
	 */
	this_save_mode = EDIT_QUICK_SAVE;
    } else {
	mc_close (fd);
	this_save_mode = option_save_mode;
    }

    if (this_save_mode != EDIT_QUICK_SAVE) {
	char *savedir, *slashpos, *saveprefix;
	slashpos = strrchr (filename, PATH_SEP);
	if (slashpos) {
	    savedir = g_strdup (filename);
	    savedir[slashpos - filename + 1] = '\0';
	} else
	    savedir = g_strdup (".");
	saveprefix = concat_dir_and_file (savedir, "cooledit");
	g_free (savedir);
	fd = mc_mkstemps (&savename, saveprefix, NULL);
	g_free (saveprefix);
	if (!savename)
	    return 0;
	/* FIXME:
	 * Close for now because mc_mkstemps use pure open system call 
	 * to create temporary file and it needs to be reopened by
	 * VFS-aware mc_open().
	 */
	close (fd);
    } else
	savename = g_strdup (filename);

    mc_chown (savename, edit->stat1.st_uid, edit->stat1.st_gid);
    mc_chmod (savename, edit->stat1.st_mode);

    if ((fd =
	 mc_open (savename, O_CREAT | O_WRONLY | O_TRUNC | O_BINARY,
		  edit->stat1.st_mode)) == -1)
	goto error_save;

/* pipe save */
    if ((p = edit_get_write_filter (savename, filename))) {
	FILE *file;

	mc_close (fd);
	file = (FILE *) popen (p, "w");

	if (file) {
	    filelen = edit_write_stream (edit, file);
#if 1
	    pclose (file);
#else
	    if (pclose (file) != 0) {
		edit_error_dialog (_("Error"),
				   catstrs (_(" Error writing to pipe: "),
					    p, " ", 0));
		g_free (p);
		goto error_save;
	    }
#endif
	} else {
	    edit_error_dialog (_("Error"),
			       get_sys_error (catstrs
					      (_
					       (" Cannot open pipe for writing: "),
					       p, " ", 0)));
	    g_free (p);
	    goto error_save;
	}
	g_free (p);
    } else {
	long buf;
	buf = 0;
	filelen = edit->last_byte;
	while (buf <= (edit->curs1 >> S_EDIT_BUF_SIZE) - 1) {
	    if (mc_write (fd, (char *) edit->buffers1[buf], EDIT_BUF_SIZE)
		!= EDIT_BUF_SIZE) {
		mc_close (fd);
		goto error_save;
	    }
	    buf++;
	}
	if (mc_write
	    (fd, (char *) edit->buffers1[buf],
	     edit->curs1 & M_EDIT_BUF_SIZE) !=
	    (edit->curs1 & M_EDIT_BUF_SIZE)) {
	    filelen = -1;
	} else if (edit->curs2) {
	    edit->curs2--;
	    buf = (edit->curs2 >> S_EDIT_BUF_SIZE);
	    if (mc_write
		(fd,
		 (char *) edit->buffers2[buf] + EDIT_BUF_SIZE -
		 (edit->curs2 & M_EDIT_BUF_SIZE) - 1,
		 1 + (edit->curs2 & M_EDIT_BUF_SIZE)) !=
		1 + (edit->curs2 & M_EDIT_BUF_SIZE)) {
		filelen = -1;
	    } else {
		while (--buf >= 0) {
		    if (mc_write
			(fd, (char *) edit->buffers2[buf],
			 EDIT_BUF_SIZE) != EDIT_BUF_SIZE) {
			filelen = -1;
			break;
		    }
		}
	    }
	    edit->curs2++;
	}
	if (mc_close (fd))
	    goto error_save;
    }

    if (filelen != edit->last_byte)
	goto error_save;
    if (this_save_mode == EDIT_DO_BACKUP)
	if (mc_rename (filename, catstrs (filename, option_backup_ext, 0))
	    == -1)
	    goto error_save;
    if (this_save_mode != EDIT_QUICK_SAVE)
	if (mc_rename (savename, filename) == -1)
	    goto error_save;
    g_free (savename);
    return 1;
  error_save:
/*  FIXME: Is this safe ?
 *  if (this_save_mode != EDIT_QUICK_SAVE)
 *	mc_unlink (savename);
 */
    g_free (savename);
    return 0;
}

/*
   I changed this from Oleg's original routine so
   that option_backup_ext works with coolwidgets as well. This
   does mean there is a memory leak - paul.
 */
void menu_save_mode_cmd (void)
{
#define DLG_X 38
#define DLG_Y 10
    static char *str_result;
    static int save_mode_new;
    static char *str[] =
    {
	N_("Quick save "),
	N_("Safe save "),
	N_("Do backups -->")};
    static QuickWidget widgets[] =
    {
	{quick_button, 18, DLG_X, 7, DLG_Y, N_("&Cancel"), 0,
	 B_CANCEL, 0, 0, "c"},
	{quick_button, 6, DLG_X, 7, DLG_Y, N_("&OK"), 0,
	 B_ENTER, 0, 0, "o"},
	{quick_input, 23, DLG_X, 5, DLG_Y, 0, 9,
	 0, 0, &str_result, "edit-backup-ext"},
	{quick_label, 22, DLG_X, 4, DLG_Y, N_("Extension:"), 0,
	 0, 0, 0, "savemext"},
	{quick_radio, 4, DLG_X, 3, DLG_Y, "", 3,
	 0, &save_mode_new, str, "t"},
	{0}};
    static QuickDialog dialog =
    {DLG_X, DLG_Y, -1, -1, N_(" Edit Save Mode "), "[Edit Save Mode]",
     widgets};
    static int i18n_flag = 0;

    if (!i18n_flag) {
        int i;
	int maxlen = 0;
	int dlg_x;
	int l1;

	/* OK/Cancel buttons */
	l1 = strlen (_(widgets[0].text)) + strlen (_(widgets[1].text)) + 5;
	maxlen = max (maxlen, l1);
        
        for (i = 0; i < 3; i++ ) {
            str[i] = _(str[i]);
	    maxlen = max (maxlen, strlen (str[i]) + 7);
	}
        i18n_flag = 1;

        dlg_x = maxlen + strlen (_(widgets[3].text)) + 5 + 1;
        widgets[2].hotkey_pos = strlen (_(widgets[3].text)); /* input field length */
        dlg_x = min (COLS, dlg_x);
	dialog.xlen = dlg_x;

        i = (dlg_x - l1)/3;
	widgets[1].relative_x = i;
	widgets[0].relative_x = i + strlen (_(widgets[1].text)) + i + 4;

	widgets[2].relative_x = widgets[3].relative_x = maxlen + 2;

	for (i = 0; i < sizeof (widgets)/sizeof (widgets[0]); i++)
		widgets[i].x_divisions = dlg_x;
    }

    widgets[2].text = option_backup_ext;
    widgets[4].value = option_save_mode;
    if (quick_dialog (&dialog) != B_ENTER)
	return;
    option_save_mode = save_mode_new;
    option_backup_ext = str_result;	/* this is a memory leak */
    option_backup_ext_int = 0;
    str_result[min (strlen (str_result), sizeof (int))] = '\0';
    memcpy ((char *) &option_backup_ext_int, str_result, strlen (option_backup_ext));
}

void
edit_set_filename (WEdit *edit, const char *f)
{
    if (edit->filename)
	g_free (edit->filename);
    if (!f)
	f = "";
    edit->filename = g_strdup (f);
}

/* Here we want to warn the users of overwriting an existing file,
   but only if they have made a change to the filename */
/* returns 1 on success */
int
edit_save_as_cmd (WEdit *edit)
{
    /* This heads the 'Save As' dialog box */
    char *exp = 0;
    int save_lock = 0;
    int different_filename = 0;

    exp = edit_get_save_file (edit->filename, _(" Save As "));
    edit_push_action (edit, KEY_PRESS + edit->start_display);

    if (exp) {
	if (!*exp) {
	    g_free (exp);
	    edit->force |= REDRAW_COMPLETELY;
	    return 0;
	} else {
	    if (strcmp (edit->filename, exp)) {
		int file;
		different_filename = 1;
		if ((file = mc_open (exp, O_RDONLY | O_BINARY)) != -1) {
		    /* the file exists */
		    mc_close (file);
		    /* Overwrite the current file or cancel the operation */
		    if (edit_query_dialog2
			(_("Warning"),
			 _(" A file already exists with this name. "),
			 _("Overwrite"), _("Cancel"))) {
			edit->force |= REDRAW_COMPLETELY;
			g_free (exp);
			return 0;
		    }
		}
		save_lock = edit_lock_file (exp);
	    } else {
		/* filenames equal, check if already locked */
		if (!edit->locked && !edit->delete_file)
		    save_lock = edit_lock_file (exp);
	    }
		
	    if (edit_save_file (edit, exp)) {
		/* Succesful, so unlock both files */
		if (strcmp (edit->filename, exp)) {
		    if (save_lock)
			edit_unlock_file (exp);
		    if (edit->locked)
			edit->locked = edit_unlock_file (edit->filename);
		} else {
		    if (edit->locked || save_lock)
			edit->locked = edit_unlock_file (edit->filename);
		}
		
		edit_set_filename (edit, exp);
		g_free (exp);
		edit->modified = 0;
		edit->delete_file = 0;
		if (different_filename)
		    edit_load_syntax (edit, 0, 0);
		edit->force |= REDRAW_COMPLETELY;
		return 1;
	    } else {
		/* Failed, so maintain modify (not save) lock */
		if (strcmp (edit->filename, exp) && save_lock)
		    edit_unlock_file (exp);
		if (save_lock)
		    edit->locked = edit_unlock_file (edit->filename);
		g_free (exp);
		edit_error_dialog (_(" Save As "),
				   get_sys_error (_
						  (" Cannot save file. ")));
		edit->force |= REDRAW_COMPLETELY;
		return 0;
	    }
	}
    }
    edit->force |= REDRAW_COMPLETELY;
    return 0;
}

/* {{{ Macro stuff starts here */

static int
raw_callback (struct Dlg_head *h, int key, int Msg)
{
    switch (Msg) {
    case DLG_KEY:
	h->running = 0;
	h->ret_value = key;
	return MSG_HANDLED;
    }
    return default_dlg_callback (h, key, Msg);;
}

/* gets a raw key from the keyboard. Passing cancel = 1 draws
   a cancel button thus allowing c-c etc.  Alternatively, cancel = 0 
   will return the next key pressed.  ctrl-a (=B_CANCEL), ctrl-g, ctrl-c,
   and Esc are cannot returned */
int
edit_raw_key_query (char *heading, char *query, int cancel)
{
    int w = strlen (query) + 7;
    struct Dlg_head *raw_dlg =
	create_dlg (0, 0, 7, w, dialog_colors, raw_callback,
		    NULL, heading,
		    DLG_CENTER | DLG_TRYUP | DLG_WANT_TAB);
    if (cancel)
	add_widget (raw_dlg,
		    button_new (4, w / 2 - 5, B_CANCEL, NORMAL_BUTTON,
				_("Cancel"), 0, 0, 0));
    add_widget (raw_dlg, label_new (3 - cancel, 2, query, 0));
    add_widget (raw_dlg,
		input_new (3 - cancel, w - 5, INPUT_COLOR, 2, "", 0));
    run_dlg (raw_dlg);
    w = raw_dlg->ret_value;
    destroy_dlg (raw_dlg);
    if (cancel) {
	if (w == XCTRL ('g') || w == XCTRL ('c') || w == ESC_CHAR
	    || w == B_CANCEL)
	    return 0;
    }

    return w;
}

/* creates a macro file if it doesn't exist */
static FILE *edit_open_macro_file (const char *r)
{
    char *filename;
    int file;
    filename = catstrs (home_dir, MACRO_FILE, 0);
    if ((file = open (filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1)
	return 0;
    close (file);
    return fopen (filename, r);
}

#define MAX_MACROS 1024
static int saved_macro[MAX_MACROS + 1];
static int saved_macros_loaded = 0;

/*
   This is just to stop the macro file be loaded over and over for keys
   that aren't defined to anything. On slow systems this could be annoying.
 */
static int
macro_exists (int k)
{
    int i;
    for (i = 0; i < MAX_MACROS && saved_macro[i]; i++)
	if (saved_macro[i] == k)
	    return i;
    return -1;
}

/* returns 1 on error */
static int
edit_delete_macro (WEdit * edit, int k)
{
    struct macro macro[MAX_MACRO_LENGTH];
    FILE *f, *g;
    int s, i, n, j = 0;

    if (saved_macros_loaded)
	if ((j = macro_exists (k)) < 0)
	    return 0;
    g = fopen (catstrs (home_dir, TEMP_FILE, 0), "w");
    if (!g) {
/* This heads the delete macro error dialog box */
	edit_error_dialog (_(" Delete macro "),
/* 'Open' = load temp file */
		 get_sys_error (_(" Cannot open temp file ")));
	return 1;
    }
    f = edit_open_macro_file ("r");
    if (!f) {
/* This heads the delete macro error dialog box */
	edit_error_dialog (_(" Delete macro "),
/* 'Open' = load temp file */
		get_sys_error (_(" Cannot open macro file ")));
	fclose (g);
	return 1;
    }
    for (;;) {
	n = fscanf (f, ("key '%d 0': "), &s);
	if (!n || n == EOF)
	    break;
	n = 0;
	while (fscanf (f, "%hd %hd, ", &macro[n].command, &macro[n].ch))
	    n++;
	fscanf (f, ";\n");
	if (s != k) {
	    fprintf (g, ("key '%d 0': "), s);
	    for (i = 0; i < n; i++)
		fprintf (g, "%hd %hd, ", macro[i].command, macro[i].ch);
	    fprintf (g, ";\n");
	}
    }
    fclose (f);
    fclose (g);
    if (rename (catstrs (home_dir, TEMP_FILE, 0), catstrs (home_dir, MACRO_FILE, 0)) == -1) {
/* This heads the delete macro error dialog box */
	edit_error_dialog (_(" Delete macro "),
	   get_sys_error (_(" Cannot overwrite macro file ")));
	return 1;
    }
    if (saved_macros_loaded)
	memmove (saved_macro + j, saved_macro + j + 1, sizeof (int) * (MAX_MACROS - j - 1));
    return 0;
}

/* returns 0 on error */
int edit_save_macro_cmd (WEdit * edit, struct macro macro[], int n)
{
    FILE *f;
    int s, i;

    edit_push_action (edit, KEY_PRESS + edit->start_display);
/* This heads the 'Macro' dialog box */
    s = edit_raw_key_query (_(" Save macro "),
/* Input line for a single key press follows the ':' */
    _(" Press the macro's new hotkey: "), 1);
    edit->force |= REDRAW_COMPLETELY;
    if (s) {
	if (edit_delete_macro (edit, s))
	    return 0;
	f = edit_open_macro_file ("a+");
	if (f) {
	    fprintf (f, ("key '%d 0': "), s);
	    for (i = 0; i < n; i++)
		fprintf (f, "%hd %hd, ", macro[i].command, macro[i].ch);
	    fprintf (f, ";\n");
	    fclose (f);
	    if (saved_macros_loaded) {
		for (i = 0; i < MAX_MACROS && saved_macro[i]; i++);
		saved_macro[i] = s;
	    }
	    return 1;
	} else
/* This heads the 'Save Macro' dialog box */
	    edit_error_dialog (_(" Save macro "), get_sys_error (_(" Cannot open macro file ")));
    }
    return 0;
}

void edit_delete_macro_cmd (WEdit * edit)
{
    int command;

    command = edit_raw_key_query (_ (" Delete macro "),
				  _ (" Press macro hotkey: "), 1);

    if (!command)
	return;

    edit_delete_macro (edit, command);
}

/* return 0 on error */
int edit_load_macro_cmd (WEdit * edit, struct macro macro[], int *n, int k)
{
    FILE *f;
    int s, i = 0, found = 0;

    if (saved_macros_loaded)
	if (macro_exists (k) < 0)
	    return 0;

    if ((f = edit_open_macro_file ("r"))) {
	struct macro dummy;
	do {
	    int u;
	    u = fscanf (f, ("key '%d 0': "), &s);
	    if (!u || u == EOF)
		break;
	    if (!saved_macros_loaded)
		saved_macro[i++] = s;
	    if (!found) {
		*n = 0;
		while (*n < MAX_MACRO_LENGTH && 2 == fscanf (f, "%hd %hd, ", &macro[*n].command, &macro[*n].ch))
		    (*n)++;
	    } else {
		while (2 == fscanf (f, "%hd %hd, ", &dummy.command, &dummy.ch));
	    }
	    fscanf (f, ";\n");
	    if (s == k)
		found = 1;
	} while (!found || !saved_macros_loaded);
	if (!saved_macros_loaded) {
	    saved_macro[i] = 0;
	    saved_macros_loaded = 1;
	}
	fclose (f);
	return found;
    } else
/* This heads the 'Load Macro' dialog box */
	edit_error_dialog (_(" Load macro "),
		get_sys_error (_(" Cannot open macro file ")));
    return 0;
}

/* }}} Macro stuff starts here */

/* returns 1 on success */
int edit_save_confirm_cmd (WEdit * edit)
{
    char *f;

    if (edit_confirm_save) {
	f = catstrs (_(" Confirm save file? : "), edit->filename, " ", 0);
/* Buttons to 'Confirm save file' query */
	if (edit_query_dialog2 (_(" Save file "), f, _("Save"), _("Cancel")))
	    return 0;
    }
    return edit_save_cmd (edit);
}


/* returns 1 on success */
int edit_save_cmd (WEdit * edit)
{
    int res, save_lock = 0;
    
    if (!edit->locked && !edit->delete_file)
	save_lock = edit_lock_file (edit->filename);
    res = edit_save_file (edit, edit->filename);
    
    /* Maintain modify (not save) lock on failure */
    if ((res && edit->locked) || save_lock)
	edit->locked = edit_unlock_file (edit->filename);
    
    /* On failure try 'save as', it does locking on its own */
    if (!res) 
	return edit_save_as_cmd (edit);
    edit->force |= REDRAW_COMPLETELY;
    edit->delete_file = 0;
    edit->modified = 0;

    return 1;
}


/* returns 1 on success */
int edit_new_cmd (WEdit * edit)
{
    if (edit->modified) {
	if (edit_query_dialog2 (_ ("Warning"), _ (" Current text was modified without a file save. \n Continue discards these changes. "), _ ("Continue"), _ ("Cancel"))) {
	    edit->force |= REDRAW_COMPLETELY;
	    return 0;
	}
    }
    edit->force |= REDRAW_COMPLETELY;

    if (edit->locked)
	edit->locked = edit_unlock_file (edit->filename);
    return edit_renew (edit);	/* if this gives an error, something has really screwed up */
}

/* returns 1 on error */
static int
edit_load_file_from_filename (WEdit * edit, char *exp)
{
    int prev_locked = edit->locked;
    char *prev_filename = g_strdup (edit->filename);
    
    if (!edit_reload (edit, exp)) {
	g_free (prev_filename);
	return 1;
    }

    if (prev_locked)
	edit_unlock_file (prev_filename);
    g_free (prev_filename);
    return 0;
}

int
edit_load_cmd (WEdit *edit)
{
    char *exp;

    if (edit->modified) {
	if (edit_query_dialog2
	    (_("Warning"),
	     _(" Current text was modified without a file save. \n"
	       " Continue discards these changes. "), _("Continue"),
	     _("Cancel"))) {
	    edit->force |= REDRAW_COMPLETELY;
	    return 0;
	}
    }

    exp = edit_get_load_file (edit->filename, _(" Load "));

    if (exp) {
	if (*exp)
	    edit_load_file_from_filename (edit, exp);
	g_free (exp);
    }
    edit->force |= REDRAW_COMPLETELY;
    return 0;
}

/*
   if mark2 is -1 then marking is from mark1 to the cursor.
   Otherwise its between the markers. This handles this.
   Returns 1 if no text is marked.
 */
int eval_marks (WEdit * edit, long *start_mark, long *end_mark)
{
    if (edit->mark1 != edit->mark2) {
	if (edit->mark2 >= 0) {
	    *start_mark = min (edit->mark1, edit->mark2);
	    *end_mark = max (edit->mark1, edit->mark2);
	} else {
	    *start_mark = min (edit->mark1, edit->curs1);
	    *end_mark = max (edit->mark1, edit->curs1);
	    edit->column2 = edit->curs_col;
	}
	return 0;
    } else {
	*start_mark = *end_mark = 0;
	edit->column2 = edit->column1 = 0;
	return 1;
    }
}

#define space_width 1

static void
edit_insert_column_of_text (WEdit * edit, unsigned char *data, int size, int width)
{
    long cursor;
    int i, col;
    cursor = edit->curs1;
    col = edit_get_col (edit);
    for (i = 0; i < size; i++) {
	if (data[i] == '\n') {	/* fill in and move to next line */
	    int l;
	    long p;
	    if (edit_get_byte (edit, edit->curs1) != '\n') {
		l = width - (edit_get_col (edit) - col);
		while (l > 0) {
		    edit_insert (edit, ' ');
		    l -= space_width;
		}
	    }
	    for (p = edit->curs1;; p++) {
		if (p == edit->last_byte) {
		    edit_cursor_move (edit, edit->last_byte - edit->curs1);
		    edit_insert_ahead (edit, '\n');
		    p++;
		    break;
		}
		if (edit_get_byte (edit, p) == '\n') {
		    p++;
		    break;
		}
	    }
	    edit_cursor_move (edit, edit_move_forward3 (edit, p, col, 0) - edit->curs1);
	    l = col - edit_get_col (edit);
	    while (l >= space_width) {
		edit_insert (edit, ' ');
		l -= space_width;
	    }
	    continue;
	}
	edit_insert (edit, data[i]);
    }
    edit_cursor_move (edit, cursor - edit->curs1);
}


void
edit_block_copy_cmd (WEdit *edit)
{
    long start_mark, end_mark, current = edit->curs1;
    int size, x;
    unsigned char *copy_buf;

    edit_update_curs_col (edit);
    x = edit->curs_col;
    if (eval_marks (edit, &start_mark, &end_mark))
	return;
    if (column_highlighting)
	if ((x >= edit->column1 && x < edit->column2)
	    || (x > edit->column2 && x <= edit->column1))
	    return;

    copy_buf = edit_get_block (edit, start_mark, end_mark, &size);

    /* all that gets pushed are deletes hence little space is used on the stack */

    edit_push_markers (edit);

    if (column_highlighting) {
	edit_insert_column_of_text (edit, copy_buf, size,
				    abs (edit->column2 - edit->column1));
    } else {
	while (size--)
	    edit_insert_ahead (edit, copy_buf[size]);
    }

    g_free (copy_buf);
    edit_scroll_screen_over_cursor (edit);

    if (column_highlighting) {
	edit_set_markers (edit, 0, 0, 0, 0);
	edit_push_action (edit, COLUMN_ON);
	column_highlighting = 0;
    } else if (start_mark < current && end_mark > current)
	edit_set_markers (edit, start_mark,
			  end_mark + end_mark - start_mark, 0, 0);

    edit->force |= REDRAW_PAGE;
}


void
edit_block_move_cmd (WEdit *edit)
{
    long count;
    long current;
    unsigned char *copy_buf;
    long start_mark, end_mark;
    int deleted = 0;
    int x = 0;

    if (eval_marks (edit, &start_mark, &end_mark))
	return;
    if (column_highlighting) {
	edit_update_curs_col (edit);
	x = edit->curs_col;
	if (start_mark <= edit->curs1 && end_mark >= edit->curs1)
	    if ((x > edit->column1 && x < edit->column2)
		|| (x > edit->column2 && x < edit->column1))
		return;
    } else if (start_mark <= edit->curs1 && end_mark >= edit->curs1)
	return;

    if ((end_mark - start_mark) > option_max_undo / 2)
	if (edit_query_dialog2
	    (_("Warning"),
	     _
	     (" Block is large, you may not be able to undo this action. "),
	     _("Continue"), _("Cancel")))
	    return;

    edit_push_markers (edit);
    current = edit->curs1;
    if (column_highlighting) {
	int size, c1, c2, line;
	line = edit->curs_line;
	if (edit->mark2 < 0)
	    edit_mark_cmd (edit, 0);
	c1 = min (edit->column1, edit->column2);
	c2 = max (edit->column1, edit->column2);
	copy_buf = edit_get_block (edit, start_mark, end_mark, &size);
	if (x < c2) {
	    edit_block_delete_cmd (edit);
	    deleted = 1;
	}
	edit_move_to_line (edit, line);
	edit_cursor_move (edit,
			  edit_move_forward3 (edit,
					      edit_bol (edit, edit->curs1),
					      x, 0) - edit->curs1);
	edit_insert_column_of_text (edit, copy_buf, size, c2 - c1);
	if (!deleted) {
	    line = edit->curs_line;
	    edit_update_curs_col (edit);
	    x = edit->curs_col;
	    edit_block_delete_cmd (edit);
	    edit_move_to_line (edit, line);
	    edit_cursor_move (edit,
			      edit_move_forward3 (edit,
						  edit_bol (edit,
							    edit->curs1),
						  x, 0) - edit->curs1);
	}
	edit_set_markers (edit, 0, 0, 0, 0);
	edit_push_action (edit, COLUMN_ON);
	column_highlighting = 0;
    } else {
	copy_buf = g_malloc (end_mark - start_mark);
	edit_cursor_move (edit, start_mark - edit->curs1);
	edit_scroll_screen_over_cursor (edit);
	count = start_mark;
	while (count < end_mark) {
	    copy_buf[end_mark - count - 1] = edit_delete (edit);
	    count++;
	}
	edit_scroll_screen_over_cursor (edit);
	edit_cursor_move (edit,
			  current - edit->curs1 -
			  (((current - edit->curs1) >
			    0) ? end_mark - start_mark : 0));
	edit_scroll_screen_over_cursor (edit);
	while (count-- > start_mark)
	    edit_insert_ahead (edit, copy_buf[end_mark - count - 1]);
	edit_set_markers (edit, edit->curs1,
			  edit->curs1 + end_mark - start_mark, 0, 0);
    }
    edit_scroll_screen_over_cursor (edit);
    g_free (copy_buf);
    edit->force |= REDRAW_PAGE;
}

static void
edit_delete_column_of_text (WEdit * edit)
{
    long p, q, r, m1, m2;
    int b, c, d;
    int n;

    eval_marks (edit, &m1, &m2);
    n = edit_move_forward (edit, m1, 0, m2) + 1;
    c = edit_move_forward3 (edit, edit_bol (edit, m1), 0, m1);
    d = edit_move_forward3 (edit, edit_bol (edit, m2), 0, m2);

    b = min (c, d);
    c = max (c, d);

    while (n--) {
	r = edit_bol (edit, edit->curs1);
	p = edit_move_forward3 (edit, r, b, 0);
	q = edit_move_forward3 (edit, r, c, 0);
	if (p < m1)
	    p = m1;
	if (q > m2)
	    q = m2;
	edit_cursor_move (edit, p - edit->curs1);
	while (q > p) {		/* delete line between margins */
	    if (edit_get_byte (edit, edit->curs1) != '\n')
		edit_delete (edit);
	    q--;
	}
	if (n)			/* move to next line except on the last delete */
	    edit_cursor_move (edit, edit_move_forward (edit, edit->curs1, 1, 0) - edit->curs1);
    }
}

/* if success return 0 */
int
edit_block_delete (WEdit *edit)
{
    long count;
    long start_mark, end_mark;
    if (eval_marks (edit, &start_mark, &end_mark))
	return 0;
    if (column_highlighting && edit->mark2 < 0)
	edit_mark_cmd (edit, 0);
    if ((end_mark - start_mark) > option_max_undo / 2) {
	/* Warning message with a query to continue or cancel the operation */
	if (edit_query_dialog2
	    (_("Warning"),
	     _
	     (" Block is large, you may not be able to undo this action. "),
	     _("Continue"), _("Cancel"))) {
	    return 1;
	}
    }
    edit_push_markers (edit);
    edit_cursor_move (edit, start_mark - edit->curs1);
    edit_scroll_screen_over_cursor (edit);
    count = start_mark;
    if (start_mark < end_mark) {
	if (column_highlighting) {
	    if (edit->mark2 < 0)
		edit_mark_cmd (edit, 0);
	    edit_delete_column_of_text (edit);
	} else {
	    while (count < end_mark) {
		edit_delete (edit);
		count++;
	    }
	}
    }
    edit_set_markers (edit, 0, 0, 0, 0);
    edit->force |= REDRAW_PAGE;
    return 0;
}

/* returns 1 if canceelled by user */
int edit_block_delete_cmd (WEdit * edit)
{
    long start_mark, end_mark;
    if (eval_marks (edit, &start_mark, &end_mark)) {
	edit_delete_line (edit);
	return 0;
    }
    return edit_block_delete (edit);
}


#define INPUT_INDEX 9
#define SEARCH_DLG_WIDTH 58
#define SEARCH_DLG_HEIGHT 10
#define REPLACE_DLG_WIDTH 58
#define REPLACE_DLG_HEIGHT 15
#define CONFIRM_DLG_WIDTH 79
#define CONFIRM_DLG_HEIGTH 6
#define B_REPLACE_ALL B_USER+1
#define B_REPLACE_ONE B_USER+2
#define B_SKIP_REPLACE B_USER+3

static int
edit_replace_prompt (WEdit * edit, char *replace_text, int xpos, int ypos)
{
    QuickWidget quick_widgets[] =
    {
	{quick_button, 63, CONFIRM_DLG_WIDTH, 3, CONFIRM_DLG_HEIGTH, N_ ("&Cancel"),
	 0, B_CANCEL, 0, 0, NULL},
	{quick_button, 50, CONFIRM_DLG_WIDTH, 3, CONFIRM_DLG_HEIGTH, N_ ("O&ne"),
	 0, B_REPLACE_ONE, 0, 0, NULL},
	{quick_button, 37, CONFIRM_DLG_WIDTH, 3, CONFIRM_DLG_HEIGTH, N_ ("A&ll"),
	 0, B_REPLACE_ALL, 0, 0, NULL},
	{quick_button, 21, CONFIRM_DLG_WIDTH, 3, CONFIRM_DLG_HEIGTH, N_ ("&Skip"),
	 0, B_SKIP_REPLACE, 0, 0, NULL},
	{quick_button, 4, CONFIRM_DLG_WIDTH, 3, CONFIRM_DLG_HEIGTH, N_ ("&Replace"),
	 0, B_ENTER, 0, 0, NULL},
	{quick_label, 2, CONFIRM_DLG_WIDTH, 2, CONFIRM_DLG_HEIGTH, 0,
	 0, 0, 0, 0, 0},
	{0}};

#ifdef HAVE_CHARSET
    char *msg = _(" Replace with: ");

    quick_widgets[5].text = catstrs (msg, replace_text, 0);

    if (*replace_text)
	convert_to_display (quick_widgets[5].text + strlen (msg));
#else
    quick_widgets[5].text = catstrs (_ (" Replace with: "), replace_text, 0);
#endif /* !HAVE_CHARSET */

    {
	QuickDialog Quick_input =
	{CONFIRM_DLG_WIDTH, CONFIRM_DLG_HEIGTH, 0, 0, N_ (" Confirm replace "),
	 "[Input Line Keys]", 0 /*quick_widgets */ };

	Quick_input.widgets = quick_widgets;

	Quick_input.xpos = xpos;

	/* Sometimes menu can hide replaced text. I don't like it */

	if ((edit->curs_row >= ypos - 1) && (edit->curs_row <= ypos + CONFIRM_DLG_HEIGTH - 1))
	    ypos -= CONFIRM_DLG_HEIGTH;

	Quick_input.ypos = ypos;
	return quick_dialog (&Quick_input);
    }
}

static void
edit_replace_dialog (WEdit * edit, char **search_text, char **replace_text, char **arg_order)
{
    int treplace_scanf = replace_scanf;
    int treplace_regexp = replace_regexp;
    int treplace_all = replace_all;
    int treplace_prompt = replace_prompt;
    int treplace_backwards = replace_backwards;
    int treplace_whole = replace_whole;
    int treplace_case = replace_case;

    QuickWidget quick_widgets[] =
    {
	{quick_button, 6, 10, 12, REPLACE_DLG_HEIGHT, N_("&Cancel"), 0, B_CANCEL, 0,
	 0, NULL},
	{quick_button, 2, 10, 12, REPLACE_DLG_HEIGHT, N_("&OK"), 0, B_ENTER, 0,
	 0, NULL},
	{quick_checkbox, 33, REPLACE_DLG_WIDTH, 11, REPLACE_DLG_HEIGHT, N_("scanf &Expression"), 0, 0,
	 0, 0, NULL},
	{quick_checkbox, 33, REPLACE_DLG_WIDTH, 10, REPLACE_DLG_HEIGHT, N_("replace &All"), 0, 0,
	 0, 0, NULL},
	{quick_checkbox, 33, REPLACE_DLG_WIDTH, 9, REPLACE_DLG_HEIGHT, N_("pr&Ompt on replace"), 0, 0,
	 0, 0, NULL},
	{quick_checkbox, 4, REPLACE_DLG_WIDTH, 11, REPLACE_DLG_HEIGHT, N_("&Backwards"), 0, 0,
	 0, 0, NULL},
	{quick_checkbox, 4, REPLACE_DLG_WIDTH, 10, REPLACE_DLG_HEIGHT, N_("&Regular expression"), 0, 0,
	 0, 0, NULL},
	{quick_checkbox, 4, REPLACE_DLG_WIDTH, 9, REPLACE_DLG_HEIGHT, N_("&Whole words only"), 0, 0,
	 0, 0, NULL},
	{quick_checkbox, 4, REPLACE_DLG_WIDTH, 8, REPLACE_DLG_HEIGHT, N_("case &Sensitive"), 0, 0,
	 0, 0, NULL},
	{quick_input,    3, REPLACE_DLG_WIDTH, 7, REPLACE_DLG_HEIGHT, "", 52, 0, 0,
	 0, "edit-argord"},
	{quick_label, 2, REPLACE_DLG_WIDTH, 6, REPLACE_DLG_HEIGHT, N_(" Enter replacement argument order eg. 3,2,1,4 "), 0, 0,
	 0, 0, 0},
	{quick_input, 3, REPLACE_DLG_WIDTH, 5, REPLACE_DLG_HEIGHT, "", 52, 0, 0,
	 0, "edit-replace"},
	{quick_label, 2, REPLACE_DLG_WIDTH, 4, REPLACE_DLG_HEIGHT, N_(" Enter replacement string:"), 0, 0, 0,
	 0, 0},
	{quick_input, 3, REPLACE_DLG_WIDTH, 3, REPLACE_DLG_HEIGHT, "", 52, 0, 0,
	 0, "edit-search"},
	{quick_label, 2, REPLACE_DLG_WIDTH, 2, REPLACE_DLG_HEIGHT, N_(" Enter search string:"), 0, 0, 0,
	 0, 0},
	{0}};

    quick_widgets[2].result = &treplace_scanf;
    quick_widgets[3].result = &treplace_all;
    quick_widgets[4].result = &treplace_prompt;
    quick_widgets[5].result = &treplace_backwards;
    quick_widgets[6].result = &treplace_regexp;
    quick_widgets[7].result = &treplace_whole;
    quick_widgets[8].result = &treplace_case;
    quick_widgets[9].str_result = arg_order;
    quick_widgets[9].text = *arg_order;
    quick_widgets[11].str_result = replace_text;
    quick_widgets[11].text = *replace_text;
    quick_widgets[13].str_result = search_text;
    quick_widgets[13].text = *search_text;
    {
	QuickDialog Quick_input =
	{REPLACE_DLG_WIDTH, REPLACE_DLG_HEIGHT, -1, 0, N_(" Replace "),
	 "[Input Line Keys]", 0 /*quick_widgets */ };

	Quick_input.widgets = quick_widgets;

	if (quick_dialog (&Quick_input) != B_CANCEL) {
	    replace_scanf = treplace_scanf;
	    replace_backwards = treplace_backwards;
	    replace_regexp = treplace_regexp;
	    replace_all = treplace_all;
	    replace_prompt = treplace_prompt;
	    replace_whole = treplace_whole;
	    replace_case = treplace_case;
	    return;
	} else {
	    *arg_order = NULL;
	    *replace_text = NULL;
	    *search_text = NULL;
	    return;
	}
    }
}


static void
edit_search_dialog (WEdit * edit, char **search_text)
{
    int treplace_scanf = replace_scanf;
    int treplace_regexp = replace_regexp;
    int treplace_whole = replace_whole;
    int treplace_case = replace_case;
    int treplace_backwards = replace_backwards;

    QuickWidget quick_widgets[] =
    {
	{quick_button, 6, 10, 7, SEARCH_DLG_HEIGHT, N_("&Cancel"), 0, B_CANCEL, 0,
	 0, NULL},
	{quick_button, 2, 10, 7, SEARCH_DLG_HEIGHT, N_("&OK"), 0, B_ENTER, 0,
	 0, NULL},
	{quick_checkbox, 33, SEARCH_DLG_WIDTH, 6, SEARCH_DLG_HEIGHT, N_("scanf &Expression"), 0, 0,
	 0, 0, NULL },
	{quick_checkbox, 33, SEARCH_DLG_WIDTH, 5, SEARCH_DLG_HEIGHT, N_("&Backwards"), 0, 0,
	 0, 0, NULL},
	{quick_checkbox, 4, SEARCH_DLG_WIDTH, 6, SEARCH_DLG_HEIGHT, N_("&Regular expression"), 0, 0,
	 0, 0, NULL},
	{quick_checkbox, 4, SEARCH_DLG_WIDTH, 5, SEARCH_DLG_HEIGHT, N_("&Whole words only"), 0, 0,
	 0, 0, NULL},
	{quick_checkbox, 4, SEARCH_DLG_WIDTH, 4, SEARCH_DLG_HEIGHT, N_("case &Sensitive"), 0, 0,
	 0, 0, NULL},
	{quick_input, 3, SEARCH_DLG_WIDTH, 3, SEARCH_DLG_HEIGHT, "", 52, 0, 0,
	 0, "edit-search"},
	{quick_label, 2, SEARCH_DLG_WIDTH, 2, SEARCH_DLG_HEIGHT, N_(" Enter search string:"), 0, 0, 0,
	 0, 0},
	{0}};

    quick_widgets[2].result = &treplace_scanf;
    quick_widgets[3].result = &treplace_backwards;
    quick_widgets[4].result = &treplace_regexp;
    quick_widgets[5].result = &treplace_whole;
    quick_widgets[6].result = &treplace_case;
    quick_widgets[7].str_result = search_text;
    quick_widgets[7].text = *search_text;

    {
	QuickDialog Quick_input =
	{SEARCH_DLG_WIDTH, SEARCH_DLG_HEIGHT, -1, 0, N_("Search"),
	 "[Input Line Keys]", 0 /*quick_widgets */ };

	Quick_input.widgets = quick_widgets;

	if (quick_dialog (&Quick_input) != B_CANCEL) {
	    replace_scanf = treplace_scanf;
	    replace_backwards = treplace_backwards;
	    replace_regexp = treplace_regexp;
	    replace_whole = treplace_whole;
	    replace_case = treplace_case;
	} else {
	    *search_text = NULL;
	}
    }
}


static long sargs[NUM_REPL_ARGS][256 / sizeof (long)];

#define SCANF_ARGS sargs[0], sargs[1], sargs[2], sargs[3], \
		     sargs[4], sargs[5], sargs[6], sargs[7], \
		     sargs[8], sargs[9], sargs[10], sargs[11], \
		     sargs[12], sargs[13], sargs[14], sargs[15]

#define PRINTF_ARGS sargs[argord[0]], sargs[argord[1]], sargs[argord[2]], sargs[argord[3]], \
		     sargs[argord[4]], sargs[argord[5]], sargs[argord[6]], sargs[argord[7]], \
		     sargs[argord[8]], sargs[argord[9]], sargs[argord[10]], sargs[argord[11]], \
		     sargs[argord[12]], sargs[argord[13]], sargs[argord[14]], sargs[argord[15]]


/* This function is a modification of mc-3.2.10/src/view.c:regexp_view_search() */
/* returns -3 on error in pattern, -1 on not found, found_len = 0 if either */
static int
string_regexp_search (char *pattern, char *string, int len, int match_type,
		      int match_bol, int icase, int *found_len, void *d)
{
    static regex_t r;
    static char *old_pattern = NULL;
    static int old_type, old_icase;
    regmatch_t *pmatch;
    static regmatch_t s[1];

    pmatch = (regmatch_t *) d;
    if (!pmatch)
	pmatch = s;

    if (!old_pattern || strcmp (old_pattern, pattern)
	|| old_type != match_type || old_icase != icase) {
	if (old_pattern) {
	    regfree (&r);
	    g_free (old_pattern);
	    old_pattern = 0;
	}
	if (regcomp (&r, pattern, REG_EXTENDED | (icase ? REG_ICASE : 0))) {
	    *found_len = 0;
	    return -3;
	}
	old_pattern = g_strdup (pattern);
	old_type = match_type;
	old_icase = icase;
    }
    if (regexec
	(&r, string, d ? NUM_REPL_ARGS : 1, pmatch,
	 ((match_bol
	   || match_type != match_normal) ? 0 : REG_NOTBOL)) != 0) {
	*found_len = 0;
	return -1;
    }
    *found_len = pmatch[0].rm_eo - pmatch[0].rm_so;
    return (pmatch[0].rm_so);
}

/* thanks to  Liviu Daia <daia@stoilow.imar.ro>  for getting this
   (and the above) routines to work properly - paul */

static long
edit_find_string (long start, unsigned char *exp, int *len, long last_byte, int (*get_byte) (void *, long), void *data, int once_only, void *d)
{
    long p, q = 0;
    long l = strlen ((char *) exp), f = 0;
    int n = 0;

    for (p = 0; p < l; p++)	/* count conversions... */
	if (exp[p] == '%')
	    if (exp[++p] != '%')	/* ...except for "%%" */
		n++;

    if (replace_scanf || replace_regexp) {
	int c;
	unsigned char *buf;
	unsigned char mbuf[MAX_REPL_LEN * 2 + 3];

	replace_scanf = (!replace_regexp);	/* can't have both */

	buf = mbuf;

	if (replace_scanf) {
	    unsigned char e[MAX_REPL_LEN];
	    if (n >= NUM_REPL_ARGS)
		return -3;

	    if (replace_case) {
		for (p = start; p < last_byte && p < start + MAX_REPL_LEN; p++)
		    buf[p - start] = (*get_byte) (data, p);
	    } else {
		for (p = 0; exp[p] != 0; p++)
		    exp[p] = my_lower_case (exp[p]);
		for (p = start; p < last_byte && p < start + MAX_REPL_LEN; p++) {
		    c = (*get_byte) (data, p);
		    buf[p - start] = my_lower_case (c);
		}
	    }

	    buf[(q = p - start)] = 0;
	    strcpy ((char *) e, (char *) exp);
	    strcat ((char *) e, "%n");
	    exp = e;

	    while (q) {
		*((int *) sargs[n]) = 0;	/* --> here was the problem - now fixed: good */
		if (n == sscanf ((char *) buf, (char *) exp, SCANF_ARGS)) {
		    if (*((int *) sargs[n])) {
			*len = *((int *) sargs[n]);
			return start;
		    }
		}
		if (once_only)
		    return -2;
		if (q + start < last_byte) {
		    if (replace_case) {
			buf[q] = (*get_byte) (data, q + start);
		    } else {
			c = (*get_byte) (data, q + start);
			buf[q] = my_lower_case (c);
		    }
		    q++;
		}
		buf[q] = 0;
		start++;
		buf++;		/* move the window along */
		if (buf == mbuf + MAX_REPL_LEN) {	/* the window is about to go past the end of array, so... */
		    memmove (mbuf, buf, strlen ((char *) buf) + 1);	/* reset it */
		    buf = mbuf;
		}
		q--;
	    }
	} else {	/* regexp matching */
	    long offset = 0;
	    int found_start, match_bol, move_win = 0; 

	    while (start + offset < last_byte) {
		match_bol = (offset == 0 || (*get_byte) (data, start + offset - 1) == '\n');
		if (!move_win) {
		    p = start + offset;
		    q = 0;
		}
		for (; p < last_byte && q < MAX_REPL_LEN; p++, q++) {
		    mbuf[q] = (*get_byte) (data, p);
		    if (mbuf[q] == '\n')
			break;
		}
		q++;
		offset += q;
		mbuf[q] = 0;

		buf = mbuf;
		while (q) {
		    found_start = string_regexp_search ((char *) exp, (char *) buf, q, match_normal, match_bol, !replace_case, len, d);

		    if (found_start <= -2) {	/* regcomp/regexec error */
			*len = 0;
			return -3;
		    }
		    else if (found_start == -1)	/* not found: try next line */
			break;
		    else if (*len == 0) { /* null pattern: try again at next character */
			q--;
			buf++;
			match_bol = 0;
			continue;
		    }
		    else	/* found */
			return (start + offset - q + found_start);
		}
		if (once_only)
		    return -2;

		if (buf[q - 1] != '\n') { /* incomplete line: try to recover */
		    buf = mbuf + MAX_REPL_LEN / 2;
		    q = strlen ((char *) buf);
		    memmove (mbuf, buf, q);
		    p = start + q;
		    move_win = 1;
		}
		else
		    move_win = 0;
	    }
	}
    } else {
 	*len = strlen ((char *) exp);
	if (replace_case) {
	    for (p = start; p <= last_byte - l; p++) {
 		if ((*get_byte) (data, p) == (unsigned char)exp[0]) {	/* check if first char matches */
		    for (f = 0, q = 0; q < l && f < 1; q++)
 			if ((*get_byte) (data, q + p) != (unsigned char)exp[q])
			    f = 1;
		    if (f == 0)
			return p;
		}
		if (once_only)
		    return -2;
	    }
	} else {
	    for (p = 0; exp[p] != 0; p++)
		exp[p] = my_lower_case (exp[p]);

	    for (p = start; p <= last_byte - l; p++) {
		if (my_lower_case ((*get_byte) (data, p)) == (unsigned char)exp[0]) {
		    for (f = 0, q = 0; q < l && f < 1; q++)
			if (my_lower_case ((*get_byte) (data, q + p)) != (unsigned char)exp[q])
			    f = 1;
		    if (f == 0)
			return p;
		}
		if (once_only)
		    return -2;
	    }
	}
    }
    return -2;
}


static long
edit_find_forwards (long search_start, unsigned char *exp, int *len, long last_byte, int (*get_byte) (void *, long), void *data, int once_only, void *d)
{				/*front end to find_string to check for
				   whole words */
    long p;
    p = search_start;

    while ((p = edit_find_string (p, exp, len, last_byte, get_byte, data, once_only, d)) >= 0) {
	if (replace_whole) {
/*If the bordering chars are not in option_whole_chars_search then word is whole */
	    if (!strcasechr (option_whole_chars_search, (*get_byte) (data, p - 1))
		&& !strcasechr (option_whole_chars_search, (*get_byte) (data, p + *len)))
		return p;
	    if (once_only)
		return -2;
	} else
	    return p;
	if (once_only)
	    break;
	p++;			/*not a whole word so continue search. */
    }
    return p;
}

static long
edit_find (long search_start, unsigned char *exp, int *len, long last_byte, int (*get_byte) (void *, long), void *data, void *d)
{
    long p;
    if (replace_backwards) {
	while (search_start >= 0) {
	    p = edit_find_forwards (search_start, exp, len, last_byte, get_byte, data, 1, d);
	    if (p == search_start)
		return p;
	    search_start--;
	}
    } else {
	return edit_find_forwards (search_start, exp, len, last_byte, get_byte, data, 0, d);
    }
    return -2;
}

#define is_digit(x) ((x) >= '0' && (x) <= '9')

#define snprintf(v) { \
		*p1++ = *p++; \
		*p1++ = '%'; \
		*p1++ = 'n'; \
		*p1 = '\0'; \
		sprintf(s,q1,v,&n); \
		s += n; \
	    }

/* this function uses the sprintf command to do a vprintf */
/* it takes pointers to arguments instead of the arguments themselves */
static int sprintf_p (char *str, const char *fmt,...)
    __attribute__ ((format (printf, 2, 3)));

static int sprintf_p (char *str, const char *fmt,...)
{
    va_list ap;
    int n;
    char *q, *p, *s = str;
    char q1[32];
    char *p1;

    va_start (ap, fmt);
    p = q = (char *) fmt;

    while ((p = strchr (p, '%'))) {
	n = p - q;
	strncpy (s, q, n);	/* copy stuff between format specifiers */
	s += n;
	*s = 0;
	q = p;
	p1 = q1;
	*p1++ = *p++;
	if (*p == '%') {
	    p++;
	    *s++ = '%';
	    q = p;
	    continue;
	}
	if (*p == 'n') {
	    p++;
/* do nothing */
	    q = p;
	    continue;
	}
	if (*p == '#')
	    *p1++ = *p++;
	if (*p == '0')
	    *p1++ = *p++;
	if (*p == '-')
	    *p1++ = *p++;
	if (*p == '+')
	    *p1++ = *p++;
	if (*p == '*') {
	    p++;
	    strcpy (p1, MY_itoa (*va_arg (ap, int *)));	/* replace field width with a number */
	    p1 += strlen (p1);
	} else {
	    while (is_digit (*p))
		*p1++ = *p++;
	}
	if (*p == '.')
	    *p1++ = *p++;
	if (*p == '*') {
	    p++;
	    strcpy (p1, MY_itoa (*va_arg (ap, int *)));	/* replace precision with a number */
	    p1 += strlen (p1);
	} else {
	    while (is_digit (*p))
		*p1++ = *p++;
	}
/* flags done, now get argument */
	if (*p == 's') {
	    snprintf (va_arg (ap, char *));
	} else if (*p == 'h') {
	    if (strchr ("diouxX", *p))
		snprintf (*va_arg (ap, short *));
	} else if (*p == 'l') {
	    *p1++ = *p++;
	    if (strchr ("diouxX", *p))
		snprintf (*va_arg (ap, long *));
	} else if (strchr ("cdiouxX", *p)) {
	    snprintf (*va_arg (ap, int *));
	} else if (*p == 'L') {
	    *p1++ = *p++;
	    if (strchr ("EefgG", *p))
		snprintf (*va_arg (ap, double *));	/* should be long double */
	} else if (strchr ("EefgG", *p)) {
	    snprintf (*va_arg (ap, double *));
	} else if (strchr ("DOU", *p)) {
	    snprintf (*va_arg (ap, long *));
	} else if (*p == 'p') {
	    snprintf (*va_arg (ap, void **));
	}
	q = p;
    }
    va_end (ap);
    sprintf (s, q);		/* print trailing leftover */
    return s - str + strlen (s);
}

static void regexp_error (WEdit *edit)
{
/* "Error: Syntax error in regular expression, or scanf expression contained too many %'s */
    edit_error_dialog (_("Error"), _(" Invalid regular expression, or scanf expression with to many conversions "));
}

/* call with edit = 0 before shutdown to close memory leaks */
void
edit_replace_cmd (WEdit *edit, int again)
{
    static regmatch_t pmatch[NUM_REPL_ARGS];
    static char *old1 = NULL;
    static char *old2 = NULL;
    static char *old3 = NULL;
    char *exp1 = "";
    char *exp2 = "";
    char *exp3 = "";
    int replace_yes;
    int replace_continue;
    int treplace_prompt = 0;
    int i = 0;
    long times_replaced = 0, last_search;
    int argord[NUM_REPL_ARGS];

    if (!edit) {
	if (old1) {
	    g_free (old1);
	    old1 = 0;
	}
	if (old2) {
	    g_free (old2);
	    old2 = 0;
	}
	if (old3) {
	    g_free (old3);
	    old3 = 0;
	}
	return;
    }
    last_search = edit->last_byte;

    edit->force |= REDRAW_COMPLETELY;

    exp1 = old1 ? old1 : exp1;
    exp2 = old2 ? old2 : exp2;
    exp3 = old3 ? old3 : exp3;

    if (again) {
	if (!old1 || !old2)
	    return;
	exp1 = g_strdup (old1);
	exp2 = g_strdup (old2);
	exp3 = g_strdup (old3);
    } else {
	edit_push_action (edit, KEY_PRESS + edit->start_display);

	convert_to_display (exp1);
	convert_to_display (exp2);

	edit_replace_dialog (edit, &exp1, &exp2, &exp3);

	convert_from_input (exp1);
	convert_from_input (exp2);

	treplace_prompt = replace_prompt;
    }

    if (!exp1 || !*exp1) {
	edit->force = REDRAW_COMPLETELY;
	g_free (exp1);
	g_free (exp2);
	g_free (exp3);
	return;
    }
    g_free (old1);
    g_free (old2);
    g_free (old3);
    old1 = g_strdup (exp1);
    old2 = g_strdup (exp2);
    old3 = g_strdup (exp3);

    {
	char *s;
	int ord;
	while ((s = strchr (exp3, ' ')))
	    memmove (s, s + 1, strlen (s));
	s = exp3;
	for (i = 0; i < NUM_REPL_ARGS; i++) {
	    if (s != (char *) 1 && *s) {
		ord = atoi (s);
		if ((ord > 0) && (ord < NUM_REPL_ARGS))
		    argord[i] = ord - 1;
		else
		    argord[i] = i;
		s = strchr (s, ',') + 1;
	    } else
		argord[i] = i;
	}
    }

    replace_continue = replace_all;

    if (edit->found_len && edit->search_start == edit->found_start + 1
	&& replace_backwards)
	edit->search_start--;

    if (edit->found_len && edit->search_start == edit->found_start - 1
	&& !replace_backwards)
	edit->search_start++;

    do {
	int len = 0;
	long new_start;
	new_start =
	    edit_find (edit->search_start, (unsigned char *) exp1, &len,
		       last_search, (int (*)(void *, long)) edit_get_byte,
		       (void *) edit, pmatch);
	if (new_start == -3) {
	    regexp_error (edit);
	    break;
	}
	edit->search_start = new_start;
	/*returns negative on not found or error in pattern */

	if (edit->search_start >= 0) {
	    edit->found_start = edit->search_start;
	    i = edit->found_len = len;

	    edit_cursor_move (edit, edit->search_start - edit->curs1);
	    edit_scroll_screen_over_cursor (edit);

	    replace_yes = 1;

	    if (treplace_prompt) {
		int l;
		l = edit->curs_row - edit->num_widget_lines / 3;
		if (l > 0)
		    edit_scroll_downward (edit, l);
		if (l < 0)
		    edit_scroll_upward (edit, -l);

		edit_scroll_screen_over_cursor (edit);
		edit->force |= REDRAW_PAGE;
		edit_render_keypress (edit);

		/*so that undo stops at each query */
		edit_push_key_press (edit);

		switch (edit_replace_prompt (edit, exp2,	/* and prompt 2/3 down */
					     (edit->num_widget_columns -
					      CONFIRM_DLG_WIDTH) / 2,
					     edit->num_widget_lines * 2 /
					     3)) {
		case B_ENTER:
		    break;
		case B_SKIP_REPLACE:
		    replace_yes = 0;
		    break;
		case B_REPLACE_ALL:
		    treplace_prompt = 0;
		    replace_continue = 1;
		    break;
		case B_REPLACE_ONE:
		    replace_continue = 0;
		    break;
		case B_CANCEL:
		    replace_yes = 0;
		    replace_continue = 0;
		    break;
		}
	    }
	    if (replace_yes) {	/* delete then insert new */
		if (replace_scanf || replace_regexp) {
		    char repl_str[MAX_REPL_LEN + 2];

		    /* we need to fill in sargs just like with scanf */
		    if (replace_regexp) {
			int k, j;
			for (k = 1;
			     k < NUM_REPL_ARGS && pmatch[k].rm_eo >= 0;
			     k++) {
			    unsigned char *t;
			    t = (unsigned char *) &sargs[k - 1][0];
			    for (j = 0;
				 j < pmatch[k].rm_eo - pmatch[k].rm_so
				 && j < 255; j++, t++)
				*t = (unsigned char) edit_get_byte (edit,
								    edit->
								    search_start
								    -
								    pmatch
								    [0].
								    rm_so +
								    pmatch
								    [k].
								    rm_so +
								    j);
			    *t = '\0';
			}
			for (; k <= NUM_REPL_ARGS; k++)
			    sargs[k - 1][0] = 0;
		    }
		    if (sprintf_p (repl_str, exp2, PRINTF_ARGS) >= 0) {
			times_replaced++;
			while (i--)
			    edit_delete (edit);
			while (repl_str[++i])
			    edit_insert (edit, repl_str[i]);
		    } else {
			edit_error_dialog (_(" Replace "),
					   _
					   (" Error in replacement format string. "));
			replace_continue = 0;
		    }
		} else {
		    times_replaced++;
		    while (i--)
			edit_delete (edit);
		    while (exp2[++i])
			edit_insert (edit, exp2[i]);
		}
		edit->found_len = i;
	    }
	    /* so that we don't find the same string again */
	    if (replace_backwards) {
		last_search = edit->search_start;
		edit->search_start--;
	    } else {
		edit->search_start += i;
		last_search = edit->last_byte;
	    }
	    edit_scroll_screen_over_cursor (edit);
	} else {
	    char *msg = _(" Replace ");
	    /* try and find from right here for next search */
	    edit->search_start = edit->curs1;
	    edit_update_curs_col (edit);

	    edit->force |= REDRAW_PAGE;
	    edit_render_keypress (edit);
	    if (times_replaced) {
		message (0, msg, _(" %ld replacements made. "),
			 times_replaced);
	    } else
		edit_message_dialog (msg, _(" Search string not found "));
	    replace_continue = 0;
	}
    } while (replace_continue);

    g_free (exp1);
    g_free (exp2);
    g_free (exp3);
    edit->force = REDRAW_COMPLETELY;
    edit_scroll_screen_over_cursor (edit);
}




void edit_search_cmd (WEdit * edit, int again)
{
    static char *old = NULL;
    char *exp = "";

    if (!edit) {
	if (old) {
	    g_free (old);
	    old = 0;
	}
	return;
    }
    exp = old ? old : exp;
    if (again) {		/*ctrl-hotkey for search again. */
	if (!old)
	    return;
	exp = g_strdup (old);
    } else {

#ifdef HAVE_CHARSET
	if (exp && *exp)
	    convert_to_display (exp);
#endif /* HAVE_CHARSET */

	edit_search_dialog (edit, &exp);

#ifdef HAVE_CHARSET
	if (exp && *exp)
	    convert_from_input (exp);
#endif /* HAVE_CHARSET */

	edit_push_action (edit, KEY_PRESS + edit->start_display);
    }

    if (exp) {
	if (*exp) {
	    int len = 0;
	    if (old)
		g_free (old);
	    old = g_strdup (exp);

	    if (search_create_bookmark) {
		int found = 0, books = 0;
		int l = 0, l_last = -1;
		long p, q = 0;
		for (;;) {
		    p = edit_find (q, (unsigned char *) exp, &len, edit->last_byte,
				   (int (*)(void *, long)) edit_get_byte, (void *) edit, 0);
		    if (p < 0)
			break;
		    found++;
		    l += edit_count_lines (edit, q, p);
		    if (l != l_last) {
			book_mark_insert (edit, l, BOOK_MARK_FOUND_COLOR);
			books++;
		    }
		    l_last = l;
		    q = p + 1;
		}
		if (found) {
/* in response to number of bookmarks added because of string being found %d times */
		    message (0, _("Search"), _(" %d finds made, %d bookmarks added "), found, books);
		} else {
		    edit_error_dialog (_ ("Search"), _ (" Search string not found "));
		}
	    } else {

		if (edit->found_len && edit->search_start == edit->found_start + 1 && replace_backwards)
		    edit->search_start--;

		if (edit->found_len && edit->search_start == edit->found_start - 1 && !replace_backwards)
		    edit->search_start++;

		edit->search_start = edit_find (edit->search_start, (unsigned char *) exp, &len, edit->last_byte,
		(int (*)(void *, long)) edit_get_byte, (void *) edit, 0);

		if (edit->search_start >= 0) {
		    edit->found_start = edit->search_start;
		    edit->found_len = len;

		    edit_cursor_move (edit, edit->search_start - edit->curs1);
		    edit_scroll_screen_over_cursor (edit);
		    if (replace_backwards)
			edit->search_start--;
		    else
			edit->search_start++;
		} else if (edit->search_start == -3) {
		    edit->search_start = edit->curs1;
		    regexp_error (edit);
		} else {
		    edit->search_start = edit->curs1;
		    edit_error_dialog (_ ("Search"), _ (" Search string not found "));
		}
	    }
	}
	g_free (exp);
    }
    edit->force |= REDRAW_COMPLETELY;
    edit_scroll_screen_over_cursor (edit);
}


/* Real edit only */
void edit_quit_cmd (WEdit * edit)
{
    if (edit->modified) {
	switch (edit_query_dialog3 (_ ("Quit"), _ (" File was modified, Save with exit? "), _ ("Cancel quit"), _ ("&Yes"), _ ("&No"))) {
	case 1:
	    edit_push_markers (edit);
	    edit_set_markers (edit, 0, 0, 0, 0);
	    if (!edit_save_cmd (edit))
		return;
	    break;
	case 2:
	    if (edit->locked)
		edit->locked = edit_unlock_file (edit->filename);
	    break;
	case 0:
	case -1:
	    return;
	}
    }
    dlg_stop (edit->widget.parent);
}

#define TEMP_BUF_LEN 1024

/* Return a null terminated length of text. Result must be g_free'd */
unsigned char *
edit_get_block (WEdit *edit, long start, long finish, int *l)
{
    unsigned char *s, *r;
    r = s = g_malloc (finish - start + 1);
    if (column_highlighting) {
	*l = 0;
	/* copy from buffer, excluding chars that are out of the column 'margins' */
	while (start < finish) {
	    int c, x;
	    x = edit_move_forward3 (edit, edit_bol (edit, start), 0,
				    start);
	    c = edit_get_byte (edit, start);
	    if ((x >= edit->column1 && x < edit->column2)
		|| (x >= edit->column2 && x < edit->column1) || c == '\n') {
		*s++ = c;
		(*l)++;
	    }
	    start++;
	}
    } else {
	*l = finish - start;
	while (start < finish)
	    *s++ = edit_get_byte (edit, start++);
    }
    *s = 0;
    return r;
}

/* save block, returns 1 on success */
int
edit_save_block (WEdit * edit, const char *filename, long start,
		 long finish)
{
    int len, file;

    if ((file =
	 mc_open (filename, O_CREAT | O_WRONLY | O_TRUNC,
		  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | O_BINARY)) == -1)
	return 0;

    if (column_highlighting) {
	unsigned char *block, *p;
	int r;
	p = block = edit_get_block (edit, start, finish, &len);
	while (len) {
	    r = mc_write (file, p, len);
	    if (r < 0)
		break;
	    p += r;
	    len -= r;
	}
	g_free (block);
    } else {
	unsigned char *buf;
	int i = start, end;
	len = finish - start;
	buf = g_malloc (TEMP_BUF_LEN);
	while (start != finish) {
	    end = min (finish, start + TEMP_BUF_LEN);
	    for (; i < end; i++)
		buf[i - start] = edit_get_byte (edit, i);
	    len -= mc_write (file, (char *) buf, end - start);
	    start = end;
	}
	g_free (buf);
    }
    mc_close (file);
    if (len)
	return 0;
    return 1;
}

/* copies a block to clipboard file */
static int edit_save_block_to_clip_file (WEdit * edit, long start, long finish)
{
    return edit_save_block (edit, catstrs (home_dir, CLIP_FILE, 0), start, finish);
}


void edit_paste_from_history (WEdit *edit)
{
}

int edit_copy_to_X_buf_cmd (WEdit * edit)
{
    long start_mark, end_mark;
    if (eval_marks (edit, &start_mark, &end_mark))
	return 0;
    if (!edit_save_block_to_clip_file (edit, start_mark, end_mark)) {
	edit_error_dialog (_(" Copy to clipboard "), get_sys_error (_(" Unable to save to file. ")));
	return 1;
    }
    edit_mark_cmd (edit, 1);
    return 0;
}

int edit_cut_to_X_buf_cmd (WEdit * edit)
{
    long start_mark, end_mark;
    if (eval_marks (edit, &start_mark, &end_mark))
	return 0;
    if (!edit_save_block_to_clip_file (edit, start_mark, end_mark)) {
	edit_error_dialog (_(" Cut to clipboard "), _(" Unable to save to file. "));
	return 1;
    }
    edit_block_delete_cmd (edit);
    edit_mark_cmd (edit, 1);
    return 0;
}

void edit_paste_from_X_buf_cmd (WEdit * edit)
{
    edit_insert_file (edit, catstrs (home_dir, CLIP_FILE, 0));
}


/*
 * Ask user for the line and go to that line.
 * Negative numbers mean line from the end (i.e. -1 is the last line).
 */
void
edit_goto_cmd (WEdit *edit)
{
    char *f;
    static long line = 0;	/* line as typed, saved as default */
    long l;
    char *error;
    char s[32];

    g_snprintf (s, sizeof (s), "%ld", line);
    f = input_dialog (_(" Goto line "), _(" Enter line: "), line ? s : "");
    if (!f)
	return;

    if (!*f) {
	g_free (f);
	return;
    }

    l = strtol (f, &error, 0);
    if (*error) {
	g_free (f);
	return;
    }

    line = l;
    if (l < 0)
	l = edit->total_lines + l + 2;
    edit_move_display (edit, l - edit->num_widget_lines / 2 - 1);
    edit_move_to_line (edit, l - 1);
    edit->force |= REDRAW_COMPLETELY;
    g_free (f);
}


/* Return 1 on success */
int
edit_save_block_cmd (WEdit *edit)
{
    long start_mark, end_mark;
    char *exp;
    if (eval_marks (edit, &start_mark, &end_mark))
	return 1;
    exp =
	edit_get_save_file (catstrs (home_dir, CLIP_FILE, 0),
			    _(" Save Block "));
    edit_push_action (edit, KEY_PRESS + edit->start_display);
    if (exp) {
	if (!*exp) {
	    g_free (exp);
	    return 0;
	} else {
	    if (edit_save_block (edit, exp, start_mark, end_mark)) {
		g_free (exp);
		edit->force |= REDRAW_COMPLETELY;
		return 1;
	    } else {
		g_free (exp);
		edit_error_dialog (_(" Save Block "),
				   get_sys_error (_
						  (" Cannot save file. ")));
	    }
	}
    }
    edit->force |= REDRAW_COMPLETELY;
    return 0;
}


/* returns 1 on success */
int
edit_insert_file_cmd (WEdit *edit)
{
    char *exp = edit_get_load_file (catstrs (home_dir, CLIP_FILE, 0),
				    _(" Insert File "));
    edit_push_action (edit, KEY_PRESS + edit->start_display);
    if (exp) {
	if (!*exp) {
	    g_free (exp);
	    return 0;
	} else {
	    if (edit_insert_file (edit, exp)) {
		g_free (exp);
		edit->force |= REDRAW_COMPLETELY;
		return 1;
	    } else {
		g_free (exp);
		edit_error_dialog (_(" Insert File "),
				   get_sys_error (_
						  (" Cannot insert file. ")));
	    }
	}
    }
    edit->force |= REDRAW_COMPLETELY;
    return 0;
}

/* sorts a block, returns -1 on system fail, 1 on cancel and 0 on success */
int edit_sort_cmd (WEdit * edit)
{
    static char *old = 0;
    char *exp;
    long start_mark, end_mark;
    int e;

    if (eval_marks (edit, &start_mark, &end_mark)) {
	edit_error_dialog (_(" Sort block "), _(" You must first highlight a block of text. "));
	return 0;
    }
    edit_save_block (edit, catstrs (home_dir, BLOCK_FILE, 0), start_mark, end_mark);

    exp = old ? old : "";

    exp = input_dialog (_(" Run Sort "), 
    _(" Enter sort options (see manpage) separated by whitespace: "), exp);

    if (!exp)
	return 1;
    if (old)
	g_free (old);
    old = exp;

    e = system (catstrs (" sort ", exp, " ", home_dir, BLOCK_FILE, " > ", home_dir, TEMP_FILE, 0));
    if (e) {
	if (e == -1 || e == 127) {
	    edit_error_dialog (_(" Sort "), 
	    get_sys_error (_(" Cannot execute sort command ")));
	} else {
	    char q[8];
	    sprintf (q, "%d ", e);
	    edit_error_dialog (_(" Sort "), 
	    catstrs (_(" Sort returned non-zero: "), q, 0));
	}
	return -1;
    }

    edit->force |= REDRAW_COMPLETELY;

    if (edit_block_delete_cmd (edit))
	return 1;
    edit_insert_file (edit, catstrs (home_dir, TEMP_FILE, 0));
    return 0;
}

/*
 * Ask user for a command, execute it and paste its output back to the
 * editor.
 */
int
edit_ext_cmd (WEdit *edit)
{
    char *exp;
    int e;

    exp =
	input_dialog (_("Paste output of external command"),
		      _("Enter shell command(s):"), NULL);

    if (!exp)
	return 1;

    e = system (catstrs (exp, " > ", home_dir, TEMP_FILE, 0));
    g_free (exp);

    if (e) {
	edit_error_dialog (_("External command"),
			   get_sys_error (_("Cannot execute command")));
	return -1;
    }

    edit->force |= REDRAW_COMPLETELY;

    edit_insert_file (edit, catstrs (home_dir, TEMP_FILE, 0));
    return 0;
}

/* if block is 1, a block must be highlighted and the shell command
   processes it. If block is 0 the shell command is a straight system
   command, that just produces some output which is to be inserted */
void
edit_block_process_cmd (WEdit *edit, const char *shell_cmd, int block)
{
    long start_mark, end_mark;
    char buf[BUFSIZ];
    FILE *script_home = NULL;
    FILE *script_src = NULL;
    FILE *block_file = NULL;
    char *o = NULL;
    char *h = NULL;
    char *b = NULL;
    char *quoted_name = NULL;

    o = catstrs (mc_home, shell_cmd, 0);	/* original source script */
    h = catstrs (home_dir, EDIT_DIR, shell_cmd, 0);	/* home script */
    b = catstrs (home_dir, BLOCK_FILE, 0);	/* block file */

    if (!(script_home = fopen (h, "r"))) {
	if (!(script_home = fopen (h, "w"))) {
	    edit_error_dialog ("", get_sys_error (catstrs
						  (_
						   ("Error creating script:"),
						   h, 0)));
	    return;
	}
	if (!(script_src = fopen (o, "r"))) {
	    fclose (script_home);
	    unlink (h);
	    edit_error_dialog ("", get_sys_error (catstrs
						  (_("Error reading script:"),
						   o, 0)));
	    return;
	}
	while (fgets (buf, sizeof (buf), script_src))
	    fputs (buf, script_home);
	if (fclose (script_home)) {
	    edit_error_dialog ("", get_sys_error (catstrs
						  (_
						   ("Error closing script:"),
						   h, 0)));
	    return;
	}
	chmod (h, 0700);
	edit_error_dialog ("", get_sys_error (catstrs
					      (_("Script created:"), h, 0)));
    }

    open_error_pipe ();

    if (block) {		/* for marked block run indent formatter */
	if (eval_marks (edit, &start_mark, &end_mark)) {
	    edit_error_dialog (_("Process block"),
			       _
			       (" You must first highlight a block of text. "));
	    return;
	}
	edit_save_block (edit, b, start_mark, end_mark);
	quoted_name = name_quote (edit->filename, 0);
	/*
	 * Run script.
	 * Initial space is to avoid polluting bash history.
	 * Arguments:
	 *   $1 - name of the edited file (to check its extension etc).
	 *   $2 - file containing the current block.
	 *   $3 - file where error messages should be put
	 *        (for compatibility with old scripts).
	 */
	system (catstrs (" ", home_dir, EDIT_DIR, shell_cmd, " ", quoted_name,
			 " ", home_dir, BLOCK_FILE " /dev/null", NULL));

    } else {
	/*
	 * No block selected, just execute the command for the file.
	 * Arguments:
	 *   $1 - name of the edited file.
	 */
	system (catstrs (" ", home_dir, EDIT_DIR, shell_cmd, " ",
			 quoted_name, NULL));
    }
    g_free (quoted_name);
    close_error_pipe (0, 0);

    edit_refresh_cmd (edit);
    edit->force |= REDRAW_COMPLETELY;

    /* insert result block */
    if (block) {
	if (edit_block_delete_cmd (edit))
	    return;
	edit_insert_file (edit, b);
	if ((block_file = fopen (b, "w")))
	    fclose (block_file);
	return;
    }

    return;
}

/* prints at the cursor */
/* returns the number of chars printed */
int edit_print_string (WEdit * e, const char *s)
{
    int i = 0;
    while (s[i])
	edit_execute_cmd (e, -1, (unsigned char) s[i++]);
    e->force |= REDRAW_COMPLETELY;
    edit_update_screen (e);
    return i;
}

int edit_printf (WEdit * e, const char *fmt, ...)
{
    int i;
    va_list pa;
    char s[1024];
    va_start (pa, fmt);
    g_vsnprintf (s, sizeof (s), fmt, pa);
    i = edit_print_string (e, s);
    va_end (pa);
    return i;
}

/* FIXME: does this function break NT_OS2 ? */

static void pipe_mail (WEdit *edit, char *to, char *subject, char *cc)
{
    FILE *p = 0;
    char *s;

    to = name_quote (to, 0);
    subject = name_quote (subject, 0);
    cc = name_quote (cc, 0);
    s = g_strdup_printf ("mail -s %s -c %s %s", subject, cc, to);
    g_free (to);
    g_free (subject);
    g_free (cc);

    if (s) {
	p = popen (s, "w");
	g_free (s);
    }

    if (p) {
	long i;
	for (i = 0; i < edit->last_byte; i++)
	    fputc (edit_get_byte (edit, i), p);
	pclose (p);
    }
}

#define MAIL_DLG_HEIGHT 12

void edit_mail_dialog (WEdit * edit)
{
    char *tmail_to;
    char *tmail_subject;
    char *tmail_cc;

    static char *mail_cc_last = 0;
    static char *mail_subject_last = 0;
    static char *mail_to_last = 0;

    QuickDialog Quick_input =
    {50, MAIL_DLG_HEIGHT, -1, 0, N_(" Mail "),
     "[Input Line Keys]", 0};

    QuickWidget quick_widgets[] =
    {
	{quick_button, 6, 10, 9, MAIL_DLG_HEIGHT, N_("&Cancel"), 0, B_CANCEL, 0,
	 0, NULL},
	{quick_button, 2, 10, 9, MAIL_DLG_HEIGHT, N_("&OK"), 0, B_ENTER, 0,
	 0, NULL},
	{quick_input, 3, 50, 8, MAIL_DLG_HEIGHT, "", 44, 0, 0,
	 0, "mail-dlg-input"},
	{quick_label, 2, 50, 7, MAIL_DLG_HEIGHT, N_(" Copies to"), 0, 0, 0,
	 0, 0},
	{quick_input, 3, 50, 6, MAIL_DLG_HEIGHT, "", 44, 0, 0,
	 0, "mail-dlg-input-2"},
	{quick_label, 2, 50, 5, MAIL_DLG_HEIGHT, N_(" Subject"), 0, 0, 0,
	 0, 0},
	{quick_input, 3, 50, 4, MAIL_DLG_HEIGHT, "", 44, 0, 0,
	 0, "mail-dlg-input-3"},
	{quick_label, 2, 50, 3, MAIL_DLG_HEIGHT, N_(" To"), 0, 0, 0,
	 0, 0},
	{quick_label, 2, 50, 2, MAIL_DLG_HEIGHT, N_(" mail -s <subject> -c <cc> <to>"), 0, 0, 0,
	 0, 0},
	{0}};

    quick_widgets[2].str_result = &tmail_cc;
    quick_widgets[2].text = mail_cc_last ? mail_cc_last : "";
    quick_widgets[4].str_result = &tmail_subject;
    quick_widgets[4].text = mail_subject_last ? mail_subject_last : "";
    quick_widgets[6].str_result = &tmail_to;
    quick_widgets[6].text = mail_to_last ? mail_to_last : "";

    Quick_input.widgets = quick_widgets;

    if (quick_dialog (&Quick_input) != B_CANCEL) {
	if (mail_cc_last)
	    g_free (mail_cc_last);
	if (mail_subject_last)
	    g_free (mail_subject_last);
	if (mail_to_last)
	    g_free (mail_to_last);
	mail_cc_last = *(quick_widgets[2].str_result);
	mail_subject_last = *(quick_widgets[4].str_result);
	mail_to_last = *(quick_widgets[6].str_result);
	pipe_mail (edit, mail_to_last, mail_subject_last, mail_cc_last);
    }
}


/*******************/
/* Word Completion */
/*******************/


/* find first character of current word */
static int edit_find_word_start (WEdit *edit, long *word_start, int *word_len)
{
    int i, c, last;
    
/* return if at begin of file */
    if (edit->curs1 <= 0)
	return 0;

    c = (unsigned char) edit_get_byte (edit, edit->curs1 - 1);
/* return if not at end or in word */
    if (isspace (c) || !(isalnum (c) || c == '_'))
	return 0; 

/* search start of word to be completed */
    for (i = 2;; i++) {
/* return if at begin of file */
	if (edit->curs1 - i < 0) 
	    return 0;
	    
	last = c;
	c = (unsigned char) edit_get_byte (edit, edit->curs1 - i);

	if (!(isalnum (c) || c == '_')) {
/* return if word starts with digit */
	    if (isdigit (last))
		return 0;

	    *word_start = edit->curs1 - (i - 1); /* start found */
	    *word_len = i - 1;
	    break;
	}
    }
/* success */
    return 1;
}


/* (re)set search parameters to the given values */
static void edit_set_search_parameters (int rs, int rb, int rr, int rw, int rc)
{
    replace_scanf = rs;
    replace_backwards = rb;
    replace_regexp = rr;
    replace_whole = rw;
    replace_case = rc;
}


#define MAX_WORD_COMPLETIONS 100	/* in listbox */

/* collect the possible completions */
static int
edit_collect_completions (WEdit *edit, long start, int word_len,
			  char *match_expr, struct selection *compl,
			  int *num)
{
    int len, max_len = 0, i, skip;
    char *bufpos;

    /* collect max MAX_WORD_COMPLETIONS completions */
    while (*num < MAX_WORD_COMPLETIONS) {
	/* get next match */
	start =
	    edit_find (start - 1, (unsigned char *) match_expr, &len,
		       edit->last_byte,
		       (int (*)(void *, long)) edit_get_byte,
		       (void *) edit, 0);

	/* not matched */
	if (start < 0)
	    break;

	/* add matched completion if not yet added */
	bufpos =
	    &edit->
	    buffers1[start >> S_EDIT_BUF_SIZE][start & M_EDIT_BUF_SIZE];
	skip = 0;
	for (i = 0; i < *num; i++) {
	    if (strncmp
		(&compl[i].text[word_len], &bufpos[word_len],
		 max (len, compl[i].len) - word_len) == 0) {
		skip = 1;
		break;		/* skip it, already added */
	    }
	}
	if (skip)
	    continue;

	compl[*num].text = g_malloc (len + 1);
	compl[*num].len = len;
	for (i = 0; i < len; i++)
	    compl[*num].text[i] = *(bufpos + i);
	compl[*num].text[i] = '\0';
	(*num)++;

	/* note the maximal length needed for the completion dialog */
	if (len > max_len)
	    max_len = len;
    }
    return max_len;
}


static int
compllist_callback (void *data)
{
    return 0;
}


/* let the user select its preferred completion */
static void
edit_completion_dialog (WEdit *edit, int max_len, int word_len,
			struct selection *compl, int num_compl)
{
    int start_x, start_y, offset, i;
    char *curr = NULL;
    Dlg_head *compl_dlg;
    WListbox *compl_list;
    unsigned int compl_dlg_h;	/* completion dialog height */
    unsigned int compl_dlg_w;	/* completion dialog width */

    /* calculate the dialog metrics */
    compl_dlg_h = num_compl + 2;
    compl_dlg_w = max_len + 4;
    start_x = edit->curs_col + edit->start_col - (compl_dlg_w / 2);
    start_y = edit->curs_row + EDIT_TEXT_VERTICAL_OFFSET + 1;

    if (start_x < 0)
	start_x = 0;
    if (compl_dlg_w > COLS)
	compl_dlg_w = COLS;
    if (compl_dlg_h > LINES - 2)
	compl_dlg_h = LINES - 2;

    offset = start_x + compl_dlg_w - COLS;
    if (offset > 0)
	start_x -= offset;
    offset = start_y + compl_dlg_h - LINES;
    if (offset > 0)
	start_y -= (offset + 1);

    /* create the dialog */
    compl_dlg =
	create_dlg (start_y, start_x, compl_dlg_h, compl_dlg_w,
		    dialog_colors, NULL, "[Completion]", NULL,
		    DLG_COMPACT);

    /* create the listbox */
    compl_list =
	listbox_new (1, 1, compl_dlg_w - 2, compl_dlg_h - 2, 0,
		     compllist_callback, NULL);

    /* add the dialog */
    add_widget (compl_dlg, compl_list);

    /* fill the listbox with the completions */
    for (i = 0; i < num_compl; i++)
	listbox_add_item (compl_list, 0, 0, compl[i].text, NULL);

    /* pop up the dialog */
    run_dlg (compl_dlg);

    /* apply the choosen completion */
    if (compl_dlg->ret_value == B_ENTER) {
	listbox_get_current (compl_list, &curr, NULL);
	if (curr)
	    for (curr += word_len; *curr; curr++)
		edit_insert (edit, *curr);
    }

    /* destroy dialog before return */
    destroy_dlg (compl_dlg);
}


/*
 * Complete current word using regular expression search
 * backwards beginning at the current cursor position.
 */
void
edit_complete_word_cmd (WEdit *edit)
{
    int word_len = 0, i, num_compl = 0, max_len;
    long word_start = 0;
    char *bufpos;
    char match_expr[MAX_REPL_LEN];
    struct selection compl[MAX_WORD_COMPLETIONS];	/* completions */

    /* don't want to disturb another search */
    int old_rs = replace_scanf;
    int old_rb = replace_backwards;
    int old_rr = replace_regexp;
    int old_rw = replace_whole;
    int old_rc = replace_case;

    /* search start of word to be completed */
    if (!edit_find_word_start (edit, &word_start, &word_len))
	return;

    /* prepare match expression */
    bufpos = &edit->buffers1[word_start >> S_EDIT_BUF_SIZE]
	[word_start & M_EDIT_BUF_SIZE];
    strncpy (match_expr, bufpos, word_len);
    match_expr[word_len] = '\0';
    strcat (match_expr, "[a-zA-Z_0-9]+");

    /* init search: backward, regexp, whole word, case sensitive */
    edit_set_search_parameters (0, 1, 1, 1, 1);

    /* collect the possible completions              */
    /* start search from curs1 down to begin of file */
    max_len =
	edit_collect_completions (edit, word_start, word_len, match_expr,
				  (struct selection *) &compl, &num_compl);

    if (num_compl > 0) {
	/* insert completed word if there is only one match */
	if (num_compl == 1) {
	    for (i = word_len; i < compl[0].len; i++)
		edit_insert (edit, *(compl[0].text + i));
	}
	/* more than one possible completion => ask the user */
	else {
	    /* !!! usually only a beep is expected and when <ALT-TAB> is !!! */
	    /* !!! pressed again the selection dialog pops up, but that  !!! */
	    /* !!! seems to require a further internal state             !!! */
	    /*beep (); */

	    /* let the user select the preferred completion */
	    edit_completion_dialog (edit, max_len, word_len,
				    (struct selection *) &compl,
				    num_compl);
	}
    }

    /* release memory before return */
    for (i = 0; i < num_compl; i++)
	g_free (compl[i].text);

    /* restore search parameters */
    edit_set_search_parameters (old_rs, old_rb, old_rr, old_rw, old_rc);
}
