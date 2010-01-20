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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>		/* for username in xterm title */

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/skin/skin.h"
#include "lib/tty/mouse.h"
#include "lib/tty/key.h"		/* For init_key() */
#include "lib/tty/win.h"		/* xterm_flag */

#include "lib/mcconfig/mcconfig.h"
#include "src/args.h"
#include "lib/filehighlight/fhl.h"

#include "dir.h"
#include "dialog.h"
#include "menu.h"
#include "panel.h"
#include "option.h"
#include "tree.h"
#include "treestore.h"
#include "cons.saver.h"
#include "subshell.h"
#include "setup.h"		/* save_setup() */
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
#include "widget.h"
#include "command.h"
#include "wtools.h"
#include "cmddef.h"		/* CK_ cmd name const */
#include "fileloc.h"		/* MC_USERCONF_DIR */
#include "user.h"		/* user_file_menu_cmd() */

#include "lib/vfs/mc-vfs/vfs.h"		/* vfs_translate_url() */

#include "chmod.h"
#include "chown.h"
#include "achown.h"

#include "main.h"

#ifdef ENABLE_VFS_SMB
#include "lib/vfs/mc-vfs/smbfs.h"	/* smbfs_set_debug() */
#endif /* ENABLE_VFS_SMB */

#ifdef USE_INTERNAL_EDIT
#   include "src/editor/edit.h"
#endif

#ifdef	HAVE_CHARSET
#include "charsets.h"
#endif				/* HAVE_CHARSET */

#ifdef ENABLE_VFS
#include "lib/vfs/mc-vfs/gc.h"
#endif

#include "keybind.h"		/* type global_keymap_t */

/* When the modes are active, left_panel, right_panel and tree_panel */
/* Point to a proper data structure.  You should check with the functions */
/* get_current_type and get_other_type the types of the panels before using */
/* This pointer variables */

/* The structures for the panels */
WPanel *left_panel = NULL;
WPanel *right_panel = NULL;

mc_fhl_t *mc_filehighlight;

/* The pointer to the tree */
WTree *the_tree = NULL;

/* The Menubar */
struct WMenuBar *the_menubar = NULL;

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

/* If true, then the +, - and \ keys have their special meaning only if the
 * command line is emtpy, otherwise they behave like regular letters
 */
int only_leading_plus_minus = 1;

int pause_after_run = pause_on_dumb_terminals;

/* It true saves the setup when quitting */
int auto_save_setup = 1;

#ifdef HAVE_CHARSET
/*
 * Don't restrict the output on the screen manager level,
 * the translation tables take care of it.
 */
#define full_eight_bits (1)
#define eight_bit_clean (1)
#else				/* HAVE_CHARSET */
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

/* Mouse type: GPM, xterm or none */
Mouse_Type use_mouse_p = MOUSE_NONE;

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

/* The dialog handle for the main program */
Dlg_head *midnight_dlg = NULL;

/* Subshell: if set, then the prompt was not saved on CONSOLE_SAVE */
/* We need to paint it after CONSOLE_RESTORE, see: load_prompt */
int update_prompt = 0;

/* The home directory */
const char *home_dir = NULL;

/* Tab size */
int option_tab_spacing = 8;

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
static char *last_wd_string = NULL;
/* Set to 1 to suppress printing the last directory */
static int print_last_revert = 0;

/* File name to view if argument was supplied */
const char *view_one_file = NULL;

/* File name to edit if argument was supplied */
const char *edit_one_file = NULL;

/* Line to start the editor on */
static int edit_one_file_start_line = 0;

/* Used so that widgets know if they are being destroyed or
   shut down */
int midnight_shutdown = 0;

/* The user's shell */
char *shell = NULL;

/* mc_home: The home of MC - /etc/mc or defined by MC_DATADIR */
char *mc_home = NULL;

/* mc_home_alt: Alternative home of MC - deprecated /usr/share/mc */
char *mc_home_alt = NULL;

/* Define this function for glib-style error handling */
GQuark
mc_main_error_quark (void)
{
  return g_quark_from_static_string (PACKAGE);
}

#ifdef USE_INTERNAL_EDIT
GArray *editor_keymap = NULL;
GArray *editor_x_keymap = NULL;
#endif
GArray *viewer_keymap = NULL;
GArray *viewer_hex_keymap = NULL;
GArray *main_keymap = NULL;
GArray *main_x_keymap = NULL;
GArray *panel_keymap = NULL;
GArray *input_keymap = NULL;
GArray *tree_keymap = NULL;
GArray *help_keymap = NULL;

const global_keymap_t *main_map;
const global_keymap_t *main_x_map;

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
do_update_prompt (void)
{
    if (update_prompt) {
	printf ("\r\n%s", subshell_prompt);
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
	do_load_dir (panel->cwd, &panel->dir, panel->current_sort_field->sort_routine,
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

    s = show_hist (panel->dir_history, &panel->widget);

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
	 * tty_get_event channels, the prompt updating does not take place
	 * automatically: force a cursor update and a screen refresh
	 */
	update_cursor (midnight_dlg);
	mc_refresh ();
    }
    update_prompt = 1;
    return 0;
}
#endif				/* HAVE_SUBSHELL_SUPPORT */

void
sort_cmd (void)
{
    WPanel *p;
    const panel_field_t *sort_order;

    if (!SELECTED_IS_PANEL)
	return;

    p = MENU_PANEL;
    sort_order = sort_box (p->current_sort_field, &p->reverse,
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
static GList *
create_panel_menu (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&Listing mode..."), CK_ListingCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Quick view"),      CK_QuickViewCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Info" ),           CK_InfoCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Tree"),            CK_TreeCmd));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Sort order..."),   CK_Sort));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Filter..."),       CK_FilterCmd));
#ifdef HAVE_CHARSET
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Encoding..."),     CK_PanelSetPanelEncoding));
#endif
#ifdef USE_NETCODE
    entries = g_list_append (entries, menu_separator_create ());
#ifdef ENABLE_VFS_MCFS
    entries = g_list_append (entries, menu_entry_create (_("&Network link..."), CK_NetlinkCmd));
#endif
    entries = g_list_append (entries, menu_entry_create (_("FT&P link..."),     CK_FtplinkCmd));
    entries = g_list_append (entries, menu_entry_create (_("S&hell link..."),   CK_FishlinkCmd));
#ifdef ENABLE_VFS_SMB
    entries = g_list_append (entries, menu_entry_create (_("SM&B link..."),     CK_SmblinkCmd));
#endif /* ENABLE_VFS_SMB */
#endif
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Rescan"),          CK_RereadCmd));

    return entries;
}

static GList *
create_file_menu (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&View"),              CK_ViewCmd));
    entries = g_list_append (entries, menu_entry_create (_("Vie&w file..."),      CK_ViewFileCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Filtered view"),     CK_FilteredViewCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Edit"),              CK_EditCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Copy"),              CK_CopyCmd));
    entries = g_list_append (entries, menu_entry_create (_("C&hmod"),             CK_ChmodCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Link"),              CK_LinkCmd));
    entries = g_list_append (entries, menu_entry_create (_("&SymLink"),           CK_SymlinkCmd));
    entries = g_list_append (entries, menu_entry_create (_("Edit s&ymlink"),      CK_EditSymlinkCmd));
    entries = g_list_append (entries, menu_entry_create (_("Ch&own"),             CK_ChownCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Advanced chown"),    CK_ChownAdvancedCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Rename/Move"),       CK_RenameCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Mkdir"),             CK_MkdirCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Delete"),            CK_DeleteCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Quick cd"),          CK_QuickCdCmd));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Select &group"),      CK_SelectCmd));
    entries = g_list_append (entries, menu_entry_create (_("U&nselect group"),    CK_UnselectCmd));
    entries = g_list_append (entries, menu_entry_create (_("Reverse selec&tion"), CK_ReverseSelectionCmd));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("E&xit"),              CK_QuitCmd));

    return entries;
}

static GList *
create_command_menu (void)
{
    /* I know, I'm lazy, but the tree widget when it's not running
     * as a panel still has some problems, I have not yet finished
     * the WTree widget port, sorry.
     */
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&User menu"),                    CK_UserMenuCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Directory tree"),               CK_TreeBoxCmd));
    entries = g_list_append (entries, menu_entry_create (_("&Find file"),                    CK_FindCmd));
    entries = g_list_append (entries, menu_entry_create (_("S&wap panels"),                  CK_SwapCmd));
    entries = g_list_append (entries, menu_entry_create (_("Switch &panels on/off"),         CK_ShowCommandLine));
    entries = g_list_append (entries, menu_entry_create (_("&Compare directories"),          CK_CompareDirsCmd));
    entries = g_list_append (entries, menu_entry_create (_("E&xternal panelize"),            CK_ExternalPanelize));
    entries = g_list_append (entries, menu_entry_create (_("Show directory s&izes"),         CK_SingleDirsizeCmd));
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("Command &history"),              CK_HistoryCmd));
    entries = g_list_append (entries, menu_entry_create (_("Di&rectory hotlist"),            CK_QuickChdirCmd));
#ifdef ENABLE_VFS
    entries = g_list_append (entries, menu_entry_create (_("&Active VFS list"),              CK_ReselectVfs));
#endif
#ifdef WITH_BACKGROUND
    entries = g_list_append (entries, menu_entry_create (_("&Background jobs"),              CK_JobsCmd));
#endif
    entries = g_list_append (entries, menu_separator_create ());
#ifdef USE_EXT2FSLIB
    entries = g_list_append (entries, menu_entry_create (_("&Undelete files (ext2fs only)"), CK_UndeleteCmd));
#endif
#ifdef LISTMODE_EDITOR
    entries = g_list_append (entries, menu_entry_create (_("&Listing format edit"),          CK_ListmodeCmd));
#endif
#if defined (USE_EXT2FSLIB) || defined (LISTMODE_EDITOR)
    entries = g_list_append (entries, menu_separator_create ());
#endif
    entries = g_list_append (entries, menu_entry_create (_("Edit &extension file"),          CK_EditExtFileCmd));
    entries = g_list_append (entries, menu_entry_create (_("Edit &menu file"),               CK_EditMcMenuCmd));
    entries = g_list_append (entries, menu_entry_create (_("Edit hi&ghlighting group file"), CK_EditFhlFileCmd));

    return entries;
}

static GList *
create_options_menu (void)
{
    GList *entries = NULL;

    entries = g_list_append (entries, menu_entry_create (_("&Configuration..."), CK_ConfigureBox));
    entries = g_list_append (entries, menu_entry_create (_("&Layout..."),        CK_LayoutCmd));
    entries = g_list_append (entries, menu_entry_create (_("C&onfirmation..."),  CK_ConfirmBox));
    entries = g_list_append (entries, menu_entry_create (_("&Display bits..."),  CK_DisplayBitsBox));
    entries = g_list_append (entries, menu_entry_create (_("Learn &keys..."),    CK_LearnKeys));
#ifdef ENABLE_VFS
    entries = g_list_append (entries, menu_entry_create (_("&Virtual FS..."),    CK_ConfigureVfs));
#endif
    entries = g_list_append (entries, menu_separator_create ());
    entries = g_list_append (entries, menu_entry_create (_("&Save setup"),       CK_SaveSetupCmd));

    return entries;
}

void
init_menu (void)
{
    menubar_add_menu (the_menubar,
	create_menu (horizontal_split ? _("&Above") : _("&Left"),
					create_panel_menu (),   "[Left and Right Menus]"));
    menubar_add_menu (the_menubar,
	create_menu (_("&File"),	create_file_menu (),    "[File Menu]"));
    menubar_add_menu (the_menubar,
	create_menu (_("&Command"),	create_command_menu (), "[Command Menu]"));
    menubar_add_menu (the_menubar,
	create_menu (_("&Options"),	create_options_menu (), "[Options Menu]"));
    menubar_add_menu (the_menubar,
	create_menu (horizontal_split ? _("&Below") : _("&Right"),
					create_panel_menu (),   "[Left and Right Menus]"));
}

void
done_menu (void)
{
    menubar_set_menu (the_menubar, NULL);
}

static void
menu_last_selected_cmd (void)
{
    the_menubar->is_active = TRUE;
    the_menubar->is_dropped = (drop_menus != 0);
    the_menubar->previous_widget = midnight_dlg->current->dlg_id;
    dlg_select_widget (the_menubar);
}

static void
menu_cmd (void)
{
    if (the_menubar->is_active)
	return;

    if ((get_current_index () == 0) == (current_panel->active != 0))
	the_menubar->selected = 0;
    else
	the_menubar->selected = g_list_length (the_menubar->menu) - 1;
    menu_last_selected_cmd ();
}

static char *
midnight_get_shortcut (unsigned long command)
{
    const char *ext_map;
    const char *shortcut = NULL;

    shortcut = lookup_keymap_shortcut (main_map, command);
    if (shortcut != NULL)
	return g_strdup (shortcut);

    shortcut = lookup_keymap_shortcut (panel_map, command);
    if (shortcut != NULL)
	return g_strdup (shortcut);

    ext_map = lookup_keymap_shortcut (main_map, CK_StartExtMap1);
    if (ext_map != NULL)
	shortcut = lookup_keymap_shortcut (main_x_map, command);
    if (shortcut != NULL)
	return g_strdup_printf ("%s %s", ext_map, shortcut);

    return NULL;
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

void
toggle_kilobyte_si (void)
{
    kilobyte_si = !kilobyte_si;
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
    panel_view_mode_t current_mode, other_mode;
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

    the_menubar = menubar_new (0, 0, COLS, NULL);
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
    if (get_other_type () == view_listing)
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
    if (get_other_type () == view_listing)
	copy_tagged (other_panel);
}

void
midnight_set_buttonbar (WButtonBar *b)
{
    buttonbar_set_label (b,  1, Q_("ButtonBar|Help"), main_map, NULL);
    buttonbar_set_label (b,  2, Q_("ButtonBar|Menu"), main_map, NULL);
    buttonbar_set_label (b,  3, Q_("ButtonBar|View"), main_map, NULL);
    buttonbar_set_label (b,  4, Q_("ButtonBar|Edit"), main_map, NULL);
    buttonbar_set_label (b,  5, Q_("ButtonBar|Copy"), main_map, NULL);
    buttonbar_set_label (b,  6, Q_("ButtonBar|RenMov"), main_map, NULL);
    buttonbar_set_label (b,  7, Q_("ButtonBar|Mkdir"), main_map, NULL);
    buttonbar_set_label (b,  8, Q_("ButtonBar|Delete"), main_map, NULL);
    buttonbar_set_label (b,  9, Q_("ButtonBar|PullDn"), main_map, NULL);
    buttonbar_set_label (b, 10, Q_("ButtonBar|Quit"), main_map, NULL);
}

static gboolean ctl_x_map_enabled = FALSE;

static void
ctl_x_cmd (void)
{
    ctl_x_map_enabled = TRUE;
}

static cb_ret_t
midnight_execute_cmd (Widget *sender, unsigned long command)
{
    cb_ret_t res = MSG_HANDLED;

    (void) sender;

    switch (command) {
    case CK_AddHotlist:
        add2hotlist_cmd ();
        break;
    case CK_ChmodCmd:
        chmod_cmd ();
        break;
    case CK_ChownCmd:
        chown_cmd ();
        break;
    case CK_ChownAdvancedCmd:
        chown_advanced_cmd ();
        break;
    case CK_CompareDirsCmd:
        compare_dirs_cmd ();
        break;
    case CK_ConfigureBox:
        configure_box ();
        break;
#ifdef ENABLE_VFS
    case CK_ConfigureVfs:
        configure_vfs ();
        break;
#endif
    case CK_ConfirmBox:
        confirm_box ();
        break;
    case CK_CopyCmd:
        copy_cmd ();
        break;
    case CK_CopyCurrentPathname:
        copy_current_pathname ();
        break;
    case CK_CopyCurrentReadlink:
        copy_current_readlink ();
        break;
    case CK_CopyCurrentTagged:
        copy_current_tagged ();
        break;
    case CK_CopyOtherPathname:
        copy_other_pathname ();
        break;
    case CK_CopyOtherReadlink:
        copy_other_readlink ();
        break;
    case CK_CopyOtherTagged:
        copy_other_tagged ();
        break;
    case CK_DeleteCmd:
        delete_cmd ();
        break;
    case CK_DisplayBitsBox:
        display_bits_box ();
        break;
    case CK_EditCmd:
        edit_cmd ();
        break;
    case CK_EditExtFileCmd:
        ext_cmd ();
        break;
    case CK_EditFhlFileCmd:
        edit_fhl_cmd ();
        break;
    case CK_EditMcMenuCmd:
        edit_mc_menu_cmd ();
        break;
    case CK_EditSymlinkCmd:
        edit_symlink_cmd ();
        break;
    case CK_ExternalPanelize:
        external_panelize ();
        break;
    case CK_FilterCmd:
        filter_cmd ();
        break;
    case CK_FilteredViewCmd:
        filtered_view_cmd ();
        break;
    case CK_FindCmd:
        find_cmd ();
        break;
#if defined (USE_NETCODE)
    case CK_FishlinkCmd:
        fishlink_cmd ();
        break;
    case CK_FtplinkCmd:
        ftplink_cmd ();
        break;
#endif
    case CK_HelpCmd:
        help_cmd ();
        break;
    case CK_HistoryCmd:
        history_cmd ();
        break;
    case CK_InfoCmd:
        if (sender == (Widget *) the_menubar)
            info_cmd ();                /* mwnu */
        else
            info_cmd_no_menu ();        /* shortcut or buttonbar */
        break;
#ifdef WITH_BACKGROUND
    case CK_JobsCmd:
        jobs_cmd ();
        break;
#endif
    case CK_LayoutCmd:
        layout_cmd ();
        break;
    case CK_LearnKeys:
        learn_keys ();
        break;
    case CK_LinkCmd:
        link_cmd ();
        break;
    case CK_ListingCmd:
        listing_cmd ();
        break;
#ifdef LISTMODE_EDITOR
    case CK_ListmodeCmd:
        listmode_cmd ();
        break;
#endif
    case CK_MenuCmd:
        menu_cmd ();
        break;
    case CK_MenuLastSelectedCmd:
        menu_last_selected_cmd ();
        break;
    case CK_MkdirCmd:
        mkdir_cmd ();
        break;
#if defined (USE_NETCODE) && defined (ENABLE_VFS_MCFS)
    case CK_NetlinkCmd:
        netlink_cmd ();
        break;
#endif
#ifdef HAVE_CHARSET
    case CK_PanelSetPanelEncoding:
        encoding_cmd ();
        break;
#endif
    case CK_QuickCdCmd:
        quick_cd_cmd ();
        break;
    case CK_QuickChdirCmd:
        quick_chdir_cmd ();
        break;
    case CK_QuickViewCmd:
        if (sender == (Widget *) the_menubar)
            quick_view_cmd ();          /* menu */
        else
            quick_cmd_no_menu ();       /* shortcut or buttonabr */
        break;
    case CK_QuietQuitCmd:
        quiet_quit_cmd ();
        break;
    case CK_QuitCmd:
        quit_cmd ();
        break;
    case CK_RenameCmd:
        rename_cmd ();
        break;
    case CK_RereadCmd:
        reread_cmd ();
        break;
#ifdef ENABLE_VFS
    case CK_ReselectVfs:
        reselect_vfs ();
        break;
#endif
    case CK_ReverseSelectionCmd:
        reverse_selection_cmd ();
        break;
    case CK_SaveSetupCmd:
        save_setup_cmd ();
        break;
    case CK_SelectCmd:
        select_cmd ();
        break;
    case CK_ShowCommandLine:
        view_other_cmd ();
        break;
    case CK_SingleDirsizeCmd:
        smart_dirsize_cmd ();
        break;
#if defined (USE_NETCODE) && defined (ENABLE_VFS_SMB)
    case CK_SmblinkCmd:
        smblink_cmd ();
        break;
#endif /* USE_NETCODE && ENABLE_VFS_SMB */
    case CK_Sort:
        sort_cmd ();
        break;
    case CK_StartExtMap1:
        ctl_x_cmd ();
        break;
    case CK_SuspendCmd:
        suspend_cmd ();
        break;
    case CK_SwapCmd:
        swap_cmd ();
        break;
    case CK_SymlinkCmd:
        symlink_cmd ();
        break;
    case CK_ToggleListingCmd:
        toggle_listing_cmd ();
        break;
    case CK_ToggleShowHidden:
        toggle_show_hidden ();
        break;
    case CK_TreeCmd:
        tree_cmd ();
        break;
    case CK_TreeBoxCmd:
        treebox_cmd ();
        break;
#ifdef USE_EXT2FSLIB
    case CK_UndeleteCmd:
        undelete_cmd ();
        break;
#endif
    case CK_UnselectCmd:
        unselect_cmd ();
        break;
    case CK_UserMenuCmd:
        user_file_menu_cmd ();
        break;
    case CK_ViewCmd:
        view_cmd ();
        break;
    case CK_ViewFileCmd:
        view_file_cmd ();
        break;
    default:
        res = MSG_NOT_HANDLED;
    }

    return res;
}

static void
setup_pre (void)
{
    /* Call all the inits */

#ifdef HAVE_SLANG
    tty_display_8bit (full_eight_bits != 0);
#else
    tty_display_8bit (eight_bit_clean != 0);
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
	mc_args__force_xterm = 1;
	use_mouse_p = MOUSE_DISABLED;
    }

    if (mc_args__force_xterm || strncmp (termvalue, "xterm", 5) == 0
	|| strncmp (termvalue, "konsole", 7) == 0
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
    create_panels ();
    setup_panels ();

#ifdef HAVE_SUBSHELL_SUPPORT
    if (use_subshell)
	add_select_channel (subshell_pty, load_prompt, 0);
#endif				/* !HAVE_SUBSHELL_SUPPORT */

    tty_setup_sigwinch (sigwinch_handler);

    verbose = !((tty_baudrate () < 9600) || tty_is_slow ());

    init_xterm_support ();
    init_mouse ();
}

static void
setup_dummy_mc (void)
{
    char d[MC_MAXPATHLEN];

    mc_get_current_wd (d, MC_MAXPATHLEN);
    setup_mc ();
    mc_chdir (d);
}

static void check_codeset()
{
    const char *current_system_codepage = NULL;

    current_system_codepage = str_detect_termencoding();

#ifdef HAVE_CHARSET
    {
    const char *_display_codepage;

    _display_codepage = get_codepage_id (display_codepage);

    if (! strcmp(_display_codepage, current_system_codepage)) {
	utf8_display = str_isutf8 (current_system_codepage);
	return;
    }

    display_codepage = get_codepage_index (current_system_codepage);
    if (display_codepage == -1) {
	display_codepage = 0;
    }

    mc_config_set_string(mc_main_config, "Misc", "display_codepage", cp_display);
    }
#endif
    utf8_display = str_isutf8 (current_system_codepage);
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
midnight_callback (Dlg_head *h, Widget *sender,
		    dlg_msg_t msg, int parm, void *data)
{
    unsigned long command;

    switch (msg) {

    case DLG_DRAW:
	load_hint (1);
	/* We handle the special case of the output lines */
	if (console_flag && output_lines)
	    show_console_contents (output_start_y,
				   LINES - output_lines - keybar_visible -
				   1, LINES - keybar_visible - 1);
	return MSG_HANDLED;

    case DLG_RESIZE:
	setup_panels ();
	menubar_arrange (the_menubar);
	return MSG_HANDLED;

    case DLG_IDLE:
	/* We only need the first idle event to show user menu after start */
	set_idle_proc (h, 0);
	if (auto_menu)
	    midnight_execute_cmd (NULL, CK_UserMenuCmd);
	return MSG_HANDLED;

    case DLG_KEY:
	if (ctl_x_map_enabled) {
	    ctl_x_map_enabled = FALSE;
	    command = lookup_keymap_command (main_x_map, parm);
	    if (command != CK_Ignore_Key)
		return midnight_execute_cmd (NULL, command);
	}

	/* FIXME: should handle all menu shortcuts before this point */
	if (the_menubar->is_active)
	    return MSG_NOT_HANDLED;

	if (parm == '\t')
	    free_completions (cmdline);

	if (parm == '\n') {
	    size_t i;

	    for (i = 0; cmdline->buffer[i] && (cmdline->buffer[i] == ' ' ||
		cmdline->buffer[i] == '\t'); i++)
		;
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
	    cb_ret_t v;

	    v = send_message ((Widget *) cmdline, WIDGET_KEY, parm);
	    if (v == MSG_HANDLED)
		return MSG_HANDLED;
	}

	if (ctl_x_map_enabled) {
	    ctl_x_map_enabled = FALSE;
	    command = lookup_keymap_command (main_x_map, parm);
	} else
	    command = lookup_keymap_command (main_map, parm);

	return (command == CK_Ignore_Key)
			? MSG_NOT_HANDLED
			: midnight_execute_cmd (NULL, command);

    case DLG_POST_KEY:
	if (!the_menubar->is_active)
	    update_dirty_panels ();
	return MSG_HANDLED;

    case DLG_ACTION:
	/* shortcut */
	if (sender == NULL)
	    midnight_execute_cmd (NULL, parm);
	/* message from menu */
	else if (sender == (Widget *) the_menubar)
	    midnight_execute_cmd (sender, parm);
	/* message from buttonbar */
	else if (sender == (Widget *) the_bar) {
	    if (data == NULL)
		midnight_execute_cmd (sender, parm);
	    else
		send_message ((Widget *) data, WIDGET_COMMAND, parm);
	}
	return MSG_HANDLED;

    default:
	return default_dlg_callback (h, sender, msg, parm, data);
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
	label_set_text (the_hint, NULL);
	return;
    }

    hint = get_random_hint (force);

    if (hint != NULL) {
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
    midnight_dlg->get_shortcut = midnight_get_shortcut;

    add_widget (midnight_dlg, the_menubar);
    init_menu ();

    add_widget (midnight_dlg, get_panel_widget (0));
    add_widget (midnight_dlg, get_panel_widget (1));
    add_widget (midnight_dlg, the_hint);
    add_widget (midnight_dlg, cmdline);
    add_widget (midnight_dlg, the_prompt);

    add_widget (midnight_dlg, the_bar);
    midnight_set_buttonbar (the_bar);

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
    int midnight_colors[DLG_COLOR_NUM];
    midnight_colors[0] = mc_skin_color_get("dialog", "_default_");
    midnight_colors[1] = mc_skin_color_get("dialog", "focus");
    midnight_colors[2] = mc_skin_color_get("dialog", "hotnormal");
    midnight_colors[3] = mc_skin_color_get("dialog", "hotfocus");

    panel_init ();

    midnight_dlg = create_dlg (0, 0, LINES, COLS, midnight_colors, midnight_callback,
			       "[main]", NULL, DLG_WANT_IDLE);

    if (view_one_file || edit_one_file)
	setup_dummy_mc ();
    else
	setup_mc ();

    /* start check display_codepage and source_codepage */
    check_codeset ();

    main_map = default_main_map;
    if (main_keymap && main_keymap->len > 0)
        main_map = (global_keymap_t *) main_keymap->data;

    main_x_map = default_main_x_map;
    if (main_x_keymap && main_x_keymap->len > 0)
        main_x_map = (global_keymap_t *) main_x_keymap->data;

    panel_map = default_panel_keymap;
    if (panel_keymap && panel_keymap->len > 0)
        panel_map = (global_keymap_t *) panel_keymap->data;

    input_map = default_input_keymap;
    if (input_keymap && input_keymap->len > 0)
        input_map = (global_keymap_t *) input_keymap->data;

    tree_map = default_tree_keymap;
    if (tree_keymap && tree_keymap->len > 0)
        tree_map = (global_keymap_t *) tree_keymap->data;

    help_map = default_help_keymap;
    if (help_keymap && help_keymap->len > 0)
        help_map = (global_keymap_t *) help_keymap->data;

    /* Check if we were invoked as an editor or file viewer */
    if (!mc_maybe_editor_or_viewer ()) {
	setup_panels_and_run_mc ();

	/* Program end */
	midnight_shutdown = 1;

	/* destroy_dlg destroys even current_panel->cwd, so we have to save a copy :) */
	if (mc_args__last_wd_file && vfs_current_is_local ())
	    last_wd_string = g_strdup (current_panel->cwd);

	done_mc ();
    }

    destroy_dlg (midnight_dlg);
    panel_deinit ();
    current_panel = 0;
    done_mc_profile ();
}

/* POSIX version.  The only version we support.  */
static void
OS_Setup (void)
{
    const char *shell_env = getenv ("SHELL");
    const char *mc_libdir;

    if ((shell_env == NULL) || (shell_env[0] == '\0')) {
        struct passwd *pwd;
        pwd = getpwuid (geteuid ());
        if (pwd != NULL)
           shell = g_strdup (pwd->pw_shell);
    } else
	shell = g_strdup (shell_env);

    if ((shell == NULL) || (shell[0] == '\0')) {
	g_free (shell);
	shell = g_strdup ("/bin/sh");
    }

    /* This is the directory, where MC was installed, on Unix this is DATADIR */
    /* and can be overriden by the MC_DATADIR environment variable */
    mc_libdir = getenv ("MC_DATADIR");
    if (mc_libdir != NULL) {
	mc_home = g_strdup (mc_libdir);
	mc_home_alt = g_strdup (SYSCONFDIR);
    } else {
	mc_home = g_strdup (SYSCONFDIR);
	mc_home_alt = g_strdup (DATADIR);
    }

    /* This variable is used by the subshell */
    home_dir = getenv ("HOME");

    if (!home_dir)
	home_dir = mc_home;
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
mc_main__setup_by_args (int argc, char *argv[])
{
    const char *base;
    char *tmp;

    if (mc_args__nomouse)
	use_mouse_p = MOUSE_DISABLED;

#ifdef USE_NETCODE
    if (mc_args__netfs_logfile != NULL) {
	mc_setctl ("/#ftp:", VFS_SETCTL_LOGFILE, (void *) mc_args__netfs_logfile);
#ifdef ENABLE_VFS_SMB
	smbfs_set_debugf (mc_args__netfs_logfile);
#endif				/* ENABLE_VFS_SMB */
    }

#ifdef ENABLE_VFS_SMB
    if (mc_args__debug_level != 0)
	smbfs_set_debug (mc_args__debug_level);
#endif				/* ENABLE_VFS_SMB */
#endif				/* USE_NETCODE */

    base = x_basename (argv[0]);
    tmp = (argc > 0) ? argv[1] : NULL;

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
			char *file = (argc > 1) ? argv[2] : NULL;
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
	    tmp = (argc > 1) ? argv[2] : NULL;
	    if (tmp)
		other_dir = g_strdup (tmp);
	}
    }
}

int
main (int argc, char *argv[])
{
    struct stat s;
    char *mc_dir;
    GError *error = NULL;
    gboolean isInitialized;

    /* We had LC_CTYPE before, LC_ALL includs LC_TYPE as well */
    setlocale (LC_ALL, "");
    bindtextdomain ("mc", LOCALEDIR);
    textdomain ("mc");

    /* Set up temporary directory */
    mc_tmpdir ();

    OS_Setup ();

    str_init_strings (NULL);

    vfs_init ();

#ifdef USE_INTERNAL_EDIT
    edit_stack_init ();
#endif

#ifdef HAVE_SLANG
    SLtt_Ignore_Beep = 1;
#endif

    if ( !mc_args_handle (&argc, &argv, "mc"))
	return 1;

    mc_main__setup_by_args (argc,argv);

    /* NOTE: This has to be called before tty_init or whatever routine
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
    tty_init ((gboolean) mc_args__slow_terminal, (gboolean) mc_args__ugly_line_drawing);

    load_setup ();

    tty_init_colors (mc_args__disable_colors, mc_args__force_colors);

    isInitialized = mc_skin_init(&error);

    mc_filehighlight = mc_fhl_new (TRUE);

    dlg_set_default_colors ();

    if ( ! isInitialized ) {
        message (D_ERROR, _("Warning"), "%s", error->message);
        g_error_free(error);
        error = NULL;
    }

    /* create home directory */
    /* do it after the screen library initialization to show the error message */
    mc_dir = concat_dir_and_file (home_dir, MC_USERCONF_DIR);
    canonicalize_pathname (mc_dir);
    if ((stat (mc_dir, &s) != 0) && (errno == ENOENT)
	&& mkdir (mc_dir, 0700) != 0)
	message (D_ERROR, _("Warning"),
		    _("Cannot create %s directory"), mc_dir);
    g_free (mc_dir);

#ifdef HAVE_SUBSHELL_SUPPORT
    /* Done here to ensure that the subshell doesn't  */
    /* inherit the file descriptors opened below, etc */
    if (use_subshell)
	init_subshell ();

#endif				/* HAVE_SUBSHELL_SUPPORT */

    /* Removing this from the X code let's us type C-c */
    load_key_defs ();

    load_keymap_defs ();

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

    free_keymap_defs ();

    /* Virtual File System shutdown */
    vfs_shut ();

    flush_extension_file ();	/* does only free memory */

    mc_fhl_free (&mc_filehighlight);
    mc_skin_deinit ();

    tty_shutdown ();

    if (console_flag && !(quit & SUBSHELL_EXIT))
	handle_console (CONSOLE_RESTORE);
    if (alternate_plus_minus)
	numeric_keypad_mode ();

    signal (SIGCHLD, SIG_DFL);	/* Disable the SIGCHLD handler */

    if (console_flag)
	handle_console (CONSOLE_DONE);
    putchar ('\n');		/* Hack to make shell's prompt start at left of screen */

    if (mc_args__last_wd_file && last_wd_string && !print_last_revert
	&& !edit_one_file && !view_one_file) {
	int last_wd_fd =
	    open (mc_args__last_wd_file, O_WRONLY | O_CREAT | O_TRUNC | O_EXCL,
		  S_IRUSR | S_IWUSR);

	if (last_wd_fd != -1) {
	    write (last_wd_fd, last_wd_string, strlen (last_wd_string));
	    close (last_wd_fd);
	}
    }
    g_free (last_wd_string);

    g_free (mc_home_alt);
    g_free (mc_home);
    g_free (shell);

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
