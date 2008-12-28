/* Routines invoked by a function key
   They normally operate on the current panel.
   
   Copyright (C) 1994, 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007 Free Software Foundation, Inc.
   
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

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
#   include <sys/mman.h>
#endif
#ifdef USE_NETCODE
#include <netdb.h>
#endif
#include <unistd.h>

#include "global.h"
#include "cmd.h"		/* Our definitions */
#include "fileopctx.h"		/* file_op_context_new() */
#include "file.h"		/* copy_file_file() */
#include "find.h"		/* do_find() */
#include "hotlist.h"		/* hotlist_cmd() */
#include "tree.h"		/* tree_chdir() */
#include "subshell.h"		/* use_subshell */
#include "cons.saver.h"		/* console_flag */
#include "tty.h"		/* LINES */
#include "dialog.h"		/* Widget */
#include "view.h"		/* mc_internal_viewer() */
#include "wtools.h"		/* message() */
#include "widget.h"		/* push_history() */
#include "key.h"		/* application_keypad_mode() */
#include "win.h"		/* do_enter_ca_mode() */
#include "main.h"		/* change_panel() */
#include "panel.h"		/* current_panel */
#include "help.h"		/* interactive_display() */
#include "user.h"		/* MC_GLOBAL_MENU */
#include "command.h"		/* cmdline */
#include "layout.h"		/* get_current_type() */
#include "ext.h"		/* regex_command() */
#include "boxes.h"		/* cd_dialog() */
#include "setup.h"		/* save_setup() */
#include "profile.h"		/* PROFILE_NAME */
#include "execute.h"		/* toggle_panels() */
#include "history.h"
#include "strutil.h"

#ifndef MAP_FILE
#   define MAP_FILE 0
#endif

#ifdef USE_INTERNAL_EDIT
#   include "../edit/edit.h"
#endif

/* If set and you don't have subshell support,then C-o will give you a shell */
int output_starts_shell = 0;

/* If set, use the builtin editor */
int use_internal_edit = 1;


int
view_file_at_line (const char *filename, int plain_view, int internal,
		   int start_line)
{
    static const char *viewer = NULL;
    int move_dir = 0;


    if (plain_view) {
	int changed_hex_mode = 0;
	int changed_nroff_flag = 0;
	int changed_magic_flag = 0;

	altered_hex_mode = 0;
	altered_nroff_flag = 0;
	altered_magic_flag = 0;
	if (default_hex_mode)
	    changed_hex_mode = 1;
	if (default_nroff_flag)
	    changed_nroff_flag = 1;
	if (default_magic_flag)
	    changed_magic_flag = 1;
	default_hex_mode = 0;
	default_nroff_flag = 0;
	default_magic_flag = 0;
	mc_internal_viewer (NULL, filename, &move_dir, start_line);
	if (changed_hex_mode && !altered_hex_mode)
	    default_hex_mode = 1;
	if (changed_nroff_flag && !altered_nroff_flag)
	    default_nroff_flag = 1;
	if (changed_magic_flag && !altered_magic_flag)
	    default_magic_flag = 1;
	repaint_screen ();
	return move_dir;
    }
    if (internal) {
	char view_entry[BUF_TINY];

	if (start_line != 0)
	    g_snprintf (view_entry, sizeof (view_entry), "View:%d",
			start_line);
	else
	    strcpy (view_entry, "View");

	if (regex_command (filename, view_entry, &move_dir) == 0) {
	    mc_internal_viewer (NULL, filename, &move_dir, start_line);
	    repaint_screen ();
	}
    } else {
	if (!viewer) {
	    viewer = getenv ("VIEWER");
	    if (!viewer)
		viewer = getenv ("PAGER");
	    if (!viewer)
		viewer = "view";
	}
	execute_with_vfs_arg (viewer, filename);
    }
    return move_dir;
}

/* view_file (filename, plain_view, internal)
 *
 * Inputs:
 *   filename:   The file name to view
 *   plain_view: If set does not do any fancy pre-processing (no filtering) and
 *               always invokes the internal viewer.
 *   internal:   If set uses the internal viewer, otherwise an external viewer.
 */
int
view_file (const char *filename, int plain_view, int internal)
{
    return view_file_at_line (filename, plain_view, internal, 0);
}

/* scan_for_file (panel, idx, direction)
 *
 * Inputs:
 *   panel:     pointer to the panel on which we operate
 *   idx:       starting file.
 *   direction: 1, or -1
 */
static int scan_for_file (WPanel *panel, int idx, int direction)
{
    int i = idx + direction;

    while (i != idx){
	if (i < 0)
	    i = panel->count - 1;
	if (i == panel->count)
	    i = 0;
	if (!S_ISDIR (panel->dir.list [i].st.st_mode))
	    return i;
	i += direction;
    }
    return i;
}

/*
 * Run viewer (internal or external) on the currently selected file.
 * If normal is 1, force internal viewer and raw mode (used for F13).
 */
static void
do_view_cmd (int normal)
{
    int dir, file_idx;

    /* Directories are viewed by changing to them */
    if (S_ISDIR (selection (current_panel)->st.st_mode)
	|| link_isdir (selection (current_panel))) {
	if (confirm_view_dir && (current_panel->marked || current_panel->dirs_marked)) {
	    if (query_dialog
		(_(" Confirmation "), _("Files tagged, want to cd?"), D_NORMAL, 2,
		 _("&Yes"), _("&No")) != 0) {
		return;
	    }
	}
	if (!do_cd (selection (current_panel)->fname, cd_exact))
	    message (D_ERROR, MSG_ERROR, _("Cannot change directory"));

	repaint_screen();
	return;
    }

    file_idx = current_panel->selected;
    while (1) {
	char *filename;

	filename = current_panel->dir.list[file_idx].fname;

	dir = view_file (filename, normal, use_internal_view);
	if (dir == 0)
	    break;
	file_idx = scan_for_file (current_panel, file_idx, dir);
    }

    repaint_screen();
}

/* Run user's preferred viewer on the currently selected file */
void
view_cmd (void)
{
    do_view_cmd (0);
}

/* Ask for file and run user's preferred viewer on it */
void
view_file_cmd (void)
{
    char *filename;

    filename =
	input_expand_dialog (_(" View file "), _(" Filename:"),
			     MC_HISTORY_FM_VIEW_FILE, selection (current_panel)->fname);
    if (!filename)
	return;

    view_file (filename, 0, use_internal_view);
    g_free (filename);
}

/* Run plain internal viewer on the currently selected file */
void
view_simple_cmd (void)
{
    do_view_cmd (1);
}

void
filtered_view_cmd (void)
{
    char *command;

    command =
	input_dialog (_(" Filtered view "),
		      _(" Filter command and arguments:"),
		      MC_HISTORY_FM_FILTERED_VIEW,
		      selection (current_panel)->fname);
    if (!command)
	return;

    mc_internal_viewer (command, "", NULL, 0);

    g_free (command);
}

void do_edit_at_line (const char *what, int start_line)
{
    static const char *editor = NULL;

#ifdef USE_INTERNAL_EDIT
    if (use_internal_edit){
	edit_file (what, start_line);
	update_panels (UP_OPTIMIZE, UP_KEEPSEL);
	repaint_screen ();
	return;
    }
#endif /* USE_INTERNAL_EDIT */
    if (!editor){
	editor = getenv ("EDITOR");
	if (!editor)
	    editor = get_default_editor ();
    }
    execute_with_vfs_arg (editor, what);
    update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    repaint_screen ();
}

static void
do_edit (const char *what)
{
    do_edit_at_line (what, 0);
}

void
edit_cmd (void)
{
    if (regex_command (selection (current_panel)->fname, "Edit", 0) == 0)
	do_edit (selection (current_panel)->fname);
}

void
edit_cmd_new (void)
{
    do_edit (NULL);
}

/* Invoked by F5.  Copy, default to the other panel.  */
void
copy_cmd (void)
{
    save_cwds_stat ();
    if (panel_operate (current_panel, OP_COPY, 0)) {
	update_panels (UP_OPTIMIZE, UP_KEEPSEL);
	repaint_screen ();
    }
}

/* Invoked by F6.  Move/rename, default to the other panel, ignore marks.  */
void ren_cmd (void)
{
    save_cwds_stat ();
    if (panel_operate (current_panel, OP_MOVE, 0)){
	update_panels (UP_OPTIMIZE, UP_KEEPSEL);
	repaint_screen ();
    }
}

/* Invoked by F15.  Copy, default to the same panel, ignore marks.  */
void copy_cmd_local (void)
{
    save_cwds_stat ();
    if (panel_operate (current_panel, OP_COPY, 1)){
	update_panels (UP_OPTIMIZE, UP_KEEPSEL);
	repaint_screen ();
    }
}

/* Invoked by F16.  Move/rename, default to the same panel.  */
void ren_cmd_local (void)
{
    save_cwds_stat ();
    if (panel_operate (current_panel, OP_MOVE, 1)){
	update_panels (UP_OPTIMIZE, UP_KEEPSEL);
	repaint_screen ();
    }
}

void
mkdir_cmd (void)
{
    char *dir, *absdir;

    dir =
	input_expand_dialog (_("Create a new Directory"),
			     _(" Enter directory name:"), 
			     MC_HISTORY_FM_MKDIR, "");
    if (!dir)
	return;

    if (dir[0] == '/' || dir[0] == '~')
	absdir = g_strdup (dir);
    else
	absdir = concat_dir_and_file (current_panel->cwd, dir);

    save_cwds_stat ();
    if (my_mkdir (absdir, 0777) == 0) {
	update_panels (UP_OPTIMIZE, dir);
	repaint_screen ();
	select_item (current_panel);
    } else {
	message (D_ERROR, MSG_ERROR, "  %s  ", unix_error_string (errno));
    }

    g_free (absdir);
    g_free (dir);
}

void delete_cmd (void)
{
    save_cwds_stat ();

    if (panel_operate (current_panel, OP_DELETE, 0)){
	update_panels (UP_OPTIMIZE, UP_KEEPSEL);
	repaint_screen ();
    }
}

/* Invoked by F18.  Remove selected file, regardless of marked files.  */
void delete_cmd_local (void)
{
    save_cwds_stat ();

    if (panel_operate (current_panel, OP_DELETE, 1)){
	update_panels (UP_OPTIMIZE, UP_KEEPSEL);
	repaint_screen ();
    }
}

void find_cmd (void)
{
    do_find ();
}

static void
set_panel_filter_to (WPanel *p, char *allocated_filter_string)
{
    g_free (p->filter);
    p->filter = 0;

    if (!(allocated_filter_string [0] == '*' && allocated_filter_string [1] == 0))
	p->filter = allocated_filter_string;
    else
	g_free (allocated_filter_string);
    reread_cmd ();
}

/* Set a given panel filter expression */
static void
set_panel_filter (WPanel *p)
{
    char *reg_exp;
    const char *x;
    
    x = p->filter ? p->filter : easy_patterns ? "*" : ".";
	
    reg_exp = input_dialog_help (_(" Filter "),
				 _(" Set expression for filtering filenames"),
				 "[Filter...]", MC_HISTORY_FM_PANEL_FILTER, x);
    if (!reg_exp)
	return;
    set_panel_filter_to (p, reg_exp);
}

/* Invoked from the left/right menus */
void filter_cmd (void)
{
    WPanel *p;

    if (!SELECTED_IS_PANEL)
	return;

    p = MENU_PANEL;
    set_panel_filter (p);
}

void reread_cmd (void)
{
    int flag;

    if (get_current_type () == view_listing &&
	get_other_type () == view_listing)
	flag = strcmp (current_panel->cwd, other_panel->cwd) ? UP_ONLY_CURRENT : 0;
    else
	flag = UP_ONLY_CURRENT;
	
    update_panels (UP_RELOAD|flag, UP_KEEPSEL);
    repaint_screen ();
}

void reverse_selection_cmd (void)
{
    file_entry *file;
    int i;

    for (i = 0; i < current_panel->count; i++){
	file = &current_panel->dir.list [i];
	if (S_ISDIR (file->st.st_mode))
	    continue;
	do_file_mark (current_panel, i, !file->f.marked);
    }
}

static void
select_unselect_cmd (const char *title, const char *history_name, int cmd)
{
    char *reg_exp, *reg_exp_t;
    int i;
    int c;
    int dirflag = 0;

    reg_exp = input_dialog (title, "", history_name, easy_patterns ? "*" : ".");
    if (!reg_exp)
	return;
    if (!*reg_exp) {
	g_free (reg_exp);
	return;
    }

    reg_exp_t = reg_exp;

    /* Check if they specified a directory */
    if (*reg_exp_t == PATH_SEP) {
	dirflag = 1;
	reg_exp_t++;
    }
    if (reg_exp_t[strlen (reg_exp_t) - 1] == PATH_SEP) {
	dirflag = 1;
	reg_exp_t[strlen (reg_exp_t) - 1] = 0;
    }

    for (i = 0; i < current_panel->count; i++) {
	if (!strcmp (current_panel->dir.list[i].fname, ".."))
	    continue;
	if (S_ISDIR (current_panel->dir.list[i].st.st_mode)) {
	    if (!dirflag)
		continue;
	} else {
	    if (dirflag)
		continue;
	}
	c = regexp_match (reg_exp_t, current_panel->dir.list[i].fname,
			  match_file);
	if (c == -1) {
	    message (D_ERROR, MSG_ERROR, _("  Malformed regular expression  "));
	    g_free (reg_exp);
	    return;
	}
	if (c) {
	    do_file_mark (current_panel, i, cmd);
	}
    }
    g_free (reg_exp);
}

void select_cmd (void)
{
    select_unselect_cmd (_(" Select "), ":select_cmd: Select ", 1);
}

void unselect_cmd (void)
{
    select_unselect_cmd (_(" Unselect "), ":unselect_cmd: Unselect ", 0);
}

/* Check if the file exists */
/* If not copy the default */
static int
check_for_default(char *default_file, char *file)
{
    struct stat s;
    off_t  count = 0;
    double bytes = 0;
    FileOpContext *ctx;
    
    if (mc_stat (file, &s)){
	if (mc_stat (default_file, &s)){
	    return -1;
	}
	ctx = file_op_context_new (OP_COPY);
	file_op_context_create_ui (ctx, 0);
	copy_file_file (ctx, default_file, file, 1, &count, &bytes, 1);
	file_op_context_destroy (ctx);
    }
    return 0;
}

void ext_cmd (void)
{
    char *buffer;
    char *extdir;
    int  dir;

    dir = 0;
    if (geteuid () == 0){
	dir = query_dialog (_("Extension file edit"),
			    _(" Which extension file you want to edit? "), D_NORMAL, 2,
			    _("&User"), _("&System Wide"));
    }
    extdir = concat_dir_and_file (mc_home, MC_LIB_EXT);

    if (dir == 0){
	buffer = concat_dir_and_file (home_dir, MC_USER_EXT);
	check_for_default (extdir, buffer);
	do_edit (buffer);
	g_free (buffer);
    } else if (dir == 1)
	do_edit (extdir);

   g_free (extdir);
   flush_extension_file ();
}

/* where  = 0 - do edit file menu for mc */
/* where  = 1 - do edit file menu for mcedit */
static void
menu_edit_cmd (int where)
{
    char *buffer;
    char *menufile;
    int dir = 0;
    
    dir = query_dialog (
	_(" Menu edit "),
	_(" Which menu file do you want to edit? "), 
	D_NORMAL, geteuid() ? 2 : 3,
	_("&Local"), _("&User"), _("&System Wide")
    );

    menufile = concat_dir_and_file (mc_home, where ? CEDIT_GLOBAL_MENU : MC_GLOBAL_MENU);

    switch (dir) {
	case 0:
	    buffer = g_strdup (where ? CEDIT_LOCAL_MENU : MC_LOCAL_MENU);
	    check_for_default (menufile, buffer);
	    break;

	case 1:
	    buffer = concat_dir_and_file (home_dir, where ? CEDIT_HOME_MENU : MC_HOME_MENU);
	    check_for_default (menufile, buffer);
	    break;
	
	case 2:
	    buffer = concat_dir_and_file (mc_home, where ? CEDIT_GLOBAL_MENU : MC_GLOBAL_MENU);
	    break;

	default:
	   g_free (menufile);
	    return;
    }
    do_edit (buffer);
	if (dir == 0)
		chmod(buffer, 0600);
    g_free (buffer);
    g_free (menufile);
}

void quick_chdir_cmd (void)
{
    char *target;

    target = hotlist_cmd (LIST_HOTLIST);
    if (!target)
	return;

    if (get_current_type () == view_tree)
	tree_chdir (the_tree, target);
    else
        if (!do_cd (target, cd_exact))
	    message (D_ERROR, MSG_ERROR, _("Cannot change directory") );
    g_free (target);
}

/* edit file menu for mc */
void
edit_mc_menu_cmd (void)
{
    menu_edit_cmd (0);
}

#ifdef USE_INTERNAL_EDIT
/* edit file menu for mcedit */
void
edit_user_menu_cmd (void)
{
    menu_edit_cmd (1);
}

/* edit syntax file for mcedit */
void
edit_syntax_cmd (void)
{
    char *buffer;
    char *extdir;
    int dir = 0;

    if (geteuid () == 0) {
	dir =
	    query_dialog (_("Syntax file edit"),
			  _(" Which syntax file you want to edit? "), D_NORMAL, 2,
			  _("&User"), _("&System Wide"));
    }
    extdir = concat_dir_and_file (mc_home, "syntax" PATH_SEP_STR "Syntax");

    if (dir == 0) {
	buffer = concat_dir_and_file (home_dir, SYNTAX_FILE);
	check_for_default (extdir, buffer);
	do_edit (buffer);
	g_free (buffer);
    } else if (dir == 1)
	do_edit (extdir);

    g_free (extdir);
}
#endif

#ifdef USE_VFS
void reselect_vfs (void)
{
    char *target;

    target = hotlist_cmd (LIST_VFSLIST);
    if (!target)
	return;

    if (!do_cd (target, cd_exact))
        message (D_ERROR, MSG_ERROR, _("Cannot change directory") );
    g_free (target);
}
#endif /* USE_VFS */

static int compare_files (char *name1, char *name2, off_t size)
{
    int file1, file2;
    int result = -1;		/* Different by default */

    file1 = open (name1, O_RDONLY);
    if (file1 >= 0){
	file2 = open (name2, O_RDONLY);
	if (file2 >= 0){
#ifdef HAVE_MMAP
	    char *data1, *data2;
	    /* Ugly if jungle */
	    data1 = mmap (0, size, PROT_READ, MAP_FILE | MAP_PRIVATE, file1, 0);
	    if (data1 != (char*) -1){
		data2 = mmap (0, size, PROT_READ, MAP_FILE | MAP_PRIVATE, file2, 0);
		if (data2 != (char*) -1){
		    rotate_dash ();
		    result = memcmp (data1, data2, size);
		    munmap (data2, size);
		}
		munmap (data1, size);
	    }
#else
	    /* Don't have mmap() :( Even more ugly :) */
	    char buf1[BUFSIZ], buf2[BUFSIZ];
	    int n1, n2;
	    rotate_dash ();
	    do
	    {
		while((n1 = read(file1,buf1,BUFSIZ)) == -1 && errno == EINTR);
		while((n2 = read(file2,buf2,BUFSIZ)) == -1 && errno == EINTR);
	    } while (n1 == n2 && n1 == BUFSIZ && !memcmp(buf1,buf2,BUFSIZ));
	    result = (n1 != n2) || memcmp(buf1,buf2,n1);
#endif /* !HAVE_MMAP */
	    close (file2);
	}
	close (file1);
    }
    return result;
}

enum CompareMode {
    compare_quick, compare_size_only, compare_thourough
};

static void
compare_dir (WPanel *panel, WPanel *other, enum CompareMode mode)
{
    int i, j;
    char *src_name, *dst_name;

    /* No marks by default */
    panel->marked = 0;
    panel->total = 0;
    panel->dirs_marked = 0;
    
    /* Handle all files in the panel */
    for (i = 0; i < panel->count; i++){
	file_entry *source = &panel->dir.list[i];

	/* Default: unmarked */
	file_mark (panel, i, 0);

	/* Skip directories */
	if (S_ISDIR (source->st.st_mode))
	    continue;

	/* Search the corresponding entry from the other panel */
	for (j = 0; j < other->count; j++){
	    if (strcmp (source->fname,
			other->dir.list[j].fname) == 0)
		break;
	}
	if (j >= other->count)
	    /* Not found -> mark */
	    do_file_mark (panel, i, 1);
	else {
	    /* Found */
	    file_entry *target = &other->dir.list[j];

	    if (mode != compare_size_only){
		/* Older version is not marked */
		if (source->st.st_mtime < target->st.st_mtime)
		    continue;
	    }
	    
	    /* Newer version with different size is marked */
	    if (source->st.st_size != target->st.st_size){
		do_file_mark (panel, i, 1);
		continue;
		
	    }
	    if (mode == compare_size_only)
		continue;
	    
	    if (mode == compare_quick){
		/* Thorough compare off, compare only time stamps */
		/* Mark newer version, don't mark version with the same date */
		if (source->st.st_mtime > target->st.st_mtime){
		    do_file_mark (panel, i, 1);
		}
		continue;
	    }

	    /* Thorough compare on, do byte-by-byte comparison */
	    src_name = concat_dir_and_file (panel->cwd, source->fname);
	    dst_name = concat_dir_and_file (other->cwd, target->fname);
	    if (compare_files (src_name, dst_name, source->st.st_size))
		do_file_mark (panel, i, 1);
	    g_free (src_name);
	    g_free (dst_name);
	}
    } /* for (i ...) */
}

void
compare_dirs_cmd (void)
{
    int choice;
    enum CompareMode thorough_flag;

    choice =
	query_dialog (_(" Compare directories "),
		      _(" Select compare method: "), 0, D_NORMAL, 
		      _("&Quick"), _("&Size only"), _("&Thorough"), _("&Cancel"));

    if (choice < 0 || choice > 2)
	return;
    else
	thorough_flag = choice;

    if (get_current_type () == view_listing
	&& get_other_type () == view_listing) {
	compare_dir (current_panel, other_panel, thorough_flag);
	compare_dir (other_panel, current_panel, thorough_flag);
    } else {
	message (D_ERROR, MSG_ERROR,
		 _(" Both panels should be in the "
		   "listing mode to use this command "));
    }
}

void
history_cmd (void)
{
    Listbox *listbox;
    GList *current;

    if (cmdline->need_push) {
	if (push_history (cmdline, cmdline->buffer) == 2)
	    cmdline->need_push = 0;
    }
    if (!cmdline->history) {
	message (D_ERROR, MSG_ERROR, _(" The command history is empty "));
	return;
    }
    current = g_list_first (cmdline->history);
    listbox = create_listbox_window (60, 10, _(" Command history "),
				     "[Command Menu]");
    while (current) {
	LISTBOX_APPEND_TEXT (listbox, 0, (char *) current->data, current);
	current = g_list_next(current);
    }
    run_dlg (listbox->dlg);
    if (listbox->dlg->ret_value == B_CANCEL)
	current = NULL;
    else
	current = listbox->list->current->data;
    destroy_dlg (listbox->dlg);
    g_free (listbox);

    if (!current)
	return;
    cmdline->history = current;
    assign_text (cmdline, (char *) current->data);
    update_input (cmdline, 1);
}

void swap_cmd (void)
{
    swap_panels ();
    touchwin (stdscr);
    repaint_screen ();
}

void
view_other_cmd (void)
{
    static int message_flag = TRUE;

    if (!xterm_flag && !console_flag && !use_subshell && !output_starts_shell) {
	if (message_flag)
	    message (D_ERROR, MSG_ERROR,
		     _(" Not an xterm or Linux console; \n"
		       " the panels cannot be toggled. "));
	message_flag = FALSE;
    } else {
	toggle_panels ();
    }
}

static void
do_link (int symbolic_link, const char *fname)
{
    char *dest = NULL, *src = NULL;

    if (!symbolic_link) {
	src = g_strdup_printf (_("Link %s to:"), name_trunc (fname, 46));
	dest = input_expand_dialog (_(" Link "), src, MC_HISTORY_FM_LINK, "");
	if (!dest || !*dest)
	    goto cleanup;
	save_cwds_stat ();
	if (-1 == mc_link (fname, dest))
	    message (D_ERROR, MSG_ERROR, _(" link: %s "),
		     unix_error_string (errno));
    } else {
	char *s;
	char *d;

	/* suggest the full path for symlink */
	s = concat_dir_and_file (current_panel->cwd, fname);

	if (get_other_type () == view_listing) {
	    d = concat_dir_and_file (other_panel->cwd, fname);
	} else {
	    d = g_strdup (fname);
	}

	symlink_dialog (s, d, &dest, &src);
	g_free (d);
	g_free (s);

	if (!dest || !*dest || !src || !*src)
	    goto cleanup;
	save_cwds_stat ();
	if (-1 == mc_symlink (dest, src))
	    message (D_ERROR, MSG_ERROR, _(" symlink: %s "),
		     unix_error_string (errno));
    }
    update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    repaint_screen ();

cleanup:
    g_free (src);
    g_free (dest);
}

void link_cmd (void)
{
    do_link (0, selection (current_panel)->fname);
}

void symlink_cmd (void)
{
    char *filename = NULL;
    filename = selection (current_panel)->fname;

    if (filename) {
	do_link (1, filename);
    }
}

void edit_symlink_cmd (void)
{
    if (S_ISLNK (selection (current_panel)->st.st_mode)) {
	char buffer [MC_MAXPATHLEN];
	char *p = NULL;
	int i;
	char *dest, *q;

	p = selection (current_panel)->fname;

	q = g_strdup_printf (_(" Symlink `%s\' points to: "), str_trunc (p, 32));

	i = readlink (p, buffer, MC_MAXPATHLEN - 1);
	if (i > 0) {
	    buffer [i] = 0;
	    dest = input_expand_dialog (_(" Edit symlink "), q, MC_HISTORY_FM_EDIT_LINK, buffer);
	    if (dest) {
		if (*dest && strcmp (buffer, dest)) {
		    save_cwds_stat ();
		    if (-1 == mc_unlink (p)){
		        message (D_ERROR, MSG_ERROR, _(" edit symlink, unable to remove %s: %s "),
				 p, unix_error_string (errno));
		    } else {
			    if (-1 == mc_symlink (dest, p))
				    message (D_ERROR, MSG_ERROR, _(" edit symlink: %s "),
					     unix_error_string (errno));
		    }
		    update_panels (UP_OPTIMIZE, UP_KEEPSEL);
		    repaint_screen ();
		}
		g_free (dest);
	    }
	}
	g_free (q);
    } else {
	message (D_ERROR, MSG_ERROR, _("`%s' is not a symbolic link"),
		 selection (current_panel)->fname);
    }
}

void help_cmd (void)
{
   interactive_display (NULL, "[main]");
}

void
user_file_menu_cmd (void)
{
    user_menu_cmd (NULL);
}

/* partly taken from dcigettext.c, returns "" for default locale */
/* value should be freed by calling function g_free() */
char *guess_message_value (void)
{
    static const char * const var[] = {
	/* The highest priority value is the `LANGUAGE' environment
        variable.  This is a GNU extension.  */
	"LANGUAGE",
	/* Setting of LC_ALL overwrites all other.  */
	"LC_ALL",
	/* Next comes the name of the desired category.  */
	"LC_MESSAGES",
        /* Last possibility is the LANG environment variable.  */
	"LANG",
	/* NULL exit loops */
	NULL
    };

    unsigned i = 0;
    const char *locale = NULL;

    while (var[i] != NULL) {
	locale = getenv (var[i]);
	if (locale != NULL && locale[0] != '\0')
	    break;
	i++;
    }

    if (locale == NULL)
	locale = "";

    return g_strdup (locale);
}

/*
 * Return a random hint.  If force is not 0, ignore the timeout.
 */
char *
get_random_hint (int force)
{
    char *data, *result = NULL, *eol;
    int len;
    int start;
    static int last_sec;
    static struct timeval tv;
    str_conv_t conv;
    struct str_buffer *buffer;

    /* Do not change hints more often than one minute */
    gettimeofday (&tv, NULL);
    if (!force && !(tv.tv_sec > last_sec + 60))
	return g_strdup ("");
    last_sec = tv.tv_sec;

    data = load_mc_home_file (MC_HINT, NULL);
    if (!data)
	return 0;

    /* get a random entry */
    srand (tv.tv_sec);
    len = strlen (data);
    start = rand () % len;

    for (; start; start--) {
	if (data[start] == '\n') {
	    start++;
	    break;
	}
    }
    eol = strchr (&data[start], '\n');
    if (eol)
	*eol = 0;
    
    /* hint files are stored in utf-8 */
    /* try convert hint file from utf-8 to terminal encoding */
    conv = str_crt_conv_from ("UTF-8");
    if (conv != INVALID_CONV) {
        buffer = str_get_buffer ();
        if (str_convert (conv, &data[start], buffer) != ESTR_FAILURE) {
            result = g_strdup (buffer->data);
        }
        
        str_release_buffer (buffer);
        str_close_conv (conv);
    }
    
    g_free (data);
    return result;
}

#if defined(USE_NETCODE) || defined(USE_EXT2FSLIB)
static void
nice_cd (const char *text, const char *xtext, const char *help,
	 const char *history_name, const char *prefix, int to_home)
{
    char *machine;
    char *cd_path;

    if (!SELECTED_IS_PANEL)
	return;

    machine = input_dialog_help (text, xtext, help, history_name, "");
    if (!machine)
	return;

    to_home = 0;	/* FIXME: how to solve going to home nicely? /~/ is 
			   ugly as hell and leads to problems in vfs layer */

    if (strncmp (prefix, machine, strlen (prefix)) == 0)
	cd_path = g_strconcat (machine, to_home ? "/~/" : (char *) NULL, (char *) NULL);
    else 
	cd_path = g_strconcat (prefix, machine, to_home ? "/~/" : (char *) NULL, (char *) NULL);
    
    if (do_panel_cd (MENU_PANEL, cd_path, 0))
	directory_history_add (MENU_PANEL, (MENU_PANEL)->cwd);
    else
	message (D_ERROR, MSG_ERROR, _(" Cannot chdir to %s "), cd_path);
    g_free (cd_path);
    g_free (machine);
}
#endif /* USE_NETCODE || USE_EXT2FSLIB */


#ifdef USE_NETCODE

static const char *machine_str = N_(" Enter machine name (F1 for details): ");

#ifdef WITH_MCFS
void netlink_cmd (void)
{
    nice_cd (_(" Link to a remote machine "), _(machine_str),
	     "[Network File System]", ":netlink_cmd: Link to a remote ",
	     "/#mc:", 1);
}
#endif /* WITH_MCFS */

void ftplink_cmd (void)
{
    nice_cd (_(" FTP to machine "), _(machine_str),
	     "[FTP File System]", ":ftplink_cmd: FTP to machine ", "/#ftp:", 1);
}

void fishlink_cmd (void)
{
    nice_cd (_(" Shell link to machine "), _(machine_str),
	     "[FIle transfer over SHell filesystem]", ":fishlink_cmd: Shell link to machine ",
	     "/#sh:", 1);
}

#ifdef WITH_SMBFS
void smblink_cmd (void)
{
    nice_cd (_(" SMB link to machine "), _(machine_str),
	     "[SMB File System]", ":smblink_cmd: SMB link to machine ",
	     "/#smb:", 0);
}
#endif /* WITH_SMBFS */
#endif /* USE_NETCODE */

#ifdef USE_EXT2FSLIB
void undelete_cmd (void)
{
    nice_cd (_(" Undelete files on an ext2 file system "),
	     _(" Enter device (without /dev/) to undelete\n "
	       "  files on: (F1 for details)"),
	     "[Undelete File System]", ":undelete_cmd: Undel on ext2 fs ",
	     "/#undel:", 0);
}
#endif /* USE_EXT2FSLIB */

void quick_cd_cmd (void)
{
    char *p = cd_dialog ();

    if (p && *p) {
        char *q = g_strconcat ("cd ", p, (char *) NULL);
        
        do_cd_command (q);
        g_free (q);
    }
    g_free (p);
}

void
single_dirsize_cmd (void)
{
    WPanel *panel = current_panel;
    file_entry *entry;
    off_t marked;
    double total;

    entry = &(panel->dir.list[panel->selected]);
    if (S_ISDIR (entry->st.st_mode) && strcmp(entry->fname, "..") != 0) {
	total = 0.0;
	compute_dir_size (entry->fname, &marked, &total);
	entry->st.st_size = (off_t) total;
	entry->f.dir_size_computed = 1;
    }

    if (mark_moves_down)
	send_message (&(panel->widget), WIDGET_KEY, KEY_DOWN);

    recalculate_panel_summary (panel);
    panel->dirty = 1;
}

void 
dirsizes_cmd (void)
{
    WPanel *panel = current_panel;
    int i;
    off_t marked;
    double total;

    for (i = 0; i < panel->count; i++) 
	if (S_ISDIR (panel->dir.list [i].st.st_mode) &&
	         ((panel->dirs_marked && panel->dir.list [i].f.marked) || 
                   !panel->dirs_marked) &&
	         strcmp (panel->dir.list [i].fname, "..") != 0) {
	    total = 0.0l;
	    compute_dir_size (panel->dir.list [i].fname, &marked, &total);
	    panel->dir.list [i].st.st_size = (off_t) total;
	    panel->dir.list [i].f.dir_size_computed = 1;
	}
	
    recalculate_panel_summary (panel);
    panel_re_sort (panel);
    panel->dirty = 1;
}

void
save_setup_cmd (void)
{
    save_setup ();
    sync_profiles ();
    
    message (D_NORMAL, _(" Setup "), _(" Setup saved to ~/%s"), PROFILE_NAME);
}

static void
configure_panel_listing (WPanel *p, int view_type, int use_msformat, char *user, char *status)
{
    p->user_mini_status = use_msformat; 
    p->list_type = view_type;
    
    if (view_type == list_user || use_msformat){
	g_free (p->user_format);
	p->user_format = user;
    
	g_free (p->user_status_format [view_type]);
	p->user_status_format [view_type] = status;
    
	set_panel_formats (p);
    }
    else {
        g_free (user);
        g_free (status);
    }

    set_panel_formats (p);
    do_refresh ();
}

void
info_cmd_no_menu (void)
{
    if (get_display_type (0) == view_info)
	set_display_type (0, view_listing);
    else if (get_display_type (1) == view_info)
	set_display_type (1, view_listing);
    else
	set_display_type (current_panel == left_panel ? 1 : 0, view_info);
}

void
quick_cmd_no_menu (void)
{
    if (get_display_type (0) == view_quick)
	set_display_type (0, view_listing);
    else if (get_display_type (1) == view_quick)
	set_display_type (1, view_listing);
    else
	set_display_type (current_panel == left_panel ? 1 : 0, view_quick);
}

static void
switch_to_listing (int panel_index)
{
    if (get_display_type (panel_index) != view_listing)
	set_display_type (panel_index, view_listing);
}

void
listing_cmd (void)
{
    int   view_type, use_msformat;
    char  *user, *status;
    WPanel *p;
    int   display_type;

    display_type = get_display_type (MENU_PANEL_IDX);
    if (display_type == view_listing)
	p = MENU_PANEL_IDX == 0 ? left_panel : right_panel;
    else
	p = 0;

    view_type = display_box (p, &user, &status, &use_msformat, MENU_PANEL_IDX);

    if (view_type == -1)
	return;

    switch_to_listing (MENU_PANEL_IDX);

    p = MENU_PANEL_IDX == 0 ? left_panel : right_panel;

    configure_panel_listing (p, view_type, use_msformat, user, status);
}

void
tree_cmd (void)
{
    set_display_type (MENU_PANEL_IDX, view_tree);
}

void
info_cmd (void)
{
    set_display_type (MENU_PANEL_IDX, view_info);
}

void
quick_view_cmd (void)
{
    if ((WPanel *) get_panel_widget (MENU_PANEL_IDX) == current_panel)
	change_panel ();
    set_display_type (MENU_PANEL_IDX, view_quick);
}

/* Handle the tree internal listing modes switching */
static int
set_basic_panel_listing_to (int panel_index, int listing_mode)
{
    WPanel *p = (WPanel *) get_panel_widget (panel_index);

    switch_to_listing (panel_index);
    p->list_type = listing_mode;
    if (set_panel_formats (p))
	return 0;
	
    do_refresh ();
    return 1;
}

void
toggle_listing_cmd (void)
{
    int current = get_current_index ();
    WPanel *p = (WPanel *) get_panel_widget (current);

    set_basic_panel_listing_to (current, (p->list_type + 1) % LIST_TYPES);
}

/* add "#enc:encodning" to end of path */
/* if path end width a previous #enc:, only encoding is changed no additional 
 * #enc: is appended 
 * retun new string
 */
static char 
*add_encoding_to_path (const char *path, const char *encoding)
{
    char *result;
    char *semi;
    char *slash;
    
    semi = g_strrstr (path, "#enc:");
    
    if (semi != NULL) {
        slash = strchr (semi, PATH_SEP);
        if (slash != NULL) {
            result = g_strconcat (path, "/#enc:", encoding, NULL);
        } else {
            *semi = 0;
            result = g_strconcat (path, "/#enc:", encoding, NULL);
            *semi = '#';
        }
    } else {
        result = g_strconcat (path, "/#enc:", encoding, NULL);
    }
    
    return result;
}

static void
set_panel_encoding (WPanel *panel)
{
    char *encoding;
    char *cd_path;
            
    encoding = input_dialog ("Encoding", "Select encoding", NULL);
    
    if (encoding) {
        cd_path = add_encoding_to_path (panel->cwd, encoding);
        if (!do_panel_cd (MENU_PANEL, cd_path, 0))
            message (1, MSG_ERROR, _(" Cannot chdir to %s "), cd_path);
        g_free (cd_path);
    }
}

void
encoding_cmd (void)
{
    WPanel *panel;

    if (!SELECTED_IS_PANEL)
        return;

    panel = MENU_PANEL;
    set_panel_encoding (panel);
}
