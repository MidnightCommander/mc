/* Main program for the Midnight Commander
   Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
   2003, 2004, 2005, 2006, 2007, 2009 Free Software Foundation, Inc.

   Written by: 1994, 1995, 1996, 1997 Miguel de Icaza
               1994, 1995 Janne Kukonlehto
	       1997 Norbert Warmuth

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

/** \file main.c
 *  \brief Source: this is a main module
 */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>

#include "global.h"

#include "../src/tty/tty.h"
#include "../src/tty/color.h"
#include "../src/tty/mouse.h"
#include "../src/tty/key.h"		/* For init_key() */
#include "../src/tty/win.h"		/* xterm_flag */

#include "dir.h"
#include "dialog.h"
#include "menu.h"
#include "panel.h"
#include "main.h"
#include "option.h"
#include "tree.h"
#include "treestore.h"
#include "cons.saver.h"
#include "subshell.h"
#include "setup.h"		/* save_setup() */
#include "../src/mcconfig/mcconfig.h"
#include "boxes.h"		/* sort_box() */
#include "layout.h"
#include "cmd.h"		/* Normal commands */
#include "hotlist.h"
#include "panelize.h"
#include "learn.h"		/* learn_keys() */
#include "listmode.h"
#include "execute.h"
#include "ext.h"		/* For flush_extension_file() */
#include "strutil.h"
#include <pwd.h>		/* for username in xterm title */

/* Listbox for the command history feature */
#include "widget.h"
#include "command.h"
#include "wtools.h"

#include "chmod.h"
#include "chown.h"
#include "achown.h"

#ifdef WITH_SMBFS
#include "../vfs/smbfs.h"	/* smbfs_set_debug() */
#endif

#ifdef USE_INTERNAL_EDIT
#   include "../edit/edit.h"
#endif

#ifdef	HAVE_CHARSET
#include "charsets.h"
#endif				/* HAVE_CHARSET */

#ifdef USE_VFS
#include "../vfs/gc.h"
#endif

#include "popt.h"

/* When the modes are active, left_panel, right_panel and tree_panel */
/* Point to a proper data structure.  You should check with the functions */
/* get_current_type and get_other_type the types of the panels before using */
/* This pointer variables */

/* The structures for the panels */
WPanel *left_panel = NULL;
WPanel *right_panel = NULL;

/* The pointer to the tree */
WTree *the_tree = NULL;

/* The Menubar */
struct WMenu *the_menubar = NULL;

/* Pointers to the selected and unselected panel */
WPanel *current_panel = NULL;

/* Set if the command is being run from the "Right" menu */
int is_right = 0;

/* Set when main loop should be terminated */
volatile int quit = 0;

/* Set if you want the possible completions dialog for the first time */
int show_all_if_ambiguous = 0;

/* Set when cd symlink following is desirable (bash mode) */
int cd_symlinks = 1;

/* If set then dialogs just clean the screen when refreshing, else */
/* they do a complete refresh, refreshing all the parts of the program */
int fast_refresh = 0;

/* If true, marking a files moves the cursor down */
int mark_moves_down = 1;

/* If true, at startup the user-menu is invoked */
int auto_menu = 0;

/* If true, use + and \ keys normally and select/unselect do if M-+ / M-\
   and M-- and keypad + / - */
int alternate_plus_minus = 0;

/* If true, then the +, - and \ keys have their special meaning only if the
 * command line is emtpy, otherwise they behave like regular letters
 */
int only_leading_plus_minus = 1;

int pause_after_run = pause_on_dumb_terminals;

/* It true saves the setup when quitting */
int auto_save_setup = 1;

#ifndef HAVE_CHARSET
/* If true, allow characters in the range 160-255 */
int eight_bit_clean = 1;

/*
 * If true, also allow characters in the range 128-159.
 * This is reported to break on many terminals (xterm, qansi-m).
 */
int full_eight_bits = 0;

#endif				/* !HAVE_CHARSET */

/*
 * If utf-8 terminal utf8_display = 1
 * Display bits set UTF-8
 *
*/
int utf8_display = 0;

/* If true use the internal viewer */
int use_internal_view = 1;

/* Have we shown the fast-reload warning in the past? */
int fast_reload_w = 0;

/* Move page/item? When clicking on the top or bottom of a panel */
int mouse_move_pages = 1;

/* If true: l&r arrows are used to chdir if the input line is empty */
int navigate_with_arrows = 0;

/* If true program softkeys (HP terminals only) on startup and after every
   command ran in the subshell to the description found in the termcap/terminfo
   database */
int reset_hp_softkeys = 0;

/* The prompt */
const char *prompt = NULL;

/* The widget where we draw the prompt */
WLabel *the_prompt;

/* The hint bar */
WLabel *the_hint;

/* The button bar */
WButtonBar *the_bar;

/* For slow terminals */
int slow_terminal = 0;

/* Mouse type: GPM, xterm or none */
Mouse_Type use_mouse_p = MOUSE_NONE;

/* If true, assume we are running on an xterm terminal */
static int force_xterm = 0;

/* If on, default for "No" in delete operations */
int safe_delete = 0;

/* Controls screen clearing before an exec */
int clear_before_exec = 1;

/* Asks for confirmation before deleting a file */
int confirm_delete = 1;

/* Asks for confirmation before deleting a hotlist entry */
int confirm_directory_hotlist_delete = 1;

/* Asks for confirmation before overwriting a file */
int confirm_overwrite = 1;

/* Asks for confirmation before executing a program by pressing enter */
int confirm_execute = 0;

/* Asks for confirmation before leaving the program */
int confirm_exit = 1;

/* Asks for confirmation when using F3 to view a directory and there
   are tagged files */
int confirm_view_dir = 0;

/* This flag indicates if the pull down menus by default drop down */
int drop_menus = 0;

/* if skip_check_codeset = 1 do not show warning about
 * system and display codeset is different
 */
int skip_check_codeset = 0;

/* The dialog handle for the main program */
Dlg_head *midnight_dlg = NULL;

/* Subshell: if set, then the prompt was not saved on CONSOLE_SAVE */
/* We need to paint it after CONSOLE_RESTORE, see: load_prompt */
int update_prompt = 0;

/* The home directory */
const char *home_dir = NULL;

/* The value of the other directory, only used when loading the setup */
char *other_dir = NULL;

/* Only used at program boot */
int boot_current_is_left = 1;

static char *this_dir = NULL;

/* If this is true, then when browsing the tree the other window will
 * automatically reload it's directory with the contents of the currently
 * selected directory.
 */
int xtree_mode = 0;

/* If set, then print to the given file the last directory we were at */
static char *last_wd_file = NULL;
static char *last_wd_string = NULL;
/* Set to 1 to suppress printing the last directory */
static int print_last_revert = 0;

/* colors specified on the command line: they override any other setting */
char *command_line_colors = NULL;

/* File name to view if argument was supplied */
static const char *view_one_file = NULL;

/* File name to edit if argument was supplied */
const char *edit_one_file = NULL;

/* Line to start the editor on */
static int edit_one_file_start_line = 0;

/* Used so that widgets know if they are being destroyed or
   shut down */
int midnight_shutdown = 0;

/* The user's shell */
const char *shell = NULL;

/* mc_home: The home of MC - /etc/mc or defined by MC_DATADIR */
char *mc_home = NULL;

/* mc_home_alt: Alternative home of MC - deprecated /usr/share/mc */
char *mc_home_alt = NULL;

char cmd_buf[512];

static void
reload_panelized (WPanel *panel)
{
    int i, j;
    dir_list *list = &panel->dir;

    if (panel != current_panel)
	mc_chdir (panel->cwd);

    for (i = 0, j = 0; i < panel->count; i++) {
	if (list->list[i].f.marked) {
	    /* Unmark the file in advance. In case the following mc_lstat
	     * fails we are done, else we have to mark the file again
	     * (Note: do_file_mark depends on a valid "list->list [i].buf").
	     * IMO that's the best way to update the panel's summary status
	     * -- Norbert
	     */
	    do_file_mark (panel, i, 0);
	}
	if (mc_lstat (list->list[i].fname, &list->list[i].st)) {
	    g_free (list->list[i].fname);
	    continue;
	}
	if (list->list[i].f.marked)
	    do_file_mark (panel, i, 1);
	if (j != i)
	    list->list[j] = list->list[i];
	j++;
    }
    if (j == 0)
	panel->count = set_zero_dir (list);
    else
	panel->count = j;

    if (panel != current_panel)
	mc_chdir (current_panel->cwd);
}

static void
update_one_panel_widget (WPanel *panel, int force_update,
			 const char *current_file)
{
    int free_pointer;
    char *my_current_file = NULL;

    if (force_update & UP_RELOAD) {
	panel->is_panelized = 0;
	mc_setctl (panel->cwd, VFS_SETCTL_FLUSH, 0);
	memset (&(panel->dir_stat), 0, sizeof (panel->dir_stat));
    }

    /* If current_file == -1 (an invalid pointer) then preserve selection */
    if (current_file == UP_KEEPSEL) {
	free_pointer = 1;
	my_current_file = g_strdup (panel->dir.list[panel->selected].fname);
	current_file = my_current_file;
    } else
	free_pointer = 0;

    if (panel->is_panelized)
	reload_panelized (panel);
    else
	panel_reload (panel);

    try_to_select (panel, current_file);
    panel->dirty = 1;

    if (free_pointer)
	g_free (my_current_file);
}

void
panel_clean_dir (WPanel *panel)
{
    int count = panel->count;

    panel->count = 0;
    panel->top_file = 0;
    panel->selected = 0;
    panel->marked = 0;
    panel->dirs_marked = 0;
    panel->total = 0;
    panel->searching = 0;
    panel->is_panelized = 0;
    panel->dirty = 1;

    clean_dir (&panel->dir, count);
}

static void
update_one_panel (int which, int force_update, const char *current_file)
{
    WPanel *panel;

    if (get_display_type (which) != view_listing)
	return;

    panel = (WPanel *) get_panel_widget (which);
    update_one_panel_widget (panel, force_update, current_file);
}

/* This routine reloads the directory in both panels. It tries to
 * select current_file in current_panel and other_file in other_panel.
 * If current_file == -1 then it automatically sets current_file and
 * other_file to the currently selected files in the panels.
 *
 * if force_update has the UP_ONLY_CURRENT bit toggled on, then it
 * will not reload the other panel.
*/
void
update_panels (int force_update, const char *current_file)
{
    int reload_other = !(force_update & UP_ONLY_CURRENT);
    WPanel *panel;

    update_one_panel (get_current_index (), force_update, current_file);
    if (reload_other)
	update_one_panel (get_other_index (), force_update, UP_KEEPSEL);

    if (get_current_type () == view_listing)
	panel = (WPanel *) get_panel_widget (get_current_index ());
    else
	panel = (WPanel *) get_panel_widget (get_other_index ());

    mc_chdir (panel->cwd);
}

/* Save current stat of directories to avoid reloading the panels */
/* when no modifications have taken place */
void
save_cwds_stat (void)
{
    if (fast_reload) {
	mc_stat (current_panel->cwd, &(current_panel->dir_stat));
	if (get_other_type () == view_listing)
	    mc_stat (other_panel->cwd, &(other_panel->dir_stat));
    }
}

#ifdef HAVE_SUBSHELL_SUPPORT
void
do_possible_cd (const char *new_dir)
{
    if (!do_cd (new_dir, cd_exact))
	message (D_ERROR, _("Warning"),
		 _(" The Commander can't change to the directory that \n"
		   " the subshell claims you are in.  Perhaps you have \n"
		   " deleted your working directory, or given yourself \n"
		   " extra access permissions with the \"su\" command? "));
}

void
do_update_prompt (void)
{
    if (update_prompt) {
	printf ("%s", subshell_prompt);
	fflush (stdout);
	update_prompt = 0;
    }
}
#endif				/* HAVE_SUBSHELL_SUPPORT */

void
change_panel (void)
{
    free_completions (cmdline);
    dlg_one_down (midnight_dlg);
}

/* Stop MC main dialog and the current dialog if it exists.
 * Needed to provide fast exit from MC viewer or editor on shell exit */
static void
stop_dialogs (void)
{
    midnight_dlg->running = 0;
    if (current_dlg) {
	current_dlg->running = 0;
    }
}

static int
quit_cmd_internal (int quiet)
{
    int q = quit;

    if (quiet || !confirm_exit) {
	q = 1;
    } else {
	if (query_dialog
	    (_(" The Midnight Commander "),
	     _(" Do you really want to quit the Midnight Commander? "), D_NORMAL,
	     2, _("&Yes"), _("&No")) == 0)
	    q = 1;
    }
    if (q) {
#ifdef HAVE_SUBSHELL_SUPPORT
	if (!use_subshell)
	    stop_dialogs ();
	else if ((q = exit_subshell ()))
#endif
	    stop_dialogs ();
    }
    if (q)
	quit |= 1;
    return quit;
}

static void
quit_cmd (void)
{
    quit_cmd_internal (0);
}

void
quiet_quit_cmd (void)
{
    print_last_revert = 1;
    quit_cmd_internal (1);
}

/*
 * Touch window and refresh window functions
 */

/* This routine untouches the first line on both panels in order */
/* to avoid the refreshing the menu bar */

void
repaint_screen (void)
{
    do_refresh ();
    tty_refresh ();
}

/* Wrapper for do_subshell_chdir, check for availability of subshell */
void
subshell_chdir (const char *directory)
{
#ifdef HAVE_SUBSHELL_SUPPORT
    if (use_subshell) {
	if (vfs_current_is_local ())
	    do_subshell_chdir (directory, 0, 1);
    }
#endif				/* HAVE_SUBSHELL_SUPPORT */
}

void
directory_history_add (struct WPanel *panel, const char *dir)
{
    char *tmp;

    tmp = g_strdup (dir);
    strip_password (tmp, 1);

    panel->dir_history = list_append_unique (panel->dir_history, tmp);
}

/*
 *  If we moved to the parent directory move the selection pointer to
 *  the old directory name; If we leave VFS dir, remove FS specificator.
 *
 *  You do _NOT_ want to add any vfs aware code here. <pavel@ucw.cz>
 */
static const char *
get_parent_dir_name (const char *cwd, const char *lwd)
{
    const char *p;
    if (strlen (lwd) > strlen (cwd))
	if ((p = strrchr (lwd, PATH_SEP)) && !strncmp (cwd, lwd, p - lwd) &&
	 ((gsize)strlen (cwd) == (gsize) p - (gsize) lwd || (p == lwd && cwd[0] == PATH_SEP &&
	  cwd[1] == '\0'))) {
	    return (p + 1);
	}
    return NULL;
}

/*
 * Changes the current directory of the panel.
 * Don't record change in the directory history.
 */
static int
_do_panel_cd (WPanel *panel, const char *new_dir, enum cd_enum cd_type)
{
    const char *directory;
    char *olddir;
    char temp[MC_MAXPATHLEN];
    char *translated_url;

    if (cd_type == cd_parse_command) {
	while (*new_dir == ' ')
	    new_dir++;
    }

    olddir = g_strdup (panel->cwd);
    new_dir = translated_url = vfs_translate_url (new_dir);

    /* Convert *new_path to a suitable pathname, handle ~user */

    if (cd_type == cd_parse_command) {
	if (!strcmp (new_dir, "-")) {
	    strcpy (temp, panel->lwd);
	    new_dir = temp;
	}
    }
    directory = *new_dir ? new_dir : home_dir;

    if (mc_chdir (directory) == -1) {
	strcpy (panel->cwd, olddir);
	g_free (olddir);
	g_free (translated_url);
	return 0;
    }
    g_free (translated_url);

    /* Success: save previous directory, shutdown status of previous dir */
    strcpy (panel->lwd, olddir);
    free_completions (cmdline);

    mc_get_current_wd (panel->cwd, sizeof (panel->cwd) - 2);

    vfs_release_path (olddir);

    subshell_chdir (panel->cwd);

    /* Reload current panel */
    panel_clean_dir (panel);
    panel->count =
	do_load_dir (panel->cwd, &panel->dir, panel->sort_type,
		     panel->reverse, panel->case_sensitive,
		     panel->exec_first, panel->filter);
    try_to_select (panel, get_parent_dir_name (panel->cwd, olddir));
    load_hint (0);
    panel->dirty = 1;
    update_xterm_title_path ();

    g_free (olddir);

    return 1;
}

/*
 * Changes the current directory of the panel.
 * Record change in the directory history.
 */
int
do_panel_cd (struct WPanel *panel, const char *new_dir, enum cd_enum cd_type)
{
    int r;

    r = _do_panel_cd (panel, new_dir, cd_type);
    if (r)
	directory_history_add (panel, panel->cwd);
    return r;
}

int
do_cd (const char *new_dir, enum cd_enum exact)
{
    return (do_panel_cd (current_panel, new_dir, exact));
}

void
directory_history_next (WPanel *panel)
{
    GList *nextdir;

    nextdir = g_list_next (panel->dir_history);

    if (!nextdir)
	return;

    if (_do_panel_cd (panel, (char *) nextdir->data, cd_exact))
	panel->dir_history = nextdir;
}

void
directory_history_prev (WPanel *panel)
{
    GList *prevdir;

    prevdir = g_list_previous (panel->dir_history);

    if (!prevdir)
	return;

    if (_do_panel_cd (panel, (char *) prevdir->data, cd_exact))
	panel->dir_history = prevdir;
}

void
directory_history_list (WPanel *panel)
{
    char *s;

    if (!panel->dir_history)
	return;

    s = show_hist (panel->dir_history, panel->widget.x, panel->widget.y);

    if (!s)
	return;

    if (_do_panel_cd (panel, s, cd_exact))
	directory_history_add (panel, panel->cwd);
    else
	message (D_ERROR, MSG_ERROR, _("Cannot change directory"));
    g_free (s);
}

#ifdef HAVE_SUBSHELL_SUPPORT
int
load_prompt (int fd, void *unused)
{
    (void) fd;
    (void) unused;

    if (!read_subshell_prompt ())
	return 0;

    /* Don't actually change the prompt if it's invisible */
    if (current_dlg == midnight_dlg && command_prompt) {
	char *tmp_prompt;
	int prompt_len;

	tmp_prompt = strip_ctrl_codes (subshell_prompt);
	prompt_len = str_term_width1 (tmp_prompt);

	/* Check for prompts too big */
	if (COLS > 8 && prompt_len > COLS - 8) {
	    tmp_prompt[COLS - 8] = '\0';
	    prompt_len = COLS - 8;
	}
	prompt = tmp_prompt;
	label_set_text (the_prompt, prompt);
	winput_set_origin ((WInput *) cmdline, prompt_len,
			   COLS - prompt_len);

	/* since the prompt has changed, and we are called from one of the
	 * get_event channels, the prompt updating does not take place
	 * automatically: force a cursor update and a screen refresh
	 */
	update_cursor (midnight_dlg);
	tty_refresh ();
    }
    update_prompt = 1;
    return 0;
}
#endif				/* HAVE_SUBSHELL_SUPPORT */

/* Used to emulate Lynx's entering leaving a directory with the arrow keys */
int
maybe_cd (int move_up_dir)
{
    if (navigate_with_arrows) {
	if (!cmdline->buffer[0]) {
	    if (move_up_dir) {
		do_cd ("..", cd_exact);
		return 1;
	    }
	    if (S_ISDIR (selection (current_panel)->st.st_mode)
		|| link_isdir (selection (current_panel))) {
		do_cd (selection (current_panel)->fname, cd_exact);
		return 1;
	    }
	}
    }
    return 0;
}

static void
sort_cmd (void)
{
    WPanel *p;
    sortfn *sort_order;

    if (!SELECTED_IS_PANEL)
	return;

    p = MENU_PANEL;
    sort_order = sort_box (p->sort_type, &p->reverse,
			   &p->case_sensitive,
			   &p->exec_first);

    panel_set_sort_order (p, sort_order);

}

static void
treebox_cmd (void)
{
    char *sel_dir;

    sel_dir = tree_box (selection (current_panel)->fname);
    if (sel_dir) {
	do_cd (sel_dir, cd_exact);
	g_free (sel_dir);
    }
}

#ifdef LISTMODE_EDITOR
static void
listmode_cmd (void)
{
    char *newmode;

    if (get_current_type () != view_listing)
	return;

    newmode = listmode_edit (current_panel->user_format);
    if (!newmode)
	return;

    g_free (current_panel->user_format);
    current_panel->list_type = list_user;
    current_panel->user_format = newmode;
    set_panel_formats (current_panel);

    do_refresh ();
}
#endif				/* LISTMODE_EDITOR */

/* NOTICE: hotkeys specified here are overriden in menubar_paint_idx (alex) */
static menu_entry LeftMenu[] = {
    {' ', N_("&Listing mode..."), NULL_HOTKEY, listing_cmd},
    {' ', N_("&Quick view     C-x q"), NULL_HOTKEY, quick_view_cmd},
    {' ', N_("&Info           C-x i"), NULL_HOTKEY, info_cmd},
    {' ', N_("&Tree"), NULL_HOTKEY, tree_cmd},
    {' ', "", NULL_HOTKEY, 0},
    {' ', N_("&Sort order..."), NULL_HOTKEY, sort_cmd},
    {' ', "", NULL_HOTKEY, 0},
    {' ', N_("&Filter..."), NULL_HOTKEY, filter_cmd},
#ifdef HAVE_CHARSET
    {' ', "",NULL_HOTKEY, 0},
    {' ', N_("&Encoding...    C-t"), NULL_HOTKEY, encoding_cmd},
#endif
#ifdef USE_NETCODE
    {' ', "", NULL_HOTKEY, 0},
#ifdef ENABLE_VFS_MCFS
    {' ', N_("&Network link..."), NULL_HOTKEY, netlink_cmd},
#endif
    {' ', N_("FT&P link..."), NULL_HOTKEY, ftplink_cmd},
    {' ', N_("S&hell link..."), NULL_HOTKEY, fishlink_cmd},
#ifdef WITH_SMBFS
    {' ', N_("SM&B link..."), NULL_HOTKEY, smblink_cmd},
#endif
#endif
    {' ', "", NULL_HOTKEY, 0},
    {' ', N_("&Rescan         C-r"), NULL_HOTKEY, reread_cmd}
};

static menu_entry RightMenu[] = {
    {' ', N_("&Listing mode..."), NULL_HOTKEY, listing_cmd},
    {' ', N_("&Quick view     C-x q"), NULL_HOTKEY, quick_view_cmd},
    {' ', N_("&Info           C-x i"), NULL_HOTKEY, info_cmd},
    {' ', N_("&Tree"), NULL_HOTKEY, tree_cmd},
    {' ', "", NULL_HOTKEY, 0},
    {' ', N_("&Sort order..."), NULL_HOTKEY, sort_cmd},
    {' ', "", NULL_HOTKEY, 0},
    {' ', N_("&Filter..."), NULL_HOTKEY, filter_cmd},
#ifdef HAVE_CHARSET
    {' ', "",NULL_HOTKEY, 0},
    {' ', N_("&Encoding...    C-t"), NULL_HOTKEY, encoding_cmd},
#endif
#ifdef USE_NETCODE
    {' ', "", NULL_HOTKEY, 0},
#ifdef ENABLE_VFS_MCFS
    {' ', N_("&Network link..."), NULL_HOTKEY, netlink_cmd},
#endif
    {' ', N_("FT&P link..."), NULL_HOTKEY, ftplink_cmd},
    {' ', N_("S&hell link..."), NULL_HOTKEY, fishlink_cmd},
#ifdef WITH_SMBFS
    {' ', N_("SM&B link..."), NULL_HOTKEY, smblink_cmd},
#endif
#endif
    {' ', "", NULL_HOTKEY, 0},
    {' ', N_("&Rescan         C-r"), NULL_HOTKEY, reread_cmd}
};

static menu_entry FileMenu[] = {
    {' ', N_("&View               F3"), NULL_HOTKEY, view_cmd},
    {' ', N_("Vie&w file...         "), NULL_HOTKEY, view_file_cmd},
    {' ', N_("&Filtered view     M-!"), NULL_HOTKEY, filtered_view_cmd},
    {' ', N_("&Edit               F4"), NULL_HOTKEY, edit_cmd},
    {' ', N_("&Copy               F5"), NULL_HOTKEY, copy_cmd},
    {' ', N_("c&Hmod           C-x c"), NULL_HOTKEY, chmod_cmd},
    {' ', N_("&Link            C-x l"), NULL_HOTKEY, link_cmd},
    {' ', N_("&SymLink         C-x s"), NULL_HOTKEY, symlink_cmd},
    {' ', N_("edit s&Ymlink  C-x C-s"), NULL_HOTKEY, edit_symlink_cmd},
    {' ', N_("ch&Own           C-x o"), NULL_HOTKEY, chown_cmd},
    {' ', N_("&Advanced chown       "), NULL_HOTKEY, chown_advanced_cmd},
    {' ', N_("&Rename/Move        F6"), NULL_HOTKEY, ren_cmd},
    {' ', N_("&Mkdir              F7"), NULL_HOTKEY, mkdir_cmd},
    {' ', N_("&Delete             F8"), NULL_HOTKEY, delete_cmd},
    {' ', N_("&Quick cd          M-c"), NULL_HOTKEY, quick_cd_cmd},
    {' ', "", NULL_HOTKEY, 0},
    {' ', N_("select &Group      M-+"), NULL_HOTKEY, select_cmd},
    {' ', N_("u&Nselect group    M-\\"), NULL_HOTKEY, unselect_cmd},
    {' ', N_("reverse selec&Tion M-*"), NULL_HOTKEY, reverse_selection_cmd},
    {' ', "", NULL_HOTKEY, 0},
    {' ', N_("e&Xit              F10"), NULL_HOTKEY, quit_cmd}
};

static menu_entry CmdMenu[] = {
    {' ', N_("&User menu          F2"), NULL_HOTKEY, user_file_menu_cmd},
    /* I know, I'm lazy, but the tree widget when it's not running
     * as a panel still has some problems, I have not yet finished
     * the WTree widget port, sorry.
     */
    {' ', N_("&Directory tree"), NULL_HOTKEY, treebox_cmd},
    {' ', N_("&Find file            M-?"), NULL_HOTKEY, find_cmd},
    {' ', N_("s&Wap panels          C-u"), NULL_HOTKEY, swap_cmd},
    {' ', N_("switch &Panels on/off C-o"), NULL_HOTKEY, view_other_cmd},
    {' ', N_("&Compare directories  C-x d"), NULL_HOTKEY, compare_dirs_cmd},
    {' ', N_("e&Xternal panelize    C-x !"), NULL_HOTKEY, external_panelize},
    {' ', N_("show directory s&Izes"), NULL_HOTKEY, dirsizes_cmd},
    {' ', "", NULL_HOTKEY, 0},
    {' ', N_("Command &history      M-h"), NULL_HOTKEY, history_cmd},
    {' ', N_("di&Rectory hotlist    C-\\"), NULL_HOTKEY, quick_chdir_cmd},
#ifdef USE_VFS
    {' ', N_("&Active VFS list      C-x a"), NULL_HOTKEY, reselect_vfs},
#endif
#ifdef WITH_BACKGROUND
    {' ', N_("&Background jobs      C-x j"), NULL_HOTKEY, jobs_cmd},
#endif
    {' ', "", NULL_HOTKEY, 0},
#ifdef USE_EXT2FSLIB
    {' ', N_("&Undelete files (ext2fs only)"), NULL_HOTKEY, undelete_cmd},
#endif
#ifdef LISTMODE_EDITOR
    {' ', N_("&Listing format edit"), NULL_HOTKEY, listmode_cmd},
#endif
#if defined (USE_EXT2FSLIB) || defined (LISTMODE_EDITOR)
    {' ', "", NULL_HOTKEY, 0},
#endif
    {' ', N_("Edit &extension file"), NULL_HOTKEY, ext_cmd},
    {' ', N_("Edit &menu file"), NULL_HOTKEY, edit_mc_menu_cmd}
};

/* Must keep in sync with the constants in menu_cmd */
static menu_entry OptMenu[] = {
    {' ', N_("&Configuration..."), NULL_HOTKEY, configure_box},
    {' ', N_("&Layout..."), NULL_HOTKEY, layout_cmd},
    {' ', N_("c&Onfirmation..."), NULL_HOTKEY, confirm_box},
    {' ', N_("&Display bits..."), NULL_HOTKEY, display_bits_box},
    {' ', N_("learn &Keys..."), NULL_HOTKEY, learn_keys},
#ifdef USE_VFS
    {' ', N_("&Virtual FS..."), NULL_HOTKEY, configure_vfs},
#endif				/* !USE_VFS */
    {' ', "", NULL_HOTKEY, 0},
    {' ', N_("&Save setup"), NULL_HOTKEY, save_setup_cmd}
};

#define menu_entries(x) sizeof(x)/sizeof(menu_entry)

static Menu *MenuBar[5];

void
init_menu (void)
{
    MenuBar[0] =
	create_menu (horizontal_split ? _(" &Above ") : _(" &Left "),
		     LeftMenu, menu_entries (LeftMenu),
		     "[Left and Right Menus]");
    MenuBar[1] =
	create_menu (_(" &File "), FileMenu, menu_entries (FileMenu),
		     "[File Menu]");
    MenuBar[2] =
	create_menu (_(" &Command "), CmdMenu, menu_entries (CmdMenu),
		     "[Command Menu]");
    MenuBar[3] =
	create_menu (_(" &Options "), OptMenu, menu_entries (OptMenu),
		     "[Options Menu]");
    MenuBar[4] =
	create_menu (horizontal_split ? _(" &Below ") : _(" &Right "),
		     RightMenu, menu_entries (RightMenu),
		     "[Left and Right Menus]");
}

void
done_menu (void)
{
    int i;

    for (i = 0; i < 5; i++) {
	destroy_menu (MenuBar[i]);
    }
}

static void
menu_last_selected_cmd (void)
{
    the_menubar->active = 1;
    the_menubar->dropped = drop_menus;
    the_menubar->previous_widget = midnight_dlg->current->dlg_id;
    dlg_select_widget (the_menubar);
}

static void
menu_cmd (void)
{
    if (the_menubar->active)
	return;

    if ((get_current_index () == 0) ^ (!current_panel->active))
	the_menubar->selected = 0;
    else
	the_menubar->selected = 4;
    menu_last_selected_cmd ();
}

/* Flag toggling functions */
void
toggle_fast_reload (void)
{
    fast_reload = !fast_reload;
    if (fast_reload_w == 0 && fast_reload) {
	message (D_NORMAL, _(" Information "),
		 _
		 (" Using the fast reload option may not reflect the exact \n"
		  " directory contents. In this case you'll need to do a   \n"
		  " manual reload of the directory. See the man page for   \n"
		  " the details.                                           "));
	fast_reload_w = 1;
    }
}

void
toggle_mix_all_files (void)
{
    mix_all_files = !mix_all_files;
    update_panels (UP_RELOAD, UP_KEEPSEL);
}

void
toggle_show_backup (void)
{
    show_backups = !show_backups;
    update_panels (UP_RELOAD, UP_KEEPSEL);
}

void
toggle_show_hidden (void)
{
    show_dot_files = !show_dot_files;
    update_panels (UP_RELOAD, UP_KEEPSEL);
}

/*
 * Just a hack for allowing url-like pathnames to be accepted from the
 * command line.
 */
static void
translated_mc_chdir (char *dir)
{
    char *newdir;

    newdir = vfs_translate_url (dir);
    mc_chdir (newdir);
    g_free (newdir);
}

static void
create_panels (void)
{
    int current_index;
    int other_index;
    int current_mode;
    int other_mode;
    char original_dir[1024];

    original_dir[0] = 0;

    if (boot_current_is_left) {
	current_index = 0;
	other_index = 1;
	current_mode = startup_left_mode;
	other_mode = startup_right_mode;
    } else {
	current_index = 1;
	other_index = 0;
	current_mode = startup_right_mode;
	other_mode = startup_left_mode;
    }
    /* Creates the left panel */
    if (this_dir) {
	if (other_dir) {
	    /* Ok, user has specified two dirs, save the original one,
	     * since we may not be able to chdir to the proper
	     * second directory later
	     */
	    mc_get_current_wd (original_dir, sizeof (original_dir) - 2);
	}
	translated_mc_chdir (this_dir);
    }
    set_display_type (current_index, current_mode);

    /* The other panel */
    if (other_dir) {
	if (original_dir[0])
	    translated_mc_chdir (original_dir);
	translated_mc_chdir (other_dir);
    }
    set_display_type (other_index, other_mode);

    if (startup_left_mode == view_listing) {
	current_panel = left_panel;
    } else {
	if (right_panel)
	    current_panel = right_panel;
	else
	    current_panel = left_panel;
    }

    /* Create the nice widgets */
    cmdline = command_new (0, 0, 0);
    the_prompt = label_new (0, 0, prompt);
    the_prompt->transparent = 1;
    the_bar = buttonbar_new (keybar_visible);

    the_hint = label_new (0, 0, 0);
    the_hint->transparent = 1;
    the_hint->auto_adjust_cols = 0;
    the_hint->widget.cols = COLS;

    the_menubar = menubar_new (0, 0, COLS, MenuBar, 5);
}

static void
copy_current_pathname (void)
{
    char *cwd_path;
    if (!command_prompt)
	return;

    cwd_path = remove_encoding_from_path (current_panel->cwd);
    command_insert (cmdline, cwd_path, 0);

    if (cwd_path [strlen (cwd_path ) - 1] != PATH_SEP)
	command_insert (cmdline, PATH_SEP_STR, 0);
    g_free (cwd_path);
}

static void
copy_other_pathname (void)
{
    char *cwd_path;

    if (get_other_type () != view_listing)
	return;

    if (!command_prompt)
	return;

    cwd_path = remove_encoding_from_path (other_panel->cwd);
    command_insert (cmdline, cwd_path, 0);

    if (cwd_path [strlen (cwd_path ) - 1] != PATH_SEP)
	command_insert (cmdline, PATH_SEP_STR, 0);
    g_free (cwd_path);
}

static void
copy_readlink (WPanel *panel)
{
    if (!command_prompt)
	return;
    if (S_ISLNK (selection (panel)->st.st_mode)) {
	char buffer[MC_MAXPATHLEN];
	char *p =
	    concat_dir_and_file (panel->cwd, selection (panel)->fname);
	int i;

	i = mc_readlink (p, buffer, MC_MAXPATHLEN - 1);
	g_free (p);
	if (i > 0) {
	    buffer[i] = 0;
	    command_insert (cmdline, buffer, 1);
	}
    }
}

static void
copy_current_readlink (void)
{
    copy_readlink (current_panel);
}

static void
copy_other_readlink (void)
{
    if (get_other_type () != view_listing)
	return;
    copy_readlink (other_panel);
}

/* Insert the selected file name into the input line */
static void
copy_prog_name (void)
{
    char *tmp;
    if (!command_prompt)
	return;

    if (get_current_type () == view_tree) {
	WTree *tree = (WTree *) get_panel_widget (get_current_index ());
	tmp = tree_selected_name (tree);
    } else
	tmp = selection (current_panel)->fname;

    command_insert (cmdline, tmp, 1);
}

static void
copy_tagged (WPanel *panel)
{
    int i;

    if (!command_prompt)
	return;
    input_disable_update (cmdline);
    if (panel->marked) {
	for (i = 0; i < panel->count; i++) {
	    if (panel->dir.list[i].f.marked)
		command_insert (cmdline, panel->dir.list[i].fname, 1);
	}
    } else {
	command_insert (cmdline, panel->dir.list[panel->selected].fname,
			1);
    }
    input_enable_update (cmdline);
}

static void
copy_current_tagged (void)
{
    copy_tagged (current_panel);
}

static void
copy_other_tagged (void)
{
    if (get_other_type () != view_listing)
	return;
    copy_tagged (other_panel);
}

static void
init_labels (void)
{
    buttonbar_set_label (midnight_dlg, 1, _("Help"), help_cmd);
    buttonbar_set_label (midnight_dlg, 2, _("Menu"), user_file_menu_cmd);
    buttonbar_set_label (midnight_dlg, 9, _("PullDn"), menu_cmd);
    buttonbar_set_label (midnight_dlg, 10, _("Quit"), quit_cmd);
}

static const key_map ctl_x_map[] = {
    {XCTRL ('c'), quit_cmd},
    {'d', compare_dirs_cmd},
#ifdef USE_VFS
    {'a', reselect_vfs},
#endif				/* USE_VFS */
    {'p', copy_current_pathname},
    {XCTRL ('p'), copy_other_pathname},
    {'t', copy_current_tagged},
    {XCTRL ('t'), copy_other_tagged},
    {'c', chmod_cmd},
    {'o', chown_cmd},
    {'r', copy_current_readlink},
    {XCTRL ('r'), copy_other_readlink},
    {'l', link_cmd},
    {'s', symlink_cmd},
    {XCTRL ('s'), edit_symlink_cmd},
    {'i', info_cmd_no_menu},
    {'q', quick_cmd_no_menu},
    {'h', add2hotlist_cmd},
    {'!', external_panelize},
#ifdef WITH_BACKGROUND
    {'j', jobs_cmd},
#endif				/* WITH_BACKGROUND */
    {0, 0}
};

static int ctl_x_map_enabled = 0;

static void
ctl_x_cmd (void)
{
    ctl_x_map_enabled = 1;
}

static void
nothing (void)
{
}

static const key_map default_map[] = {
    {KEY_F (19), menu_last_selected_cmd},
    {KEY_F (20), quiet_quit_cmd},

    {XCTRL ('@'), smart_dirsize_cmd},

    /* Copy useful information to the command line */
    {ALT ('a'), copy_current_pathname},
    {ALT ('A'), copy_other_pathname},

    {ALT ('c'), quick_cd_cmd},

    /* To access the directory hotlist */
    {XCTRL ('\\'), quick_chdir_cmd},

    /* Suspend */
    {XCTRL ('z'), suspend_cmd},

    /* The filtered view command */
    {ALT ('!'), filtered_view_cmd},

    /* Find file */
    {ALT ('?'), find_cmd},

    /* Panel refresh */
    {XCTRL ('r'), reread_cmd},

    /* Toggle listing between long, user defined and full formats */
    {ALT ('t'), toggle_listing_cmd},

    /* Swap panels */
    {XCTRL ('u'), swap_cmd},

    /* View output */
    {XCTRL ('o'), view_other_cmd},

    /* Control-X keybindings */
    {XCTRL ('x'), ctl_x_cmd},

    /* Show/hide hidden files */
    {ALT ('.'), toggle_show_hidden},

    /* Trap dlg's exit commands */
    {ESC_CHAR, nothing},
    {XCTRL ('c'), nothing},
    {XCTRL ('g'), nothing},
    {0, 0},
};

static void
setup_sigwinch (void)
{
#if (defined(HAVE_SLANG) || (NCURSES_VERSION_MAJOR >= 4)) && defined(SIGWINCH)
    struct sigaction act, oact;
    act.sa_handler = flag_winch;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
#ifdef SA_RESTART
    act.sa_flags |= SA_RESTART;
#endif
    sigaction (SIGWINCH, &act, &oact);
#endif
}

static void
setup_pre (void)
{
    /* Call all the inits */
#ifdef HAVE_CHARSET
/*
 * Don't restrict the output on the screen manager level,
 * the translation tables take care of it.
 */
#define full_eight_bits (1)
#define eight_bit_clean (1)
#endif				/* !HAVE_CHARSET */

#ifndef HAVE_SLANG
    meta (stdscr, eight_bit_clean);
#else
    SLsmg_Display_Eight_Bit = full_eight_bits ? 128 : 160;
#endif
}

static void
init_xterm_support (void)
{
    const char *termvalue;

    termvalue = getenv ("TERM");
    if (!termvalue || !(*termvalue)) {
	fputs (_("The TERM environment variable is unset!\n"), stderr);
	exit (1);
    }

    /* Check mouse capabilities */
    xmouse_seq = tty_tgetstr ("Km");

    if (strcmp (termvalue, "cygwin") == 0) {
	force_xterm = 1;
	use_mouse_p = MOUSE_DISABLED;
    }

    if (force_xterm || strncmp (termvalue, "xterm", 5) == 0
	|| strncmp (termvalue, "rxvt", 4) == 0
	|| strcmp (termvalue, "Eterm") == 0
	|| strcmp (termvalue, "dtterm") == 0) {
	xterm_flag = 1;

	/* Default to the standard xterm sequence */
	if (!xmouse_seq) {
	    xmouse_seq = ESC_STR "[M";
	}

	/* Enable mouse unless explicitly disabled by --nomouse */
	if (use_mouse_p != MOUSE_DISABLED) {
	    const char *color_term = getenv ("COLORTERM");
	    if (strncmp (termvalue, "rxvt", 4) == 0 ||
		(color_term != NULL && strncmp (color_term, "rxvt", 4) == 0) ||
		strcmp (termvalue, "Eterm") == 0) {
		    use_mouse_p = MOUSE_XTERM_NORMAL_TRACKING;
	    } else {
		use_mouse_p = MOUSE_XTERM_BUTTON_EVENT_TRACKING;
	    }
	}
    }
}

static void
setup_mc (void)
{
    setup_pre ();
    init_menu ();
    create_panels ();
    setup_panels ();

#ifdef HAVE_SUBSHELL_SUPPORT
    if (use_subshell)
	add_select_channel (subshell_pty, load_prompt, 0);
#endif				/* !HAVE_SUBSHELL_SUPPORT */

    setup_sigwinch ();

    verbose = !((tty_baudrate () < 9600) || slow_terminal);

    init_mouse ();
}

static void
setup_dummy_mc ()
{
    char d[MC_MAXPATHLEN];

    mc_get_current_wd (d, MC_MAXPATHLEN);
    setup_mc ();
    mc_chdir (d);
}

static void check_codeset()
{
    const char *_system_codepage = NULL;
#ifdef HAVE_CHARSET
    const char *_source_codepage = NULL;
    const char *_display_codepage = NULL;
    int profile_changed = 0;

#define CONFY 16
#define CONFX 54

    if ( !skip_check_codeset ) {

        QuickWidget ecs_widgets [] = {
        { quick_button, 4, 6, 13, CONFY, N_("&Skip"),
          0, B_EXIT, 0, 0, NULL , NULL, NULL},
        { quick_button, 1, 11, 13, CONFY, N_("&Fix it"),
          0, B_ENTER, 0, 0, NULL , NULL, NULL},
        { quick_checkbox, 1, 13, 11, CONFY, N_("don't ask again"),
          11, 0, &skip_check_codeset, NULL, NULL , NULL, NULL},
        { quick_label, 2, 30, 3, CONFY, N_("Chosen display charset (Settings->Display bits)\n"
                                           "or source codeset (in mcedit ctrl-t) \n"
                                           "does not match one set via locale. \n"
                                           "Set correct codeset manually or press <<Fix it>> \n"
                                           "to set locale default.\n\n"
                                           "Or set \'don't ask again\' and press <<Skip>>"),
          0, 0, 0, 0, NULL , NULL, NULL},

          NULL_QuickWidget
        };

        QuickDialog ecs =
        { CONFX, CONFY, -1, -1, N_(" Confirmation "), "[Confirmation]",
            ecs_widgets, 0
        };


        _system_codepage = str_detect_termencoding();
        _source_codepage = get_codepage_id (source_codepage);
        _display_codepage = get_codepage_id (display_codepage);

        if ( (strcmp (_system_codepage, _display_codepage)) ||
             (strcmp (_system_codepage, _source_codepage)) ) {
            if (quick_dialog (&ecs) == B_ENTER){
                display_codepage = get_codepage_index (_system_codepage);
                cp_display = get_codepage_id (display_codepage);
                if ( !strcmp (cp_display, _system_codepage)) {
                    mc_config_set_string(mc_main_config, "Misc", "display_codepage", cp_display);
                    mc_config_set_string(mc_main_config, "Misc", "source_codepage", cp_display);
                    display_codepage = get_codepage_index ( cp_display );
                    utf8_display = str_isutf8 (_system_codepage);
                    source_codepage = display_codepage;
                    cp_source = cp_display;
                    profile_changed = 1;
                }
            } else {
                if ( skip_check_codeset ) {
                    mc_config_set_int(mc_main_config, "Midnight-Commander", "skip_check_codeset", 1);
                    profile_changed = 1;
                }
            }
        }
        if ( profile_changed )
            save_configure ();
    }
#else /* HAVE_CHARSET */
    _system_codepage = str_detect_termencoding();
    utf8_display = str_isutf8 (_system_codepage);
#endif /* HAVE_CHARSET */
}

static void
done_screen (void)
{
    if (!(quit & SUBSHELL_EXIT))
	clr_scr ();
    tty_reset_shell_mode ();
    tty_noraw_mode ();
    tty_keypad (FALSE);
    tty_colors_done ();
}

static void
done_mc (void)
{
    disable_mouse ();

    done_menu ();

    /* Setup shutdown
     *
     * We sync the profiles since the hotlist may have changed, while
     * we only change the setup data if we have the auto save feature set
     */

    if (auto_save_setup)
	save_setup ();		/* does also call save_hotlist */
    else {
	save_hotlist ();
	save_panel_types ();
    }
    done_screen ();
    vfs_add_current_stamps ();
}

/* This should be called after destroy_dlg since panel widgets
 *  save their state on the profiles
 */
static void
done_mc_profile (void)
{
    done_setup ();
}

static cb_ret_t
midnight_callback (struct Dlg_head *h, dlg_msg_t msg, int parm)
{
    int i;

    switch (msg) {

    case DLG_IDLE:
	/* We only need the first idle event */
	set_idle_proc (h, 0);
	if (auto_menu) {
	    user_file_menu_cmd ();
	}
	return MSG_HANDLED;

    case DLG_KEY:
	if (ctl_x_map_enabled) {
	    ctl_x_map_enabled = 0;
	    for (i = 0; ctl_x_map[i].key_code; i++)
		if (parm == ctl_x_map[i].key_code) {
		    (*ctl_x_map[i].fn) ();
		    return MSG_HANDLED;
		}
	}

	/* FIXME: should handle all menu shortcuts before this point */
	if (the_menubar->active)
	    return MSG_NOT_HANDLED;

	if (parm == KEY_F (10)) {
	    quit_cmd ();
	    return MSG_HANDLED;
	}

	if (parm == '\t')
	    free_completions (cmdline);

	if (parm == '\n') {
	    for (i = 0; cmdline->buffer[i] && (cmdline->buffer[i] == ' ' ||
		cmdline->buffer[i] == '\t'); i++);
	    if (cmdline->buffer[i]) {
	        send_message ((Widget *) cmdline, WIDGET_KEY, parm);
		return MSG_HANDLED;
	    }
	    stuff (cmdline, "", 0);
	    cmdline->point = 0;
	}

	/* Ctrl-Enter and Alt-Enter */
	if (((parm & ~(KEY_M_CTRL | KEY_M_ALT)) == '\n')
	    && (parm & (KEY_M_CTRL | KEY_M_ALT))) {
	    copy_prog_name ();
	    return MSG_HANDLED;
	}

	/* Ctrl-Shift-Enter */
	if (parm == (KEY_M_CTRL | KEY_M_SHIFT | '\n')) {
	    copy_current_pathname ();
	    copy_prog_name ();
	    return MSG_HANDLED;
	}

	if ((!alternate_plus_minus || !(console_flag || xterm_flag))
	    && !quote && !current_panel->searching) {
	    if (!only_leading_plus_minus) {
		/* Special treatement, since the input line will eat them */
		if (parm == '+') {
		    select_cmd ();
		    return MSG_HANDLED;
		}

		if (parm == '\\' || parm == '-') {
		    unselect_cmd ();
		    return MSG_HANDLED;
		}

		if (parm == '*') {
		    reverse_selection_cmd ();
		    return MSG_HANDLED;
		}
	    } else if (!command_prompt || !cmdline->buffer[0]) {
		/* Special treatement '+', '-', '\', '*' only when this is
		 * first char on input line
		 */

		if (parm == '+') {
		    select_cmd ();
		    return MSG_HANDLED;
		}

		if (parm == '\\' || parm == '-') {
		    unselect_cmd ();
		    return MSG_HANDLED;
		}

		if (parm == '*') {
		    reverse_selection_cmd ();
		    return MSG_HANDLED;
		}
	    }
	}
	return MSG_NOT_HANDLED;

    case DLG_HOTKEY_HANDLED:
	if ((get_current_type () == view_listing) && current_panel->searching) {
	    current_panel->searching = 0;
	    current_panel->dirty = 1;
	}
	return MSG_HANDLED;

    case DLG_UNHANDLED_KEY:
	if (command_prompt) {
	    int v;

	    v = send_message ((Widget *) cmdline, WIDGET_KEY, parm);
	    if (v)
		return v;
	}
	if (ctl_x_map_enabled) {
	    ctl_x_map_enabled = 0;
	    for (i = 0; ctl_x_map[i].key_code; i++)
		if (parm == ctl_x_map[i].key_code) {
		    (*ctl_x_map[i].fn) ();
		    return MSG_HANDLED;
		}
	} else {
	    for (i = 0; default_map[i].key_code; i++) {
		if (parm == default_map[i].key_code) {
		    (*default_map[i].fn) ();
		    return MSG_HANDLED;
		}
	    }
	}
	return MSG_NOT_HANDLED;

    case DLG_DRAW:
	/* We handle the special case of the output lines */
	if (console_flag && output_lines)
	    show_console_contents (output_start_y,
				   LINES - output_lines - keybar_visible -
				   1, LINES - keybar_visible - 1);
	return MSG_HANDLED;

    case DLG_POST_KEY:
	if (!the_menubar->active)
	    update_dirty_panels ();
	return MSG_HANDLED;

    default:
	return default_dlg_callback (h, msg, parm);
    }
}

/* Show current directory in the xterm title */
void
update_xterm_title_path (void)
{
    const char *path;
    char host[BUF_TINY];
    char *p;
    struct passwd *pw = NULL;
    char *login = NULL;
    int res = 0;
    if (xterm_flag && xterm_title) {
	path = strip_home_and_password (current_panel->cwd);
	res = gethostname(host, sizeof (host));
	if ( res ) { /* On success, res = 0 */
	    host[0] = '\0';
	} else {
	    host[sizeof (host) - 1] = '\0';
	}
	pw = getpwuid(getuid());
	if ( pw ) {
	    login = g_strdup_printf ("%s@%s", pw->pw_name, host);
	} else {
	    login = g_strdup (host);
	}
	p = g_strdup_printf ("mc [%s]:%s", login, path);
	fprintf (stdout, "\33]0;%s\7", str_term_form (p));
	g_free (login);
	g_free (p);
	if (!alternate_plus_minus)
	    numeric_keypad_mode ();
	fflush (stdout);
    }
}

/*
 * Load new hint and display it.
 * IF force is not 0, ignore the timeout.
 */
void
load_hint (int force)
{
    char *hint;

    if (!the_hint->widget.parent)
	return;

    if (!message_visible) {
	label_set_text (the_hint, 0);
	return;
    }

    if ((hint = get_random_hint (force))) {
	if (*hint)
	    set_hintbar (hint);
	g_free (hint);
    } else {
	char text[BUF_SMALL];

	g_snprintf (text, sizeof (text), _("GNU Midnight Commander %s\n"),
		    VERSION);
	set_hintbar (text);
    }
}

static void
setup_panels_and_run_mc (void)
{
    add_widget (midnight_dlg, the_menubar);
    add_widget (midnight_dlg, get_panel_widget (0));
    add_widget (midnight_dlg, get_panel_widget (1));
    add_widget (midnight_dlg, the_hint);
    load_hint (1);
    add_widget (midnight_dlg, cmdline);
    add_widget (midnight_dlg, the_prompt);
    add_widget (midnight_dlg, the_bar);
    init_labels ();

    if (boot_current_is_left)
	dlg_select_widget (get_panel_widget (0));
    else
	dlg_select_widget (get_panel_widget (1));

    /* Run the Midnight Commander if no file was specified in the command line */
    run_dlg (midnight_dlg);
}

/* result must be free'd (I think this should go in util.c) */
static char *
prepend_cwd_on_local (const char *filename)
{
    char *d;
    int l;

    if (vfs_file_is_local (filename)) {
	if (*filename == PATH_SEP)	/* an absolute pathname */
	    return g_strdup (filename);
	d = g_malloc (MC_MAXPATHLEN + strlen (filename) + 2);
	mc_get_current_wd (d, MC_MAXPATHLEN);
	l = strlen (d);
	d[l++] = PATH_SEP;
	strcpy (d + l, filename);
	canonicalize_pathname (d);
	return d;
    } else
	return g_strdup (filename);
}

static int
mc_maybe_editor_or_viewer (void)
{
    if (!(view_one_file || edit_one_file))
	return 0;

    setup_dummy_mc ();

    /* Invoke the internal view/edit routine with:
     * the default processing and forcing the internal viewer/editor
     */
    if (view_one_file) {
	char *path = NULL;
	path = prepend_cwd_on_local (view_one_file);
	view_file (path, 0, 1);
	g_free (path);
    }
#ifdef USE_INTERNAL_EDIT
    else {
	edit_file (edit_one_file, edit_one_file_start_line);
    }
#endif				/* USE_INTERNAL_EDIT */
    midnight_shutdown = 1;
    done_mc ();
    return 1;
}

/* Run the main dialog that occupies the whole screen */
static void
do_nc (void)
{
    int midnight_colors[4];

    midnight_colors[0] = NORMAL_COLOR;	/* NORMALC */
    midnight_colors[1] = REVERSE_COLOR;	/* FOCUSC */
    midnight_colors[2] = INPUT_COLOR;	/* HOT_NORMALC */
    midnight_colors[3] = NORMAL_COLOR;	/* HOT_FOCUSC */


    midnight_dlg = create_dlg (0, 0, LINES, COLS, midnight_colors, midnight_callback,
			       "[main]", NULL, DLG_WANT_IDLE);

    /* start check display_codepage and source_codepage */
    check_codeset();

    /* Check if we were invoked as an editor or file viewer */
    if (!mc_maybe_editor_or_viewer ()) {
	setup_mc ();
	setup_panels_and_run_mc ();

	/* Program end */
	midnight_shutdown = 1;

	/* destroy_dlg destroys even current_panel->cwd, so we have to save a copy :) */
	if (last_wd_file && vfs_current_is_local ()) {
	    last_wd_string = g_strdup (current_panel->cwd);
	}
	done_mc ();
    }
    destroy_dlg (midnight_dlg);
    current_panel = 0;
    done_mc_profile ();
}

/* POSIX version.  The only version we support.  */
static void
OS_Setup (void)
{
    const char *mc_libdir;
    shell = getenv ("SHELL");
    if (!shell || !*shell) {
        struct passwd *pwd;
        pwd = getpwuid (geteuid ());
        if (pwd != NULL)
           shell = g_strdup (pwd->pw_shell);
    }
    if (!shell || !*shell)
	shell = "/bin/sh";

    /* This is the directory, where MC was installed, on Unix this is DATADIR */
    /* and can be overriden by the MC_DATADIR environment variable */
    if ((mc_libdir = getenv ("MC_DATADIR")) != NULL) {
	mc_home = g_strdup (mc_libdir);
    } else {
	mc_home = g_strdup (SYSCONFDIR);
    }
    mc_home_alt = mc_libdir != NULL ? g_strdup (SYSCONFDIR) : g_strdup (DATADIR);
}

static void
sigchld_handler_no_subshell (int sig)
{
#ifdef __linux__
    int pid, status;

    if (!console_flag)
	return;

    /* COMMENT: if it were true that after the call to handle_console(..INIT)
       the value of console_flag never changed, we could simply not install
       this handler at all if (!console_flag && !use_subshell). */

    /* That comment is no longer true.  We need to wait() on a sigchld
       handler (that's at least what the tarfs code expects currently). */

    pid = waitpid (cons_saver_pid, &status, WUNTRACED | WNOHANG);

    if (pid == cons_saver_pid) {

	if (WIFSTOPPED (status)) {
	    /* Someone has stopped cons.saver - restart it */
	    kill (pid, SIGCONT);
	} else {
	    /* cons.saver has died - disable console saving */
	    handle_console (CONSOLE_DONE);
	    console_flag = 0;
	}
    }
    /* If we got here, some other child exited; ignore it */
#endif				/* __linux__ */

    (void) sig;
}

static void
init_sigchld (void)
{
    struct sigaction sigchld_action;

    sigchld_action.sa_handler =
#ifdef HAVE_SUBSHELL_SUPPORT
	use_subshell ? sigchld_handler :
#endif				/* HAVE_SUBSHELL_SUPPORT */
	sigchld_handler_no_subshell;

    sigemptyset (&sigchld_action.sa_mask);

#ifdef SA_RESTART
    sigchld_action.sa_flags = SA_RESTART;
#else
    sigchld_action.sa_flags = 0;
#endif				/* !SA_RESTART */

    if (sigaction (SIGCHLD, &sigchld_action, NULL) == -1) {
#ifdef HAVE_SUBSHELL_SUPPORT
	/*
	 * This may happen on QNX Neutrino 6, where SA_RESTART
	 * is defined but not implemented.  Fallback to no subshell.
	 */
	use_subshell = 0;
#endif				/* HAVE_SUBSHELL_SUPPORT */
    }
}

static void
print_mc_usage (poptContext ctx, FILE *stream)
{
    int leftColWidth;

    poptSetOtherOptionHelp (ctx,
			    _("[flags] [this_dir] [other_panel_dir]\n"));

    /* print help for options */
    leftColWidth = poptPrintHelp (ctx, stream, 0);
    fprintf (stream, "  %-*s   %s\n", leftColWidth, _("+number"),
	     _("Set initial line number for the internal editor"));
    fputs (_
	   ("\n"
	    "Please send any bug reports (including the output of `mc -V')\n"
	    "to mc-devel@gnome.org\n"), stream);
    show_version (0);
}

static void
print_color_usage (void)
{
    /*
     * FIXME: undocumented keywords: viewunderline, editnormal, editbold,
     * and editmarked.  To preserve translations, lines should be split.
     */
    /* TRANSLATORS: don't translate keywords and names of colors */
    fputs (_
	   ("--colors KEYWORD={FORE},{BACK}\n\n"
	    "{FORE} and {BACK} can be omitted, and the default will be used\n"
	    "\n" "Keywords:\n"
	    "   Global:       errors, reverse, gauge, input, viewunderline\n"
	    "   File display: normal, selected, marked, markselect\n"
	    "   Dialog boxes: dnormal, dfocus, dhotnormal, dhotfocus, errdhotnormal,\n"
	    "                 errdhotfocus\n"
	    "   Menus:        menu, menuhot, menusel, menuhotsel\n"
	    "   Editor:       editnormal, editbold, editmarked, editwhitespace,\n"
	    "                 editlinestate\n"), stdout);
    fputs (_
	   (
	    "   Help:         helpnormal, helpitalic, helpbold, helplink, helpslink\n"
	    "   File types:   directory, executable, link, stalelink, device, special, core\n"
	    "\n" "Colors:\n"
	    "   black, gray, red, brightred, green, brightgreen, brown,\n"
	    "   yellow, blue, brightblue, magenta, brightmagenta, cyan,\n"
	    "   brightcyan, lightgray and white\n\n"), stdout);
}

static void
process_args (poptContext ctx, int c, const char *option_arg)
{
    switch (c) {
    case 'V':
	show_version (1);
	exit (0);
	break;

    case 'a':
	tty_set_ugly_line_drawing (TRUE);
	break;

    case 'b':
	tty_disable_colors (TRUE, FALSE);
	break;

    case 'c':
	tty_disable_colors (FALSE, TRUE);
	break;

    case 'f':
	printf ("%s (%s)\n", mc_home, mc_home_alt);
	exit (0);
	break;

#ifdef USE_NETCODE
    case 'l':
	mc_setctl ("/#ftp:", VFS_SETCTL_LOGFILE, (void *) option_arg);
#ifdef WITH_SMBFS
	smbfs_set_debugf (option_arg);
#endif				/* WITH_SMBFS */
	break;

#ifdef WITH_SMBFS
    case 'D':
	smbfs_set_debug (atoi (option_arg));
	break;
#endif				/* WITH_SMBFS */
#endif				/* USE_NETCODE */

    case 'd':
	use_mouse_p = MOUSE_DISABLED;
	break;

#ifdef HAVE_SUBSHELL_SUPPORT
    case 'u':
	use_subshell = 0;
	break;
#endif				/* HAVE_SUBSHELL_SUPPORT */

    case 'H':
	print_color_usage ();
	exit (0);
	break;

    case 'h':
	print_mc_usage (ctx, stdout);
	exit (0);
    }
}

static const struct poptOption argument_table[] = {
    /* generic options */
    {"help", 'h', POPT_ARG_NONE, {NULL}, 'h',
     N_("Displays this help message"), NULL},
    {"version", 'V', POPT_ARG_NONE, {NULL}, 'V',
     N_("Displays the current version"), NULL},

    /* terminal options */
    {"xterm", 'x', POPT_ARG_NONE, {&force_xterm}, 0,
     N_("Forces xterm features"), NULL},
    {"nomouse", 'd', POPT_ARG_NONE, {NULL}, 'd',
     N_("Disable mouse support in text version"), NULL},
#if defined(HAVE_SLANG)
    {"termcap", 't', 0, {&SLtt_Try_Termcap}, 0,
     N_("Tries to use termcap instead of terminfo"), NULL},
#endif
    {"resetsoft", 'k', POPT_ARG_NONE, {&reset_hp_softkeys}, 0,
     N_("Resets soft keys on HP terminals"), NULL},
    {"slow", 's', POPT_ARG_NONE, {&slow_terminal}, 0,
     N_("To run on slow terminals"), NULL},
    {"stickchars", 'a', POPT_ARG_NONE, {NULL}, 'a',
     N_("Use stickchars to draw"), NULL},

    /* color options */
    {"nocolor", 'b', POPT_ARG_NONE, {NULL}, 'b',
     N_("Requests to run in black and white"), NULL},
    {"color", 'c', POPT_ARG_NONE, {NULL}, 'c',
     N_("Request to run in color mode"), NULL},
    {"colors", 'C', POPT_ARG_STRING, {&command_line_colors}, 0,
     N_("Specifies a color configuration"), NULL},
    {"help-colors", 'H', POPT_ARG_NONE, {NULL}, 'H',
     N_("Displays a help screen on how to change the color scheme"), NULL},

    /* debug options */
#ifdef USE_NETCODE
    {"ftplog", 'l', POPT_ARG_STRING, {NULL}, 'l',
     N_("Log ftp dialog to specified file"), NULL},
#ifdef WITH_SMBFS
    {"debuglevel", 'D', POPT_ARG_STRING, {NULL}, 'D',
     N_("Set debug level"), NULL},
#endif
#endif

    /* options for wrappers */
    {"datadir", 'f', POPT_ARG_NONE, {NULL}, 'f',
     N_("Print data directory"), NULL},
    {"printwd", 'P', POPT_ARG_STRING, {&last_wd_file}, 0,
     N_("Print last working directory to specified file"), NULL},

    /* subshell options */
#ifdef HAVE_SUBSHELL_SUPPORT
    {"subshell", 'U', POPT_ARG_NONE, {&use_subshell}, 0,
     N_("Enables subshell support (default)"), NULL},
    {"nosubshell", 'u', POPT_ARG_NONE, {NULL}, 'u',
     N_("Disables subshell support"), NULL},
#endif

    /* single file operations */
    {"view", 'v', POPT_ARG_STRING, {&view_one_file}, 0,
     N_("Launches the file viewer on a file"), NULL},
#ifdef USE_INTERNAL_EDIT
    {"edit", 'e', POPT_ARG_STRING, {&edit_one_file}, 0,
     N_("Edits one file"), NULL},
#endif

    {NULL, '\0', 0, {NULL}, 0, NULL , NULL}
};

static void
handle_args (int argc, char *argv[])
{
    char *tmp;
    poptContext ctx;
    const char *base;
    int c;

    ctx =
	poptGetContext ("mc", argc, argv, argument_table,
			POPT_CONTEXT_NO_EXEC);

    while ((c = poptGetNextOpt (ctx)) > 0) {
	process_args (ctx, c, poptGetOptArg (ctx));
    }

    if (c < -1) {
	print_mc_usage (ctx, stderr);
	fprintf (stderr, "%s: %s\n",
		 poptBadOption (ctx, POPT_BADOPTION_NOALIAS),
		 poptStrerror (c));
	exit (1);
    }

    tmp = poptGetArg (ctx);

    /*
     * Check for special invocation names mcedit and mcview,
     * if none apply then set the current directory and the other
     * directory from the command line arguments
     */
    base = x_basename (argv[0]);
    if (!STRNCOMP (base, "mce", 3) || !STRCOMP (base, "vi")) {
	edit_one_file = "";
	if (tmp) {
	    /*
	     * Check for filename:lineno, followed by an optional colon.
	     * This format is used by many programs (especially compilers)
	     * in error messages and warnings. It is supported so that
	     * users can quickly copy and paste file locations.
	     */
	    char *end = tmp + strlen (tmp), *p = end;
	    if (p > tmp && p[-1] == ':')
		p--;
	    while (p > tmp && g_ascii_isdigit ((gchar) p[-1]))
		p--;
	    if (tmp < p && p < end && p[-1] == ':') {
	        struct stat st;
		gchar *fname = g_strndup (tmp, p - 1 - tmp);
		/*
		 * Check that the file before the colon actually exists.
		 * If it doesn't exist, revert to the old behavior.
		 */
		if (mc_stat (tmp, &st) == -1 && mc_stat (fname, &st) != -1) {
		    edit_one_file = fname;
		    edit_one_file_start_line = atoi (p);
		} else {
		    g_free (fname);
		    goto try_plus_filename;
		}
	    } else {
	    try_plus_filename:
		if (*tmp == '+' && g_ascii_isdigit ((gchar) tmp[1])) {
		    int start_line = atoi (tmp);
		    if (start_line > 0) {
			char *file = poptGetArg (ctx);
			if (file) {
			    tmp = file;
			    edit_one_file_start_line = start_line;
			}
		    }
		}
		edit_one_file = g_strdup (tmp);
	    }
	}
    } else if (!STRNCOMP (base, "mcv", 3) || !STRCOMP (base, "view")) {
	if (tmp)
	    view_one_file = g_strdup (tmp);
	else {
	    fputs ("No arguments given to the viewer\n", stderr);
	    exit (1);
	}
    } else {
	/* sets the current dir and the other dir */
	if (tmp) {
	    this_dir = g_strdup (tmp);
	    if ((tmp = poptGetArg (ctx)))
		other_dir = g_strdup (tmp);
	}
    }

    poptFreeContext (ctx);
}

int
main (int argc, char *argv[])
{
    struct stat s;
    char *mc_dir;

    /* We had LC_CTYPE before, LC_ALL includs LC_TYPE as well */
    setlocale (LC_ALL, "");
    bindtextdomain ("mc", LOCALEDIR);
    textdomain ("mc");

    /* Set up temporary directory */
    mc_tmpdir ();

    OS_Setup ();

    /* This variable is used by the subshell */
    home_dir = getenv ("HOME");
    if (!home_dir) {
	/* mc_home was computed by OS_Setup */
	home_dir = mc_home;
    }

    str_init_strings (NULL);

    vfs_init ();

#ifdef USE_INTERNAL_EDIT
    edit_stack_init ();
#endif

#ifdef HAVE_SLANG
    SLtt_Ignore_Beep = 1;
#endif

    handle_args (argc, argv);

    /* NOTE: This has to be called before init_slang or whatever routine
       calls any define_sequence */
    init_key ();

    /* Must be done before installing the SIGCHLD handler [[FIXME]] */
    handle_console (CONSOLE_INIT);

#ifdef HAVE_SUBSHELL_SUPPORT
    /* Don't use subshell when invoked as viewer or editor */
    if (edit_one_file || view_one_file)
	use_subshell = 0;

    if (use_subshell)
	subshell_get_console_attributes ();
#endif				/* HAVE_SUBSHELL_SUPPORT */

    /* Install the SIGCHLD handler; must be done before init_subshell() */
    init_sigchld ();

    /* We need this, since ncurses endwin () doesn't restore the signals */
    save_stop_handler ();

    /* Must be done before init_subshell, to set up the terminal size: */
    /* FIXME: Should be removed and LINES and COLS computed on subshell */
#ifdef HAVE_SLANG
    init_slang ();
#endif

    start_interrupt_key ();

    /* NOTE: This call has to be after slang_init. It's the small part from
       the previous init_key which had to be moved after the call of slang_init */
    init_key_input_fd ();

    load_setup ();

    init_curses ();

    tty_init_colors ();

    dlg_set_default_colors ();

    /* create home directory */
    /* do it after the screen library initialization to show the error message */
    mc_dir = concat_dir_and_file (home_dir, MC_BASE);
    canonicalize_pathname (mc_dir);
    if ((stat (mc_dir, &s) != 0) && (errno == ENOENT)
	&& mkdir (mc_dir, 0700) != 0)
	message (D_ERROR, _("Warning"),
		    _("Cannot create %s directory"), mc_dir);
    g_free (mc_dir);

    init_xterm_support ();

#ifdef HAVE_SUBSHELL_SUPPORT

    /* Done here to ensure that the subshell doesn't  */
    /* inherit the file descriptors opened below, etc */
    if (use_subshell)
	init_subshell ();

#endif				/* HAVE_SUBSHELL_SUPPORT */

    /* Removing this from the X code let's us type C-c */
    load_key_defs ();

    /* Also done after init_subshell, to save any shell init file messages */
    if (console_flag)
	handle_console (CONSOLE_SAVE);

    if (alternate_plus_minus)
	application_keypad_mode ();

#ifdef HAVE_SUBSHELL_SUPPORT
    if (use_subshell) {
	prompt = strip_ctrl_codes (subshell_prompt);
	if (!prompt)
	    prompt = "";
    } else
#endif				/* HAVE_SUBSHELL_SUPPORT */
	prompt = (geteuid () == 0) ? "# " : "$ ";

    /* Program main loop */
    if (!midnight_shutdown)
	do_nc ();

    /* Save the tree store */
    tree_store_save ();

    /* Virtual File System shutdown */
    vfs_shut ();

    flush_extension_file ();	/* does only free memory */

    tty_shutdown ();

    if (console_flag && !(quit & SUBSHELL_EXIT))
	handle_console (CONSOLE_RESTORE);
    if (alternate_plus_minus)
	numeric_keypad_mode ();

    signal (SIGCHLD, SIG_DFL);	/* Disable the SIGCHLD handler */

    if (console_flag)
	handle_console (CONSOLE_DONE);
    putchar ('\n');		/* Hack to make shell's prompt start at left of screen */

    if (last_wd_file && last_wd_string && !print_last_revert
	&& !edit_one_file && !view_one_file) {
	int last_wd_fd =
	    open (last_wd_file, O_WRONLY | O_CREAT | O_TRUNC | O_EXCL,
		  S_IRUSR | S_IWUSR);

	if (last_wd_fd != -1) {
	    write (last_wd_fd, last_wd_string, strlen (last_wd_string));
	    close (last_wd_fd);
	}
    }
    g_free (last_wd_string);

    g_free (mc_home_alt);
    g_free (mc_home);
    done_key ();
#ifdef HAVE_CHARSET
    free_codepages_list ();
#endif
    str_uninit_strings ();

    g_free (this_dir);
    g_free (other_dir);

#ifdef USE_INTERNAL_EDIT
    edit_stack_free ();
#endif

    return 0;
}
