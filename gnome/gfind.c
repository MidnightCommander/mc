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

#ifndef PORT_HAS_FLUSH_EVENTS
#    define x_flush_events()
#endif

#define FIND2_X_USE 35
#define verbose 1

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

static int running = 0;		/* nice flag */
static char *find_pattern;	/* Pattern to search */
static char *content_pattern;	/* pattern to search inside files */
static int count;		/* Number of files displayed */
static int matches;		/* Number of matches */
static int is_start;		/* Status of the start/stop toggle button */
static char *old_dir;

static GtkWidget *g_find_dlg;
static GtkWidget *g_status_label;
static GtkWidget *g_clist;
static GtkWidget *g_start_stop;
static GtkWidget *g_start_stop_label;
static GtkWidget *g_view, *g_edit;
static GtkWidget *g_panelize;
static int current_row;
static int idle_tag;
static int stop;

/* This keeps track of the directory stack */
typedef struct dir_stack {
	char *name;
	struct dir_stack *prev;
} dir_stack;

static dir_stack *dir_stack_base = 0;

static char *add_to_list(char *text, void *closure);
static void stop_idle(void *data);
static void status_update(char *text);
static void get_list_info(char **file, char **dir);

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
	GtkWidget *find_dialog;
	GtkWidget *case_box;
	GtkWidget *start_entry, *name_entry, *content_entry;
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
	find_dialog =
	    gnome_dialog_new(_("Find File"), GNOME_STOCK_BUTTON_OK,
			     GNOME_STOCK_BUTTON_CANCEL, NULL);
	gmc_window_setup_from_panel(GNOME_DIALOG(find_dialog), cpanel);

	start_label = gtk_label_new(_(labs[0]));
	gtk_misc_set_alignment(GTK_MISC(start_label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(find_dialog)->vbox), start_label, FALSE,
			   FALSE, 0);

	start_entry = gnome_entry_new("start");
	gnome_entry_load_history(GNOME_ENTRY(start_entry));
	gtk_entry_set_text(GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(start_entry))),
			   in_start_dir);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(find_dialog)->vbox), start_entry, FALSE,
			   FALSE, 0);

	name_label = gtk_label_new(_(labs[1]));
	gtk_misc_set_alignment(GTK_MISC(name_label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(find_dialog)->vbox), name_label, FALSE,
			   FALSE, 0);

	name_entry = gnome_entry_new("name");
	gnome_entry_load_history(GNOME_ENTRY(name_entry));
	gtk_entry_set_text(GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(name_entry))),
			   in_start_name);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(find_dialog)->vbox), name_entry, FALSE,
			   FALSE, 0);

	content_label = gtk_label_new(_(labs[2]));
	gtk_misc_set_alignment(GTK_MISC(content_label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(find_dialog)->vbox), content_label, FALSE,
			   FALSE, 0);

	content_entry = gnome_entry_new("content");
	gnome_entry_load_history(GNOME_ENTRY(content_entry));
	gtk_entry_set_text(GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(content_entry))),
			   in_contents);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(find_dialog)->vbox), content_entry, FALSE,
			   FALSE, 0);

	case_box = gtk_check_button_new_with_label(_(case_label));

	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(case_box), case_sensitive);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(find_dialog)->vbox), case_box, FALSE,
			   FALSE, 0);

	gtk_widget_grab_focus(gnome_entry_gtk_entry(GNOME_ENTRY(start_entry)));
	gnome_dialog_set_default(GNOME_DIALOG(find_dialog), 0);

	gtk_widget_show_all(GNOME_DIALOG(find_dialog)->vbox);

	switch (gnome_dialog_run(GNOME_DIALOG(find_dialog))) {
	case 0:
		return_value = 1;
		*start_dir =
		    strdup(gtk_entry_get_text
			   (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(start_entry)))));
		*pattern =
		    strdup(gtk_entry_get_text
			   (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(name_entry)))));
		*content =
		    strdup(gtk_entry_get_text
			   (GTK_ENTRY
			    (gnome_entry_gtk_entry(GNOME_ENTRY(content_entry)))));

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

	gtk_widget_destroy(find_dialog);

	return return_value;
}

static void push_directory(char *dir)
{
	dir_stack *new;

	new = g_new(dir_stack, 1);
	new->name = g_strdup(dir);
	new->prev = dir_stack_base;
	dir_stack_base = new;
}

static char *pop_directory(void)
{
	char *name;
	dir_stack *next;

	if (dir_stack_base) {
		name = dir_stack_base->name;
		next = dir_stack_base->prev;
		g_free(dir_stack_base);
		dir_stack_base = next;
		return name;
	} else
		return 0;
}

static void insert_file(char *dir, char *file)
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

	if (old_dir) {
		if (strcmp(old_dir, dir)) {
			g_free(old_dir);
			old_dir = g_strdup(dir);
			dirname = add_to_list(dir, NULL);
		}
	} else {
		old_dir = g_strdup(dir);
		dirname = add_to_list(dir, NULL);
	}

	tmp_name = g_strconcat("    ", file, NULL);
	add_to_list(tmp_name, dirname);
	g_free(tmp_name);
}

static void find_add_match(Dlg_head * h, char *dir, char *file)
{
	insert_file(dir, file);
}

/* 
 * search_content:
 *
 * Search with egrep the global (FIXME) content_pattern string in the
 * DIRECTORY/FILE.  It will add the found entries to the find listbox.
 */
static void search_content(Dlg_head * h, char *directory, char *filename)
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
			   content_pattern, NULL);
#else				/* GREP_STDIN */
	pipe =
	    mc_doublepopen(file_fd, -1, &pid, egrep_path, egrep_path, egrep_opts,
			   content_pattern, "-", NULL);
#endif				/* GREP STDIN */

	if (pipe == -1) {
		mc_close(file_fd);
		return;
	}

	g_snprintf(buffer, sizeof(buffer), _("Grepping in %s"),
		   name_trunc(filename, FIND2_X_USE));

	status_update(buffer);
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
			find_add_match(h, directory, the_name);
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

static int do_search(struct Dlg_head *h)
{
	static struct dirent *dp = 0;
	static DIR *dirp = 0;
	static char directory[MC_MAXPATHLEN + 2];
	struct stat tmp_stat;
	static int subdirs_left = 0;
	char *tmp_name;		/* For bulding file names */

	if (!h) {		/* someone forces me to close dirp */
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

			while (1) {
				tmp = pop_directory();
				if (!tmp) {
					running = 0;
					status_update(_("Finished"));
					stop_idle(h);
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

			if (verbose) {
				char buffer[BUF_SMALL];

				g_snprintf(buffer, sizeof(buffer), _("Searching %s"),
					   name_trunc(directory, FIND2_X_USE));
				status_update(buffer);
			}
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
			push_directory(tmp_name);
			subdirs_left--;
		}
	}

	if (regexp_match(find_pattern, dp->d_name, match_file)) {
		if (content_pattern)
			search_content(h, directory, dp->d_name);
		else
			find_add_match(h, directory, dp->d_name);
	}

	g_free(tmp_name);
	dp = mc_readdir(dirp);

	/* Displays the nice dot */
	count++;
	x_flush_events();
	return 1;
}

static void init_find_vars(void)
{
	char *dir;

	if (old_dir) {
		g_free(old_dir);
		old_dir = 0;
	}
	count = 0;
	matches = 0;

	/* Remove all the items in the stack */
	while ((dir = pop_directory()) != NULL)
		g_free(dir);
}

static void find_do_view_edit(int unparsed_view, int edit, char *dir, char *file)
{
	char *fullname, *filename;
	int line;

	if (content_pattern) {
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

static void select_row(GtkCList * clist, gint row, gint column, GdkEvent * event)
{
	gtk_widget_set_sensitive(g_edit, TRUE);
	gtk_widget_set_sensitive(g_view, TRUE);
	current_row = row;
}

static void find_do_chdir(void)
{
	gtk_idle_remove(idle_tag);
	idle_tag = 0;
	stop = B_ENTER;
	gtk_main_quit();
}

static void find_do_again(void)
{
	gtk_idle_remove(idle_tag);
	idle_tag = 0;
	stop = B_AGAIN;
	gtk_main_quit();
}

static void find_do_panelize(void)
{
	gtk_idle_remove(idle_tag);
	idle_tag = 0;
	stop = B_PANELIZE;
	gtk_main_quit();
}


static void find_start_stop(void)
{

	if (is_start) {
		idle_tag = gtk_idle_add((GtkFunction) do_search, g_find_dlg);
	} else {
		gtk_idle_remove(idle_tag);
		idle_tag = 0;
	}

	gtk_label_set_text(GTK_LABEL(g_start_stop_label),
			   is_start ? _("Suspend") : _("Restart"));
	is_start = !is_start;
	status_update(is_start ? _("Stopped") : _("Searching"));
}


static void find_do_view(void)
{
	char *file, *dir;

	get_list_info(&file, &dir);

	find_do_view_edit(0, 0, dir, file);
}

static void find_do_edit(void)
{
	char *file, *dir;

	get_list_info(&file, &dir);

	find_do_view_edit(0, 1, dir, file);
}

static void setup_gui(void)
{
	GtkWidget *sw, *b1, *b2;
	GtkWidget *box, *box2;

	g_find_dlg = gnome_dialog_new(_("Find file"), GNOME_STOCK_BUTTON_OK, NULL);

	/* The buttons */
	b1 = gtk_button_new_with_label(_("Change to this directory"));
	b2 = gtk_button_new_with_label(_("Search again"));
	g_start_stop_label = gtk_label_new(_("Suspend"));
	g_start_stop = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(g_start_stop), g_start_stop_label);

	g_view = gtk_button_new_with_label(_("View this file"));
	g_edit = gtk_button_new_with_label(_("Edit this file"));
	g_panelize = gtk_button_new_with_label(_("Send the results to a Panel"));

	box = gtk_hbox_new(TRUE, GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(box), b1, 0, 1, 0);
	gtk_box_pack_start(GTK_BOX(box), b2, 0, 1, 0);
	gtk_box_pack_start(GTK_BOX(box), g_start_stop, 0, 1, 0);

/*	RECOONECT	_("Panelize contents"), */
/*		_("View"),
		_("Edit"), */

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER,
				       GTK_POLICY_AUTOMATIC);
	g_clist = gtk_clist_new(1);
	gtk_clist_set_selection_mode(GTK_CLIST(g_clist), GTK_SELECTION_SINGLE);
	gtk_widget_set_usize(g_clist, -1, 200);
	gtk_container_add(GTK_CONTAINER(sw), g_clist);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(g_find_dlg)->vbox), sw, TRUE, TRUE,
			   GNOME_PAD_SMALL);

	current_row = -1;
	stop = B_EXIT;
	gtk_signal_connect(GTK_OBJECT(g_clist), "select_row", GTK_SIGNAL_FUNC(select_row),
			   NULL);

	/*
	 * Connect the buttons
	 */
	gtk_signal_connect(GTK_OBJECT(b1), "clicked", GTK_SIGNAL_FUNC(find_do_chdir),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(b2), "clicked", GTK_SIGNAL_FUNC(find_do_again),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(g_start_stop), "clicked",
			   GTK_SIGNAL_FUNC(find_start_stop), NULL);
	gtk_signal_connect(GTK_OBJECT(g_panelize), "clicked",
			   GTK_SIGNAL_FUNC(find_do_panelize), NULL);

	/*
	 * View/edit buttons
	 */
	gtk_signal_connect(GTK_OBJECT(g_view), "clicked", GTK_SIGNAL_FUNC(find_do_view),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(g_edit), "clicked", GTK_SIGNAL_FUNC(find_do_edit),
			   NULL);

	gtk_widget_set_sensitive(g_view, FALSE);
	gtk_widget_set_sensitive(g_edit, FALSE);
	box2 = gtk_hbox_new(1, GNOME_PAD + GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(box2), g_view, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(box2), g_edit, 0, 0, 0);

	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(g_find_dlg)->vbox), box, TRUE, TRUE,
			   GNOME_PAD_SMALL);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(g_find_dlg)->vbox), box2, TRUE, TRUE,
			   GNOME_PAD_SMALL);

	g_status_label = gtk_label_new(_("Searching"));
	gtk_misc_set_alignment(GTK_MISC(g_status_label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(g_find_dlg)->vbox), g_status_label, TRUE,
			   TRUE, GNOME_PAD_SMALL);

	gtk_widget_show_all(g_find_dlg);
	gtk_widget_hide(GTK_WIDGET(g_view));
	gtk_widget_hide(GTK_WIDGET(g_edit));
}

static int run_process(void)
{
	idle_tag = gtk_idle_add((GtkFunction) do_search, g_find_dlg);

	switch (gnome_dialog_run(GNOME_DIALOG(g_find_dlg))) {
	case 0:
		/* Ok button */
		stop = B_CANCEL;
		break;
	}
	g_start_stop = NULL;

	/* B_EXIT means that user closed the dialog */
	if (stop == B_EXIT) {
		g_find_dlg = NULL;
	}

	return stop;
}

static void kill_gui(void)
{
	if (g_find_dlg) {
		gtk_object_destroy(GTK_OBJECT(g_find_dlg));
	}
}

static void stop_idle(void *data)
{
	if (g_start_stop)
		gtk_widget_set_sensitive(GTK_WIDGET(g_start_stop), FALSE);
}

static void status_update(char *text)
{
	gtk_label_set_text(GTK_LABEL(g_status_label), text);
	x_flush_events();
}

static char *add_to_list(char *text, void *data)
{
	int row;
	char *texts[1];

	texts[0] = text;

	row = gtk_clist_append(GTK_CLIST(g_clist), texts);
	gtk_clist_set_row_data(GTK_CLIST(g_clist), row, data);
#if 1
	if (gtk_clist_row_is_visible(GTK_CLIST(g_clist), row) != GTK_VISIBILITY_FULL)
		gtk_clist_moveto(GTK_CLIST(g_clist), row, 0, 0.5, 0.0);
#endif
	return text;
}

static void get_list_info(char **file, char **dir)
{
	if (current_row == -1)
		*file = *dir = NULL;
	gtk_clist_get_text(GTK_CLIST(g_clist), current_row, 0, file);
	*dir = gtk_clist_get_row_data(GTK_CLIST(g_clist), current_row);
}

static int find_file(char *start_dir, char *pattern, char *content, char **dirname,
		     char **filename)
{
	int return_value = 0;
	char *dir;
	char *dir_tmp, *file_tmp;

	setup_gui();

	/* FIXME: Need to cleanup this, this ought to be passed non-globaly */
	find_pattern = pattern;
	content_pattern = content;

	init_find_vars();
	push_directory(start_dir);

	return_value = run_process();

	/* Remove all the items in the stack */
	while ((dir = pop_directory()) != NULL)
		g_free(dir);

	if (return_value == B_ENTER || return_value == B_PANELIZE) {
		get_list_info(&file_tmp, &dir_tmp);

		if (dir_tmp)
			*dirname = g_strdup(dir_tmp);
		if (file_tmp)
			*filename = g_strdup(file_tmp);
	}

	kill_gui();
	do_search(0);		/* force do_search to release resources */
	if (old_dir) {
		g_free(old_dir);
		old_dir = 0;
	}
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
		is_start = 0;
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
