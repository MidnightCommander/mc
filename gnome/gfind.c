/* Find file command for the Midnight Commander
   Copyright (C) The Free Software Foundation
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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <config.h>
#include <string.h>
#include <stdio.h>
#ifdef NEEDS_IO_H
#    include <io.h>
#endif
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <ctype.h>
#include "tty.h"
#include "x.h"
#include "global.h"
#include "find.h"

#include "dialog.h"		/* For message() */
#include "main.h"		/* do_cd, try_to_select */
#include "cmd.h"		/* view_file_at_line */
#include "../vfs/vfs.h"

/* A couple of extra messages we need */
enum {
	B_STOP = B_USER + 1,
	B_AGAIN,
	B_PANELIZE,
	B_TREE,
	B_VIEW
};

/* A list of directories to be ignores, separated with ':' */
char *find_ignore_dirs = 0;

/* This keeps track of the directory stack */
typedef struct dir_stack {
	char *name;
	struct dir_stack *prev;
} dir_stack;

typedef struct {
	GtkWidget *g_find_dlg;
	GtkWidget *g_status_label;
	GtkWidget *g_clist;
	GtkWidget *g_start_stop;
	GtkWidget *g_start_stop_label;
	GtkWidget *g_again;
	GtkWidget *g_change;
	GtkWidget *g_view;
	GtkWidget *g_edit;
	GtkWidget *g_panelize;
	int current_row;
	int idle_tag;
	int stop;
	char *find_pattern;	/* Pattern to search */
	char *content_pattern;	/* pattern to search inside files */
	int is_start;		/* Status of the start/stop toggle button */
	char *old_dir;
	dir_stack *dir_stack_base;
} GFindDlg;

static char *add_to_list(GFindDlg *head, char *text, void *closure);
static void stop_idle(GFindDlg *head);
static void status_update(GFindDlg *head, char *text);
static void get_list_info(GFindDlg *head, char **file, char **dir);

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

static int case_sensitive = 1;

static int find_parameters(char **start_dir, char **pattern, char **content)
{
	int return_value;
	GnomeDialog *find_dialog;
	GtkWidget *case_box;
	/* Gnome entries */
	GtkWidget *start_entry, *name_entry, *content_entry;
	/* Corresponding Gtk entries */
	GtkEntry *start_gentry, *name_gentry, *content_gentry;
	GtkWidget *start_label, *name_label, *content_label;
	static char *case_label = N_("Case sensitive");

	static char *in_contents = NULL;
	static char *in_start_dir = NULL;
	static char *in_start_name = NULL;

	static char *labs[] = { N_("Start at:"), N_("Filename:"), N_("Content: ") };

	if (!in_start_dir)
		in_start_dir = g_strdup(".");
	if (!in_start_name)
		in_start_name = g_strdup(easy_patterns ? "*" : ".");
	if (!in_contents)
		in_contents = g_strdup("");

	/* Create dialog */
	find_dialog = GNOME_DIALOG(gnome_dialog_new(_("Find File"),
				   GNOME_STOCK_BUTTON_OK,
				   GNOME_STOCK_BUTTON_CANCEL, NULL));
	gmc_window_setup_from_panel(find_dialog, cpanel);

	start_label = gtk_label_new(_(labs[0]));
	gtk_misc_set_alignment(GTK_MISC(start_label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(find_dialog->vbox), start_label, FALSE,
			   FALSE, 0);

	start_entry = gnome_entry_new("start");
	start_gentry = GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(start_entry)));
	gnome_entry_load_history(GNOME_ENTRY(start_entry));
	gtk_entry_set_text(start_gentry, in_start_dir);
	gtk_entry_set_position (start_gentry, 0);
	gtk_entry_select_region (start_gentry, 0,
				 start_gentry->text_length);
	gnome_dialog_editable_enters (find_dialog,
				      GTK_EDITABLE(start_gentry));
	gtk_box_pack_start(GTK_BOX(find_dialog->vbox), start_entry, FALSE,
			   FALSE, 0);

	name_label = gtk_label_new(_(labs[1]));
	gtk_misc_set_alignment(GTK_MISC(name_label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(find_dialog->vbox), name_label, FALSE,
			   FALSE, 0);

	name_entry = gnome_entry_new("name");
	name_gentry = GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(name_entry)));
	gnome_entry_load_history(GNOME_ENTRY(name_entry));
	gtk_entry_set_text(name_gentry, in_start_name);
	gtk_entry_set_position (name_gentry, 0);
	gtk_entry_select_region (name_gentry, 0,
				 name_gentry->text_length);
	gnome_dialog_editable_enters (find_dialog,
				      GTK_EDITABLE(name_gentry));
	gtk_box_pack_start(GTK_BOX(find_dialog->vbox), name_entry, FALSE,
			   FALSE, 0);

	content_label = gtk_label_new(_(labs[2]));
	gtk_misc_set_alignment(GTK_MISC(content_label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(find_dialog->vbox), content_label, FALSE,
			   FALSE, 0);

	content_entry = gnome_entry_new("content");
	content_gentry = GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(content_entry)));
	gnome_entry_load_history(GNOME_ENTRY(content_entry));
	gtk_entry_set_text(content_gentry, in_contents);
	gtk_entry_set_position (content_gentry, 0);
	gtk_entry_select_region (content_gentry, 0,
				 content_gentry->text_length);
	gnome_dialog_editable_enters (find_dialog,
				      GTK_EDITABLE(content_gentry));
	gtk_box_pack_start(GTK_BOX(find_dialog->vbox), content_entry, FALSE,
			   FALSE, 0);

	case_box = gtk_check_button_new_with_label(_(case_label));

	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(case_box), case_sensitive);
	gtk_box_pack_start(GTK_BOX(find_dialog->vbox), case_box, FALSE,
			   FALSE, 0);

	gtk_widget_grab_focus(gnome_entry_gtk_entry(GNOME_ENTRY(start_entry)));
	gnome_dialog_set_default(find_dialog, 0);

	gtk_widget_show_all(find_dialog->vbox);

	switch (gnome_dialog_run(find_dialog)) {
	case 0:
		return_value = 1;
		*start_dir = strdup(gtk_entry_get_text(start_gentry));
		*pattern = strdup(gtk_entry_get_text(name_gentry));
		*content = strdup(gtk_entry_get_text(content_gentry));

		g_free(in_start_dir);
		in_start_dir = g_strdup(*start_dir);

		g_free(in_start_name);
		in_start_name = g_strdup(*pattern);

		g_free(in_contents);
		if (*content[0]) {
			in_contents = g_strdup(*content);
		} else {
			*content = in_contents = NULL;
		}
		break;

	case 1:
		return_value = 0;
		break;

	default:
		return 0;
	}

	gtk_widget_destroy(GTK_WIDGET(find_dialog));

	return return_value;
}

static void push_directory(GFindDlg *head, char *dir)
{
	dir_stack *new;

	new = g_new(dir_stack, 1);
	new->name = g_strdup(dir);
	new->prev = head->dir_stack_base;
	head->dir_stack_base = new;
}

static char *pop_directory(GFindDlg *head)
{
	char *name;
	dir_stack *next;

	if (head->dir_stack_base) {
		name = head->dir_stack_base->name;
		next = head->dir_stack_base->prev;
		g_free(head->dir_stack_base);
		head->dir_stack_base = next;
		return name;
	} else
		return 0;
}

static void find_add_match(GFindDlg *head, char *dir, char *file)
{
	char *tmp_name;
	static char *dirname;
	int i;

	if (dir[0] == PATH_SEP && dir[1] == PATH_SEP)
		dir++;
	i = strlen(dir);
	if (i) {
		if (dir[i - 1] != PATH_SEP) {
			dir[i] = PATH_SEP;
			dir[i + 1] = 0;
		}
	}

	if (head->old_dir) {
		if (strcmp(head->old_dir, dir)) {
			g_free(head->old_dir);
			head->old_dir = g_strdup(dir);
			dirname = add_to_list(head, dir, NULL);
		}
	} else {
		head->old_dir = g_strdup(dir);
		dirname = add_to_list(head, dir, NULL);
	}

	tmp_name = g_strconcat("    ", file, NULL);
	add_to_list(head, tmp_name, dirname);
	g_free(tmp_name);
}

/* 
 * search_content:
 *
 * Search with egrep the global (FIXME) content_pattern string in the
 * DIRECTORY/FILE.  It will add the found entries to the find listbox.
 */
static void search_content(GFindDlg *head, char *directory, char *filename)
{
	struct stat s;
	char buffer[BUF_SMALL];
	char *fname, *p;
	int file_fd, pipe, ignoring;
	char c;
	int i;
	pid_t pid;
	char *egrep_path = "egrep";
	char *egrep_opts = case_sensitive ? "-n" : "-in";

	fname = concat_dir_and_file(directory, filename);

	if (mc_stat(fname, &s) != 0 || !S_ISREG(s.st_mode)) {
		g_free(fname);
		return;
	}

	file_fd = mc_open(fname, O_RDONLY);
	g_free(fname);

	if (file_fd == -1)
		return;

#ifndef GREP_STDIN
	pipe =
	    mc_doublepopen(file_fd, -1, &pid, egrep_path, egrep_path, egrep_opts,
			   head->content_pattern, NULL);
#else				/* GREP_STDIN */
	pipe =
	    mc_doublepopen(file_fd, -1, &pid, egrep_path, egrep_path, egrep_opts,
			   head->content_pattern, "-", NULL);
#endif				/* GREP STDIN */

	if (pipe == -1) {
		mc_close(file_fd);
		return;
	}

	g_snprintf(buffer, sizeof(buffer), _("Grepping in %s"), filename);

	status_update(head, buffer);
	mc_refresh();
	p = buffer;
	ignoring = 0;

	enable_interrupt_key();
	got_interrupt();

	while ((i = read(pipe, &c, 1)) == 1) {

		if (c == '\n') {
			p = buffer;
			ignoring = 0;
		}

		if (ignoring)
			continue;

		if (c == ':') {
			char *the_name;

			*p = 0;
			ignoring = 1;
			the_name = g_strconcat(buffer, ":", filename, NULL);
			find_add_match(head, directory, the_name);
			g_free(the_name);
		} else {
			if (p - buffer < (sizeof(buffer) - 1) && ISASCII(c) && isdigit(c))
				*p++ = c;
			else
				*p = 0;
		}
	}
	disable_interrupt_key();
	if (i == -1)
		message(1, _(" Find/read "), _(" Problem reading from child "));

	mc_doublepclose(pipe, pid);
	mc_close(file_fd);
}

static int do_search(GFindDlg *head)
{
	static struct dirent *dp = 0;
	static DIR *dirp = 0;
	static char directory[MC_MAXPATHLEN + 2];
	struct stat tmp_stat;
	static int subdirs_left = 0;
	char *tmp_name;		/* For bulding file names */

	if (!head) {	/* someone forces me to close dirp */
		if (dirp) {
			mc_closedir(dirp);
			dirp = 0;
		}
		dp = 0;
		return 1;
	}
	while (!dp) {

		if (dirp) {
			mc_closedir(dirp);
			dirp = 0;
		}

		while (!dirp) {
			char *tmp;
			char buffer[BUF_SMALL];

			while (1) {
				tmp = pop_directory(head);
				if (!tmp) {
					status_update(head, _("Finished"));
					stop_idle(head);
					return 0;
				}
				if (find_ignore_dirs) {
					int found;
					char *temp_dir = g_strconcat(":", tmp, ":", NULL);

					found = strstr(find_ignore_dirs, temp_dir) != 0;
					g_free(temp_dir);
					if (found)
						g_free(tmp);
					else
						break;
				} else
					break;
			}

			strcpy(directory, tmp);
			g_free(tmp);

			g_snprintf(buffer, sizeof(buffer), _("Searching %s"),
				   directory);
			status_update(head, buffer);
			/* mc_stat should not be called after mc_opendir
			   because vfs_s_opendir modifies the st_nlink
			 */
			mc_stat(directory, &tmp_stat);
			subdirs_left = tmp_stat.st_nlink - 2;
			/* Commented out as unnecessary
			   if (subdirs_left < 0)
			   subdirs_left = MAXINT;
			 */
			dirp = mc_opendir(directory);
		}
		dp = mc_readdir(dirp);
	}

	if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
		dp = mc_readdir(dirp);
		return 1;
	}

	tmp_name = concat_dir_and_file(directory, dp->d_name);

	if (subdirs_left) {
		mc_lstat(tmp_name, &tmp_stat);
		if (S_ISDIR(tmp_stat.st_mode)) {
			push_directory(head, tmp_name);
			subdirs_left--;
		}
	}

	if (regexp_match(head->find_pattern, dp->d_name, match_file)) {
		if (head->content_pattern)
			search_content(head, directory, dp->d_name);
		else
			find_add_match(head, directory, dp->d_name);
	}

	g_free(tmp_name);
	dp = mc_readdir(dirp);

	/* Displays the nice dot */
	x_flush_events();
	return 1;
}

static void init_find_vars(GFindDlg *head)
{
	char *dir;

	if (head->old_dir) {
		g_free(head->old_dir);
		head->old_dir = 0;
	}

	/* Remove all the items in the stack */
	while ((dir = pop_directory(head)) != NULL)
		g_free(dir);
}

static void
find_do_view_edit(GFindDlg *head, int unparsed_view, int edit, char *dir, char *file)
{
	char *fullname, *filename;
	int line;

	if (head->content_pattern) {
		filename = strchr(file + 4, ':') + 1;
		line = atoi(file + 4);
	} else {
		filename = file + 4;
		line = 0;
	}
	if (dir[0] == '.' && dir[1] == 0)
		fullname = g_strdup(filename);
	else if (dir[0] == '.' && dir[1] == PATH_SEP)
		fullname = concat_dir_and_file(dir + 2, filename);
	else
		fullname = concat_dir_and_file(dir, filename);

	if (edit)
		do_edit_at_line(fullname, line);
	else
		view_file_at_line(fullname, unparsed_view, use_internal_view, line);
	g_free(fullname);
}

static void
select_row(GtkCList *clist, gint row, gint column, GdkEvent *event, GFindDlg *head)
{
	gtk_widget_set_sensitive(head->g_edit, TRUE);
	gtk_widget_set_sensitive(head->g_view, TRUE);
	head->current_row = row;
}

static void find_do_chdir(GtkWidget *widget, GFindDlg *head)
{
	head->stop = B_ENTER;
	gtk_main_quit();
}

static void find_do_again(GtkWidget *widget, GFindDlg *head)
{
	head->stop = B_AGAIN;
	gtk_main_quit();
}

static void find_do_panelize(GtkWidget *widget, GFindDlg *head)
{
	head->stop = B_PANELIZE;
	gtk_main_quit();
}


static void find_start_stop(GtkWidget *widget, GFindDlg *head)
{

	if (head->is_start) {
		head->idle_tag = gtk_idle_add((GtkFunction) do_search, head);
	} else {
		gtk_idle_remove(head->idle_tag);
		head->idle_tag = 0;
	}

	gtk_label_set_text(GTK_LABEL(head->g_start_stop_label),
			   head->is_start ? _("Suspend") : _("Restart"));
	head->is_start = !head->is_start;
	status_update(head, head->is_start ? _("Stopped") : _("Searching"));
}


static void find_do_view(GtkWidget *widget, GFindDlg *head)
{
	char *file, *dir;

	get_list_info(head, &file, &dir);

	find_do_view_edit(head, 0, 0, dir, file);
}

static void find_do_edit(GtkWidget *widget, GFindDlg *head)
{
	char *file, *dir;

	get_list_info(head, &file, &dir);

	find_do_view_edit(head, 0, 1, dir, file);
}

static void find_destroy(GtkWidget *widget, GFindDlg *head)
{
	if (head->idle_tag) {
		gtk_idle_remove(head->idle_tag);
		head->idle_tag = 0;
	}
}

static void setup_gui(GFindDlg *head)
{
	GtkWidget *sw;
	GtkWidget *box1, *box2, *box3;

	head->g_find_dlg = gnome_dialog_new(_("Find file"),
						GNOME_STOCK_BUTTON_OK, NULL);

	/* The buttons */
	head->g_change = gtk_button_new_with_label(_("Change to this directory"));
	head->g_again = gtk_button_new_with_label(_("Search again"));
	head->g_start_stop_label = gtk_label_new(_("Suspend"));
	head->g_start_stop = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(head->g_start_stop),
			  head->g_start_stop_label);

	head->g_view = gtk_button_new_with_label(_("View this file"));
	head->g_edit = gtk_button_new_with_label(_("Edit this file"));
	head->g_panelize = gtk_button_new_with_label(_("Send the results to a Panel"));

	box1 = gtk_hbox_new(TRUE, GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(box1), head->g_change, 0, 1, 0);
	gtk_box_pack_start(GTK_BOX(box1), head->g_again, 0, 1, 0);
	gtk_box_pack_start(GTK_BOX(box1), head->g_start_stop, 0, 1, 0);

/*	RECOONECT	_("Panelize contents"), */
/*		_("View"),
		_("Edit"), */

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER,
				       GTK_POLICY_AUTOMATIC);
	head->g_clist = gtk_clist_new(1);
	gtk_clist_set_selection_mode(GTK_CLIST(head->g_clist), GTK_SELECTION_SINGLE);
	gtk_widget_set_usize(head->g_clist, -1, 200);
	gtk_container_add(GTK_CONTAINER(sw), head->g_clist);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(head->g_find_dlg)->vbox),
			   sw, TRUE, TRUE, GNOME_PAD_SMALL);

	head->current_row = -1;
	head->stop = B_EXIT;
	gtk_signal_connect(GTK_OBJECT(head->g_clist), "select_row",
			   GTK_SIGNAL_FUNC(select_row), head);

	gtk_signal_connect (GTK_OBJECT(head->g_find_dlg), "destroy",
			   GTK_SIGNAL_FUNC(find_destroy), head);

	/*
	 * Connect the buttons
	 */
	gtk_signal_connect(GTK_OBJECT(head->g_change), "clicked",
			   GTK_SIGNAL_FUNC(find_do_chdir), head);
	gtk_signal_connect(GTK_OBJECT(head->g_again), "clicked",
			   GTK_SIGNAL_FUNC(find_do_again), head);
	gtk_signal_connect(GTK_OBJECT(head->g_start_stop), "clicked",
			   GTK_SIGNAL_FUNC(find_start_stop), head);
	gtk_signal_connect(GTK_OBJECT(head->g_panelize), "clicked",
			   GTK_SIGNAL_FUNC(find_do_panelize), head);

	/*
	 * View/edit buttons
	 */
	gtk_signal_connect(GTK_OBJECT(head->g_view), "clicked",
			   GTK_SIGNAL_FUNC(find_do_view), head);
	gtk_signal_connect(GTK_OBJECT(head->g_edit), "clicked",
			   GTK_SIGNAL_FUNC(find_do_edit), head);

	gtk_widget_set_sensitive(head->g_view, FALSE);
	gtk_widget_set_sensitive(head->g_edit, FALSE);
	box2 = gtk_hbox_new(1, GNOME_PAD + GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(box2), head->g_view, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(box2), head->g_edit, 0, 0, 0);

	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(head->g_find_dlg)->vbox),
			   box1, TRUE, TRUE, GNOME_PAD_SMALL);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(head->g_find_dlg)->vbox),
			   box2, TRUE, TRUE, GNOME_PAD_SMALL);

	head->g_status_label = gtk_label_new(_("Searching"));
	gtk_misc_set_alignment(GTK_MISC(head->g_status_label), 0.0, 0.5);
	box3 = gtk_hbox_new(1, GNOME_PAD);
	gtk_container_add(GTK_CONTAINER(box3), head->g_status_label);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(head->g_find_dlg)->vbox),
			   box3, TRUE, TRUE, GNOME_PAD_SMALL);

	gtk_widget_show_all(head->g_find_dlg);
	gtk_widget_hide(GTK_WIDGET(head->g_view));
	gtk_widget_hide(GTK_WIDGET(head->g_edit));
}

static int run_process(GFindDlg *head)
{
	head->idle_tag = gtk_idle_add((GtkFunction) do_search, head);

	switch (gnome_dialog_run(GNOME_DIALOG(head->g_find_dlg))) {
	case 0:
		/* Ok button */
		head->stop = B_CANCEL;
		break;
	}
	head->g_start_stop = NULL;

	/* B_EXIT means that user closed the dialog */
	if (head->stop == B_EXIT) {
		head->g_find_dlg = NULL;
	}

	return head->stop;
}

static void kill_gui(GFindDlg *head)
{
	if (head->g_find_dlg) {
		gtk_object_destroy(GTK_OBJECT(head->g_find_dlg));
	}
}

static void stop_idle(GFindDlg *head)
{
	if (head->g_start_stop) {
		gtk_widget_set_sensitive(GTK_WIDGET(head->g_start_stop), FALSE);
	}
}

static void status_update(GFindDlg *head, char *text)
{
	gtk_label_set_text(GTK_LABEL(head->g_status_label), text);
	x_flush_events();
}

static char *add_to_list(GFindDlg *head, char *text, void *data)
{
	int row;
	char *texts[1];

	texts[0] = text;

	row = gtk_clist_append(GTK_CLIST(head->g_clist), texts);
	gtk_clist_set_row_data(GTK_CLIST(head->g_clist), row, data);
#if 1
	if (gtk_clist_row_is_visible(GTK_CLIST(head->g_clist), row)
			!= GTK_VISIBILITY_FULL) {
		gtk_clist_moveto(GTK_CLIST(head->g_clist), row, 0, 0.5, 0.0);
	}
#endif
	return text;
}

static void get_list_info(GFindDlg *head, char **file, char **dir)
{
	if (head->current_row == -1)
		*file = *dir = NULL;
	gtk_clist_get_text(GTK_CLIST(head->g_clist),
			   head->current_row, 0, file);
	*dir = gtk_clist_get_row_data(GTK_CLIST(head->g_clist),
				      head->current_row);
}

static int find_file(char *start_dir, char *pattern, char *content, char **dirname,
		     char **filename)
{
	GFindDlg *head;
	int return_value = 0;
	char *dir;
	char *dir_tmp, *file_tmp;

	head = g_new0 (GFindDlg, 1);
	setup_gui(head);

	/* FIXME: Need to cleanup this, this ought to be passed non-globaly */
	head->find_pattern = pattern;
	head->content_pattern = content;

	init_find_vars(head);
	push_directory(head, start_dir);

	return_value = run_process(head);

	/* Remove all the items in the stack */
	while ((dir = pop_directory(head)) != NULL)
		g_free(dir);

	if (return_value == B_ENTER || return_value == B_PANELIZE) {
		get_list_info(head, &file_tmp, &dir_tmp);

		if (dir_tmp)
			*dirname = g_strdup(dir_tmp);
		if (file_tmp)
			*filename = g_strdup(file_tmp);
	}

	kill_gui(head);
	do_search(0);		/* force do_search to release resources */
	if (head->old_dir) {
		g_free(head->old_dir);
		head->old_dir = 0;
	}
	g_free (head);

	return return_value;
}

void do_find(void)
{
	char *start_dir, *pattern, *content;
	char *filename, *dirname;
	int v, dir_and_file_set;

	while (1) {
		if (!find_parameters(&start_dir, &pattern, &content))
			break;

		dirname = filename = NULL;
		v = find_file(start_dir, pattern, content, &dirname, &filename);
		g_free(start_dir);
		g_free(pattern);

		if (v == B_ENTER) {
			if (dirname || filename) {
				if (dirname) {
					do_cd(dirname, cd_exact);
					if (filename)
						try_to_select(cpanel,
							      filename +
							      (content
							       ? (strchr
								  (filename + 4,
								   ':') - filename +
								  1) : 4));
				} else if (filename)
					do_cd(filename, cd_exact);
				paint_panel(cpanel);
				select_item(cpanel);
			}
			if (dirname)
				g_free(dirname);
			if (filename)
				g_free(filename);
			break;
		}
		if (content)
			g_free(content);
		dir_and_file_set = dirname && filename;
		if (dirname)
			g_free(dirname);
		if (filename)
			g_free(filename);
		if (v == B_CANCEL || v == B_EXIT)
			break;

		if (v == B_PANELIZE) {
			if (dir_and_file_set) {
				try_to_select(cpanel, NULL);
				paint_panel(cpanel);
			}
			break;
		}
	}
}
