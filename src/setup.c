/* Setup loading/saving.
   Copyright (C) 1994 Miguel de Icaza

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <config.h>
#include <sys/types.h>		/* Needed to include local .h files */
#include <sys/stat.h>
#include <sys/param.h>
#include <string.h>
#include "tty.h"
#include <stdio.h>
#include "global.h"
#include "dir.h"
#include "panel.h"
#include "main.h"
#include "tree.h"
#include "profile.h"
#define WANT_WIDGETS
#include "setup.h"
#include "mouse.h"		/* To make view.h happy */
#include "view.h"		/* For the externs */
#include "key.h"		/* For the externs */
#include "hotlist.h"		/* load/save/done hotlist */
#include "panelize.h"		/* load/save/done panelize */
#include "layout.h"
#include "menu.h"		/* menubar_visible declaration */
#include "win.h"		/* lookup_key */
#include "cmd.h"
#include "file.h"
#include "x.h"

#ifdef HAVE_GNOME
#    include "gdesktop.h"
#endif

#ifndef PORT_LIST_MODE_NAME
#     define PORT_LIST_MODE_NAME "list_mode"
#endif

#ifndef PORT_LIST_MODE_DEFAULT
#     define PORT_LIST_MODE_DEFAULT "full"
#endif

#include "../vfs/vfs.h"
#ifdef USE_NETCODE
#   include "../vfs/ftpfs.h"
extern int use_netrc;
extern int ftpfs_retry_seconds;
extern int ftpfs_use_passive_connections;
extern int ftpfs_use_unix_list_options;
extern int ftpfs_first_cd_then_ls;
#endif

#ifdef HAVE_GNOME
extern int tree_panel_visible;
extern int we_can_afford_the_speed;
int use_magic = 0;
#endif

/* "$Id$" */

#ifdef USE_VFS
extern int vfs_timeout;
extern int tar_gzipped_memlimit;
#endif

extern char *find_ignore_dirs;

extern int num_history_items_recorded;

char *profile_name;

char setup_color_string [4096];
char term_color_string [4096];
char color_terminal_string [512];

#define load_int(a,b,c) GetPrivateProfileInt(a,b,c,profile_name)
#define load_string(a,b,c,d,e) GetPrivateProfileString(a,b,c,d,e,profile_name)
#define save_string WritePrivateProfileString

int startup_left_mode;
int startup_right_mode;

/* Ugly hack to allow panel_save_setup to work as a place holder for */
/* default panel values */
int saving_setup;

static const struct {
    char *key;
    sortfn *sort_type;
} sort_names [] = {
    { "name",      (sortfn *) sort_name },
    { "extension", (sortfn *) sort_ext },
    { "time",      (sortfn *) sort_time },
    { "atime",     (sortfn *) sort_atime },
    { "ctime",     (sortfn *) sort_ctime },
    { "size",      (sortfn *) sort_size },
    { "inode",     (sortfn *) sort_inode },
    { "unsorted",  (sortfn *) unsorted },
    { 0, 0 }
};

static const struct {
    char *key;
    int  list_type;
} list_types [] = {
    { "full",  list_full  },
    { "brief", list_brief },
    { "long",  list_long  },
    { "user",  list_user  },
    { "icons", list_icons },
    { 0, 0 }
};

static const struct {
    char *opt_name;
    int  opt_type;
} panel_types [] = {
    { "listing",   view_listing },
    { "quickview", view_quick   },
    { "info",      view_info },
    { "tree",      view_tree },
    { 0, 0 }
};

static const struct {
    char *opt_name;
    int *opt_addr;
} layout [] = {
#ifndef HAVE_GNOME
    { "equal_split", &equal_split },
    { "first_panel_size", &first_panel_size },
    { "message_visible", &message_visible },
    { "keybar_visible", &keybar_visible },
    { "xterm_hintbar", &xterm_hintbar },
    { "output_lines", &output_lines },
#endif
    { "menubar_visible", &menubar_visible },
    { "command_prompt", &command_prompt },
    { "show_mini_info", &show_mini_info },
    { "permission_mode", &permission_mode },
    { "filetype_mode", &filetype_mode },
    { 0, 0 }
};


#undef SAVE_CHANGES_OUTSIDE_OPTIONS_MENU

#ifdef SAVE_CHANGES_OUTSIDE_OPTIONS_MENU
extern int preserve_uidgid;
#endif

static const struct {
    char *opt_name;
    int  *opt_addr;
} options [] = {
    { "show_backups", &show_backups },
#ifdef HAVE_GNOME
    { "gnome_show_dot_files", &show_dot_files },
#else
    { "show_dot_files", &show_dot_files },
#endif
    { "verbose", &verbose },
    { "mark_moves_down", &mark_moves_down },
    { "pause_after_run", &pause_after_run },
    { "shell_patterns", &easy_patterns },
    { "auto_save_setup", &auto_save_setup },
    { "align_extensions", &align_extensions },
    { "auto_menu", &auto_menu },
    { "use_internal_view", &use_internal_view },
    { "use_internal_edit", &use_internal_edit },
    { "clear_before_exec", &clear_before_exec },
    { "mix_all_files", &mix_all_files },
    { "fast_reload", &fast_reload },
    { "fast_reload_msg_shown", &fast_reload_w },
    { "confirm_delete", &confirm_delete },
    { "confirm_overwrite", &confirm_overwrite },
    { "confirm_execute", &confirm_execute },
    { "confirm_exit", &confirm_exit },
    { "safe_delete", &know_not_what_am_i_doing },
    { "mouse_repeat_rate", &mou_auto_repeat },
    { "double_click_speed", &double_click_speed },
    { "eight_bit_clean", &eight_bit_clean },
    { "full_eight_bits", &full_eight_bits },
    { "use_8th_bit_as_meta", &use_8th_bit_as_meta },
    { "confirm_view_dir", &confirm_view_dir },
    { "mouse_move_pages", &mouse_move_pages },
    { "mouse_move_pages_viewer", &mouse_move_pages_viewer },
    { "fast_refresh", &fast_refresh },
    { "navigate_with_arrows", &navigate_with_arrows },
    { "advanced_chown", &advanced_chfns },
    { "drop_menus", &drop_menus },
    { "wrap_mode",  &global_wrap_mode},
    { "old_esc_mode", &old_esc_mode },
#ifndef HAVE_GNOME
    { "cd_symlinks", &cd_symlinks },
#endif
    { "show_all_if_ambiguous", &show_all_if_ambiguous },
    { "have_fast_cpu", &have_fast_cpu },
    { "iconify_on_exec", &iconify_on_exec },
    { "torben_fj_mode", &torben_fj_mode },
    { "use_file_to_guess_type", &use_file_to_check_type },
    { "alternate_plus_minus", &alternate_plus_minus },
    { "only_leading_plus_minus", &only_leading_plus_minus },
    { "show_output_starts_shell", &output_starts_shell },
    { "panel_scroll_pages", &panel_scroll_pages },
    { "xtree_mode", &xtree_mode },
    { "num_history_items_recorded", &num_history_items_recorded },
    { "file_op_compute_totals", &file_op_compute_totals },
#ifdef HAVE_GNOME
    { "use_magic", &use_magic },
#endif
#ifdef SAVE_CHANGES_OUTSIDE_OPTIONS_MENU
    { "dive_into_subdirs", &dive_into_subdirs },
    { "preserve_uidgid", &preserve_uidgid },
    /* What about the other two options in the copy dialog
       (follow links, stable symlinks) -Norbert */
    { "tree_navigation_flag", &tree_navigation_flag },
#endif
#ifdef USE_VFS
    { "vfs_timeout", &vfs_timeout },
#ifdef USE_NETCODE
    { "ftpfs_directory_timeout", &ftpfs_directory_timeout },
    { "use_netrc", &use_netrc },
    { "ftpfs_retry_seconds", &ftpfs_retry_seconds },
    { "ftpfs_always_use_proxy", &ftpfs_always_use_proxy },
    { "ftpfs_use_passive_connections", &ftpfs_use_passive_connections },
    { "ftpfs_use_unix_list_options", &ftpfs_use_unix_list_options },
    { "ftpfs_first_cd_then_ls", &ftpfs_first_cd_then_ls },
#endif
#endif
#ifdef USE_INTERNAL_EDIT
    { "editor_word_wrap_line_length", &option_word_wrap_line_length },
    { "editor_key_emulation", &edit_key_emulation },
    { "editor_tab_spacing", &option_tab_spacing },
    { "editor_fill_tabs_with_spaces", &option_fill_tabs_with_spaces },
    { "editor_return_does_auto_indent", &option_return_does_auto_indent },
    { "editor_backspace_through_tabs", &option_backspace_through_tabs },
    { "editor_fake_half_tabs", &option_fake_half_tabs },
    { "editor_option_save_mode", &option_save_mode },
    { "editor_option_backup_ext_int", &option_backup_ext_int },
    { "editor_option_auto_para_formatting", &option_auto_para_formatting },
    { "editor_option_typewriter_wrap", &option_typewriter_wrap },
    { "editor_edit_confirm_save", &edit_confirm_save },
    { "editor_syntax_highlighting", &option_syntax_highlighting },
#endif

#ifdef HAVE_GNOME
    { "desktop_use_shaped_icons", &desktop_use_shaped_icons },
    { "desktop_use_shaped_text", &desktop_use_shaped_text },
    { "desktop_auto_placement", &desktop_auto_placement },
    { "desktop_snap_icons", &desktop_snap_icons },
    { "desktop_arr_r2l", &desktop_arr_r2l },
    { "desktop_arr_b2t", &desktop_arr_b2t },
    { "desktop_arr_rows", &desktop_arr_rows },
    { "tree_panel_visible", &tree_panel_visible },
    { "we_can_afford_the_speed", &we_can_afford_the_speed },

    /* Keep the following in sync with panel.h */
    { "column_width_brief_icon", &default_column_width[0] },
    { "column_width_brief_filename", &default_column_width[1] },
    { "column_width_detailed_icon", &default_column_width[2] },
    { "column_width_detailed_filename", &default_column_width[3] },
    { "column_width_detailed_size", &default_column_width[4] },
    { "column_width_detailed_mtime", &default_column_width[5] },
    { "column_width_custom_0", &default_column_width[6] },
    { "column_width_custom_1", &default_column_width[7] },
    { "column_width_custom_2", &default_column_width[8] },
    { "column_width_custom_3", &default_column_width[9] },
    { "column_width_custom_4", &default_column_width[10] },
    { "column_width_custom_5", &default_column_width[11] },
    { "column_width_custom_6", &default_column_width[12] },
    { "column_width_custom_7", &default_column_width[13] },
    { "column_width_custom_8", &default_column_width[14] },
    { "column_width_custom_9", &default_column_width[15] },
    { "column_width_custom_10", &default_column_width[16] },
    { "column_width_custom_11", &default_column_width[17] },
    { "column_width_custom_12", &default_column_width[18] },
    { "column_width_custom_13", &default_column_width[19] },
    { "column_width_custom_14", &default_column_width[20] },
#else
    { "nice_rotating_dash", &nice_rotating_dash },
    { "horizontal_split",   &horizontal_split },
#endif
    { 0, 0 }
};

void
panel_save_setup (WPanel *panel, char *section)
{
    char buffer [BUF_TINY];
    int  i;

    g_snprintf (buffer, sizeof (buffer), "%d", panel->reverse);
    save_string (section, "reverse", buffer, profile_name);
    g_snprintf (buffer, sizeof (buffer), "%d", panel->case_sensitive);
    save_string (section, "case_sensitive", buffer, profile_name);
    for (i = 0; sort_names [i].key; i++)
	if (sort_names [i].sort_type == (sortfn *) panel->sort_type){
	    save_string (section, "sort_order",
				       sort_names [i].key, profile_name);
	    break;
	}

    for (i = 0; list_types [i].key; i++)
	if (list_types [i].list_type == panel->list_type){
	    save_string (section, PORT_LIST_MODE_NAME, list_types [i].key, profile_name);
	    break;
	}

    save_string (section, "user_format",
			       panel->user_format, profile_name);

    for (i = 0; i < LIST_TYPES; i++){
	g_snprintf (buffer, sizeof (buffer), "user_status%d", i);
	save_string (section, buffer,
	    panel->user_status_format [i], profile_name);
    }

    g_snprintf (buffer, sizeof (buffer), "%d", panel->user_mini_status);
    save_string (section, "user_mini_status", buffer,
			       profile_name);
}

void
save_layout (void)
{
    char *profile;
    int  i;
    char buffer [BUF_TINY];

    profile = concat_dir_and_file (home_dir, PROFILE_NAME);

    /* Save integer options */
    for (i = 0; layout [i].opt_name; i++){
	g_snprintf (buffer, sizeof (buffer), "%d", *layout [i].opt_addr);
	save_string ("Layout", layout [i].opt_name, buffer, profile);
    }

    g_free (profile);
}

void
save_configure (void)
{
    char *profile;
    int  i;

    profile = concat_dir_and_file (home_dir, PROFILE_NAME);

    /* Save integer options */
    for (i = 0; options [i].opt_name; i++)
	set_int (profile, options [i].opt_name, *options [i].opt_addr);

#ifdef HAVE_GNOME
    save_string (app_text, "default_user_format", default_user_format, profile_name);
#endif

    g_free (profile);
}

static void
panel_save_type (char *section, int type)
{
    int i;

    for (i = 0; panel_types [i].opt_name; i++)
	if (panel_types [i].opt_type == type){
	    save_string (section, "display", panel_types [i].opt_name,
			 profile_name);
	    break;
	}
}

#ifndef PORT_HAS_SAVE_PANEL_TYPES
void
save_panel_types ()
{
    int type;

    type = get_display_type (0);
    panel_save_type ("New Left Panel", type);
    if (type == view_listing)
	panel_save_setup (left_panel, left_panel->panel_name);
    type = get_display_type (1);
    panel_save_type ("New Right Panel", type);
    if (type == view_listing)
	panel_save_setup (right_panel, right_panel->panel_name);
}
#endif

void
save_setup (void)
{
    char *profile;
#ifdef USE_VFS
#ifdef USE_NETCODE
    extern char *ftpfs_anonymous_passwd;
    extern char *ftpfs_proxy_host;
#endif
#endif
    saving_setup = 1;
    profile = concat_dir_and_file (home_dir, PROFILE_NAME);

    save_layout ();
    save_configure ();
    save_string ("Dirs", "other_dir",
			       get_other_type () == view_listing
			       ? opanel->cwd : ".", profile);
    if (get_current_panel () != NULL)
	    WritePrivateProfileString ("Dirs", "current_is_left",
				       get_current_index () == 0 ? "1" : "0", profile);
    save_hotlist ();
    save_panelize ();
    save_panel_types ();
/*     directory_history_save (); */

#ifdef USE_VFS
#ifdef USE_NETCODE
    WritePrivateProfileString ("Misc", "ftpfs_password",
			       ftpfs_anonymous_passwd, profile);
    if (ftpfs_proxy_host)
	WritePrivateProfileString ("Misc", "ftp_proxy_host",
				   ftpfs_proxy_host, profile);
#endif
#endif
    g_free (profile);
    saving_setup = 0;
}

void
panel_load_setup (WPanel *panel, char *section)
{
    int i;
    char buffer [BUF_TINY];

    panel->reverse = load_int (section, "reverse", 0);
    panel->case_sensitive = load_int (section, "case_sensitive", OS_SORT_CASE_SENSITIVE_DEFAULT);

    /* Load sort order */
    load_string (section, "sort_order", "name", buffer, sizeof (buffer));
    panel->sort_type = (sortfn *) sort_name;
    for (i = 0; sort_names [i].key; i++)
	if ( g_strcasecmp (sort_names [i].key, buffer) == 0){
	    panel->sort_type = sort_names [i].sort_type;
	    break;
	}

    /* Load the listing mode */
    load_string (section, PORT_LIST_MODE_NAME, PORT_LIST_MODE_DEFAULT, buffer, sizeof (buffer));
    panel->list_type = list_full;
    for (i = 0; list_types [i].key; i++)
	if ( g_strcasecmp (list_types [i].key, buffer) == 0){
	    panel->list_type = list_types [i].list_type;
	    break;
	}
#ifndef PORT_HAS_ICON_VIEW
    if (panel->list_type == list_icons)
        panel->list_type = list_full;
#endif
    /* User formats */
    if (panel->user_format){
	g_free (panel->user_format);
	panel->user_format = 0;
    }
    panel->user_format = g_strdup (get_profile_string (section, "user_format",
						     DEFAULT_USER_FORMAT,
						     profile_name));
    for (i = 0; i < LIST_TYPES; i++){
        if (panel->user_status_format [i])
	    g_free (panel->user_status_format [i]);
	g_snprintf (buffer, sizeof (buffer), "user_status%d", i);
	panel->user_status_format [i] =
	    g_strdup (get_profile_string (section, buffer,
			DEFAULT_USER_FORMAT, profile_name));
    }

    panel->user_mini_status =
	load_int (section, "user_mini_status", 0);

}

static void
load_layout (char *profile_name)
{
    int i;

    for (i = 0; layout [i].opt_name; i++)
	*layout [i].opt_addr =
	    load_int ("Layout", layout [i].opt_name,
				  *layout [i].opt_addr);
}

static int
load_mode (char *section)
{
    char buffer [20];
    int  i;

    int mode = view_listing;

    /* Load the display mode */
    load_string (section, "display", "listing", buffer, sizeof (buffer));

    for (i = 0; panel_types [i].opt_name; i++)
	if ( g_strcasecmp (panel_types [i].opt_name, buffer) == 0){
	    mode = panel_types [i].opt_type;
	    break;
	}

    return mode;
}

static char *
do_load_string (char *s, char *ss, char *def)
{
    char *buffer = g_malloc (BUF_SMALL);
    char *p;

    load_string (s, ss, def, buffer, BUF_SMALL);

    p = g_strdup (buffer);
    g_free (buffer);
    return p;
}

char *
setup_init (void)
{
    char *buffer;
    char   *profile;
    char   *inifile;

    if (profile_name)
	    return profile_name;

    buffer = concat_dir_and_file (home_dir, PROFILE_NAME);
    inifile = concat_dir_and_file (mc_home, "mc.ini");
    if (exist_file (buffer)){
	profile = buffer;
    } else if (exist_file (inifile)){
	profile = g_strdup (inifile);
	g_free (buffer);
    } else {
	profile = buffer;
    }
    g_free (inifile);

    profile_name = profile;

    return profile;
}

void
load_setup (void)
{
    char *profile;

    int    i;
#ifdef USE_NETCODE
    extern char *ftpfs_proxy_host;
#endif

    profile = setup_init ();
    /* Load integer boolean options */
    for (i = 0; options [i].opt_name; i++)
	*options [i].opt_addr =
	    get_int (profile, options [i].opt_name, *options [i].opt_addr);

#ifdef HAVE_GNOME
    g_free (default_user_format);
    default_user_format = do_load_string (app_text, "default_user_format", DEFAULT_USER_FORMAT);
#endif

    load_layout (profile);

    load_panelize ();

    startup_left_mode = load_mode ("New Left Panel");
    startup_right_mode = load_mode ("New Right Panel");

    /* At least one of the panels is a listing panel */
    if (startup_left_mode != view_listing && startup_right_mode!=view_listing)
	startup_left_mode = view_listing;

    if (!other_dir){
	char *buffer;

	buffer = (char*) g_malloc (MC_MAXPATHLEN);
	load_string ("Dirs", "other_dir", ".", buffer,
			     MC_MAXPATHLEN);
	if (vfs_file_is_local (buffer))
	    other_dir = buffer;
	else
	    g_free (buffer);
    }
#ifdef USE_NETCODE
    ftpfs_proxy_host = do_load_string ("Misc", "ftp_proxy_host", "gate");
#endif
    boot_current_is_left =
	GetPrivateProfileInt ("Dirs", "current_is_left", 1, profile);

    load_string ("Misc", "find_ignore_dirs", "", setup_color_string,
		 sizeof (setup_color_string));
    if (setup_color_string [0])
	find_ignore_dirs = g_strconcat (":", setup_color_string, ":", NULL);

    /* The default color and the terminal dependent color */
    load_string ("Colors", "base_color", "", setup_color_string,
			     sizeof (setup_color_string));
    load_string ("Colors", getenv ("TERM"), "",
			     term_color_string, sizeof (term_color_string));
    load_string ("Colors", "color_terminals", "",
			     color_terminal_string, sizeof (color_terminal_string));

    /* Load the directory history */
/*    directory_history_load (); */
    /* Remove the temporal entries */
    profile_clean_section ("Temporal:New Left Panel", profile_name);
    profile_clean_section ("Temporal:New Right Panel", profile_name);
#ifdef USE_VFS
#ifdef USE_NETCODE
    ftpfs_init_passwd ();
#endif
#endif
}

#ifdef USE_VFS
#ifdef USE_NETCODE
char *
load_anon_passwd ()
{
    char buffer [255];

    load_string ("Misc", "ftpfs_password", "", buffer, sizeof (buffer));
    if (buffer [0])
	return g_strdup (buffer);
    else
	return 0;
}
#endif
#endif

void done_setup (void)
{
    g_free (profile_name);
    done_hotlist ();
    done_panelize ();
/*    directory_history_free (); */
}

static void
load_keys_from_section (char *terminal, char *profile_name)
{
    char *section_name;
    void *profile_keys;
    char *key, *value, *valcopy;
    int  key_code;

    if (!terminal)
	return;

    section_name = g_strconcat ("terminal:", terminal, NULL);
    profile_keys = profile_init_iterator (section_name, profile_name);
    if (!profile_keys){
	g_free (section_name);
	return;
    }

    while (profile_keys){
	profile_keys = profile_iterator_next (profile_keys, &key, &value);
	key_code = lookup_key (key);
	valcopy = convert_controls (value);
	if (key_code)
	    define_sequence (key_code, valcopy, MCKEY_NOACTION);
	g_free (valcopy);
    }
    g_free (section_name);
    return;
}

void load_key_defs (void)
{
    char *libfile = concat_dir_and_file (mc_home, "mc.lib");
    load_keys_from_section (getenv ("TERM"), profile_name);
    load_keys_from_section ("general", profile_name);

    load_keys_from_section (getenv ("TERM"), libfile);
    load_keys_from_section ("general", libfile);

    /* We don't want a huge database loaded in core */
    free_profile_name (libfile);
    g_free (libfile);
}
