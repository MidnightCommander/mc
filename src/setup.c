/* Setup loading/saving.
   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2006, 2007, 2009 Free Software Foundation, Inc.

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

/** \file setup.c
 *  \brief Source: setup loading/saving
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "global.h"

#include "../src/tty/tty.h"
#include "../src/tty/key.h"		/* For the externs */
#include "../src/tty/mouse.h"		/* To make view.h happy */
#include "../src/tty/win.h"		/* lookup_key */

#include "dir.h"
#include "panel.h"
#include "main.h"
#include "tree.h"		/* xtree_mode */
#include "../src/mcconfig/mcconfig.h"
#include "setup.h"
#include "view.h"		/* For the externs */
#include "hotlist.h"		/* load/save/done hotlist */
#include "panelize.h"		/* load/save/done panelize */
#include "layout.h"
#include "menu.h"		/* menubar_visible declaration */
#include "cmd.h"
#include "file.h"		/* safe_delete */

#ifdef USE_VFS
#include "../vfs/gc.h"
#endif

#ifdef HAVE_CHARSET
#include "charsets.h"
#endif

#ifdef USE_NETCODE
#   include "../vfs/ftpfs.h"
#   include "../vfs/fish.h"
#endif

#ifdef USE_INTERNAL_EDIT
#   include "../edit/edit.h"
#endif

#include "../src/strutil.h"	/* str_isutf8 () */


extern char *find_ignore_dirs;

extern int num_history_items_recorded;

char *profile_name;		/* .mc/ini */
char *global_profile_name;	/* mc.lib */
char *panels_profile_name;	/* .mc/panels.ini */

char *setup_color_string;
char *term_color_string;
char *color_terminal_string;

int startup_left_mode;
int startup_right_mode;

/* Ugly hack to allow panel_save_setup to work as a place holder for */
/* default panel values */
int saving_setup;

static const struct {
    const char *key;
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
    const char *key;
    int  list_type;
} list_types [] = {
    { "full",  list_full  },
    { "brief", list_brief },
    { "long",  list_long  },
    { "user",  list_user  },
    { 0, 0 }
};

static const struct {
    const char *opt_name;
    int  opt_type;
} panel_types [] = {
    { "listing",   view_listing },
    { "quickview", view_quick   },
    { "info",      view_info },
    { "tree",      view_tree },
    { 0, 0 }
};

static const struct {
    const char *opt_name;
    int *opt_addr;
} layout [] = {
    { "equal_split", &equal_split },
    { "first_panel_size", &first_panel_size },
    { "message_visible", &message_visible },
    { "keybar_visible", &keybar_visible },
    { "xterm_title", &xterm_title },
    { "output_lines", &output_lines },
    { "command_prompt", &command_prompt },
    { "menubar_visible", &menubar_visible },
    { "show_mini_info", &show_mini_info },
    { "permission_mode", &permission_mode },
    { "filetype_mode", &filetype_mode },
    { "free_space", &free_space },
    { 0, 0 }
};

static const struct {
    const char *opt_name;
    int  *opt_addr;
} int_options [] = {
    { "show_backups", &show_backups },
    { "show_dot_files", &show_dot_files },
    { "verbose", &verbose },
    { "mark_moves_down", &mark_moves_down },
    { "pause_after_run", &pause_after_run },
    { "shell_patterns", &easy_patterns },
    { "auto_save_setup", &auto_save_setup },
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
    { "confirm_directory_hotlist_delete", &confirm_directory_hotlist_delete },
    { "safe_delete", &safe_delete },
    { "mouse_repeat_rate", &mou_auto_repeat },
    { "double_click_speed", &double_click_speed },
#ifndef HAVE_CHARSET
    { "eight_bit_clean", &eight_bit_clean },
    { "full_eight_bits", &full_eight_bits },
#endif /* !HAVE_CHARSET */
    { "use_8th_bit_as_meta", &use_8th_bit_as_meta },
    { "confirm_view_dir", &confirm_view_dir },
    { "mouse_move_pages", &mouse_move_pages },
    { "mouse_move_pages_viewer", &mouse_move_pages_viewer },
    { "mouse_close_dialog", &mouse_close_dialog},
    { "fast_refresh", &fast_refresh },
    { "navigate_with_arrows", &navigate_with_arrows },
    { "drop_menus", &drop_menus },
    { "wrap_mode",  &global_wrap_mode},
    { "old_esc_mode", &old_esc_mode },
    { "cd_symlinks", &cd_symlinks },
    { "show_all_if_ambiguous", &show_all_if_ambiguous },
    { "max_dirt_limit", &max_dirt_limit },
    { "torben_fj_mode", &torben_fj_mode },
    { "use_file_to_guess_type", &use_file_to_check_type },
    { "alternate_plus_minus", &alternate_plus_minus },
    { "only_leading_plus_minus", &only_leading_plus_minus },
    { "show_output_starts_shell", &output_starts_shell },
    { "panel_scroll_pages", &panel_scroll_pages },
    { "xtree_mode", &xtree_mode },
    { "num_history_items_recorded", &num_history_items_recorded },
    { "file_op_compute_totals", &file_op_compute_totals },
    { "skip_check_codeset", &skip_check_codeset },
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
    { "fish_directory_timeout", &fish_directory_timeout },
#endif /* USE_NETCODE */
#endif /* USE_VFS */
#ifdef USE_INTERNAL_EDIT
    { "editor_word_wrap_line_length", &option_word_wrap_line_length },
    { "editor_key_emulation", &edit_key_emulation },
    { "editor_tab_spacing", &option_tab_spacing },
    { "editor_fill_tabs_with_spaces", &option_fill_tabs_with_spaces },
    { "editor_return_does_auto_indent", &option_return_does_auto_indent },
    { "editor_backspace_through_tabs", &option_backspace_through_tabs },
    { "editor_fake_half_tabs", &option_fake_half_tabs },
    { "editor_option_save_mode", &option_save_mode },
    { "editor_option_save_position", &option_save_position },
    { "editor_option_auto_para_formatting", &option_auto_para_formatting },
    { "editor_option_typewriter_wrap", &option_typewriter_wrap },
    { "editor_edit_confirm_save", &edit_confirm_save },
    { "editor_syntax_highlighting", &option_syntax_highlighting },
    { "editor_persistent_selections", &option_persistent_selections },
    { "editor_visible_tabs", &visible_tabs },
    { "editor_visible_spaces", &visible_tws },
    { "editor_line_state", &option_line_state },
    { "editor_simple_statusbar", &simple_statusbar },
#endif /* USE_INTERNAL_EDIT */

    { "nice_rotating_dash", &nice_rotating_dash },
    { "horizontal_split",   &horizontal_split },
    { "mcview_remember_file_position", &mcview_remember_file_position },
    { "auto_fill_mkdir_name", &auto_fill_mkdir_name },
    { 0, 0 }
};

static const struct {
    const char *opt_name;
    char **opt_addr;
    const char *opt_defval;
} str_options [] = {
#ifdef USE_INTERNAL_EDIT
    { "editor_backup_extension", &option_backup_ext, "~" },
#endif
    { NULL, NULL, NULL }
};

void
panel_save_setup (struct WPanel *panel, const char *section)
{
    char *buffer;
    int  i;

    mc_config_set_int(mc_panels_config, section, "reverse", panel->reverse);
    mc_config_set_int(mc_panels_config, section, "case_sensitive", panel->case_sensitive);
    mc_config_set_int(mc_panels_config, section, "exec_first", panel->exec_first);


    for (i = 0; sort_names [i].key; i++)
	if (sort_names [i].sort_type == (sortfn *) panel->sort_type){
	    mc_config_set_string(mc_panels_config, section, "sort_order", sort_names [i].key);
	    break;
	}

    for (i = 0; list_types [i].key; i++)
	if (list_types [i].list_type == panel->list_type){
	    mc_config_set_string(mc_panels_config, section, "list_mode", list_types [i].key);
	    break;
	}

    mc_config_set_string(mc_panels_config, section, "user_format", panel->user_format);

    for (i = 0; i < LIST_TYPES; i++){
	buffer = g_strdup_printf("user_status%d", i);
	mc_config_set_string(mc_panels_config, section, buffer, panel->user_status_format [i]);
	g_free(buffer);
    }

    mc_config_set_int(mc_panels_config, section, "user_mini_status", panel->user_mini_status);
}

void
save_layout (void)
{
    char *profile;
    int  i;

    profile = concat_dir_and_file (home_dir, PROFILE_NAME);

    /* Save integer options */
    for (i = 0; layout [i].opt_name; i++){
	mc_config_set_int(mc_main_config, "Layout", layout [i].opt_name, *layout [i].opt_addr);
    }
    mc_config_save_to_file (mc_main_config, profile);

    g_free (profile);
}

void
save_configure (void)
{
    char *profile;
    int  i;

    profile = concat_dir_and_file (home_dir, PROFILE_NAME);

    /* Save integer options */
    for (i = 0; int_options[i].opt_name; i++)
	mc_config_set_int(mc_main_config, CONFIG_APP_SECTION, int_options[i].opt_name, *int_options[i].opt_addr);

    /* Save string options */
    for (i = 0; str_options[i].opt_name != NULL; i++)
	mc_config_set_string(mc_main_config, CONFIG_APP_SECTION, str_options[i].opt_name, *str_options[i].opt_addr);

    mc_config_save_to_file (mc_main_config, profile);
    g_free (profile);
}

static void
panel_save_type (const char *section, int type)
{
    int i;

    for (i = 0; panel_types [i].opt_name; i++)
	if (panel_types [i].opt_type == type){
	    mc_config_set_string(mc_panels_config, section,
	                "display", panel_types [i].opt_name);
	    break;
	}
}

void
save_panel_types (void)
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

    mc_config_set_string(mc_panels_config, "Dirs" , "other_dir",
			       get_other_type () == view_listing
			       ? other_panel->cwd : ".");
    if (current_panel != NULL)
	    mc_config_set_string(mc_panels_config, "Dirs" , "current_is_left",
				       get_current_index () == 0 ? "1" : "0");

    if (mc_panels_config->ini_path == NULL)
        mc_panels_config->ini_path = g_strdup(panels_profile_name);

    mc_config_del_group (mc_panels_config, "Temporal:New Left Panel");
    mc_config_del_group (mc_panels_config, "Temporal:New Right Panel");

    mc_config_save_file (mc_panels_config);
}

void
save_setup (void)
{
    char *tmp_profile;

    saving_setup = 1;

    save_configure ();

    save_layout ();

    save_hotlist ();

    save_panelize ();
    save_panel_types ();
/*     directory_history_save (); */

#if defined(USE_VFS) && defined (USE_NETCODE)
    mc_config_set_string(mc_main_config, "Misc" , "ftpfs_password",
			       ftpfs_anonymous_passwd);
    if (ftpfs_proxy_host)
	mc_config_set_string(mc_main_config, "Misc" , "ftp_proxy_host",
				   ftpfs_proxy_host);
#endif /* USE_VFS && USE_NETCODE */

#ifdef HAVE_CHARSET
    mc_config_set_string(mc_main_config, "Misc" , "display_codepage",
		 get_codepage_id( display_codepage ));
    mc_config_set_string(mc_main_config, "Misc" , "source_codepage",
		 get_codepage_id( source_codepage ));
#endif /* HAVE_CHARSET */
    tmp_profile = concat_dir_and_file (home_dir, PROFILE_NAME);
    mc_config_save_to_file (mc_main_config, tmp_profile);
    g_free (tmp_profile);
    saving_setup = 0;
}

void
panel_load_setup (WPanel *panel, const char *section)
{
    int i;
    char *buffer;

    panel->reverse = mc_config_get_int(mc_panels_config, section, "reverse", 0);
    panel->case_sensitive = mc_config_get_int(mc_panels_config, section, "case_sensitive", OS_SORT_CASE_SENSITIVE_DEFAULT);
    panel->exec_first = mc_config_get_int(mc_panels_config, section, "exec_first", 0);

    /* Load sort order */
    buffer = mc_config_get_string(mc_panels_config, section, "sort_order", "name");

    panel->sort_type = (sortfn *) sort_name;
    for (i = 0; sort_names [i].key; i++)
	if ( g_strcasecmp (sort_names [i].key, buffer) == 0){
	    panel->sort_type = sort_names [i].sort_type;
	    break;
	}
    g_free(buffer);
    /* Load the listing mode */
    buffer = mc_config_get_string(mc_panels_config, section, "list_mode", "full");
    panel->list_type = list_full;
    for (i = 0; list_types [i].key; i++)
	if ( g_strcasecmp (list_types [i].key, buffer) == 0){
	    panel->list_type = list_types [i].list_type;
	    break;
	}
    g_free(buffer);

    /* User formats */
    g_free (panel->user_format);
    panel->user_format = mc_config_get_string(mc_panels_config, section, "user_format", DEFAULT_USER_FORMAT);

    for (i = 0; i < LIST_TYPES; i++){
	g_free (panel->user_status_format [i]);
	buffer = g_strdup_printf("user_status%d",i);
	panel->user_status_format [i] =
	    mc_config_get_string(mc_panels_config, section, buffer, DEFAULT_USER_FORMAT);
        g_free(buffer);
    }

    panel->user_mini_status =
        mc_config_get_int(mc_panels_config, section, "user_mini_status", 0);

}

static void
load_layout ()
{
    int i;

    for (i = 0; layout [i].opt_name; i++)
	*layout [i].opt_addr =
	    mc_config_get_int(mc_main_config,"Layout", layout [i].opt_name, *layout [i].opt_addr);
}

static int
setup__load_panel_state (const char *section)
{
    char *buffer;
    int  i;

    int mode = view_listing;

    /* Load the display mode */
    buffer = mc_config_get_string(mc_panels_config, section, "display", "listing");

    for (i = 0; panel_types [i].opt_name; i++)
	if ( g_strcasecmp (panel_types [i].opt_name, buffer) == 0){
	    mode = panel_types [i].opt_type;
	    break;
	}
    g_free(buffer);
    return mode;
}

static const char *
setup__is_cfg_group_must_panel_config(const char *grp)
{
    if (
        ! strcasecmp("Dirs",grp) ||
        ! strcasecmp("Temporal:New Right Panel",grp) ||
        ! strcasecmp("Temporal:New Left Panel",grp) ||
        ! strcasecmp("New Left Panel",grp) ||
        ! strcasecmp("New Right Panel",grp)
    )
        return grp;
    return NULL;
}

static void
setup__move_panels_config_into_separate_file(const char*profile)
{
    mc_config_t *tmp_cfg;
    char **groups, **curr_grp;
    const char *need_grp;
    gsize groups_count;

    if (!exist_file(profile))
        return;

    tmp_cfg = mc_config_init(profile);
    if (!tmp_cfg)
        return;

    curr_grp = groups = mc_config_get_groups (tmp_cfg, &groups_count);
    if (!groups)
    {
        mc_config_deinit(tmp_cfg);
        return;
    }
    while (*curr_grp)
    {
        if ( setup__is_cfg_group_must_panel_config(*curr_grp) == NULL)
        {
            mc_config_del_group (tmp_cfg, *curr_grp);
        }
        curr_grp++;
    }

    mc_config_save_to_file (tmp_cfg, panels_profile_name);
    mc_config_deinit(tmp_cfg);

    tmp_cfg = mc_config_init(profile);
    if (!tmp_cfg)
    {
        g_strfreev(groups);
        return;
    }

    curr_grp = groups;

    while (*curr_grp)
    {
        need_grp = setup__is_cfg_group_must_panel_config(*curr_grp);
        if ( need_grp != NULL)
        {
            mc_config_del_group (tmp_cfg, need_grp);
        }
        curr_grp++;
    }
    g_strfreev(groups);
    mc_config_save_file (tmp_cfg);
    mc_config_deinit(tmp_cfg);

}

char *
setup_init (void)
{
    char   *profile;
    char   *inifile;

    if (profile_name)
	    return profile_name;

    profile = concat_dir_and_file (home_dir, PROFILE_NAME);
    if (!exist_file (profile)){
	inifile = concat_dir_and_file (mc_home, "mc.ini");
	if (exist_file (inifile)){
	    g_free (profile);
	    profile = inifile;
	} else {
	    g_free (inifile);
	    inifile = concat_dir_and_file (mc_home_alt, "mc.ini");
	    if (exist_file (inifile)) {
		g_free (profile);
		profile = inifile;
	    } else
		g_free (inifile);
	}
    }

    profile_name = profile;

    return profile;
}

void
load_setup (void)
{
    char *profile;
    int    i;
    char *buffer;

    profile = setup_init ();

    /* mc.lib is common for all users, but has priority lower than
       ~/.mc/ini.  FIXME: it's only used for keys and treestore now */
    global_profile_name = concat_dir_and_file (mc_home, "mc.lib");

    if (!exist_file(global_profile_name)) {
	g_free (global_profile_name);
	global_profile_name = concat_dir_and_file (mc_home_alt, "mc.lib");
    }
    panels_profile_name = concat_dir_and_file (home_dir, PANELS_PROFILE_NAME);

    mc_main_config = mc_config_init(profile);

    if (!exist_file(panels_profile_name))
        setup__move_panels_config_into_separate_file(profile);

    mc_panels_config = mc_config_init(panels_profile_name);

    /* Load integer boolean options */
    for (i = 0; int_options[i].opt_name; i++)
	*int_options[i].opt_addr =
	    mc_config_get_int(mc_main_config, CONFIG_APP_SECTION, int_options[i].opt_name, *int_options[i].opt_addr);

    /* Load string options */
    for (i = 0; str_options[i].opt_name != NULL; i++)
	*str_options[i].opt_addr =
	    mc_config_get_string (mc_main_config, CONFIG_APP_SECTION, str_options[i].opt_name, str_options[i].opt_defval);

    load_layout ();

    load_panelize ();

    startup_left_mode = setup__load_panel_state ("New Left Panel");
    startup_right_mode = setup__load_panel_state ("New Right Panel");

    /* At least one of the panels is a listing panel */
    if (startup_left_mode != view_listing && startup_right_mode!=view_listing)
	startup_left_mode = view_listing;

    if (!other_dir){
	buffer = mc_config_get_string(mc_panels_config, "Dirs", "other_dir", ".");
	if (vfs_file_is_local (buffer))
	    other_dir = buffer;
	else
	    g_free (buffer);
    }

    boot_current_is_left =
        mc_config_get_int(mc_panels_config, "Dirs", "current_is_left", 1);

#ifdef USE_NETCODE
    ftpfs_proxy_host = mc_config_get_string(mc_main_config, "Misc", "ftp_proxy_host", "gate");
#endif
    setup_color_string = mc_config_get_string(mc_main_config, "Misc", "find_ignore_dirs", "");
    if (setup_color_string [0])
	find_ignore_dirs = g_strconcat (":", setup_color_string, ":", (char *) NULL);
    g_free(setup_color_string);

    /* The default color and the terminal dependent color */
    setup_color_string = mc_config_get_string(mc_main_config, "Colors", "base_color", "");
    term_color_string = mc_config_get_string(mc_main_config, "Colors", getenv ("TERM"), "");
    color_terminal_string = mc_config_get_string(mc_main_config, "Colors", "color_terminals", "");

    /* Load the directory history */
/*    directory_history_load (); */
    /* Remove the temporal entries */
#if defined(USE_VFS) && defined (USE_NETCODE)
    ftpfs_init_passwd ();
#endif /* USE_VFS && USE_NETCODE */

#ifdef HAVE_CHARSET
    if ( load_codepages_list() > 0 ) {
	buffer = mc_config_get_string(mc_main_config, "Misc", "display_codepage", "");
	if ( buffer[0] != '\0' )
	{
	    display_codepage = get_codepage_index( buffer );
	    cp_display = get_codepage_id (display_codepage);
	}
	g_free(buffer);
	buffer = mc_config_get_string(mc_main_config, "Misc", "source_codepage", "");
	if ( buffer[0] != '\0' )
	{
	    source_codepage = get_codepage_index( buffer );
	    cp_source = get_codepage_id (source_codepage);
	}
	g_free(buffer);
    }
    init_translation_table( source_codepage, display_codepage );
    if ( get_codepage_id( display_codepage ) )
        utf8_display = str_isutf8 (get_codepage_id( display_codepage ));
#endif /* HAVE_CHARSET */
}

#if defined(USE_VFS) && defined (USE_NETCODE)
char *
load_anon_passwd ()
{
    char *buffer;

    buffer = mc_config_get_string(mc_main_config, "Misc", "ftpfs_password", "");
    if (buffer [0])
	return buffer;

    g_free(buffer);
    return NULL;
}
#endif /* USE_VFS && USE_NETCODE */

void done_setup (void)
{
    g_free (profile_name);
    g_free (global_profile_name);
    g_free(color_terminal_string);
    g_free(term_color_string);
    g_free(setup_color_string);
    mc_config_deinit(mc_main_config);
    mc_config_deinit(mc_panels_config);
    done_hotlist ();
    done_panelize ();
/*    directory_history_free (); */
}

static void
load_keys_from_section (const char *terminal, mc_config_t *cfg)
{
    char *section_name;
    gchar **profile_keys, **keys;
    gchar **values, **curr_values;
    char *valcopy, *value;
    int  key_code;
    gsize len, values_len;

    if (!terminal)
	return;

    section_name = g_strconcat ("terminal:", terminal, (char *) NULL);
    profile_keys = keys = mc_config_get_keys (cfg, section_name, &len);

    while (*profile_keys){

	/* copy=other causes all keys from [terminal:other] to be loaded.  */
	if (g_strcasecmp (*profile_keys, "copy") == 0) {
	    valcopy = mc_config_get_string (cfg, section_name, *profile_keys, "");
	    load_keys_from_section (valcopy, cfg);
	    g_free(valcopy);
	    profile_keys++;
	    continue;
	}
	curr_values = values = mc_config_get_string_list (cfg, section_name, *profile_keys, &values_len);

	key_code = lookup_key (*profile_keys);
	if (key_code){
	    if (curr_values){
	        while (*curr_values){
	            valcopy = convert_controls (*curr_values);
	            define_sequence (key_code, valcopy, MCKEY_NOACTION);
	            g_free (valcopy);
	            curr_values++;
	        }
	    } else {
	        value = mc_config_get_string (cfg, section_name, *profile_keys, "");
	        valcopy = convert_controls (value);
	        define_sequence (key_code, valcopy, MCKEY_NOACTION);
	        g_free (valcopy);
	        g_free(value);
	    }
	}
	profile_keys++;
	if (values)
	    g_strfreev(values);
    }
    g_strfreev(keys);
    g_free (section_name);
}

void load_key_defs (void)
{
    /*
     * Load keys from mc.lib before ~/.mc/ini, so that the user
     * definitions override global settings.
     */
    mc_config_t *mc_global_config;

    mc_global_config = mc_config_init(global_profile_name);
    if (mc_global_config != NULL)
    {
        load_keys_from_section ("general", mc_global_config);
        load_keys_from_section (getenv ("TERM"), mc_global_config);
        mc_config_deinit(mc_global_config);
    }
    load_keys_from_section ("general", mc_main_config);
    load_keys_from_section (getenv ("TERM"), mc_main_config);

}
