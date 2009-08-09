/* editor high level editing commands

   Copyright (C) 1996, 1997, 1998, 2001, 2002, 2003, 2004, 2005, 2006,
   2007 Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

*/

/** \file
 *  \brief Source: editor high level editing commands
 *  \author Paul Sheer
 *  \date 1996, 1997
 */

/* #define PIPE_BLOCKS_SO_READ_BYTE_BY_BYTE */

#include <config.h>

#include <assert.h>
#include <ctype.h>

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>

#include "../src/global.h"
#include "../src/history.h"

#include "../src/tty.h"		/* LINES */
#include "../src/widget.h"	/* listbox_new() */
#include "../src/layout.h"	/* clr_scr() */
#include "../src/main.h"	/* mc_home source_codepage */
#include "../src/help.h"	/* interactive_display() */
#include "../src/key.h"		/* XCTRL */
#include "../src/wtools.h"	/* message() */
#include "../src/charsets.h"
#include "../src/selcodepage.h"
#include "../src/strutil.h"	/* utf string functions */

#include "../edit/edit-impl.h"
#include "../edit/edit.h"
#include "../edit/editlock.h"
#include "../edit/editcmddef.h"
#include "../edit/edit-widget.h"
#include "../edit/editcmd_dialogs.h"
#include "../edit/etags.h"

/* globals: */

/* search and replace: */
static int search_create_bookmark = 0;
/* static int search_in_all_charsets = 0; */

/* queries on a save */
int edit_confirm_save = 1;

static int edit_save_cmd (WEdit *edit);
static unsigned char *edit_get_block (WEdit *edit, long start,
				      long finish, int *l);

static void
edit_search_cmd_search_create_bookmark(WEdit * edit)
{
    int found = 0, books = 0;
    int l = 0, l_last = -1;
    long q = 0;
    gsize len = 0;

    for (;;) {
	if (!mc_search_run(edit->search, (void *) edit, q, edit->last_byte, &len))
	    break;
	
	found++;
	l += edit_count_lines (edit, q, edit->search->normal_offset);
	if (l != l_last) {
	    book_mark_insert (edit, l, BOOK_MARK_FOUND_COLOR);
	    books++;
	}
	l_last = l;
	q = edit->search->normal_offset + 1;
    }

    if (found) {
/* in response to number of bookmarks added because of string being found %d times */
	message (D_NORMAL, _("Search"), _(" %d items found, %d bookmarks added "), found, books);
    } else {
	edit_error_dialog (_ ("Search"), _ (" Search string not found "));
    }
}

static int
edit_search_cmd_callback(const void *user_data, gsize char_offset)
{
    return edit_get_byte ((WEdit * )user_data, (long) char_offset);
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

/* returns 0 on error, -1 on abort */
static int
edit_save_file (WEdit *edit, const char *filename)
{
    char *p;
    gchar *tmp;
    long filelen = 0;
    char *savename = 0;
    gchar *real_filename;
    int this_save_mode, fd = -1;

    if (!filename)
	return 0;
    if (!*filename)
	return 0;

    if (*filename != PATH_SEP && edit->dir) {
	real_filename = concat_dir_and_file (edit->dir, filename);
    } else {
	real_filename = g_strdup(filename);
    }

    this_save_mode = option_save_mode;
    if (this_save_mode != EDIT_QUICK_SAVE) {
	if (!vfs_file_is_local (real_filename) ||
	    (fd = mc_open (real_filename, O_RDONLY | O_BINARY)) == -1) {
	    /*
	     * The file does not exists yet, so no safe save or
	     * backup are necessary.
	     */
	    this_save_mode = EDIT_QUICK_SAVE;
	}
	if (fd != -1)
	    mc_close (fd);
    }

    if (this_save_mode == EDIT_QUICK_SAVE &&
	!edit->skip_detach_prompt) {
	int rv;
	struct stat sb;

	rv = mc_stat (real_filename, &sb);
	if (rv == 0 && sb.st_nlink > 1) {
	    rv = edit_query_dialog3 (_("Warning"),
				     _(" File has hard-links. Detach before saving? "),
				     _("&Yes"), _("&No"), _("&Cancel"));
	    switch (rv) {
	    case 0:
		this_save_mode = EDIT_SAFE_SAVE;
		/* fallthrough */
	    case 1:
		edit->skip_detach_prompt = 1;
		break;
	    default:
		g_free(real_filename);
		return -1;
	    }
	}

	/* Prevent overwriting changes from other editor sessions. */
	if (rv == 0 && edit->stat1.st_mtime != 0 && edit->stat1.st_mtime != sb.st_mtime) {

	    /* The default action is "Cancel". */
	    query_set_sel(1);

	    rv = edit_query_dialog2 (
		_("Warning"),
		_("The file has been modified in the meantime. Save anyway?"),
		_("&Yes"),
		_("&Cancel"));
	    if (rv != 0){
		g_free(real_filename);
		return -1;
	    }
	}
    }

    if (this_save_mode != EDIT_QUICK_SAVE) {
	char *savedir, *saveprefix;
	const char *slashpos;
	slashpos = strrchr (real_filename, PATH_SEP);
	if (slashpos) {
	    savedir = g_strdup (real_filename);
	    savedir[slashpos - real_filename + 1] = '\0';
	} else
	    savedir = g_strdup (".");
	saveprefix = concat_dir_and_file (savedir, "cooledit");
	g_free (savedir);
	fd = mc_mkstemps (&savename, saveprefix, NULL);
	g_free (saveprefix);
	if (!savename){
	    g_free(real_filename);
	    return 0;
	}
	/* FIXME:
	 * Close for now because mc_mkstemps use pure open system call
	 * to create temporary file and it needs to be reopened by
	 * VFS-aware mc_open().
	 */
	close (fd);
    } else
	savename = g_strdup (real_filename);

    mc_chown (savename, edit->stat1.st_uid, edit->stat1.st_gid);
    mc_chmod (savename, edit->stat1.st_mode);

    if ((fd =
	 mc_open (savename, O_CREAT | O_WRONLY | O_TRUNC | O_BINARY,
		  edit->stat1.st_mode)) == -1)
	goto error_save;

/* pipe save */
    if ((p = edit_get_write_filter (savename, real_filename))) {
	FILE *file;

	mc_close (fd);
	file = (FILE *) popen (p, "w");

	if (file) {
	    filelen = edit_write_stream (edit, file);
#if 1
	    pclose (file);
#else
	    if (pclose (file) != 0) {
		tmp = g_strconcat (_(" Error writing to pipe: "),
					    p, " ", (char *) NULL);
		edit_error_dialog (_("Error"), tmp);
		g_free(tmp);
		g_free (p);
		goto error_save;
	    }
#endif
	} else {
	    tmp = g_strconcat (_(" Cannot open pipe for writing: "),
					p, " ", (char *) NULL);

	    edit_error_dialog (_("Error"),
			       get_sys_error (tmp));
	    g_free (p);
	    g_free(tmp);
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

	/* Update the file information, especially the mtime. */
	if (mc_stat (savename, &edit->stat1) == -1)
	    goto error_save;
    }

    if (filelen != edit->last_byte)
	goto error_save;

    if (this_save_mode == EDIT_DO_BACKUP) {
	assert (option_backup_ext != NULL);
	tmp = g_strconcat (real_filename, option_backup_ext,(char *) NULL);
	if (mc_rename (real_filename, tmp) == -1){
	    g_free(tmp);
	    goto error_save;
	}
    }

    if (this_save_mode != EDIT_QUICK_SAVE)
	if (mc_rename (savename, real_filename) == -1)
	    goto error_save;
    g_free (savename);
    g_free(real_filename);
    return 1;
  error_save:
/*  FIXME: Is this safe ?
 *  if (this_save_mode != EDIT_QUICK_SAVE)
 *	mc_unlink (savename);
 */
    g_free(real_filename);
    g_free (savename);
    return 0;
}

void menu_save_mode_cmd (void)
{
#define DLG_X 38
#define DLG_Y 10
    static char *str_result;
    static int save_mode_new;
    static const char *str[] =
    {
	N_("Quick save "),
	N_("Safe save "),
	N_("Do backups -->")};

    static QuickWidget widgets[] =
    {
	{quick_button, 18, DLG_X, 7, DLG_Y, N_("&Cancel"), 0,
	 B_CANCEL, 0, 0, NULL, NULL, NULL},
	{quick_button, 6, DLG_X, 7, DLG_Y, N_("&OK"), 0,
	 B_ENTER, 0, 0, NULL, NULL, NULL},
	{quick_input, 23, DLG_X, 5, DLG_Y, 0, 9,
	 0, 0, &str_result, "edit-backup-ext", NULL, NULL},
	{quick_label, 22, DLG_X, 4, DLG_Y, N_("Extension:"), 0,
	 0, 0, 0, NULL, NULL, NULL},
	{quick_radio, 4, DLG_X, 3, DLG_Y, "", 3,
	 0, &save_mode_new, (char **) str, NULL, NULL, NULL},
	NULL_QuickWidget};
    static QuickDialog dialog =
    {DLG_X, DLG_Y, -1, -1, N_(" Edit Save Mode "), "[Edit Save Mode]",
     widgets, 0};
    static int i18n_flag = 0;

    if (!i18n_flag) {
        size_t i;
	size_t maxlen = 0;
	int dlg_x;
	size_t l1;

	/* OK/Cancel buttons */
	l1 = str_term_width1 (_(widgets[0].text)) + str_term_width1 (_(widgets[1].text)) + 5;
	maxlen = max (maxlen, l1);

        for (i = 0; i < 3; i++ ) {
            str[i] = _(str[i]);
	    maxlen = max (maxlen, (size_t) str_term_width1 (str[i]) + 7);
	}
        i18n_flag = 1;

        dlg_x = maxlen + str_term_width1 (_(widgets[3].text)) + 5 + 1;
        widgets[2].hotkey_pos = str_term_width1 (_(widgets[3].text)); /* input field length */
        dlg_x = min (COLS, dlg_x);
	dialog.xlen = dlg_x;

        i = (dlg_x - l1)/3;
	widgets[1].relative_x = i;
	widgets[0].relative_x = i + str_term_width1 (_(widgets[1].text)) + i + 4;

	widgets[2].relative_x = widgets[3].relative_x = maxlen + 2;

	for (i = 0; i < sizeof (widgets)/sizeof (widgets[0]); i++)
		widgets[i].x_divisions = dlg_x;
    }

    assert (option_backup_ext != NULL);
    widgets[2].text = option_backup_ext;
    widgets[4].value = option_save_mode;
    if (quick_dialog (&dialog) != B_ENTER)
	return;
    option_save_mode = save_mode_new;

    g_free (option_backup_ext);
    option_backup_ext = str_result;
    str_result = NULL;
}

void
edit_set_filename (WEdit *edit, const char *f)
{
    g_free (edit->filename);
    if (!f)
	f = "";
    edit->filename = g_strdup (f);
    if (edit->dir == NULL && *f != PATH_SEP)
#ifdef USE_VFS
	edit->dir = g_strdup (vfs_get_current_dir ());
#else
	edit->dir = g_get_current_dir ();
#endif
}

/* Here we want to warn the users of overwriting an existing file,
   but only if they have made a change to the filename */
/* returns 1 on success */
int
edit_save_as_cmd (WEdit *edit)
{
    /* This heads the 'Save As' dialog box */
    char *exp;
    int save_lock = 0;
    int different_filename = 0;

    exp = input_expand_dialog (
	_(" Save As "), _(" Enter file name: "),MC_HISTORY_EDIT_SAVE_AS, edit->filename);
    edit_push_action (edit, KEY_PRESS + edit->start_display);

    if (exp) {
	if (!*exp) {
	    g_free (exp);
	    edit->force |= REDRAW_COMPLETELY;
	    return 0;
	} else {
	    int rv;
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
			 _("&Overwrite"), _("&Cancel"))) {
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

	    if (different_filename)
	    {
		/*
		 * Allow user to write into saved (under another name) file
		 * even if original file had r/o user permissions.
		 */
		edit->stat1.st_mode |= S_IWRITE;
	    }

	    rv = edit_save_file (edit, exp);
	    switch (rv) {
	    case 1:
		/* Succesful, so unlock both files */
		if (different_filename) {
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
		    edit_load_syntax (edit, NULL, option_syntax_type);
		edit->force |= REDRAW_COMPLETELY;
		return 1;
	    default:
		edit_error_dialog (_(" Save As "),
				   get_sys_error (_
						  (" Cannot save file. ")));
		/* fallthrough */
	    case -1:
		/* Failed, so maintain modify (not save) lock */
		if (save_lock)
		    edit_unlock_file (exp);
		g_free (exp);
		edit->force |= REDRAW_COMPLETELY;
		return 0;
	    }
	}
    }
    edit->force |= REDRAW_COMPLETELY;
    return 0;
}

/* {{{ Macro stuff starts here */

/* creates a macro file if it doesn't exist */
static FILE *edit_open_macro_file (const char *r)
{
    gchar *filename;
    FILE *fd;
    int file;
    filename = concat_dir_and_file (home_dir, EDIT_MACRO_FILE);
    if ((file = open (filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1){
	g_free(filename);
	return 0;
    }
    close (file);
    fd = fopen (filename, r);
    g_free(filename);
    return fd;
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
    gchar *tmp, *tmp2;
    struct macro macro[MAX_MACRO_LENGTH];
    FILE *f, *g;
    int s, i, n, j = 0;

    (void) edit;

    if (saved_macros_loaded)
	if ((j = macro_exists (k)) < 0)
	    return 0;
    tmp = concat_dir_and_file (home_dir, EDIT_TEMP_FILE);
    g = fopen (tmp , "w");
    g_free(tmp);
    if (!g) {
	edit_error_dialog (_(" Delete macro "),
		 get_sys_error (_(" Cannot open temp file ")));
	return 1;
    }
    f = edit_open_macro_file ("r");
    if (!f) {
	edit_error_dialog (_(" Delete macro "),
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
    tmp = concat_dir_and_file (home_dir, EDIT_TEMP_FILE);
    tmp2 = concat_dir_and_file (home_dir, EDIT_MACRO_FILE);
    if (rename ( tmp, tmp2) == -1) {
	edit_error_dialog (_(" Delete macro "),
	   get_sys_error (_(" Cannot overwrite macro file ")));
	g_free(tmp);
	g_free(tmp2);
	return 1;
    }
    g_free(tmp);
    g_free(tmp2);

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
    s = editcmd_dialog_raw_key_query (_(" Save macro "),
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
	    edit_error_dialog (_(" Save macro "), get_sys_error (_(" Cannot open macro file ")));
    }
    return 0;
}

void edit_delete_macro_cmd (WEdit * edit)
{
    int command;

    command = editcmd_dialog_raw_key_query (_ (" Delete macro "),
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

    (void) edit;

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
	edit_error_dialog (_(" Load macro "),
		get_sys_error (_(" Cannot open macro file ")));
    return 0;
}

/* }}} Macro stuff starts here */

/* returns 1 on success */
int edit_save_confirm_cmd (WEdit * edit)
{
    gchar *f = NULL;

    if (edit_confirm_save) {
	f = g_strconcat (_(" Confirm save file? : "), edit->filename, " ", NULL);
	if (edit_query_dialog2 (_(" Save file "), f, _("&Save"), _("&Cancel"))){
	    g_free(f);
	    return 0;
	}
        g_free(f);
    }
    return edit_save_cmd (edit);
}


/* returns 1 on success */
static int
edit_save_cmd (WEdit *edit)
{
    int res, save_lock = 0;

    if (!edit->locked && !edit->delete_file)
	save_lock = edit_lock_file (edit->filename);
    res = edit_save_file (edit, edit->filename);

    /* Maintain modify (not save) lock on failure */
    if ((res > 0 && edit->locked) || save_lock)
	edit->locked = edit_unlock_file (edit->filename);

    /* On failure try 'save as', it does locking on its own */
    if (!res)
	return edit_save_as_cmd (edit);
    edit->force |= REDRAW_COMPLETELY;
    if (res > 0) {
        edit->delete_file = 0;
        edit->modified = 0;
    }

    return 1;
}


/* returns 1 on success */
int edit_new_cmd (WEdit * edit)
{
    if (edit->modified) {
	if (edit_query_dialog2 (_ ("Warning"), _ (" Current text was modified without a file save. \n Continue discards these changes. "), _ ("C&ontinue"), _ ("&Cancel"))) {
	    edit->force |= REDRAW_COMPLETELY;
	    return 0;
	}
    }
    edit->force |= REDRAW_COMPLETELY;

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

static void
edit_load_syntax_file (WEdit * edit)
{
    char *extdir;
    int dir = 0;

    if (geteuid () == 0) {
	dir = query_dialog (_("Syntax file edit"),
			    _(" Which syntax file you want to edit? "), D_NORMAL, 2,
			    _("&User"), _("&System Wide"));
    }

    extdir = concat_dir_and_file (mc_home, "syntax" PATH_SEP_STR "Syntax");
    if (!exist_file(extdir)) {
	g_free (extdir);
	extdir = concat_dir_and_file (mc_home_alt, "syntax" PATH_SEP_STR "Syntax");
    }

    if (dir == 0) {
	char *buffer;

	buffer = concat_dir_and_file (home_dir, EDIT_SYNTAX_FILE);
	check_for_default (extdir, buffer);
	edit_load_file_from_filename (edit, buffer);
	g_free (buffer);
    } else if (dir == 1)
	edit_load_file_from_filename (edit, extdir);

    g_free (extdir);
}

static void
edit_load_menu_file (WEdit * edit)
{
    char *buffer;
    char *menufile;
    int dir = 0;

    dir = query_dialog (
	_(" Menu edit "),
	_(" Which menu file do you want to edit? "), D_NORMAL,
	geteuid() ? 2 : 3, _("&Local"), _("&User"), _("&System Wide")
    );

    menufile = concat_dir_and_file (mc_home, EDIT_GLOBAL_MENU);

    if (!exist_file (menufile)) {
	g_free (menufile);
	menufile = concat_dir_and_file (mc_home_alt, EDIT_GLOBAL_MENU);
    }

    switch (dir) {
	case 0:
	    buffer = g_strdup (EDIT_LOCAL_MENU);
	    check_for_default (menufile, buffer);
	    chmod (buffer, 0600);
	    break;

	case 1:
	    buffer = concat_dir_and_file (home_dir, EDIT_HOME_MENU);
	    check_for_default (menufile, buffer);
	    break;
	
	case 2:
	    buffer = concat_dir_and_file (mc_home, EDIT_GLOBAL_MENU);
	    if (!exist_file (buffer)) {
		g_free (buffer);
		buffer = concat_dir_and_file (mc_home_alt, EDIT_GLOBAL_MENU);
	    }
	    break;

	default:
	    g_free (menufile);
	    return;
    }

    edit_load_file_from_filename (edit, buffer);

    g_free (buffer);
    g_free (menufile);
}

int
edit_load_cmd (WEdit *edit, edit_current_file_t what)
{
    char *exp;

    if (edit->modified
	&& (edit_query_dialog2
	    (_("Warning"),
	     _(" Current text was modified without a file save. \n"
	       " Continue discards these changes. "),
	     _("C&ontinue"), _("&Cancel")) == 1)) {
	    edit->force |= REDRAW_COMPLETELY;
	    return 0;
	}

    switch (what) {
    case EDIT_FILE_COMMON:
	exp = input_expand_dialog (_(" Load "), _(" Enter file name: "),
				    MC_HISTORY_EDIT_LOAD, edit->filename);

	if (exp) {
	    if (*exp)
		edit_load_file_from_filename (edit, exp);
	    g_free (exp);
	}
	break;

    case EDIT_FILE_SYNTAX:
	edit_load_syntax_file (edit);
	break;

    case EDIT_FILE_MENU:
	edit_load_menu_file (edit);
	break;

    default:
	break;
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

void
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

#define TEMP_BUF_LEN 1024

int
edit_insert_column_of_text_from_file (WEdit * edit, int file)
{
    long cursor;
    int i, col;
    int blocklen = -1, width;
    unsigned char *data;
    cursor = edit->curs1;
    col = edit_get_col (edit);
    data = g_malloc (TEMP_BUF_LEN);
    while ((blocklen = mc_read (file, (char *) data, TEMP_BUF_LEN)) > 0) {
        for (width = 0; width < blocklen; width++) {
            if (data[width] == '\n')
                break;
        }
        for (i = 0; i < blocklen; i++) {
            if (data[i] == '\n') {      /* fill in and move to next line */
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
    }
    edit_cursor_move (edit, cursor - edit->curs1);
    g_free(data);
    edit->force |= REDRAW_PAGE;
    return blocklen;
}

void
edit_block_copy_cmd (WEdit *edit)
{
    long start_mark, end_mark, current = edit->curs1;
    int size;
    unsigned char *copy_buf;

    edit_update_curs_col (edit);
    if (eval_marks (edit, &start_mark, &end_mark))
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
	     _("C&ontinue"), _("&Cancel")))
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
	    copy_buf[end_mark - count - 1] = edit_delete (edit, 1);
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
		edit_delete (edit, 1);
	    q--;
	}
	if (n)			/* move to next line except on the last delete */
	    edit_cursor_move (edit, edit_move_forward (edit, edit->curs1, 1, 0) - edit->curs1);
    }
}

/* if success return 0 */
static int
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
	     _("C&ontinue"), _("&Cancel"))) {
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
		edit_delete (edit, 1);
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

static gboolean
editcmd_find (WEdit *edit, gsize *len)
{
    gsize search_start = edit->search_start;
    gsize search_end;

    if (edit->replace_backwards) {
	search_end = edit->curs1-1;
	while ((int) search_start >= 0) {
	    if (search_end - search_start > edit->search->original_len && mc_search_is_fixed_search_str(edit->search))
		search_end = search_start + edit->search->original_len +1;
	    if ( mc_search_run(edit->search, (void *) edit, search_start, search_end, len))
	    {
		return TRUE;
	    }
	    search_start--;
	}
	edit->search->error_str = g_strdup(_(" Search string not found "));
    } else {
	return mc_search_run(edit->search, (void *) edit, edit->search_start, edit->last_byte, len);
    }
    return FALSE;
}


/* thanks to  Liviu Daia <daia@stoilow.imar.ro>  for getting this
   (and the above) routines to work properly - paul */

#define is_digit(x) ((x) >= '0' && (x) <= '9')

static char *
edit_replace_cmd__conv_to_display(char *str)
{
#ifdef HAVE_CHARSET
    GString *tmp;
    tmp = str_convert_to_display (str);

    if (tmp && tmp->len){
	g_free(str);
	str = tmp->str;
    }
    g_string_free (tmp, FALSE);
    return str;
#else
    return g_strdup(str);
#endif
}

static char *
edit_replace_cmd__conv_to_input(char *str)
{
#ifdef HAVE_CHARSET
    GString *tmp;
    tmp = str_convert_to_input (str);

    if (tmp && tmp->len){
	g_free(str);
	str = tmp->str;
    }
    g_string_free (tmp, FALSE);
    return str;
#else
    return g_strdup(str);
#endif
}
/* call with edit = 0 before shutdown to close memory leaks */
void
edit_replace_cmd (WEdit *edit, int again)
{
    /* 1 = search string, 2 = replace with */
    static char *saved1 = NULL;	/* saved default[123] */
    static char *saved2 = NULL;
    char *input1 = NULL;	/* user input from the dialog */
    char *input2 = NULL;
    int replace_yes;
    long times_replaced = 0, last_search;
    gboolean once_found = FALSE;

    if (!edit) {
	g_free (saved1), saved1 = NULL;
	g_free (saved2), saved2 = NULL;
	return;
    }

    last_search = edit->last_byte;

    edit->force |= REDRAW_COMPLETELY;

    if (again && !saved1 && !saved2)
	again = 0;

    if (again) {
	input1 = g_strdup (saved1 ? saved1 : "");
	input2 = g_strdup (saved2 ? saved2 : "");
    } else {
	char *disp1 = edit_replace_cmd__conv_to_display(g_strdup (saved1 ? saved1 : ""));
	char *disp2 = edit_replace_cmd__conv_to_display(g_strdup (saved2 ? saved2 : ""));

	edit_push_action (edit, KEY_PRESS + edit->start_display);

	editcmd_dialog_replace_show (edit, disp1, disp2, &input1, &input2 );

	g_free (disp1);
	g_free (disp2);

	if (input1 == NULL || *input1 == '\0') {
	    edit->force = REDRAW_COMPLETELY;
	    goto cleanup;
	}

	input1 = edit_replace_cmd__conv_to_input(input1);
	input2 = edit_replace_cmd__conv_to_input(input2);

	g_free (saved1), saved1 = g_strdup (input1);
	g_free (saved2), saved2 = g_strdup (input2);

	if (edit->search) {
	    mc_search_free(edit->search);
	    edit->search = NULL;
	}
    }

    if (!edit->search) {
	edit->search = mc_search_new(input1, -1);
	if (edit->search == NULL) {
	    edit->search_start = edit->curs1;
	    return;
	}
	edit->search->search_type = edit->search_type;
	edit->search->is_all_charsets = edit->all_codepages;
	edit->search->is_case_sentitive = edit->replace_case;
	edit->search->search_fn = edit_search_cmd_callback;
    }

    if (edit->found_len && edit->search_start == edit->found_start + 1
	&& edit->replace_backwards)
	edit->search_start--;

    if (edit->found_len && edit->search_start == edit->found_start - 1
	&& !edit->replace_backwards)
	edit->search_start++;

    do {
	gsize len = 0;
	long new_start;
	
	if (! editcmd_find(edit, &len)) {
	    if (!(edit->search->error == MC_SEARCH_E_OK ||
	       (once_found && edit->search->error == MC_SEARCH_E_NOTFOUND))) {
		edit_error_dialog (_ ("Search"), edit->search->error_str);
	    }
	    break;
	}
	once_found = TRUE;
	new_start = edit->search->normal_offset;

	edit->search_start = new_start = edit->search->normal_offset;
	/*returns negative on not found or error in pattern */

	if (edit->search_start >= 0) {
	    guint i;

	    edit->found_start = edit->search_start;
	    i = edit->found_len = len;

	    edit_cursor_move (edit, edit->search_start - edit->curs1);
	    edit_scroll_screen_over_cursor (edit);

	    replace_yes = 1;

	    if (edit->replace_mode == 0) {
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
		/* and prompt 2/3 down */
		switch (editcmd_dialog_replace_prompt_show (edit, input1, input2, -1, -1)) {
		case B_ENTER:
		    replace_yes = 1;
		    break;
		case B_SKIP_REPLACE:
		    replace_yes = 0;
		    break;
		case B_REPLACE_ALL:
		    edit->replace_mode=1;
		    break;
		case B_CANCEL:
		    replace_yes = 0;
		    edit->replace_mode = -1;
		    break;
		}
	    }
	    if (replace_yes) {	/* delete then insert new */
		GString *repl_str, *tmp_str;
		tmp_str = g_string_new(input2);
		
		repl_str = mc_search_prepare_replace_str (edit->search, tmp_str);
		g_string_free(tmp_str, TRUE);
		if (edit->search->error != MC_SEARCH_E_OK)
		{
		    edit_error_dialog (_ ("Replace"), edit->search->error_str);
		    break;
		}

		while (i--)
		    edit_delete (edit, 1);

		while (++i < repl_str->len)
			edit_insert (edit, repl_str->str[i]);
		
		g_string_free(repl_str, TRUE);
		edit->found_len = i;
	    }
	    /* so that we don't find the same string again */
	    if (edit->replace_backwards) {
		last_search = edit->search_start;
		edit->search_start--;
	    } else {
		edit->search_start += i;
		last_search = edit->last_byte;
	    }
	    edit_scroll_screen_over_cursor (edit);
	} else {
	    const char *msg = _(" Replace ");
	    /* try and find from right here for next search */
	    edit->search_start = edit->curs1;
	    edit_update_curs_col (edit);

	    edit->force |= REDRAW_PAGE;
	    edit_render_keypress (edit);
	    if (times_replaced) {
		message (D_NORMAL, msg, _(" %ld replacements made. "),
			 times_replaced);
	    } else
		query_dialog (msg, _(" Search string not found "),
			      D_NORMAL, 1, _("&OK"));
	    edit->replace_mode = -1;
	}
    } while (edit->replace_mode >= 0);

    edit->force = REDRAW_COMPLETELY;
    edit_scroll_screen_over_cursor (edit);
  cleanup:
    g_free (input1);
    g_free (input2);
}


void edit_search_cmd (WEdit * edit, int again)
{
    char *search_string = NULL, *search_string_dup = NULL;

    gsize len = 0;

    if (!edit)
	return;

    if (edit->search != NULL) {
	search_string = g_strndup(edit->search->original, edit->search->original_len);
	search_string_dup = search_string;
    } else {
        GList *history;
        history = history_get (MC_HISTORY_SHARED_SEARCH);
        if (history != NULL && history->data != NULL) {
            search_string_dup = search_string = (char *) g_strdup(history->data);
            history = g_list_first (history);
            g_list_foreach (history, (GFunc) g_free, NULL);
            g_list_free (history);
        }
        edit->search_start = edit->curs1;
    }

    if (!again) {
#ifdef HAVE_CHARSET
	GString *tmp;
	if (search_string && *search_string) {
	    tmp = str_convert_to_display (search_string);

	    g_free(search_string_dup);
	    search_string_dup = NULL;

	    if (tmp && tmp->len)
		search_string = search_string_dup = tmp->str;
	    g_string_free (tmp, FALSE);
	}
#endif /* HAVE_CHARSET */
	editcmd_dialog_search_show (edit, &search_string);
#ifdef HAVE_CHARSET
	if (search_string && *search_string) {
	    tmp = str_convert_to_input (search_string);
	    if (tmp && tmp->len)
		search_string = tmp->str;

	    g_string_free (tmp, FALSE);

	    if (search_string_dup)
		g_free(search_string_dup);
	}
#endif /* HAVE_CHARSET */

	edit_push_action (edit, KEY_PRESS + edit->start_display);

	if (!search_string) {
	    edit->force |= REDRAW_COMPLETELY;
	    edit_scroll_screen_over_cursor (edit);
	    return;
	}

	if (edit->search) {
	    mc_search_free(edit->search);
	    edit->search = NULL;
	}
    }

    if (!edit->search) {
	edit->search = mc_search_new(search_string, -1);
	if (edit->search == NULL) {
	    edit->search_start = edit->curs1;
	    return;
	}
	edit->search->search_type = edit->search_type;
	edit->search->is_all_charsets = edit->all_codepages;
	edit->search->is_case_sentitive = edit->replace_case;
	edit->search->search_fn = edit_search_cmd_callback;
    }

    if (search_create_bookmark) {
	edit_search_cmd_search_create_bookmark(edit);
    } else {
	if (edit->found_len && edit->search_start == edit->found_start + 1 && edit->replace_backwards)
	    edit->search_start--;

	if (edit->found_len && edit->search_start == edit->found_start - 1 && !edit->replace_backwards)
	    edit->search_start++;

	
	if (editcmd_find(edit, &len)) {
	    edit->found_start = edit->search_start = edit->search->normal_offset;
	    edit->found_len = len;

	    edit_cursor_move (edit, edit->search_start - edit->curs1);
	    edit_scroll_screen_over_cursor (edit);
	    if (edit->replace_backwards)
		edit->search_start--;
	    else
		edit->search_start++;
	} else {
	    edit->search_start = edit->curs1;
	    if (edit->search->error_str)
		edit_error_dialog (_ ("Search"), edit->search->error_str);
	}
    }

    edit->force |= REDRAW_COMPLETELY;
    edit_scroll_screen_over_cursor (edit);
}


/*
 * Check if it's OK to close the editor.  If there are unsaved changes,
 * ask user.  Return 1 if it's OK to exit, 0 to continue editing.
 */
int
edit_ok_to_exit (WEdit *edit)
{
    if (!edit->modified)
	return 1;

    switch (edit_query_dialog3
	    (_("Quit"), _(" File was modified, Save with exit? "),
	     _("&Cancel quit"), _("&Yes"), _("&No"))) {
    case 1:
	edit_push_markers (edit);
	edit_set_markers (edit, 0, 0, 0, 0);
	if (!edit_save_cmd (edit))
	    return 0;
	break;
    case 2:
	break;
    case 0:
    case -1:
	return 0;
    }

    return 1;
}

/* Return a null terminated length of text. Result must be g_free'd */
static unsigned char *
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
	int r;
	r = mc_write (file, VERTICAL_MAGIC, sizeof(VERTICAL_MAGIC));
	if (r > 0) {
	    unsigned char *block, *p;
	    p = block = edit_get_block (edit, start, finish, &len);
	    while (len) {
	        r = mc_write (file, p, len);
	        if (r < 0)
	            break;
	        p += r;
	        len -= r;
	    }
	    g_free (block);
	}
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
    int ret;
    gchar *tmp;
    tmp = concat_dir_and_file (home_dir, EDIT_CLIP_FILE);
    ret = edit_save_block (edit, tmp, start, finish);
    g_free(tmp);
    return ret;
}


void edit_paste_from_history (WEdit *edit)
{
    (void) edit;
    edit_error_dialog (_(" Error "), _(" This function is not implemented. "));
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
    gchar *tmp;
    tmp = concat_dir_and_file (home_dir, EDIT_CLIP_FILE);
    edit_insert_file (edit, tmp);
    g_free(tmp);
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
    f = input_dialog (_(" Goto line "), _(" Enter line: "), MC_HISTORY_EDIT_GOTO_LINE,
		      line ? s : "");
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
    char *exp, *tmp;

    if (eval_marks (edit, &start_mark, &end_mark))
	return 1;

    tmp = concat_dir_and_file (home_dir, EDIT_CLIP_FILE);
    exp =
	input_expand_dialog (_(" Save Block "), _(" Enter file name: "),
			     MC_HISTORY_EDIT_SAVE_BLOCK, 
			    tmp);
    g_free(tmp);
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
    gchar *tmp;
    char *exp;

    tmp = concat_dir_and_file (home_dir, EDIT_CLIP_FILE);
    exp = input_expand_dialog (_(" Insert File "), _(" Enter file name: "),
				     MC_HISTORY_EDIT_INSERT_FILE,
				     tmp);
    g_free(tmp);
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
    char *exp, *tmp;
    long start_mark, end_mark;
    int e;

    if (eval_marks (edit, &start_mark, &end_mark)) {
	edit_error_dialog (_(" Sort block "), _(" You must first highlight a block of text. "));
	return 0;
    }

    tmp = concat_dir_and_file (home_dir, EDIT_BLOCK_FILE);
    edit_save_block (edit, tmp, start_mark, end_mark);
    g_free(tmp);

    exp = input_dialog (_(" Run Sort "),
	_(" Enter sort options (see manpage) separated by whitespace: "),
	MC_HISTORY_EDIT_SORT, (old != NULL) ? old : "");

    if (!exp)
	return 1;
    g_free (old);
    old = exp;
    tmp = g_strconcat (" sort ", exp, " ", home_dir, PATH_SEP_STR EDIT_BLOCK_FILE, " > ",
			home_dir, PATH_SEP_STR EDIT_TEMP_FILE, (char *) NULL);
    e = system (tmp);
    g_free(tmp);
    if (e) {
	if (e == -1 || e == 127) {
	    edit_error_dialog (_(" Sort "),
	    get_sys_error (_(" Cannot execute sort command ")));
	} else {
	    char q[8];
	    sprintf (q, "%d ", e);
	    tmp = g_strconcat (_(" Sort returned non-zero: "), q, (char *) NULL);
	    edit_error_dialog (_(" Sort "), tmp);
	    g_free(tmp);
	}
	return -1;
    }

    edit->force |= REDRAW_COMPLETELY;

    if (edit_block_delete_cmd (edit))
	return 1;
    tmp = concat_dir_and_file (home_dir, EDIT_TEMP_FILE);
    edit_insert_file (edit, tmp);
    g_free(tmp);
    return 0;
}

/*
 * Ask user for a command, execute it and paste its output back to the
 * editor.
 */
int
edit_ext_cmd (WEdit *edit)
{
    char *exp, *tmp;
    int e;

    exp =
	input_dialog (_("Paste output of external command"),
		      _("Enter shell command(s):"),
		      MC_HISTORY_EDIT_PASTE_EXTCMD, NULL);

    if (!exp)
	return 1;

    tmp = g_strconcat (exp, " > ", home_dir, PATH_SEP_STR EDIT_TEMP_FILE, (char *) NULL);
    e = system (tmp);
    g_free(tmp);
    g_free (exp);

    if (e) {
	edit_error_dialog (_("External command"),
			   get_sys_error (_("Cannot execute command")));
	return -1;
    }

    edit->force |= REDRAW_COMPLETELY;
    tmp = concat_dir_and_file (home_dir, EDIT_TEMP_FILE);
    edit_insert_file (edit, tmp);
    g_free(tmp);
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
    gchar *o, *h, *b, *tmp;
    char *quoted_name = NULL;

    o = g_strconcat (mc_home, shell_cmd, (char *) NULL);				/* original source script */
    h = g_strconcat (home_dir, PATH_SEP_STR EDIT_DIR, shell_cmd, (char *) NULL);	/* home script */
    b = concat_dir_and_file (home_dir, EDIT_BLOCK_FILE);				/* block file */

    if (!(script_home = fopen (h, "r"))) {
	if (!(script_home = fopen (h, "w"))) {
	    tmp = g_strconcat (_("Error creating script:"), h, (char *) NULL);
	    edit_error_dialog ("", get_sys_error (tmp));
	    g_free(tmp);
	    goto edit_block_process_cmd__EXIT;
	}
	if (!(script_src = fopen (o, "r"))) {
	    o = g_strconcat (mc_home_alt, shell_cmd, (char *) NULL);
	    if (!(script_src = fopen (o, "r"))) {
		fclose (script_home);
		unlink (h);
		tmp = g_strconcat (_("Error reading script:"), o, (char *) NULL);
		edit_error_dialog ("", get_sys_error (tmp));
		g_free(tmp);
		goto edit_block_process_cmd__EXIT;
	    }
	}
	while (fgets (buf, sizeof (buf), script_src))
	    fputs (buf, script_home);
	if (fclose (script_home)) {
	    tmp = g_strconcat (_("Error closing script:"), h, (char *) NULL);
	    edit_error_dialog ("", get_sys_error (tmp));
	    g_free(tmp);
	    goto edit_block_process_cmd__EXIT;
	}
	chmod (h, 0700);
	tmp = g_strconcat (_("Script created:"), h, (char *) NULL);
	edit_error_dialog ("", get_sys_error (tmp));
	g_free(tmp);
    }

    open_error_pipe ();

    if (block) {		/* for marked block run indent formatter */
	if (eval_marks (edit, &start_mark, &end_mark)) {
	    edit_error_dialog (_("Process block"),
			       _
			       (" You must first highlight a block of text. "));
	    goto edit_block_process_cmd__EXIT;
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
	tmp = g_strconcat (" ", home_dir, PATH_SEP_STR EDIT_DIR, shell_cmd, " ", quoted_name,
			 " ", home_dir, PATH_SEP_STR EDIT_BLOCK_FILE " /dev/null", (char *) NULL);
	system (tmp);
	g_free(tmp);
    } else {
	/*
	 * No block selected, just execute the command for the file.
	 * Arguments:
	 *   $1 - name of the edited file.
	 */
	tmp = g_strconcat (" ", home_dir, PATH_SEP_STR EDIT_DIR, shell_cmd, " ",
			 quoted_name, (char *) NULL);
	system (tmp);
	g_free(tmp);
    }
    g_free (quoted_name);
    close_error_pipe (D_NORMAL, NULL);

    edit_refresh_cmd (edit);
    edit->force |= REDRAW_COMPLETELY;

    /* insert result block */
    if (block) {
	if (edit_block_delete_cmd (edit))
	    goto edit_block_process_cmd__EXIT;
	edit_insert_file (edit, b);
	if ((block_file = fopen (b, "w")))
	    fclose (block_file);
	goto edit_block_process_cmd__EXIT;
    }
edit_block_process_cmd__EXIT:
    g_free(b);
    g_free(h);
    g_free(o);
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


static void pipe_mail (WEdit *edit, char *to, char *subject, char *cc)
{
    FILE *p = 0;
    char *s;

    to = name_quote (to, 0);
    subject = name_quote (subject, 0);
    cc = name_quote (cc, 0);
    s = g_strconcat ("mail -s ", subject, *cc ? " -c " : "" , cc, " ",  to, (char *) NULL);
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
     "[Input Line Keys]", 0, 0};

    QuickWidget quick_widgets[] =
    {
	{quick_button, 6, 10, 9, MAIL_DLG_HEIGHT, N_("&Cancel"), 0, B_CANCEL, 0,
	 0, NULL, NULL, NULL},
	{quick_button, 2, 10, 9, MAIL_DLG_HEIGHT, N_("&OK"), 0, B_ENTER, 0,
	 0, NULL, NULL, NULL},
	{quick_input, 3, 50, 8, MAIL_DLG_HEIGHT, "", 44, 0, 0,
	 0, "mail-dlg-input", NULL, NULL},
	{quick_label, 2, 50, 7, MAIL_DLG_HEIGHT, N_(" Copies to"), 0, 0, 0,
	 0, 0, NULL, NULL},
	{quick_input, 3, 50, 6, MAIL_DLG_HEIGHT, "", 44, 0, 0,
	 0, "mail-dlg-input-2", NULL, NULL},
	{quick_label, 2, 50, 5, MAIL_DLG_HEIGHT, N_(" Subject"), 0, 0, 0,
	 0, 0, NULL, NULL},
	{quick_input, 3, 50, 4, MAIL_DLG_HEIGHT, "", 44, 0, 0,
	 0, "mail-dlg-input-3", NULL, NULL},
	{quick_label, 2, 50, 3, MAIL_DLG_HEIGHT, N_(" To"), 0, 0, 0,
	 0, 0, NULL, NULL},
	{quick_label, 2, 50, 2, MAIL_DLG_HEIGHT, N_(" mail -s <subject> -c <cc> <to>"), 0, 0, 0,
	 0, 0, NULL, NULL},
	NULL_QuickWidget};

    quick_widgets[2].str_result = &tmail_cc;
    quick_widgets[2].text = mail_cc_last ? mail_cc_last : "";
    quick_widgets[4].str_result = &tmail_subject;
    quick_widgets[4].text = mail_subject_last ? mail_subject_last : "";
    quick_widgets[6].str_result = &tmail_to;
    quick_widgets[6].text = mail_to_last ? mail_to_last : "";

    Quick_input.widgets = quick_widgets;

    if (quick_dialog (&Quick_input) != B_CANCEL) {
	g_free (mail_cc_last);
	g_free (mail_subject_last);
	g_free (mail_to_last);
	mail_cc_last = tmail_cc;
	mail_subject_last = tmail_subject;
	mail_to_last = tmail_to;
	pipe_mail (edit, mail_to_last, mail_subject_last, mail_cc_last);
    }
}


/*******************/
/* Word Completion */
/*******************/

static gboolean is_break_char(char c)
{
    return (isspace(c) || strchr("{}[]()<>=|/\\!?~'\",.;:#$%^&*", c));
}

/* find first character of current word */
static int edit_find_word_start (WEdit *edit, long *word_start, int *word_len)
{
    int i, c, last;

/* return if at begin of file */
    if (edit->curs1 <= 0)
	return 0;

    c = (unsigned char) edit_get_byte (edit, edit->curs1 - 1);
/* return if not at end or in word */
    if (is_break_char(c))
	return 0;

/* search start of word to be completed */
    for (i = 2;; i++) {
/* return if at begin of file */
	if (edit->curs1 - i < 0)
	    return 0;

	last = c;
	c = (unsigned char) edit_get_byte (edit, edit->curs1 - i);

	if (is_break_char(c)) {
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

#define MAX_WORD_COMPLETIONS 100	/* in listbox */

/* collect the possible completions */
static int
edit_collect_completions (WEdit *edit, long start, int word_len,
			  char *match_expr, struct selection *compl,
			  int *num)
{
    int max_len = 0, i, skip;
    gsize len = 0;
    GString *temp;
    mc_search_t *srch;

    srch = mc_search_new(match_expr, -1);
    if (srch == NULL)
        return 0;

    srch->search_type = MC_SEARCH_T_REGEX;
    srch->is_case_sentitive = TRUE;
    srch->search_fn = edit_search_cmd_callback;

    /* collect max MAX_WORD_COMPLETIONS completions */
    start--;
    while (*num < MAX_WORD_COMPLETIONS) {
	/* get next match */
	if (mc_search_run (srch, (void *) edit, start+1, edit->last_byte, &len) == FALSE)
	    break;
	start = srch->normal_offset;

	/* add matched completion if not yet added */
	temp = g_string_new("");
	for (i = 0; i < len; i++){
	    skip =  edit_get_byte(edit, start+i);
	    if (isspace(skip))
	        continue;
	    g_string_append_c (temp, skip);
	}

	skip = 0;

	for (i = 0; i < *num; i++) {
	    if (strncmp
		    (
			(char *) &compl[i].text[word_len],
			(char *) &temp->str[word_len],
			max (len, compl[i].len) - word_len
		    ) == 0) {
		skip = 1;
		break;		/* skip it, already added */
	    }
	}
	if (skip) {
	    g_string_free(temp, TRUE);
	    continue;
	}

	compl[*num].text = temp->str;
	compl[*num].len = temp->len;
	(*num)++;
	g_string_free(temp, FALSE);

	/* note the maximal length needed for the completion dialog */
	if (len > max_len)
	    max_len = len;
    }
    mc_search_free(srch);
    return max_len;
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
    unsigned char *bufpos;
    char *match_expr;
    struct selection compl[MAX_WORD_COMPLETIONS];	/* completions */

    /* search start of word to be completed */
    if (!edit_find_word_start (edit, &word_start, &word_len))
	return;

    /* prepare match expression */
    bufpos = &edit->buffers1[word_start >> S_EDIT_BUF_SIZE]
	[word_start & M_EDIT_BUF_SIZE];

    match_expr = g_strdup_printf ("(^|\\s)%.*s[^\\s\\.=\\+\\{\\}\\[\\]\\(\\)\\\\\\!\\,<>\\?\\/@#\\$%%\\^&\\*\\~\\|\\\"'\\:\\;]+", word_len, bufpos);

    /* collect the possible completions              */
    /* start search from begin to end of file */
    max_len =
	edit_collect_completions (edit, 0, word_len, match_expr,
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
	    editcmd_dialog_completion_show (edit, max_len, word_len,
				    (struct selection *) &compl,
				    num_compl);
	}
    }

    g_free (match_expr);
    /* release memory before return */
    for (i = 0; i < num_compl; i++)
	g_free (compl[i].text);

}

void
edit_select_codepage_cmd (WEdit *edit)
{
#ifdef HAVE_CHARSET
    if (do_select_codepage ()) {
	const char *cp_id;

	cp_id = get_codepage_id (source_codepage >= 0 ?
				    source_codepage : display_codepage);

	if (cp_id != NULL)
	    edit->utf8 = str_isutf8 (cp_id);
    }

    edit->force = REDRAW_COMPLETELY;
    edit_refresh_cmd (edit);
#endif
}

void
edit_insert_literal_cmd (WEdit *edit)
{
    int char_for_insertion =
	    editcmd_dialog_raw_key_query (_(" Insert Literal "),
				_(" Press any key: "), 0);
    edit_execute_key_command (edit, -1,
	ascii_alpha_to_cntrl (char_for_insertion));
}

void
edit_execute_macro_cmd (WEdit *edit)
{
    int command =
	    CK_Macro (editcmd_dialog_raw_key_query
		      (_(" Execute Macro "), _(" Press macro hotkey: "),
		       1));
    if (command == CK_Macro (0))
        command = CK_Insert_Char;

    edit_execute_key_command (edit, command, -1);
}

void
edit_begin_end_macro_cmd(WEdit *edit)
{
    int command;
    
    /* edit is a pointer to the widget */
    if (edit) {
	    command =
		edit->macro_i <
		0 ? CK_Begin_Record_Macro : CK_End_Record_Macro;
	    edit_execute_key_command (edit, command, -1);
    }
}

int
edit_load_forward_cmd (WEdit *edit)
{
    if (edit->modified) {
        if (edit_query_dialog2
            (_("Warning"),
             _(" Current text was modified without a file save. \n"
               " Continue discards these changes. "), _("C&ontinue"),
             _("&Cancel"))) {
            edit->force |= REDRAW_COMPLETELY;
            return 0;
        }
    }
    if ( edit_stack_iterator + 1 < MAX_HISTORY_MOVETO ) {
        if ( edit_history_moveto[edit_stack_iterator + 1].line < 1 ) {
            return 1;
        }
        edit_stack_iterator++;
        if ( edit_history_moveto[edit_stack_iterator].filename ) {
            edit_reload_line (edit, edit_history_moveto[edit_stack_iterator].filename,
                              edit_history_moveto[edit_stack_iterator].line);
            return 0;
        } else {
            return 1;
        }
    } else {
        return 1;
    }
}

int
edit_load_back_cmd (WEdit *edit)
{
    if (edit->modified) {
        if (edit_query_dialog2
            (_("Warning"),
             _(" Current text was modified without a file save. \n"
               " Continue discards these changes. "), _("C&ontinue"),
             _("&Cancel"))) {
            edit->force |= REDRAW_COMPLETELY;
            return 0;
        }
    }
    if ( edit_stack_iterator > 0 ) {
        edit_stack_iterator--;
        if ( edit_history_moveto[edit_stack_iterator].filename ) {
            edit_reload_line (edit, edit_history_moveto[edit_stack_iterator].filename,
                              edit_history_moveto[edit_stack_iterator].line);
            return 0;
        } else {
            return 1;
        }
    } else {
        return 1;
    }
}

void
edit_get_match_keyword_cmd (WEdit *edit)
{
    int word_len = 0;
    int num_def = 0;
    int max_len = 0;
    int i;
    long word_start = 0;
    unsigned char *bufpos;
    char *match_expr;
    char *path = NULL;
    char *ptr = NULL;
    char *tagfile = NULL;

    etags_hash_t def_hash[MAX_DEFINITIONS];

    for ( i = 0; i < MAX_DEFINITIONS; i++) {
        def_hash[i].filename = NULL;
    }

    /* search start of word to be completed */
    if (!edit_find_word_start (edit, &word_start, &word_len))
        return;

    /* prepare match expression */
    bufpos = &edit->buffers1[word_start >> S_EDIT_BUF_SIZE]
                            [word_start & M_EDIT_BUF_SIZE];
    match_expr = g_strdup_printf ("%.*s", word_len, bufpos);

    ptr = g_get_current_dir ();
    path = g_strconcat (ptr, G_DIR_SEPARATOR_S, (char *) NULL);
    g_free (ptr);

    /* Recursive search file 'TAGS' in parent dirs */
    do {
	ptr = g_path_get_dirname (path);
	g_free(path); path = ptr;
	g_free (tagfile);
	tagfile = g_build_filename (path, TAGS_NAME, (char *) NULL);
	if ( exist_file (tagfile) )
	    break;
    } while (strcmp( path, G_DIR_SEPARATOR_S) != 0);

    if (tagfile){
	num_def = etags_set_definition_hash(tagfile, path, match_expr, (etags_hash_t *) &def_hash);
	g_free (tagfile);
    }
    g_free (path);

    max_len = MAX_WIDTH_DEF_DIALOG;
    word_len = 0;
    if ( num_def > 0 ) {
        editcmd_dialog_select_definition_show (edit, match_expr, max_len, word_len,
                                       (etags_hash_t *) &def_hash,
                                       num_def);
    }
    g_free (match_expr);
}

void
edit_move_block_to_right (WEdit * edit)
{
    long start_mark, end_mark;
    long cur_bol, start_bol;

    if ( eval_marks (edit, &start_mark, &end_mark) )
        return;

    start_bol = edit_bol (edit, start_mark);
    cur_bol = edit_bol (edit, end_mark - 1);
    do {
        edit_cursor_move (edit, cur_bol - edit->curs1);
        if ( option_fill_tabs_with_spaces ) {
            if ( option_fake_half_tabs ) {
                insert_spaces_tab (edit, 1);
            } else {
                insert_spaces_tab (edit, 0);
            }
        } else {
            edit_insert (edit, '\t');
        }
        edit_cursor_move (edit, edit_bol (edit, cur_bol) - edit->curs1);
        if ( cur_bol == 0 ) {
            break;
        }
        cur_bol = edit_bol (edit, cur_bol - 1);
    } while (cur_bol >= start_bol) ;
    edit->force |= REDRAW_PAGE;
}

void
edit_move_block_to_left (WEdit * edit)
{
    long start_mark, end_mark;
    long cur_bol, start_bol;
    int i, del_tab_width;
    int next_char;

    if ( eval_marks (edit, &start_mark, &end_mark) )
        return;

    start_bol = edit_bol (edit, start_mark);
    cur_bol = edit_bol (edit, end_mark - 1);
    do {
        edit_cursor_move (edit, cur_bol - edit->curs1);
        if (option_fake_half_tabs) {
            del_tab_width = HALF_TAB_SIZE;
        } else {
            del_tab_width = option_tab_spacing;
        }
        next_char = edit_get_byte (edit, edit->curs1);
        if ( next_char == '\t' ) {
            edit_delete (edit, 1);
        } else if ( next_char == ' ' ) {
            for (i = 1; i <= del_tab_width; i++) {
                if ( next_char == ' ' ) {
                    edit_delete (edit, 1);
                }
                next_char = edit_get_byte (edit, edit->curs1);
            }
        }
        if ( cur_bol == 0 ) {
            break;
        }
        cur_bol = edit_bol (edit, cur_bol - 1);
    } while (cur_bol >= start_bol) ;
    edit->force |= REDRAW_PAGE;
}
