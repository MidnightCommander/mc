/* Main program for the Midnight Commander
   Copyright (C) 1994, 1995, 1996, 1997 The Free Software Foundation
   
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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <config.h>
#include <locale.h>

#ifdef NATIVE_WIN32
#    include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <sys/stat.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <errno.h>
#include <ctype.h>
#include <signal.h>

/* Program include files */
#include "global.h"
#include "tty.h"
#include "dir.h"
#include "color.h"
#include "dialog.h"
#include "menu.h"
#include "panel.h"
#include "main.h"
#include "win.h"
#include "mouse.h"
#include "option.h"
#include "tree.h"
#include "cons.saver.h"
#include "subshell.h"
#include "key.h"		/* For init_key() and mi_getch() */
#include "setup.h"		/* save_setup() */
#include "profile.h"		/* free_profiles() */
#include "boxes.h"		/* sort_box() */
#include "layout.h"
#include "cmd.h"		/* Normal commands */
#include "hotlist.h"
#include "panelize.h"
#include "learn.h"		/* learn_keys() */
#include "listmode.h"
#include "background.h"
#include "ext.h"		/* For flush_extension_file() */

/* Listbox for the command history feature */
#include "widget.h"
#include "command.h"
#include "wtools.h"
#include "complete.h"		/* For the free_completion */

#include "chmod.h"
#include "chown.h"

#ifdef NATIVE_WIN32
#    include "drive.h"
#endif

#ifdef WITH_SMBFS
#include "../vfs/smbfs.h"	/* smbfs_set_debug() */
#endif

#ifdef USE_INTERNAL_EDIT
#   include "../edit/edit.h"
#endif

#ifdef	HAVE_CHARSET
#include "charsets.h"
#endif				/* HAVE_CHARSET */

#include "popt.h"

/* When the modes are active, left_panel, right_panel and tree_panel */
/* Point to a proper data structure.  You should check with the functions */
/* get_current_type and get_other_type the types of the panels before using */
/* This pointer variables */

/* The structures for the panels */
WPanel *left_panel;
WPanel *right_panel;

/* The pointer to the tree */
WTree *the_tree;

/* The Menubar */
struct WMenu *the_menubar;

/* Pointers to the selected and unselected panel */
WPanel *current_panel = NULL;

/* Set if the command is being run from the "Right" menu */
int is_right;

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

/* If true, after executing a command, wait for a keystroke */
enum { pause_never, pause_on_dumb_terminals, pause_always };

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

/* If true use the internal viewer */
int use_internal_view = 1;

/* Have we shown the fast-reload warning in the past? */
int fast_reload_w = 0;

/* Move page/item? When clicking on the top or bottom of a panel */
int mouse_move_pages = 1;

/* If true: l&r arrows are used to chdir if the input line is empty */
int navigate_with_arrows = 0;

/* If true use +, -, | for line drawing */
int force_ugly_line_drawing = 0;

/* If true message "The shell is already running a command" never */
int force_subshell_execution = 0;

/* If true program softkeys (HP terminals only) on startup and after every 
   command ran in the subshell to the description found in the termcap/terminfo 
   database */
int reset_hp_softkeys = 0;

/* The prompt */
char *prompt = 0;

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

/* Controls screen clearing before an exec */
int clear_before_exec = 1;

/* Asks for confirmation before deleting a file */
int confirm_delete = 1;

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

/* The dialog handle for the main program */
Dlg_head *midnight_dlg;

/* Subshell: if set, then the prompt was not saved on CONSOLE_SAVE */
/* We need to paint it after CONSOLE_RESTORE, see: load_prompt */
int update_prompt = 0;

#if 0
/* The name which was used to invoke mc */
char *program_name;
#endif

/* The home directory */
char *home_dir;

/* The value of the other directory, only used when loading the setup */
char *other_dir = 0;

/* Only used at program boot */
int boot_current_is_left = 1;

char *this_dir = 0;

/* If this is true, then when browsing the tree the other window will
 * automatically reload it's directory with the contents of the currently
 * selected directory.
 */
int xtree_mode = 0;

/* If set, then print to the given file the last directory we were at */
static char *last_wd_file;
static char *last_wd_string;
/* Set to 1 to suppress printing the last directory */
static int print_last_revert = 0;

/* Force colors, only used by Slang */
int force_colors = 0;

/* colors specified on the command line: they override any other setting */
char *command_line_colors;

/* File name to view if argument was supplied */
char *view_one_file = 0;

/* File name to edit if argument was supplied */
char *edit_one_file = 0;

/* Line to start the editor on */
static int edit_one_file_start_line = 0;

/* Used so that widgets know if they are being destroyed or
   shut down */
int midnight_shutdown = 0;

/* to show nice prompts */
static int last_paused = 0;

/* Used for keeping track of the original stdout */
int stdout_fd = 0;

/* The user's shell */
char *shell;

/* mc_home: The home of MC */
char *mc_home;

char cmd_buf[512];

WPanel *
get_current_panel (void)
{
    return current_panel;
}

WPanel *
get_other_panel (void)
{
    return (WPanel *) get_panel_widget (get_other_index ());
}

static void
reload_panelized (WPanel *panel)
{
    int i, j;
    dir_list *list = &panel->dir;

    if (panel != cpanel)
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
	if (mc_lstat (list->list[i].fname, &list->list[i].buf)) {
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

    if (panel != cpanel)
	mc_chdir (cpanel->cwd);
}

static void
update_one_panel_widget (WPanel *panel, int force_update,
			 char *current_file)
{
    int free_pointer;

    if (force_update & UP_RELOAD) {
	panel->is_panelized = 0;

#if 1
	ftpfs_flushdir ();
#endif
/* FIXME: Should supply flushdir method */
	memset (&(panel->dir_stat), 0, sizeof (panel->dir_stat));
    }

    /* If current_file == -1 (an invalid pointer) then preserve selection */
    if (current_file == UP_KEEPSEL) {
	free_pointer = 1;
	current_file = g_strdup (panel->dir.list[panel->selected].fname);
    } else
	free_pointer = 0;

    if (panel->is_panelized)
	reload_panelized (panel);
    else
	panel_reload (panel);

    try_to_select (panel, current_file);
    panel->dirty = 1;

    if (free_pointer)
	g_free (current_file);
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

    clean_dir (&panel->dir, count);
}

static void
update_one_panel (int which, int force_update, char *current_file)
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
update_panels (int force_update, char *current_file)
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

/* Sets up the terminal before executing a program */
static void
pre_exec (void)
{
    use_dash (0);
    edition_pre_exec ();
}

/* Save current stat of directories to avoid reloading the panels */
/* when no modifications have taken place */
void
save_cwds_stat (void)
{
    if (fast_reload) {
	mc_stat (cpanel->cwd, &(cpanel->dir_stat));
	if (get_other_type () == view_listing)
	    mc_stat (opanel->cwd, &(opanel->dir_stat));
    }
}

#ifdef HAVE_SUBSHELL_SUPPORT
void
do_possible_cd (char *new_dir)
{
    if (!do_cd (new_dir, cd_exact))
	message (1, _("Warning"),
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
restore_console (void)
{
    handle_console (CONSOLE_RESTORE);
}

void
exec_shell (void)
{
    do_execute (shell, 0, 0);
}

void
do_execute (const char *shell, const char *command, int flags)
{
#ifdef HAVE_SUBSHELL_SUPPORT
    char *new_dir = NULL;
#endif				/* HAVE_SUBSHELL_SUPPORT */

#ifdef USE_VFS
    char *old_vfs_dir = 0;

    if (!vfs_current_is_local ())
	old_vfs_dir = g_strdup (vfs_get_current_dir ());
#endif				/* USE_VFS */

    save_cwds_stat ();
    pre_exec ();
    if (console_flag)
	restore_console ();

    if (!use_subshell && !(flags & EXECUTE_INTERNAL && command)) {
	printf ("%s%s%s\n", last_paused ? "\r\n" : "", prompt, command);
	last_paused = 0;
    }
#ifdef HAVE_SUBSHELL_SUPPORT
    if (use_subshell && !(flags & EXECUTE_INTERNAL)) {
	do_update_prompt ();

	/* We don't care if it died, higher level takes care of this */
#ifdef USE_VFS
	invoke_subshell (command, VISIBLY, old_vfs_dir ? 0 : &new_dir);
#else
	invoke_subshell (command, VISIBLY, &new_dir);
#endif				/* !USE_VFS */
    } else
#endif				/* HAVE_SUBSHELL_SUPPORT */
	my_system (flags, shell, command);

    if (!(flags & EXECUTE_INTERNAL)) {
	if ((pause_after_run == pause_always
	     || (pause_after_run == pause_on_dumb_terminals && !xterm_flag
		 && !console_flag)) && !quit
#ifdef HAVE_SUBSHELL_SUPPORT
	    && subshell_state != RUNNING_COMMAND
#endif				/* HAVE_SUBSHELL_SUPPORT */
	    ) {
	    printf ("%s\r\n", _("Press any key to continue..."));
	    last_paused = 1;
	    fflush (stdout);
	    mc_raw_mode ();
	    getch ();
	}
	if (console_flag) {
	    if (output_lines && keybar_visible) {
		putchar ('\n');
		fflush (stdout);
	    }
	}
    }

    if (console_flag)
	handle_console (CONSOLE_SAVE);
    edition_post_exec ();

#ifdef HAVE_SUBSHELL_SUPPORT
    if (new_dir)
	do_possible_cd (new_dir);

#endif				/* HAVE_SUBSHELL_SUPPORT */

#ifdef USE_VFS
    if (old_vfs_dir) {
	mc_chdir (old_vfs_dir);
	g_free (old_vfs_dir);
    }
#endif				/* USE_VFS */

    update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    update_xterm_title_path ();

    do_refresh ();
    use_dash (TRUE);
}

/* Executes a command */
void
shell_execute (char *command, int flags)
{
#ifdef HAVE_SUBSHELL_SUPPORT
    if (use_subshell)
	if (subshell_state == INACTIVE || force_subshell_execution)
	    do_execute (shell, command, flags | EXECUTE_AS_SHELL);
	else
	    message (1, MSG_ERROR,
		     _(" The shell is already running a command "));
    else
#endif				/* HAVE_SUBSHELL_SUPPORT */
	do_execute (shell, command, flags | EXECUTE_AS_SHELL);
}

void
execute (char *command)
{
    shell_execute (command, 0);
}

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
	     _(" Do you really want to quit the Midnight Commander? "), 0,
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

int
quiet_quit_cmd (void)
{
    print_last_revert = 1;
    quit_cmd_internal (1);
    return quit;
}

/*
 * Touch window and refresh window functions
 */

/* This routine untouches the first line on both panels in order */
/* to avoid the refreshing the menu bar */

void
untouch_bar (void)
{
    do_refresh ();
}

void
repaint_screen (void)
{
    do_refresh ();
    mc_refresh ();
}

/* Wrapper for do_subshell_chdir, check for availability of subshell */
void
subshell_chdir (char *directory)
{
#ifdef HAVE_SUBSHELL_SUPPORT
    if (use_subshell) {
	if (vfs_current_is_local ())
	    do_subshell_chdir (directory, 0, 1);
    }
#endif				/* HAVE_SUBSHELL_SUPPORT */
}

void
directory_history_add (struct WPanel *panel, char *s)
{
    char *text;

    text = g_strdup (s);
    strip_password (s, 1);

    panel->dir_history = list_append_unique (panel->dir_history, text);
}

/*
 *  If we moved to the parent directory move the selection pointer to
 *  the old directory name; If we leave VFS dir, remove FS specificator.
 *  Warn: This code spoils lwd string.
 *
 *  You do _NOT_ want to add any vfs aware code here. <pavel@ucw.cz>
 */
static char *
get_parent_dir_name (char *cwd, char *lwd)
{
    char *p;
    if (strlen (lwd) > strlen (cwd))
	if ((p = strrchr (lwd, PATH_SEP)) && !strncmp (cwd, lwd, p - lwd)) {
	    return (p + 1);
	}
    return NULL;
}

/*
 * Changes the current directory of the panel.
 * Don't record change in the directory history.
 */
static int
_do_panel_cd (WPanel *panel, char *new_dir, enum cd_enum cd_type)
{
    char *directory, *olddir;
    char temp[MC_MAXPATHLEN];
    char *translated_url;
#ifdef USE_VFS
    vfs *oldvfs;
    vfsid oldvfsid;
    struct vfs_stamping *parent;
#endif
    olddir = g_strdup (panel->cwd);
    translated_url = new_dir = vfs_translate_url (new_dir);

    /* Convert *new_path to a suitable pathname, handle ~user */

    if (cd_type == cd_parse_command) {
	while (*new_dir == ' ')
	    new_dir++;

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

#ifdef USE_VFS
    oldvfs = vfs_type (olddir);
    oldvfsid = vfs_ncs_getid (oldvfs, olddir, &parent);
    vfs_add_noncurrent_stamps (oldvfs, oldvfsid, parent);
    vfs_rm_parents (parent);
#endif

    subshell_chdir (panel->cwd);

    /* Reload current panel */
    panel_clean_dir (panel);
    panel->count =
	do_load_dir (&panel->dir, panel->sort_type, panel->reverse,
		     panel->case_sensitive, panel->filter);
    try_to_select (panel, get_parent_dir_name (panel->cwd, olddir));
    load_hint (0);
    panel_update_contents (panel);
    update_xterm_title_path ();

    g_free (olddir);

    return 1;
}

/*
 * Changes the current directory of the panel.
 * Record change in the directory history.
 */
int
do_panel_cd (struct WPanel *panel, char *new_dir, enum cd_enum cd_type)
{
    int r;

    r = _do_panel_cd (panel, new_dir, cd_type);
    if (r)
	directory_history_add (panel, panel->cwd);
    return r;
}

int
do_cd (char *new_dir, enum cd_enum exact)
{
    return (do_panel_cd (cpanel, new_dir, exact));
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
	message (1, MSG_ERROR, _("Cannot change directory"));
    g_free (s);
}

#ifdef HAVE_SUBSHELL_SUPPORT
int
load_prompt (int fd, void *unused)
{
    if (!read_subshell_prompt ())
	return 0;

    /* Don't actually change the prompt if it's invisible */
    if (current_dlg == midnight_dlg && command_prompt) {
	int prompt_len;

	prompt = strip_ctrl_codes (subshell_prompt);
	prompt_len = strlen (prompt);

	/* Check for prompts too big */
	if (COLS > 8 && prompt_len > COLS - 8) {
	    prompt[COLS - 8] = 0;
	    prompt_len = COLS - 8;
	}
	label_set_text (the_prompt, prompt);
	winput_set_origin ((WInput *) cmdline, prompt_len,
			   COLS - prompt_len);

	/* since the prompt has changed, and we are called from one of the 
	 * get_event channels, the prompt updating does not take place
	 * automatically: force a cursor update and a screen refresh
	 */
	update_cursor (midnight_dlg);
	mc_refresh ();
    }
    update_prompt = 1;
    return 0;
}
#endif				/* HAVE_SUBSHELL_SUPPORT */

/* Used to emulate Lynx's entering leaving a directory with the arrow keys */
int
maybe_cd (int char_code, int move_up_dir)
{
    if (navigate_with_arrows) {
	if (!cmdline->buffer[0]) {
	    if (!move_up_dir) {
		do_cd ("..", cd_exact);
		return 1;
	    }
	    if (S_ISDIR (selection (cpanel)->buf.st_mode)
		|| link_isdir (selection (cpanel))) {
		do_cd (selection (cpanel)->fname, cd_exact);
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
    sort_order = sort_box (p->sort_type, &p->reverse, &p->case_sensitive);

    panel_set_sort_order (p, sort_order);

}

static void
treebox_cmd (void)
{
    char *sel_dir;

    sel_dir = tree_box (selection (cpanel)->fname);
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
    newmode = listmode_edit ("half <type,>name,|,size:8,|,perm:4+");
    message (0, _(" Listing format edit "), _(" New mode is \"%s\" "),
	     newmode);
    g_free (newmode);
}
#endif				/* LISTMODE_EDITOR */

/* NOTICE: hotkeys specified here are overriden in menubar_paint_idx (alex) */
static menu_entry PanelMenu[] = {
    {' ', N_("&Listing mode..."), 'L', listing_cmd},
    {' ', N_("&Quick view     C-x q"), 'Q', quick_view_cmd},
    {' ', N_("&Info           C-x i"), 'I', info_cmd},
    {' ', N_("&Tree"), 'T', tree_cmd},
    {' ', "", ' ', 0},
    {' ', N_("&Sort order..."), 'S', sort_cmd},
    {' ', "", ' ', 0},
    {' ', N_("&Filter..."), 'F', filter_cmd},
#ifdef USE_NETCODE
    {' ', "", ' ', 0},
#ifdef WITH_MCFS
    {' ', N_("&Network link..."), 'N', netlink_cmd},
#endif
    {' ', N_("FT&P link..."), 'P', ftplink_cmd},
    {' ', N_("S&hell link..."), 'H', fishlink_cmd},
#ifdef WITH_SMBFS
    {' ', N_("SM&B link..."), 'B', smblink_cmd},
#endif
#endif
    {' ', "", ' ', 0},
#ifdef NATIVE_WIN32
    {' ', N_("&Drive...       M-d"), 'D', drive_cmd_a},
#endif
    {' ', N_("&Rescan         C-r"), 'R', reread_cmd}
};

static menu_entry RightMenu[] = {
    {' ', N_("&Listing mode..."), 'L', listing_cmd},
    {' ', N_("&Quick view     C-x q"), 'Q', quick_view_cmd},
    {' ', N_("&Info           C-x i"), 'I', info_cmd},
    {' ', N_("&Tree"), 'T', tree_cmd},
    {' ', "", ' ', 0},
    {' ', N_("&Sort order..."), 'S', sort_cmd},
    {' ', "", ' ', 0},
    {' ', N_("&Filter..."), 'F', filter_cmd},
#ifdef USE_NETCODE
    {' ', "", ' ', 0},
#ifdef WITH_MCFS
    {' ', N_("&Network link..."), 'N', netlink_cmd},
#endif
    {' ', N_("FT&P link..."), 'P', ftplink_cmd},
    {' ', N_("S&hell link..."), 'H', fishlink_cmd},
#ifdef WITH_SMBFS
    {' ', N_("SM&B link..."), 'B', smblink_cmd},
#endif
#endif
    {' ', "", ' ', 0},
#ifdef NATIVE_WIN32
    {' ', N_("&Drive...       M-d"), 'D', drive_cmd_b},
#endif
    {' ', N_("&Rescan         C-r"), 'R', reread_cmd}
};

static menu_entry FileMenu[] = {
    {' ', N_("&User menu          F2"), 'U', user_file_menu_cmd},
    {' ', N_("&View               F3"), 'V', view_cmd},
    {' ', N_("Vie&w file...         "), 'W', view_file_cmd},
    {' ', N_("&Filtered view     M-!"), 'F', filtered_view_cmd},
    {' ', N_("&Edit               F4"), 'E', edit_cmd},
    {' ', N_("&Copy               F5"), 'C', copy_cmd},
    {' ', N_("c&Hmod           C-x c"), 'H', chmod_cmd},
#ifndef NATIVE_WIN32
    {' ', N_("&Link            C-x l"), 'L', link_cmd},
    {' ', N_("&SymLink         C-x s"), 'S', symlink_cmd},
    {' ', N_("edit s&Ymlink  C-x C-s"), 'Y', edit_symlink_cmd},
    {' ', N_("ch&Own           C-x o"), 'O', chown_cmd},
    {' ', N_("&Advanced chown       "), 'A', chown_advanced_cmd},
#endif
    {' ', N_("&Rename/Move        F6"), 'R', ren_cmd},
    {' ', N_("&Mkdir              F7"), 'M', mkdir_cmd},
    {' ', N_("&Delete             F8"), 'D', delete_cmd},
    {' ', N_("&Quick cd          M-c"), 'Q', quick_cd_cmd},
    {' ', "", ' ', 0},
    {' ', N_("select &Group      M-+"), 'G', select_cmd},
    {' ', N_("u&Nselect group    M-\\"), 'N', unselect_cmd},
    {' ', N_("reverse selec&Tion M-*"), 'T', reverse_selection_cmd},
    {' ', "", ' ', 0},
    {' ', N_("e&Xit              F10"), 'X', quit_cmd}
};

static menu_entry CmdMenu[] = {
    /* I know, I'm lazy, but the tree widget when it's not running
     * as a panel still has some problems, I have not yet finished
     * the WTree widget port, sorry.
     */
    {' ', N_("&Directory tree"), 'D', treebox_cmd},
    {' ', N_("&Find file            M-?"), 'F', find_cmd},
    {' ', N_("s&Wap panels          C-u"), 'W', swap_cmd},
    {' ', N_("switch &Panels on/off C-o"), 'P', view_other_cmd},
    {' ', N_("&Compare directories  C-x d"), 'C', compare_dirs_cmd},
    {' ', N_("e&Xternal panelize    C-x !"), 'X', external_panelize},
    {' ', N_("show directory s&Izes"), 'I', dirsizes_cmd},
    {' ', "", ' ', 0},
    {' ', N_("command &History"), 'H', history_cmd},
    {' ', N_("di&Rectory hotlist    C-\\"), 'R', quick_chdir_cmd},
#ifdef USE_VFS
    {' ', N_("&Active VFS list      C-x a"), 'A', reselect_vfs},
    {' ', N_("Fr&ee VFSs now"), 'E', free_vfs_now},
#endif
#ifdef WITH_BACKGROUND
    {' ', N_("&Background jobs      C-x j"), 'B', jobs_cmd},
#endif
    {' ', "", ' ', 0},
#ifdef USE_EXT2FSLIB
    {' ', N_("&Undelete files (ext2fs only)"), 'U', undelete_cmd},
#endif
#ifdef LISTMODE_EDITOR
    {' ', N_("&Listing format edit"), 'L', listmode_cmd},
#endif
#if defined (USE_EXT2FSLIB) || defined (LISTMODE_EDITOR)
    {' ', "", ' ', 0},
#endif
    {' ', N_("Edit &extension file"), 'E', ext_cmd},
    {' ', N_("Edit &menu file"), 'M', edit_mc_menu_cmd},
#ifdef USE_INTERNAL_EDIT
    {' ', N_("Edit edi&tor menu file"), 'T', edit_user_menu_cmd},
    {' ', N_("Edit &syntax file"), 'S', edit_syntax_cmd}
#endif				/* USE_INTERNAL_EDIT */
};

/* Must keep in sync with the constants in menu_cmd */
static menu_entry OptMenu[] = {
    {' ', N_("&Configuration..."), 'C', configure_box},
    {' ', N_("&Layout..."), 'L', layout_cmd},
    {' ', N_("c&Onfirmation..."), 'O', confirm_box},
    {' ', N_("&Display bits..."), 'D', display_bits_box},
#ifndef NATIVE_WIN32
    {' ', N_("learn &Keys..."), 'K', learn_keys},
#endif				/* !NATIVE_WIN32 */
#ifdef USE_VFS
    {' ', N_("&Virtual FS..."), 'V', configure_vfs},
#endif				/* !USE_VFS */
    {' ', "", ' ', 0},
    {' ', N_("&Save setup"), 'S', save_setup_cmd}
};

#define menu_entries(x) sizeof(x)/sizeof(menu_entry)

static Menu *MenuBar[5];

void
init_menu (void)
{
    MenuBar[0] =
	create_menu (horizontal_split ? _(" &Above ") : _(" &Left "),
		     PanelMenu, menu_entries (PanelMenu),
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
		     RightMenu, menu_entries (PanelMenu),
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
    the_menubar->previous_selection = dlg_item_number (midnight_dlg);
    dlg_select_widget (midnight_dlg, the_menubar);
}

static void
menu_cmd (void)
{
    if (the_menubar->active)
	return;

    if (get_current_index () == 0)
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
	message (0, _(" Information "),
		 _
		 (" Using the fast reload option may not reflect the exact \n"
		  " directory contents. In this cases you'll need to do a  \n"
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

void
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
    the_prompt = label_new (0, 0, prompt, NULL);
    the_prompt->transparent = 1;
    the_bar = buttonbar_new (keybar_visible);

    the_hint = label_new (0, 0, 0, NULL);
    the_hint->transparent = 1;
    the_hint->auto_adjust_cols = 0;
    the_hint->widget.cols = COLS;

    the_menubar = menubar_new (0, 0, COLS, MenuBar, 5);
}

static void
copy_current_pathname (void)
{
    if (!command_prompt)
	return;

    command_insert (cmdline, cpanel->cwd, 0);
    if (cpanel->cwd[strlen (cpanel->cwd) - 1] != PATH_SEP)
	command_insert (cmdline, PATH_SEP_STR, 0);
}

static void
copy_other_pathname (void)
{
    if (get_other_type () != view_listing)
	return;

    if (!command_prompt)
	return;

    command_insert (cmdline, opanel->cwd, 0);
    if (cpanel->cwd[strlen (opanel->cwd) - 1] != PATH_SEP)
	command_insert (cmdline, PATH_SEP_STR, 0);
}

static void
copy_readlink (WPanel *panel)
{
    if (!command_prompt)
	return;
    if (S_ISLNK (selection (panel)->buf.st_mode)) {
	char buffer[MC_MAXPATHLEN];
	char *p =
	    concat_dir_and_file (panel->cwd, selection (panel)->fname);
	int i;

	i = mc_readlink (p, buffer, MC_MAXPATHLEN);
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
    copy_readlink (cpanel);
}

static void
copy_other_readlink (void)
{
    if (get_other_type () != view_listing)
	return;
    copy_readlink (opanel);
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
	tmp = selection (cpanel)->fname;

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
    copy_tagged (cpanel);
}

static void
copy_other_tagged (void)
{
    if (get_other_type () != view_listing)
	return;
    copy_tagged (opanel);
}

static void
do_suspend_cmd (void)
{
    pre_exec ();

    if (console_flag && !use_subshell)
	restore_console ();

#ifdef SIGTSTP
    {
	struct sigaction sigtstp_action;

	/* Make sure that the SIGTSTP below will suspend us directly,
	   without calling ncurses' SIGTSTP handler; we *don't* want
	   ncurses to redraw the screen immediately after the SIGCONT */
	sigaction (SIGTSTP, &startup_handler, &sigtstp_action);

	kill (getpid (), SIGTSTP);

	/* Restore previous SIGTSTP action */
	sigaction (SIGTSTP, &sigtstp_action, NULL);
    }
#endif				/* SIGTSTP */

    if (console_flag && !use_subshell)
	handle_console (CONSOLE_SAVE);

    edition_post_exec ();
}

void
suspend_cmd (void)
{
    save_cwds_stat ();
    do_suspend_cmd ();
    update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    do_refresh ();
}

static void
init_labels (void)
{
    define_label (midnight_dlg, 1, _("Help"), help_cmd);
    define_label (midnight_dlg, 2, _("Menu"), user_file_menu_cmd);
    define_label (midnight_dlg, 9, _("PullDn"), menu_cmd);
    define_label (midnight_dlg, 10, _("Quit"), quit_cmd);
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
#ifndef NATIVE_WIN32
    {'o', chown_cmd},
    {'r', copy_current_readlink},
    {XCTRL ('r'), copy_other_readlink},
    {'l', link_cmd},
    {'s', symlink_cmd},
    {XCTRL ('s'), edit_symlink_cmd},
#endif				/* !NATIVE_WIN32 */
    {'i', info_cmd_no_menu},
    {'q', quick_cmd_no_menu},
    {'h', add2hotlist_cmd},
    {'!', external_panelize},
#ifdef WITH_BACKGROUND
    {'j', jobs_cmd},
#endif				/* WITH_BACKGROUND */
#ifdef HAVE_SETSOCKOPT
    {'%', source_routing},
#endif				/* HAVE_SETSOCKOPT */
    {0, 0}
};

static int ctl_x_map_enabled = 0;

static void
ctl_x_cmd (int ignore)
{
    ctl_x_map_enabled = 1;
}

static void
nothing (void)
{
}

static const key_map default_map[] = {
    {KEY_F (19), menu_last_selected_cmd},
    {KEY_F (20), (key_callback) quiet_quit_cmd},

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

    /* Trap dlg's exit commands */
    {ESC_CHAR, nothing},
    {XCTRL ('c'), nothing},
    {XCTRL ('g'), nothing},
    {0, 0},
};

static void
setup_sigwinch (void)
{
#if (defined(HAVE_SLANG) || (NCURSES_VERSION_MAJOR >= 4)) && \
   !defined(NATIVE_WIN32) && defined(SIGWINCH)
    struct sigaction act, oact;
    act.sa_handler = flag_winch;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
#   ifdef SA_RESTART
    act.sa_flags |= SA_RESTART;
#   endif
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
    char *termvalue;
#ifdef HAVE_SLANG
    char *term_entry;
#endif

    termvalue = getenv ("TERM");
    if (!termvalue || !(*termvalue)) {
	fputs (_("The TERM environment variable is unset!\n"), stderr);
	exit (1);
    }

    /* Check mouse capabilities */
#ifdef HAVE_SLANG
    term_entry = SLtt_tigetent (termvalue);
    xmouse_seq = SLtt_tigetstr ("Km", &term_entry);
#else
    xmouse_seq = tigetstr ("kmous");
#endif

    /* -1 means invalid capability, shouldn't happen, but let's make it 0 */
    if ((long) xmouse_seq == -1) {
	xmouse_seq = NULL;
    }

    if (strcmp (termvalue, "cygwin") == 0) {
	force_xterm = 1;
	use_mouse_p = MOUSE_DISABLED;
    }

    if (force_xterm || strncmp (termvalue, "xterm", 5) == 0
	|| strncmp (termvalue, "rxvt", 4) == 0
	|| strcmp (termvalue, "dtterm") == 0) {
	xterm_flag = 1;

	/* Default to the standard xterm sequence */
	if (!xmouse_seq) {
	    xmouse_seq = ESC_STR "[M";
	}

	/* Enable mouse unless explicitly disabled by --nomouse */
	if (use_mouse_p != MOUSE_DISABLED) {
	    use_mouse_p = MOUSE_XTERM;
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

    if (baudrate () < 9600 || slow_terminal) {
	verbose = 0;
    }
    init_mouse ();
}

static void
setup_dummy_mc (const char *file)
{
    char d[MC_MAXPATHLEN];

    mc_get_current_wd (d, MC_MAXPATHLEN);
    setup_mc ();
    mc_chdir (d);

    /* Create a fake current_panel, this is needed because the
     * expand_format routine will use current panel.
     */
    strcpy (cpanel->cwd, d);
    cpanel->selected = 0;
    cpanel->count = 1;
    cpanel->dir.list[0].fname = (char *) file;
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
    else
	save_hotlist ();
    done_screen ();
    vfs_add_current_stamps ();
}

/* This should be called after destroy_dlg since panel widgets
 *  save their state on the profiles
 */
static void
done_mc_profile (void)
{
    if (!auto_save_setup)
	profile_forget_profile (profile_name);
    sync_profiles ();
    done_setup ();
    free_profiles ();
}

/*
 * Partly repaint the contents of the panels.
 * Ideally, all painting should be done in the panel's callback.
 * Since we are bypassing the standard widget library by forcing
 * the repaint, the cursor position needs to be preserved.
 */
static void
make_panels_dirty (void)
{
    int col, row;

    /* Preserve current cursor position */
    getyx (stdscr, row, col);

    if (cpanel->dirty)
	panel_update_contents (cpanel);

    if ((get_other_type () == view_listing) && opanel->dirty)
	panel_update_contents (opanel);

    /* Restore cursor position */
    move (row, col);
}

/* In Windows people want to actually type the '\' key frequently */
#ifdef NATIVE_WIN32
#   define check_key_backslash(x) 0
#else
#   define check_key_backslash(x) ((x) == '\\')
#endif

static int
midnight_callback (struct Dlg_head *h, int id, int msg)
{
    int i;
    static int first_pre_event = 1;

    switch (msg) {

    case DLG_PRE_EVENT:
	make_panels_dirty ();
	if (auto_menu && first_pre_event) {
	    user_file_menu_cmd ();
	}
	first_pre_event = 0;
	return MSG_HANDLED;

    case DLG_KEY:
	if (ctl_x_map_enabled) {
	    ctl_x_map_enabled = 0;
	    for (i = 0; ctl_x_map[i].key_code; i++)
		if (id == ctl_x_map[i].key_code) {
		    (*ctl_x_map[i].fn) (id);
		    return MSG_HANDLED;
		}
	}

	/* FIXME: should handle all menu shortcuts before this point */
	if (the_menubar->active)
	    break;

	if (id == KEY_F (10)) {
	    quit_cmd ();
	    return MSG_HANDLED;
	}

	if (id == '\t')
	    free_completions (cmdline);

	if (id == '\n' && cmdline->buffer[0]) {
	    send_message ((Widget *) cmdline, WIDGET_KEY, id);
	    return MSG_HANDLED;
	}

	/* Ctrl-Enter and Alt-Enter */
	if (((id & ~(KEY_M_CTRL | KEY_M_ALT)) == '\n')
	    && (id & (KEY_M_CTRL | KEY_M_ALT))) {
	    copy_prog_name ();
	    return MSG_HANDLED;
	}

	if ((!alternate_plus_minus || !(console_flag || xterm_flag))
	    && !quote && !cpanel->searching) {
	    if (!only_leading_plus_minus) {
		/* Special treatement, since the input line will eat them */
		if (id == '+') {
		    select_cmd ();
		    return MSG_HANDLED;
		}

		if (check_key_backslash (id) || id == '-') {
		    unselect_cmd ();
		    return MSG_HANDLED;
		}

		if (id == '*') {
		    reverse_selection_cmd ();
		    return MSG_HANDLED;
		}
	    } else if (command_prompt && !strlen (cmdline->buffer)) {
		/* Special treatement '+', '-', '\', '*' only when this is 
		 * first char on input line
		 */

		if (id == '+') {
		    select_cmd ();
		    return MSG_HANDLED;
		}

		if (check_key_backslash (id) || id == '-') {
		    unselect_cmd ();
		    return MSG_HANDLED;
		}

		if (id == '*') {
		    reverse_selection_cmd ();
		    return MSG_HANDLED;
		}
	    }
	}
	break;

    case DLG_HOTKEY_HANDLED:
	if (get_current_type () == view_listing)
	    cpanel->searching = 0;
	break;

    case DLG_UNHANDLED_KEY:
	if (command_prompt) {
	    int v;

	    v = send_message ((Widget *) cmdline, WIDGET_KEY, id);
	    if (v)
		return v;
	}
	if (ctl_x_map_enabled) {
	    ctl_x_map_enabled = 0;
	    for (i = 0; ctl_x_map[i].key_code; i++)
		if (id == ctl_x_map[i].key_code) {
		    (*ctl_x_map[i].fn) (id);
		    return MSG_HANDLED;
		}
	} else {
	    for (i = 0; default_map[i].key_code; i++) {
		if (id == default_map[i].key_code) {
		    (*default_map[i].fn) (id);
		    return MSG_HANDLED;
		}
	    }
	}
	return MSG_NOT_HANDLED;

	/* We handle the special case of the output lines */
    case DLG_DRAW:
	attrset (SELECTED_COLOR);
	if (console_flag && output_lines)
	    show_console_contents (output_start_y,
				   LINES - output_lines - keybar_visible -
				   1, LINES - keybar_visible - 1);
	return MSG_HANDLED;

    }
    return default_dlg_callback (h, id, msg);
}

#define xtoolkit_panel_setup()

/* Show current directory in the xterm title */
void
update_xterm_title_path (void)
{
    unsigned char *p, *s;

    if (xterm_flag && xterm_title) {
	p = s = g_strdup (strip_home_and_password (cpanel->cwd));
	do {
	    if (!is_printable (*s))
		*s = '?';
	} while (*++s);
	fprintf (stdout, "\33]0;mc - %s\7", p);
	fflush (stdout);
	g_free (p);
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
    int first, second;

    xtoolkit_panel_setup ();
    add_widget (midnight_dlg, the_hint);
    load_hint (1);
    add_widget (midnight_dlg, cmdline);
    add_widget (midnight_dlg, the_prompt);
    add_widget (midnight_dlg, the_bar);

    if (boot_current_is_left) {
	first = 1;
	second = 0;
    } else {
	first = 0;
	second = 1;
    }
    add_widget (midnight_dlg, get_panel_widget (first));
    add_widget (midnight_dlg, get_panel_widget (second));
    add_widget (midnight_dlg, the_menubar);

    init_labels ();

    /* Run the Midnight Commander if no file was specified in the command line */
    run_dlg (midnight_dlg);
}

/* result must be free'd (I think this should go in util.c) */
static char *
prepend_cwd_on_local (char *filename)
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
	return canonicalize_pathname (d);
    } else
	return g_strdup (filename);
}

static int
mc_maybe_editor_or_viewer (void)
{
    char *path = NULL;

    if (!(view_one_file || edit_one_file))
	return 0;

    /* Invoke the internal view/edit routine with:
     * the default processing and forcing the internal viewer/editor
     */
    if (view_one_file) {
	path = prepend_cwd_on_local (view_one_file);
	setup_dummy_mc (path);
	view_file (path, 0, 1);
    }
#ifdef USE_INTERNAL_EDIT
    else {
	path = prepend_cwd_on_local ("");
	setup_dummy_mc (path);
	edit (edit_one_file, edit_one_file_start_line);
    }
#endif				/* USE_INTERNAL_EDIT */
    g_free (path);
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

    midnight_dlg =
	create_dlg (0, 0, LINES, COLS, midnight_colors, midnight_callback,
		    "[main]", NULL, DLG_HAS_MENUBAR);

    /* Check if we were invoked as an editor or file viewer */
    if (mc_maybe_editor_or_viewer ())
	return;

    setup_mc ();

    setup_panels_and_run_mc ();

    /* Program end */
    midnight_shutdown = 1;

    /* destroy_dlg destroys even cpanel->cwd, so we have to save a copy :) */
    if (last_wd_file && vfs_current_is_local ()) {
	last_wd_string = g_strdup (cpanel->cwd);
    }
    done_mc ();

    destroy_dlg (midnight_dlg);
    current_panel = 0;
    done_mc_profile ();
}

#if defined (NATIVE_WIN32)
/* Windows NT code */

void
OS_Setup (void)
{
    SetConsoleTitle ("GNU Midnight Commander");

    shell = getenv ("COMSPEC");
    if (!shell || !*shell)
	shell = get_default_shell ();

    /* Default opening mode for files is binary, not text (CR/LF translation) */
#ifndef __EMX__
    _fmode = O_BINARY;
#endif

    mc_home = get_mc_lib_dir ();
}

/* Nothing to be done on Windows */
static void
sigchld_handler_no_subshell (int sig)
{
}

void
init_sigchld (void)
{
}

void
init_sigfatals (void)
{
}


#else

/* Unix version */
static void
OS_Setup (void)
{
    char *mc_libdir;
    shell = getenv ("SHELL");
    if (!shell || !*shell)
	shell = g_strdup (getpwuid (geteuid ())->pw_shell);
    if (!shell || !*shell)
	shell = "/bin/sh";

    /* This is the directory, where MC was installed, on Unix this is DATADIR */
    /* and can be overriden by the MC_DATADIR environment variable */
    if ((mc_libdir = getenv ("MC_DATADIR")) != NULL) {
	mc_home = g_strdup (mc_libdir);
    } else {
	mc_home = g_strdup (DATADIR);
    }
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
#endif				/* __linux__ */

    /* If we got here, some other child exited; ignore it */
}

void
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

#endif				/* NATIVE_WIN32, UNIX */

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
    version (0);
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
	    "   Global:       errors, reverse, gauge, input\n"
	    "   File display: normal, selected, marked, markselect\n"
	    "   Dialog boxes: dnormal, dfocus, dhotnormal, dhotfocus\n"
	    "   Menus:        menu, menuhot, menusel, menuhotsel\n"
	    "   Help:         helpnormal, helpitalic, helplink, helpslink\n"
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
	version (1);
	exit (0);
	break;

    case 'c':
	disable_colors = 0;
#ifdef HAVE_SLANG
	force_colors = 1;
#endif				/* HAVE_SLANG */
	break;

    case 'f':
	printf ("%s\n", mc_home);
	exit (0);
	break;

#ifdef USE_NETCODE
    case 'l':
	ftpfs_set_debug (option_arg);
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
#ifdef WITH_BACKGROUND
    {"background", 'B', POPT_ARG_NONE, &background_wait, 0,
     N_("Use to debug the background code")},
#endif
    {"color", 'c', POPT_ARG_NONE, NULL, 'c',
     N_("Request to run in color mode")},
    {"colors", 'C', POPT_ARG_STRING, &command_line_colors, 0,
     N_("Specifies a color configuration")},

#ifdef USE_INTERNAL_EDIT
    {"edit", 'e', POPT_ARG_STRING, &edit_one_file, 0,
     N_("Edits one file")},
#endif

    {"help", 'h', POPT_ARG_NONE, NULL, 'h',
     N_("Displays this help message")},
    {"help-colors", 'H', POPT_ARG_NONE, NULL, 'H',
     N_("Displays a help screen on how to change the color scheme")},
#ifdef USE_NETCODE
    {"ftplog", 'l', POPT_ARG_STRING, NULL, 'l',
     N_("Log ftp dialog to specified file")},
#ifdef WITH_SMBFS
    {"debuglevel", 'D', POPT_ARG_STRING, NULL, 'D',
     N_("Set debug level")},
#endif
#endif
    {"datadir", 'f', POPT_ARG_NONE, NULL, 'f',
     N_("Print data directory")},
    {"nocolor", 'b', POPT_ARG_NONE, &disable_colors, 0,
     N_("Requests to run in black and white")},
    {"nomouse", 'd', POPT_ARG_NONE, NULL, 'd',
     N_("Disable mouse support in text version")},
#ifdef HAVE_SUBSHELL_SUPPORT
    {"nosubshell", 'u', POPT_ARG_NONE, NULL, 'u',
     N_("Disables subshell support")},
    {"forceexec", 'r', POPT_ARG_NONE, &force_subshell_execution, 0,
     N_("Force subshell execution")},
#endif
    {"printwd", 'P', POPT_ARG_STRING, &last_wd_file, 0,
     N_("Print last working directory to specified file")},
    {"resetsoft", 'k', POPT_ARG_NONE, &reset_hp_softkeys, 0,
     N_("Resets soft keys on HP terminals")},
    {"slow", 's', POPT_ARG_NONE, &slow_terminal, 0,
     N_("To run on slow terminals")},
#ifndef NATIVE_WIN32
    {"stickchars", 'a', 0, &force_ugly_line_drawing, 0,
     N_("Use stickchars to draw")},
#endif				/* !NATIVE_WIN32 */
#ifdef HAVE_SUBSHELL_SUPPORT
    {"subshell", 'U', POPT_ARG_NONE, &use_subshell, 0,
     N_("Enables subshell support (default)")},
#endif
#if defined(HAVE_SLANG) && !defined(NATIVE_WIN32)
    {"termcap", 't', 0, &SLtt_Try_Termcap, 0,
     N_("Tries to use termcap instead of terminfo")},
#endif
    {"version", 'V', POPT_ARG_NONE, NULL, 'V',
     N_("Displays the current version")},
    {"view", 'v', POPT_ARG_STRING, &view_one_file, 0,
     N_("Launches the file viewer on a file")},
    {"xterm", 'x', POPT_ARG_NONE, &force_xterm, 0,
     N_("Forces xterm features")},

    {NULL, 0, 0, NULL, 0}
};

static void
handle_args (int argc, char *argv[])
{
    char *tmp;
    poptContext ctx;
    char *base;
    int c;

    ctx =
	poptGetContext ("mc", argc, argv, argument_table,
			POPT_CONTEXT_NO_EXEC);

#ifdef USE_TERMCAP
    SLtt_Try_Termcap = 1;
#endif

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
	    if (*tmp == '+' && isdigit ((unsigned char) tmp[1])) {
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

/*
 * The compatibility_move_mc_files routine is intended to
 * move all of the hidden .mc files into a private ~/.mc
 * directory in the home directory, to avoid cluttering the users.
 *
 * Previous versions of the program had all of their files in
 * the $HOME, we are now putting them in $HOME/.mc
 */
#ifdef NATIVE_WIN32
#    define compatibility_move_mc_files() 0
#else

static int
do_mc_filename_rename (char *mc_dir, char *o_name, char *n_name)
{
    char *full_o_name = concat_dir_and_file (home_dir, o_name);
    char *full_n_name = g_strconcat (home_dir, MC_BASE, n_name, NULL);
    int move;

    move = 0 == rename (full_o_name, full_n_name);
    g_free (full_o_name);
    g_free (full_n_name);
    return move;
}

static int
compatibility_move_mc_files (void)
{
    struct stat s;
    int move = 0;
    char *mc_dir = concat_dir_and_file (home_dir, ".mc");

    if (stat (mc_dir, &s) && (errno == ENOENT)
	&& (mkdir (mc_dir, 0777) != -1)) {

	move = do_mc_filename_rename (mc_dir, ".mc.ini", "ini");
	move += do_mc_filename_rename (mc_dir, ".mc.hot", "hotlist");
	move += do_mc_filename_rename (mc_dir, ".mc.ext", "bindings");
	move += do_mc_filename_rename (mc_dir, ".mc.menu", "menu");
	move += do_mc_filename_rename (mc_dir, ".mc.bashrc", "bashrc");
	move += do_mc_filename_rename (mc_dir, ".mc.inputrc", "inputrc");
	move += do_mc_filename_rename (mc_dir, ".mc.tcshrc", "tcshrc");
	move += do_mc_filename_rename (mc_dir, ".mc.tree", "Tree");
    }
    g_free (mc_dir);
    return move;
}
#endif				/* NATIVE_WIN32 */

int
main (int argc, char *argv[])
{
    /* if on, it displays the information that files have been moved to ~/.mc */
    int show_change_notice = 0;

    /* We had LC_CTYPE before, LC_ALL includs LC_TYPE as well */
    setlocale (LC_ALL, "");
    bindtextdomain ("mc", LOCALEDIR);
    textdomain ("mc");

    /* Initialize list of all user group for timur_clr_mode */
    init_groups ();

    /* Set up temporary directory */
    mc_tmpdir ();

    OS_Setup ();

    /* This variable is used by the subshell */
    home_dir = getenv ("HOME");
    if (!home_dir) {
	/* mc_home was computed by OS_Setup */
	home_dir = mc_home;
    }

    vfs_init ();

#ifdef HAVE_SLANG
    SLtt_Ignore_Beep = 1;
#endif

    /* NOTE: This has to be called before slang_init or whatever routine
       calls any define_sequence */
    init_key ();

    handle_args (argc, argv);

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

    show_change_notice = compatibility_move_mc_files ();

    /* We need this, since ncurses endwin () doesn't restore the signals */
    save_stop_handler ();

    /* Must be done before init_subshell, to set up the terminal size: */
    /* FIXME: Should be removed and LINES and COLS computed on subshell */
#ifdef HAVE_SLANG
    slang_init ();
#endif
    /* NOTE: This call has to be after slang_init. It's the small part from
       the previous init_key which had to be moved after the call of slang_init */
    init_key_input_fd ();

    load_setup ();

    init_curses ();

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

    if (show_change_notice) {
	message (1, _(" Notice "),
		 _(" The Midnight Commander configuration files \n"
		   " are now stored in the ~/.mc directory, the \n"
		   " files have been moved now\n"));
    }
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

    /* Delete list of all user groups */
    destroy_groups ();

    flush_extension_file ();	/* does only free memory */

    endwin ();
#ifdef HAVE_SLANG
    slang_shutdown ();
#endif

    if (console_flag && !(quit & SUBSHELL_EXIT))
	restore_console ();
    if (alternate_plus_minus)
	numeric_keypad_mode ();

#ifndef NATIVE_WIN32
    signal (SIGCHLD, SIG_DFL);	/* Disable the SIGCHLD handler */
#endif

    if (console_flag)
	handle_console (CONSOLE_DONE);
    putchar ('\n');		/* Hack to make shell's prompt start at left of screen */

#ifdef NATIVE_WIN32
    /* On NT, home_dir is malloced */
    g_free (home_dir);
#endif
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

#ifndef NATIVE_WIN32
    g_free (mc_home);
#endif				/* NATIVE_WIN32 */
    done_key ();
#ifdef HAVE_CHARSET
    free_codepages_list ();
#endif
    if (this_dir)
	g_free (this_dir);
    if (other_dir)
	g_free (other_dir);

    return 0;
}
