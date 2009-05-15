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

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "global.h"
#include "tty.h"
#include "dir.h"
#include "panel.h"
#include "main.h"
#include "tree.h"		/* xtree_mode */
#include "../src/mcconfig/mcconfig.h"
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

/*** global variables **************************************************/

extern char *find_ignore_dirs;

extern int num_history_items_recorded;

mc_config_t *mc_profile;	/* .mc/ini */
mc_config_t *mc_global_profile;/* mc.lib */

char *setup_color_string;
char *term_color_string;
char *color_terminal_string;

int startup_left_mode;
int startup_right_mode;

/* Ugly hack to allow panel_save_setup to work as a place holder for */
/* default panel values */
int saving_setup;

/*** file scope macro definitions **************************************/

/*** file scope type declarations **************************************/

/*** file scope variables **********************************************/

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

/*** file scope functions **********************************************/

static void
setup__panel_save (struct WPanel *panel, const char *section)
{
    char buffer [BUF_TINY];
    int  i;

    g_snprintf (buffer, sizeof (buffer), "%d", panel->reverse);
    mc_config_set_string (mc_profile, section, "reverse", buffer);

    g_snprintf (buffer, sizeof (buffer), "%d", panel->case_sensitive);
    mc_config_set_string (mc_profile, section, "case_sensitive", buffer);

    g_snprintf (buffer, sizeof (buffer), "%d", panel->exec_first);
    mc_config_set_string (mc_profile, section, "exec_first", buffer);

    for (i = 0; sort_names [i].key; i++)
	if (sort_names [i].sort_type == (sortfn *) panel->sort_type){
	    mc_config_set_string (mc_profile, section, "sort_order", sort_names [i].key);
	    break;
	}

    for (i = 0; list_types [i].key; i++)
	if (list_types [i].list_type == panel->list_type){
	    mc_config_set_string (mc_profile, section, "list_mode", list_types [i].key);
	    break;
	}

    mc_config_set_string (mc_profile, section, "user_format", panel->user_format);

    for (i = 0; i < LIST_TYPES; i++){
	g_snprintf (buffer, sizeof (buffer), "user_status%d", i);
	mc_config_set_string (mc_profile, section, buffer,
	    panel->user_status_format [i]);
    }

    g_snprintf (buffer, sizeof (buffer), "%d", panel->user_mini_status);
    mc_config_set_string (mc_profile,section, "user_mini_status", buffer);

}

static void
panel_save_type (const char *section, int type)
{
    int i;

    for (i = 0; panel_types [i].opt_name; i++)
	if (panel_types [i].opt_type == type){
	    mc_config_set_string(mc_profile, section, "display", panel_types[i].opt_name);
	    break;
	}
}

static void
save_panel_types ()
{
    int type;

    type = get_display_type (0);
    panel_save_type ( "New Left Panel", type);
    if (type == view_listing)
	setup__panel_save (left_panel, left_panel->panel_name);
    type = get_display_type (1);
    panel_save_type ( "New Right Panel", type);
    if (type == view_listing)
	setup__panel_save (right_panel, right_panel->panel_name);
}

static void
load_layout (void)
{
    int i;

    for (i = 0; layout [i].opt_name; i++)
	*layout [i].opt_addr =
	    mc_config_get_int(mc_profile,"Layout", layout [i].opt_name, *layout [i].opt_addr);
}

static int
load_mode (const char *section)
{
    char *buffer;
    int  i;

    int mode = view_listing;

    /* Load the display mode */
    buffer = mc_config_get_string(mc_profile, section, "display", "listing");

    for (i = 0; panel_types [i].opt_name; i++)
	if ( g_strcasecmp (panel_types [i].opt_name, buffer) == 0){
	    mode = panel_types [i].opt_type;
	    break;
	}
    g_free(buffer);
    return mode;
}

static void
load_keys_from_section (const char *terminal, mc_config_t *cfg)
{
    char *section_name;
    char **profile_keys, **keys;
    gsize len;
    char *valcopy, *value;
    int  key_code;

    if (!terminal)
	return;

    section_name = g_strconcat ("terminal:", terminal, (char *) NULL);
    keys = mc_config_get_keys (cfg, section_name,&len);
    profile_keys = keys;

    while (*profile_keys){
	value = mc_config_get_string(cfg, section_name, *profile_keys, "");
	/* copy=other causes all keys from [terminal:other] to be loaded.  */
	if (g_strcasecmp (*profile_keys, "copy") == 0) {
	    load_keys_from_section (value, cfg);
	    continue;
	}

	key_code = lookup_key (*profile_keys);
	if (key_code){
	    valcopy = convert_controls (value);
	    define_sequence (key_code, valcopy, MCKEY_NOACTION);
	    g_free (valcopy);
	}
	g_free(value);
	profile_keys++;
    }
    g_free (section_name);
    g_strfreev(keys);
}

/*** public functions **************************************************/

void
panel_save_setup ( struct WPanel *panel, const char *section)
{
    g_free(mc_profile->ini_path);
    mc_profile->ini_path = concat_dir_and_file (home_dir, PROFILE_NAME);

    setup__panel_save ( panel, section);
    mc_config_save_file(mc_profile);
}

void
save_layout (void)
{
    int  i;
    /* Save integer options */
    for (i = 0; layout [i].opt_name; i++){
	mc_config_set_int(mc_profile, "Layout", layout [i].opt_name, *layout [i].opt_addr);
    }
}

void
save_configure (void)
{
    int  i;

    /* Save integer options */
    for (i = 0; int_options[i].opt_name; i++)
	mc_config_set_int (mc_profile, CONFIG_APP_SECTION, int_options[i].opt_name, *int_options[i].opt_addr);

    /* Save string options */
    for (i = 0; str_options[i].opt_name != NULL; i++)
	mc_config_set_string (mc_profile, CONFIG_APP_SECTION, str_options[i].opt_name, *str_options[i].opt_addr);
}

void
save_setup (void)
{
    saving_setup = 1;
    g_free(mc_profile->ini_path);
    mc_profile->ini_path = concat_dir_and_file (home_dir, PROFILE_NAME);

    save_configure ();

    save_layout ();
    mc_config_set_string(mc_profile, "Dirs", "other_dir", 
			 get_other_type () == view_listing  ? other_panel->cwd : ".");

    if (current_panel != NULL)
	    mc_config_set_string(mc_profile,"Dirs", "current_is_left", get_current_index () == 0 ? "1" : "0");

    save_hotlist ();

    save_panelize ();
    save_panel_types ();
/*     directory_history_save (); */

#if defined(USE_VFS) && defined (USE_NETCODE)
    mc_config_set_string(mc_profile, "Misc", "ftpfs_password", ftpfs_anonymous_passwd);
    if (ftpfs_proxy_host)
	mc_config_set_string(mc_profile, "Misc", "ftp_proxy_host", ftpfs_proxy_host);
#endif /* USE_VFS && USE_NETCODE */

#ifdef HAVE_CHARSET
    mc_config_set_string(mc_profile, "Misc", "display_codepage", get_codepage_id( display_codepage ));
    mc_config_set_string(mc_profile, "Misc", "source_codepage", get_codepage_id( source_codepage ));
#endif /* HAVE_CHARSET */

    saving_setup = 0;
}

void
panel_load_setup (WPanel *panel, const char *section)
{
    int i;
    char *buff;

    panel->reverse = mc_config_get_int(mc_profile, section, "reverse", 0);
    panel->case_sensitive = mc_config_get_int(mc_profile, section, "case_sensitive", OS_SORT_CASE_SENSITIVE_DEFAULT);
    panel->exec_first = mc_config_get_int(mc_profile, section, "exec_first", 0);

    /* Load sort order */
    buff = mc_config_get_string(mc_profile, section, "sort_order", "name");
    panel->sort_type = (sortfn *) sort_name;
    for (i = 0; sort_names [i].key; i++)
	if ( g_strcasecmp (sort_names [i].key, buff) == 0){
	    panel->sort_type = sort_names [i].sort_type;
	    break;
	}
    g_free(buff);

    /* Load the listing mode */
    buff = mc_config_get_string(mc_profile, section, "list_mode", "full");
    panel->list_type = list_full;
    for (i = 0; list_types [i].key; i++)
	if ( g_strcasecmp (list_types [i].key, buff) == 0){
	    panel->list_type = list_types [i].list_type;
	    break;
	}
    g_free(buff);

    /* User formats */
    g_free (panel->user_format);
    panel->user_format = mc_config_get_string(mc_profile, section, "user_format", DEFAULT_USER_FORMAT);

    for (i = 0; i < LIST_TYPES; i++){
	g_free (panel->user_status_format [i]);
	buff = g_strdup_printf("user_status%d", i);
	panel->user_status_format [i] = 
	    mc_config_get_string (mc_profile, section, buff, DEFAULT_USER_FORMAT);
	g_free(buff);
    }

    panel->user_mini_status =
	mc_config_get_int(mc_profile, section, "user_mini_status", 0);
}

mc_config_t *
setup_init (void)
{
    char   *profile;
    char   *inifile;

    if (mc_profile)
	    return mc_profile;

    profile = concat_dir_and_file (home_dir, PROFILE_NAME);
    if (!exist_file (profile)){
	inifile = concat_dir_and_file (mc_home, "mc.ini");
	if (exist_file (inifile)){
	    g_free (profile);
	    profile = inifile;
	} else
	    g_free (inifile);
    }
    mc_profile = mc_config_init(profile);

    if (mc_profile == NULL){
	gchar *init_data;

	init_data = g_strdup_printf ("[%s]\n",CONFIG_APP_SECTION);
	if (!g_file_set_contents(profile, init_data,strlen(init_data),NULL))
	{
	    g_free(init_data);
	    return NULL;
	}
	g_free(init_data);
	mc_profile = mc_config_init(profile);
    }
    return mc_profile;
}

void
load_setup (void)
{
    char   *gp_name;
    int    i;

    setup_init ();

    /* mc.lib is common for all users, but has priority lower than
       ~/.mc/ini.  FIXME: it's only used for keys and treestore now */
    gp_name = concat_dir_and_file (mc_home, "mc.lib");
    mc_global_profile = mc_config_init(gp_name);

    /* Load integer boolean options */
    for (i = 0; int_options[i].opt_name; i++)
	*int_options[i].opt_addr =
	    mc_config_get_int(mc_profile, CONFIG_APP_SECTION, int_options[i].opt_name, *int_options[i].opt_addr);

    /* Load string options */
    for (i = 0; str_options[i].opt_name != NULL; i++)
	*str_options[i].opt_addr =
	    mc_config_get_string (mc_profile, CONFIG_APP_SECTION, str_options[i].opt_name, str_options[i].opt_defval);

    load_layout ();

    load_panelize ();

    startup_left_mode = load_mode ("New Left Panel");
    startup_right_mode = load_mode ("New Right Panel");

    /* At least one of the panels is a listing panel */
    if (startup_left_mode != view_listing && startup_right_mode!=view_listing)
	startup_left_mode = view_listing;

    if (!other_dir){
	char *buffer;

	buffer = mc_config_get_string(mc_profile, "Dirs", "other_dir", ".");
	if (vfs_file_is_local (buffer))
	    other_dir = buffer;
	else
	    g_free (buffer);
    }

    boot_current_is_left = mc_config_get_int( mc_profile, "Dirs", "current_is_left", 1);

#ifdef USE_NETCODE
    ftpfs_proxy_host = mc_config_get_string(mc_profile, "Misc", "ftp_proxy_host", "gate");
#endif

    setup_color_string = mc_config_get_string(mc_profile, "Misc", "find_ignore_dirs", "");
    if (setup_color_string[0])
	find_ignore_dirs = g_strconcat (":", setup_color_string, ":", (char *) NULL);
    g_free(setup_color_string);

    /* The default color and the terminal dependent color */
    setup_color_string = mc_config_get_string(mc_profile, "Colors", "base_color", "");
    term_color_string = mc_config_get_string(mc_profile, "Colors", getenv ("TERM"), "");
    color_terminal_string = mc_config_get_string(mc_profile, "Colors", "color_terminals", "");

    /* Load the directory history */
/*    directory_history_load (); */
    /* Remove the temporal entries */
    mc_config_del_group (mc_profile, "Temporal:New Left Panel");
    mc_config_del_group (mc_profile, "Temporal:New Right Panel");
#if defined(USE_VFS) && defined (USE_NETCODE)
    ftpfs_init_passwd ();
#endif /* USE_VFS && USE_NETCODE */

#ifdef HAVE_CHARSET
    if ( load_codepages_list() > 0 ) {
	char *cpname;
	cpname = mc_config_get_string(mc_profile,  "Misc", "display_codepage", "");
	if ( cpname[0] != '\0' )
	{
	    display_codepage = get_codepage_index( cpname );
	    cp_display = get_codepage_id (display_codepage);
	}
	g_free(cpname);

	cpname = mc_config_get_string(mc_profile,  "Misc", "source_codepage", "");
	if ( cpname[0] != '\0' )
	{
	    source_codepage = get_codepage_index( cpname );
	    cp_source = get_codepage_id (source_codepage);
	}
	g_free(cpname);
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
    char *ret;

    buffer = mc_config_get_string(mc_profile, "Misc", "ftpfs_password", "");
    if (buffer [0])
	ret = g_strdup (buffer);
    else
	ret = NULL;
    g_free(buffer);
    return ret;
}
#endif /* USE_VFS && USE_NETCODE */

void done_setup (void)
{
    g_free(color_terminal_string);
    g_free(term_color_string);
    g_free(setup_color_string);

    mc_config_deinit(mc_profile);
    mc_config_deinit(mc_global_profile);
    done_hotlist ();
    done_panelize ();
/*    directory_history_free (); */
}

void load_key_defs (void)
{
    /*
     * Load keys from mc.lib before ~/.mc/ini, so that the user
     * definitions override global settings.
     */
    load_keys_from_section ("general", mc_global_profile);
    load_keys_from_section (getenv ("TERM"), mc_global_profile);
    load_keys_from_section ("general", mc_profile);
    load_keys_from_section (getenv ("TERM"), mc_profile);
}
