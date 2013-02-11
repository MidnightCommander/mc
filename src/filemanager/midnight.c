/*
   Main dialog (file panels) of the Midnight Commander

   Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
   2003, 2004, 2005, 2006, 2007, 2009, 2010, 2011
   The Free Software Foundation, Inc.

   Written by:
   Miguel de Icaza, 1994, 1995, 1996, 1997
   Janne Kukonlehto, 1994, 1995
   Norbert Warmuth, 1997
   Andrew Borodin <aborodin@vmail.ru>, 2009, 2010

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file main.c
 *  \brief Source: main dialog (file panels) of the Midnight Commander
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
#include <pwd.h>                /* for username in xterm title */

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/key.h"        /* KEY_M_* masks */
#include "lib/skin.h"
#include "lib/util.h"

#include "lib/vfs/vfs.h"

#include "src/args.h"
#ifdef ENABLE_SUBSHELL
#include "src/subshell.h"
#endif
#include "src/setup.h"          /* variables */
#include "src/learn.h"          /* learn_keys() */
#include "src/keybind-defaults.h"
#include "lib/keybind.h"
#include "lib/event.h"

#include "tree.h"
#include "boxes.h"              /* sort_box(), tree_box() */
#include "layout.h"
#include "cmd.h"                /* commands */
#include "hotlist.h"
#include "panelize.h"
#include "command.h"            /* cmdline */
#include "dir.h"                /* clean_dir() */

#include "chmod.h"
#include "chown.h"
#include "achown.h"

#ifdef USE_INTERNAL_EDIT
#include "src/editor/edit.h"
#endif

#ifdef USE_DIFF_VIEW
#include "src/diffviewer/ydiff.h"
#endif

#include "src/consaver/cons.saver.h"    /* show_console_contents */

#include "midnight.h"

/* TODO: merge content of layout.c here */
extern int ok_to_refresh;

/*** global variables ****************************************************************************/

/* When the modes are active, left_panel, right_panel and tree_panel */
/* point to a proper data structure.  You should check with the functions */
/* get_current_type and get_other_type the types of the panels before using */
/* this pointer variables */

/* The structures for the panels */
WPanel *left_panel = NULL;
WPanel *right_panel = NULL;
/* Pointer to the selected and unselected panel */
WPanel *current_panel = NULL;

/* The Menubar */
WMenuBar *the_menubar = NULL;
/* The widget where we draw the prompt */
WLabel *the_prompt;
/* The hint bar */
WLabel *the_hint;
/* The button bar */
WButtonBar *the_bar;

/* The prompt */
const char *mc_prompt = NULL;

/*** file scope macro definitions ****************************************************************/

#ifdef HAVE_CHARSET
/*
 * Don't restrict the output on the screen manager level,
 * the translation tables take care of it.
 */
#endif /* !HAVE_CHARSET */

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static menu_t *left_menu, *right_menu;

static gboolean ctl_x_map_enabled = FALSE;

/*** file scope functions ************************************************************************/

/** Stop MC main dialog and the current dialog if it exists.
  * Needed to provide fast exit from MC viewer or editor on shell exit */
static void
stop_dialogs (void)
{
    midnight_dlg->state = DLG_CLOSED;

    if ((top_dlg != NULL) && (top_dlg->data != NULL))
        DIALOG (top_dlg->data)->state = DLG_CLOSED;
}

/* --------------------------------------------------------------------------------------------- */

static void
treebox_cmd (void)
{
    char *sel_dir;

    sel_dir = tree_box (selection (current_panel)->fname);
    if (sel_dir != NULL)
    {
        vfs_path_t *sel_vdir;

        sel_vdir = vfs_path_from_str (sel_dir);
        do_cd (sel_vdir, cd_exact);
        vfs_path_free (sel_vdir);
        g_free (sel_dir);
    }
}

/* --------------------------------------------------------------------------------------------- */

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
#endif /* LISTMODE_EDITOR */

/* --------------------------------------------------------------------------------------------- */

static GList *
create_panel_menu (void)
{
    GList *entries = NULL;

    entries = g_list_prepend (entries, menu_entry_create (_("File listin&g"), CK_PanelListing));
    entries = g_list_prepend (entries, menu_entry_create (_("&Quick view"), CK_PanelQuickView));
    entries = g_list_prepend (entries, menu_entry_create (_("&Info"), CK_PanelInfo));
    entries = g_list_prepend (entries, menu_entry_create (_("&Tree"), CK_PanelTree));
    entries = g_list_prepend (entries, menu_separator_create ());
    entries =
        g_list_prepend (entries, menu_entry_create (_("&Listing mode..."), CK_PanelListingChange));
    entries = g_list_prepend (entries, menu_entry_create (_("&Sort order..."), CK_Sort));
    entries = g_list_prepend (entries, menu_entry_create (_("&Filter..."), CK_Filter));
#ifdef HAVE_CHARSET
    entries = g_list_prepend (entries, menu_entry_create (_("&Encoding..."), CK_SelectCodepage));
#endif
    entries = g_list_prepend (entries, menu_separator_create ());
#ifdef ENABLE_VFS_FTP
    entries = g_list_prepend (entries, menu_entry_create (_("FT&P link..."), CK_ConnectFtp));
#endif
#ifdef ENABLE_VFS_FISH
    entries = g_list_prepend (entries, menu_entry_create (_("S&hell link..."), CK_ConnectFish));
#endif
#ifdef ENABLE_VFS_SFTP
    entries = g_list_prepend (entries, menu_entry_create (_("S&FTP link..."), CK_ConnectSftp));
#endif
#ifdef ENABLE_VFS_SMB
    entries = g_list_prepend (entries, menu_entry_create (_("SM&B link..."), CK_ConnectSmb));
#endif
    entries = g_list_prepend (entries, menu_entry_create (_("Paneli&ze"), CK_Panelize));
    entries = g_list_prepend (entries, menu_separator_create ());
    entries = g_list_prepend (entries, menu_entry_create (_("&Rescan"), CK_Reread));

    return g_list_reverse (entries);
}

/* --------------------------------------------------------------------------------------------- */

static GList *
create_file_menu (void)
{
    GList *entries = NULL;

    entries = g_list_prepend (entries, menu_entry_create (_("&View"), CK_View));
    entries = g_list_prepend (entries, menu_entry_create (_("Vie&w file..."), CK_ViewFile));
    entries = g_list_prepend (entries, menu_entry_create (_("&Filtered view"), CK_ViewFiltered));
    entries = g_list_prepend (entries, menu_entry_create (_("&Edit"), CK_Edit));
    entries = g_list_prepend (entries, menu_entry_create (_("&Copy"), CK_Copy));
    entries = g_list_prepend (entries, menu_entry_create (_("C&hmod"), CK_ChangeMode));
    entries = g_list_prepend (entries, menu_entry_create (_("&Link"), CK_Link));
    entries = g_list_prepend (entries, menu_entry_create (_("&Symlink"), CK_LinkSymbolic));
    entries =
        g_list_prepend (entries,
                        menu_entry_create (_("Relative symlin&k"), CK_LinkSymbolicRelative));
    entries = g_list_prepend (entries, menu_entry_create (_("Edit s&ymlink"), CK_LinkSymbolicEdit));
    entries = g_list_prepend (entries, menu_entry_create (_("Ch&own"), CK_ChangeOwn));
    entries =
        g_list_prepend (entries, menu_entry_create (_("&Advanced chown"), CK_ChangeOwnAdvanced));
    entries = g_list_prepend (entries, menu_entry_create (_("&Rename/Move"), CK_Move));
    entries = g_list_prepend (entries, menu_entry_create (_("&Mkdir"), CK_MakeDir));
    entries = g_list_prepend (entries, menu_entry_create (_("&Delete"), CK_Delete));
    entries = g_list_prepend (entries, menu_entry_create (_("&Quick cd"), CK_CdQuick));
    entries = g_list_prepend (entries, menu_separator_create ());
    entries = g_list_prepend (entries, menu_entry_create (_("Select &group"), CK_Select));
    entries = g_list_prepend (entries, menu_entry_create (_("U&nselect group"), CK_Unselect));
    entries = g_list_prepend (entries, menu_entry_create (_("&Invert selection"), CK_SelectInvert));
    entries = g_list_prepend (entries, menu_separator_create ());
    entries = g_list_prepend (entries, menu_entry_create (_("E&xit"), CK_Quit));

    return g_list_reverse (entries);
}

/* --------------------------------------------------------------------------------------------- */

static GList *
create_command_menu (void)
{
    /* I know, I'm lazy, but the tree widget when it's not running
     * as a panel still has some problems, I have not yet finished
     * the WTree widget port, sorry.
     */
    GList *entries = NULL;

    entries = g_list_prepend (entries, menu_entry_create (_("&User menu"), CK_UserMenu));
    entries = g_list_prepend (entries, menu_entry_create (_("&Directory tree"), CK_Tree));
    entries = g_list_prepend (entries, menu_entry_create (_("&Find file"), CK_Find));
    entries = g_list_prepend (entries, menu_entry_create (_("S&wap panels"), CK_Swap));
    entries = g_list_prepend (entries, menu_entry_create (_("Switch &panels on/off"), CK_Shell));
    entries =
        g_list_prepend (entries, menu_entry_create (_("&Compare directories"), CK_CompareDirs));
#ifdef USE_DIFF_VIEW
    entries = g_list_prepend (entries, menu_entry_create (_("C&ompare files"), CK_CompareFiles));
#endif
    entries =
        g_list_prepend (entries, menu_entry_create (_("E&xternal panelize"), CK_ExternalPanelize));
    entries = g_list_prepend (entries, menu_entry_create (_("Show directory s&izes"), CK_DirSize));
    entries = g_list_prepend (entries, menu_separator_create ());
    entries = g_list_prepend (entries, menu_entry_create (_("Command &history"), CK_History));
    entries = g_list_prepend (entries, menu_entry_create (_("Di&rectory hotlist"), CK_HotList));
#ifdef ENABLE_VFS
    entries = g_list_prepend (entries, menu_entry_create (_("&Active VFS list"), CK_VfsList));
#endif
#ifdef ENABLE_BACKGROUND
    entries = g_list_prepend (entries, menu_entry_create (_("&Background jobs"), CK_Jobs));
#endif
    entries = g_list_prepend (entries, menu_entry_create (_("Screen lis&t"), CK_ScreenList));
    entries = g_list_prepend (entries, menu_separator_create ());
#ifdef ENABLE_VFS_UNDELFS
    entries =
        g_list_prepend (entries,
                        menu_entry_create (_("&Undelete files (ext2fs only)"), CK_Undelete));
#endif
#ifdef LISTMODE_EDITOR
    entries = g_list_prepend (entries, menu_entry_create (_("&Listing format edit"), CK_ListMode));
#endif
#if defined (ENABLE_VFS_UNDELFS) || defined (LISTMODE_EDITOR)
    entries = g_list_prepend (entries, menu_separator_create ());
#endif
    entries =
        g_list_prepend (entries,
                        menu_entry_create (_("Edit &extension file"), CK_EditExtensionsFile));
    entries = g_list_prepend (entries, menu_entry_create (_("Edit &menu file"), CK_EditUserMenu));
    entries =
        g_list_prepend (entries,
                        menu_entry_create (_("Edit hi&ghlighting group file"),
                                           CK_EditFileHighlightFile));

    return g_list_reverse (entries);
}

/* --------------------------------------------------------------------------------------------- */

static GList *
create_options_menu (void)
{
    GList *entries = NULL;

    entries = g_list_prepend (entries, menu_entry_create (_("&Configuration..."), CK_Options));
    entries = g_list_prepend (entries, menu_entry_create (_("&Layout..."), CK_OptionsLayout));
    entries = g_list_prepend (entries, menu_entry_create (_("&Panel options..."), CK_OptionsPanel));
    entries =
        g_list_prepend (entries, menu_entry_create (_("C&onfirmation..."), CK_OptionsConfirm));
    entries =
        g_list_prepend (entries, menu_entry_create (_("&Display bits..."), CK_OptionsDisplayBits));
    entries = g_list_prepend (entries, menu_entry_create (_("Learn &keys..."), CK_LearnKeys));
#ifdef ENABLE_VFS
    entries = g_list_prepend (entries, menu_entry_create (_("&Virtual FS..."), CK_OptionsVfs));
#endif
    entries = g_list_prepend (entries, menu_separator_create ());
    entries = g_list_prepend (entries, menu_entry_create (_("&Save setup"), CK_SaveSetup));

    return g_list_reverse (entries);
}

/* --------------------------------------------------------------------------------------------- */

static void
init_menu (void)
{
    left_menu = create_menu ("", create_panel_menu (), "[Left and Right Menus]");
    menubar_add_menu (the_menubar, left_menu);
    menubar_add_menu (the_menubar, create_menu (_("&File"), create_file_menu (), "[File Menu]"));
    menubar_add_menu (the_menubar,
                      create_menu (_("&Command"), create_command_menu (), "[Command Menu]"));
    menubar_add_menu (the_menubar,
                      create_menu (_("&Options"), create_options_menu (), "[Options Menu]"));
    right_menu = create_menu ("", create_panel_menu (), "[Left and Right Menus]");
    menubar_add_menu (the_menubar, right_menu);
    update_menu ();
}

/* --------------------------------------------------------------------------------------------- */

static void
menu_last_selected_cmd (void)
{
    the_menubar->is_active = TRUE;
    the_menubar->is_dropped = (drop_menus != 0);
    the_menubar->previous_widget = dlg_get_current_widget_id (midnight_dlg);
    dlg_select_widget (the_menubar);
}

/* --------------------------------------------------------------------------------------------- */

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

/* --------------------------------------------------------------------------------------------- */

static void
sort_cmd (void)
{
    WPanel *p;
    const panel_field_t *sort_order;

    if (!SELECTED_IS_PANEL)
        return;

    p = MENU_PANEL;
    sort_order = sort_box (&p->sort_info);
    panel_set_sort_order (p, sort_order);
}

/* --------------------------------------------------------------------------------------------- */

static char *
midnight_get_shortcut (unsigned long command)
{
    const char *ext_map;
    const char *shortcut = NULL;

    shortcut = keybind_lookup_keymap_shortcut (main_map, command);
    if (shortcut != NULL)
        return g_strdup (shortcut);

    shortcut = keybind_lookup_keymap_shortcut (panel_map, command);
    if (shortcut != NULL)
        return g_strdup (shortcut);

    ext_map = keybind_lookup_keymap_shortcut (main_map, CK_ExtendedKeyMap);
    if (ext_map != NULL)
        shortcut = keybind_lookup_keymap_shortcut (main_x_map, command);
    if (shortcut != NULL)
        return g_strdup_printf ("%s %s", ext_map, shortcut);

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static char *
midnight_get_title (const WDialog * h, size_t len)
{
    char *path;
    char *login;
    char *p;

    (void) h;

    title_path_prepare (&path, &login);

    p = g_strdup_printf ("%s [%s]:%s", _("Panels:"), login, path);
    g_free (path);
    g_free (login);
    path = g_strdup (str_trunc (p, len - 4));
    g_free (p);

    return path;
}

/* --------------------------------------------------------------------------------------------- */

static void
toggle_panels_split (void)
{
    panels_layout.horizontal_split = !panels_layout.horizontal_split;
    layout_change ();
    do_refresh ();
}

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_VFS
/* event helper */
static gboolean
check_panel_timestamp (const WPanel * panel, panel_view_mode_t mode, struct vfs_class *vclass,
                       vfsid id)
{
    if (mode == view_listing)
    {
        const vfs_path_element_t *path_element;

        path_element = vfs_path_get_by_index (panel->cwd_vpath, -1);

        if (path_element->class != vclass)
            return FALSE;

        if (vfs_getid (panel->cwd_vpath) != id)
            return FALSE;
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* event callback */
static gboolean
check_current_panel_timestamp (const gchar * event_group_name, const gchar * event_name,
                               gpointer init_data, gpointer data)
{
    ev_vfs_stamp_create_t *event_data = (ev_vfs_stamp_create_t *) data;

    (void) event_group_name;
    (void) event_name;
    (void) init_data;

    event_data->ret =
        check_panel_timestamp (current_panel, get_current_type (), event_data->vclass,
                               event_data->id);
    return !event_data->ret;
}

/* --------------------------------------------------------------------------------------------- */

/* event callback */
static gboolean
check_other_panel_timestamp (const gchar * event_group_name, const gchar * event_name,
                             gpointer init_data, gpointer data)
{
    ev_vfs_stamp_create_t *event_data = (ev_vfs_stamp_create_t *) data;

    (void) event_group_name;
    (void) event_name;
    (void) init_data;

    event_data->ret =
        check_panel_timestamp (other_panel, get_other_type (), event_data->vclass, event_data->id);
    return !event_data->ret;
}
#endif /* ENABLE_VFS */

/* --------------------------------------------------------------------------------------------- */

/* event callback */
static gboolean
print_vfs_message (const gchar * event_group_name, const gchar * event_name,
                   gpointer init_data, gpointer data)
{
    char str[128];
    ev_vfs_print_message_t *event_data = (ev_vfs_print_message_t *) data;

    (void) event_group_name;
    (void) event_name;
    (void) init_data;

    g_vsnprintf (str, sizeof (str), event_data->msg, event_data->ap);

    if (mc_global.midnight_shutdown)
        return TRUE;

    if (!mc_global.message_visible || the_hint == NULL || WIDGET (the_hint)->owner == NULL)
    {
        int col, row;

        if (!nice_rotating_dash || (ok_to_refresh <= 0))
            return TRUE;

        /* Preserve current cursor position */
        tty_getyx (&row, &col);

        tty_gotoyx (0, 0);
        tty_setcolor (NORMAL_COLOR);
        tty_print_string (str_fit_to_term (str, COLS - 1, J_LEFT));

        /* Restore cursor position */
        tty_gotoyx (row, col);
        mc_refresh ();
        return TRUE;
    }

    if (mc_global.message_visible)
        set_hintbar (str);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
create_panels (void)
{
    int current_index, other_index;
    panel_view_mode_t current_mode, other_mode;
    char *current_dir, *other_dir;
    vfs_path_t *original_dir;

    if (boot_current_is_left)
    {
        /* left panel is active */
        current_index = 0;
        other_index = 1;
        current_mode = startup_left_mode;
        other_mode = startup_right_mode;
        /* if mc_run_param0 is NULL, working directory will be used for the left panel */
        current_dir = (char *) mc_run_param0;
        /* mc_run_param1 is never NULL. It is setup from command line or from panels.ini
         * (value of other_dir). mc_run_param1 will be used for the right panel */
        other_dir = mc_run_param1;
    }
    else
    {
        /* right panel is active */
        current_index = 1;
        other_index = 0;
        current_mode = startup_right_mode;
        other_mode = startup_left_mode;

        /* if mc_run_param0 is not NULL (it was setup from command line), it will be used
         * for the left panel, working directory will be used for the right one;
         * if mc_run_param0 is NULL, working directory will be used for the right (active) panel,
         * mc_run_param1 will be used for the left one */
        if (mc_run_param0 != NULL)
        {
            current_dir = NULL;
            other_dir = (char *) mc_run_param0;
        }
        else
        {
            current_dir = NULL;
            other_dir = mc_run_param1;
        }
    }

    /* 1. Get current dir */
    original_dir = vfs_path_clone (vfs_get_raw_current_dir ());

    /* 2. Create passive panel */
    if (other_dir != NULL)
    {
        vfs_path_t *vpath;

        if (g_path_is_absolute (other_dir))
            vpath = vfs_path_from_str (other_dir);
        else
            vpath = vfs_path_append_new (original_dir, other_dir, (char *) NULL);
        mc_chdir (vpath);
        vfs_path_free (vpath);
    }
    set_display_type (other_index, other_mode);

    /* 3. Create active panel */
    if (current_dir == NULL)
        mc_chdir (original_dir);
    else
    {
        vfs_path_t *vpath;

        if (g_path_is_absolute (current_dir))
            vpath = vfs_path_from_str (current_dir);
        else
            vpath = vfs_path_append_new (original_dir, current_dir, (char *) NULL);
        mc_chdir (vpath);
        vfs_path_free (vpath);
    }
    set_display_type (current_index, current_mode);

    if (startup_left_mode == view_listing)
        current_panel = left_panel;
    else if (right_panel != NULL)
        current_panel = right_panel;
    else
        current_panel = left_panel;

    vfs_path_free (original_dir);

#ifdef ENABLE_VFS
    mc_event_add (MCEVENT_GROUP_CORE, "vfs_timestamp", check_other_panel_timestamp, NULL, NULL);
    mc_event_add (MCEVENT_GROUP_CORE, "vfs_timestamp", check_current_panel_timestamp, NULL, NULL);
#endif /* ENABLE_VFS */

    mc_event_add (MCEVENT_GROUP_CORE, "vfs_print_message", print_vfs_message, NULL, NULL);

    /* Create the nice widgets */
    cmdline = command_new (0, 0, 0);
    the_prompt = label_new (0, 0, mc_prompt);
    the_prompt->transparent = 1;
    the_bar = buttonbar_new (mc_global.keybar_visible);

    the_hint = label_new (0, 0, 0);
    the_hint->transparent = 1;
    the_hint->auto_adjust_cols = 0;
    WIDGET (the_hint)->cols = COLS;

    the_menubar = menubar_new (0, 0, COLS, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
put_current_path (void)
{
    char *cwd_path;

    if (!command_prompt)
        return;

#ifdef HAVE_CHARSET
    {
        vfs_path_t *cwd_vpath;

        cwd_vpath = remove_encoding_from_path (current_panel->cwd_vpath);
        cwd_path = vfs_path_to_str (cwd_vpath);
        vfs_path_free (cwd_vpath);
    }
#else
    cwd_path = vfs_path_to_str (current_panel->cwd_vpath);
#endif

    command_insert (cmdline, cwd_path, FALSE);
    if (cwd_path[strlen (cwd_path) - 1] != PATH_SEP)
        command_insert (cmdline, PATH_SEP_STR, FALSE);

    g_free (cwd_path);
}

/* --------------------------------------------------------------------------------------------- */

static void
put_other_path (void)
{
    char *cwd_path;

    if (get_other_type () != view_listing)
        return;

    if (!command_prompt)
        return;

#ifdef HAVE_CHARSET
    {
        vfs_path_t *cwd_vpath;

        cwd_vpath = remove_encoding_from_path (other_panel->cwd_vpath);
        cwd_path = vfs_path_to_str (cwd_vpath);
        vfs_path_free (cwd_vpath);
    }
#else
    cwd_path = vfs_path_to_str (other_panel->cwd_vpath);
#endif

    command_insert (cmdline, cwd_path, FALSE);
    if (cwd_path[strlen (cwd_path) - 1] != PATH_SEP)
        command_insert (cmdline, PATH_SEP_STR, FALSE);

    g_free (cwd_path);
}

/* --------------------------------------------------------------------------------------------- */

static void
put_link (WPanel * panel)
{
    if (!command_prompt)
        return;
    if (S_ISLNK (selection (panel)->st.st_mode))
    {
        char buffer[MC_MAXPATHLEN];
        vfs_path_t *vpath;
        int i;

        vpath = vfs_path_append_new (panel->cwd_vpath, selection (panel)->fname, NULL);
        i = mc_readlink (vpath, buffer, MC_MAXPATHLEN - 1);
        vfs_path_free (vpath);

        if (i > 0)
        {
            buffer[i] = '\0';
            command_insert (cmdline, buffer, TRUE);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
put_current_link (void)
{
    put_link (current_panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
put_other_link (void)
{
    if (get_other_type () == view_listing)
        put_link (other_panel);
}

/* --------------------------------------------------------------------------------------------- */

/** Insert the selected file name into the input line */
static void
put_prog_name (void)
{
    char *tmp;
    if (!command_prompt)
        return;

    if (get_current_type () == view_tree)
    {
        WTree *tree = (WTree *) get_panel_widget (get_current_index ());

        tmp = vfs_path_to_str (tree_selected_name (tree));
    }
    else
        tmp = g_strdup (selection (current_panel)->fname);

    command_insert (cmdline, tmp, TRUE);
    g_free (tmp);
}

/* --------------------------------------------------------------------------------------------- */

static void
put_tagged (WPanel * panel)
{
    int i;

    if (!command_prompt)
        return;
    input_disable_update (cmdline);
    if (panel->marked)
    {
        for (i = 0; i < panel->count; i++)
        {
            if (panel->dir.list[i].f.marked)
                command_insert (cmdline, panel->dir.list[i].fname, TRUE);
        }
    }
    else
    {
        command_insert (cmdline, panel->dir.list[panel->selected].fname, TRUE);
    }
    input_enable_update (cmdline);
}

/* --------------------------------------------------------------------------------------------- */

static void
put_current_tagged (void)
{
    put_tagged (current_panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
put_other_tagged (void)
{
    if (get_other_type () == view_listing)
        put_tagged (other_panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
ctl_x_cmd (void)
{
    ctl_x_map_enabled = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
setup_mc (void)
{
#ifdef HAVE_SLANG
#ifdef HAVE_CHARSET
    tty_display_8bit (TRUE);
#else
    tty_display_8bit (mc_global.full_eight_bits != 0);
#endif /* HAVE_CHARSET */

#else /* HAVE_SLANG */

#ifdef HAVE_CHARSET
    tty_display_8bit (TRUE);
#else
    tty_display_8bit (mc_global.eight_bit_clean != 0);
#endif /* HAVE_CHARSET */
#endif /* HAVE_SLANG */

#ifdef ENABLE_SUBSHELL
    if (mc_global.tty.use_subshell)
        add_select_channel (mc_global.tty.subshell_pty, load_prompt, 0);
#endif /* !ENABLE_SUBSHELL */

    if ((tty_baudrate () < 9600) || mc_global.tty.slow_terminal)
        verbose = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
setup_dummy_mc (void)
{
    vfs_path_t *vpath;
    char *d;
    int ret;

    d = _vfs_get_cwd ();
    setup_mc ();
    vpath = vfs_path_from_str (d);
    ret = mc_chdir (vpath);
    (void) ret;
    vfs_path_free (vpath);
    g_free (d);
}

/* --------------------------------------------------------------------------------------------- */

static void
done_mc (void)
{
    /* Setup shutdown
     *
     * We sync the profiles since the hotlist may have changed, while
     * we only change the setup data if we have the auto save feature set
     */
    char *curr_dir;

    save_setup (auto_save_setup, panels_options.auto_save_setup);

    curr_dir = vfs_get_current_dir ();
    vfs_stamp_path (curr_dir);
    g_free (curr_dir);

    if ((current_panel != NULL) && (get_current_type () == view_listing))
    {
        char *tmp_path;

        tmp_path = vfs_path_to_str (current_panel->cwd_vpath);
        vfs_stamp_path (tmp_path);
        g_free (tmp_path);
    }

    if ((other_panel != NULL) && (get_other_type () == view_listing))
    {
        char *tmp_path;

        tmp_path = vfs_path_to_str (other_panel->cwd_vpath);
        vfs_stamp_path (tmp_path);
        g_free (tmp_path);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
create_panels_and_run_mc (void)
{
    midnight_dlg->get_shortcut = midnight_get_shortcut;
    midnight_dlg->get_title = midnight_get_title;

    create_panels ();

    add_widget (midnight_dlg, the_menubar);
    init_menu ();

    add_widget (midnight_dlg, get_panel_widget (0));
    add_widget (midnight_dlg, get_panel_widget (1));

    add_widget (midnight_dlg, the_hint);
    add_widget (midnight_dlg, cmdline);
    add_widget (midnight_dlg, the_prompt);

    add_widget (midnight_dlg, the_bar);
    midnight_set_buttonbar (the_bar);

    /* Run the Midnight Commander if no file was specified in the command line */
    run_dlg (midnight_dlg);
}

/* --------------------------------------------------------------------------------------------- */

/** result must be free'd (I think this should go in util.c) */
static vfs_path_t *
prepend_cwd_on_local (const char *filename)
{
    vfs_path_t *vpath;

    vpath = vfs_path_from_str (filename);
    if (!vfs_file_is_local (vpath) || g_path_is_absolute (filename))
        return vpath;

    vfs_path_free (vpath);

    return vfs_path_append_new (vfs_get_raw_current_dir (), filename, NULL);
}

/* --------------------------------------------------------------------------------------------- */

/** Invoke the internal view/edit routine with:
 * the default processing and forcing the internal viewer/editor
 */
static gboolean
mc_maybe_editor_or_viewer (void)
{
    gboolean ret;

    switch (mc_global.mc_run_mode)
    {
#ifdef USE_INTERNAL_EDIT
    case MC_RUN_EDITOR:
        ret = edit_files ((GList *) mc_run_param0);
        break;
#endif /* USE_INTERNAL_EDIT */
    case MC_RUN_VIEWER:
        {
            vfs_path_t *vpath = NULL;

            if (mc_run_param0 != NULL && *(char *) mc_run_param0 != '\0')
                vpath = prepend_cwd_on_local ((char *) mc_run_param0);

            ret = view_file (vpath, 0, 1);
            vfs_path_free (vpath);
            break;
        }
#ifdef USE_DIFF_VIEW
    case MC_RUN_DIFFVIEWER:
        ret = dview_diff_cmd (mc_run_param0, mc_run_param1);
        break;
#endif /* USE_DIFF_VIEW */
    default:
        ret = FALSE;
    }

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
quit_cmd_internal (int quiet)
{
    int q = quit;
    size_t n;

    n = dialog_switch_num () - 1;
    if (n != 0)
    {
        char msg[BUF_MEDIUM];

        g_snprintf (msg, sizeof (msg),
                    ngettext ("You have %zd opened screen. Quit anyway?",
                              "You have %zd opened screens. Quit anyway?", n), n);

        if (query_dialog (_("The Midnight Commander"), msg, D_NORMAL, 2, _("&Yes"), _("&No")) != 0)
            return FALSE;
        q = 1;
    }
    else if (quiet || !confirm_exit)
        q = 1;
    else if (query_dialog (_("The Midnight Commander"),
                           _("Do you really want to quit the Midnight Commander?"),
                           D_NORMAL, 2, _("&Yes"), _("&No")) == 0)
        q = 1;

    if (q != 0)
    {
#ifdef ENABLE_SUBSHELL
        if (!mc_global.tty.use_subshell)
            stop_dialogs ();
        else if ((q = exit_subshell ()? 1 : 0) != 0)
#endif
            stop_dialogs ();
    }

    if (q != 0)
        quit |= 1;
    return (quit != 0);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
quit_cmd (void)
{
    return quit_cmd_internal (0);
}

/* --------------------------------------------------------------------------------------------- */

static void
toggle_show_hidden (void)
{
    panels_options.show_dot_files = !panels_options.show_dot_files;
    update_panels (UP_RELOAD, UP_KEEPSEL);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Repaint the contents of the panels without frames.  To schedule panel
 * for repainting, set panel->dirty to 1.  There are many reasons why
 * the panels need to be repainted, and this is a costly operation, so
 * it's done once per event.
 */

static void
update_dirty_panels (void)
{
    if (get_current_type () == view_listing && current_panel->dirty)
        widget_redraw (WIDGET (current_panel));

    if (get_other_type () == view_listing && other_panel->dirty)
        widget_redraw (WIDGET (other_panel));
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
midnight_execute_cmd (Widget * sender, unsigned long command)
{
    cb_ret_t res = MSG_HANDLED;

    (void) sender;

    /* stop quick search before executing any command */
    send_message (current_panel, NULL, MSG_ACTION, CK_SearchStop, NULL);

    switch (command)
    {
    case CK_HotListAdd:
        add2hotlist_cmd ();
        break;
    case CK_PanelListingChange:
        change_listing_cmd ();
        break;
    case CK_ChangeMode:
        chmod_cmd ();
        break;
    case CK_ChangeOwn:
        chown_cmd ();
        break;
    case CK_ChangeOwnAdvanced:
        chown_advanced_cmd ();
        break;
    case CK_CompareDirs:
        compare_dirs_cmd ();
        break;
    case CK_Options:
        configure_box ();
        break;
#ifdef ENABLE_VFS
    case CK_OptionsVfs:
        configure_vfs ();
        break;
#endif
    case CK_OptionsConfirm:
        confirm_box ();
        break;
    case CK_Copy:
        copy_cmd ();
        break;
    case CK_PutCurrentPath:
        put_current_path ();
        break;
    case CK_PutCurrentLink:
        put_current_link ();
        break;
    case CK_PutCurrentTagged:
        put_current_tagged ();
        break;
    case CK_PutOtherPath:
        put_other_path ();
        break;
    case CK_PutOtherLink:
        put_other_link ();
        break;
    case CK_PutOtherTagged:
        put_other_tagged ();
        break;
    case CK_Delete:
        delete_cmd ();
        break;
    case CK_ScreenList:
        dialog_switch_list ();
        break;
#ifdef USE_DIFF_VIEW
    case CK_CompareFiles:
        diff_view_cmd ();
        break;
#endif
    case CK_OptionsDisplayBits:
        display_bits_box ();
        break;
    case CK_Edit:
        edit_cmd ();
        break;
#ifdef USE_INTERNAL_EDIT
    case CK_EditForceInternal:
        edit_cmd_force_internal ();
        break;
#endif
    case CK_EditExtensionsFile:
        ext_cmd ();
        break;
    case CK_EditFileHighlightFile:
        edit_fhl_cmd ();
        break;
    case CK_EditUserMenu:
        edit_mc_menu_cmd ();
        break;
    case CK_LinkSymbolicEdit:
        edit_symlink_cmd ();
        break;
    case CK_ExternalPanelize:
        external_panelize ();
        break;
    case CK_Filter:
        filter_cmd ();
        break;
    case CK_ViewFiltered:
        view_filtered_cmd ();
        break;
    case CK_Find:
        find_cmd ();
        break;
#ifdef ENABLE_VFS_FISH
    case CK_ConnectFish:
        fishlink_cmd ();
        break;
#endif
#ifdef ENABLE_VFS_FTP
    case CK_ConnectFtp:
        ftplink_cmd ();
        break;
#endif
#ifdef ENABLE_VFS_SFTP
    case CK_ConnectSftp:
        sftplink_cmd ();
        break;
#endif
#ifdef ENABLE_VFS_SMB
    case CK_ConnectSmb:
        smblink_cmd ();
        break;
#endif /* ENABLE_VFS_SMB */
    case CK_Panelize:
        cd_panelize_cmd ();
        break;
    case CK_Help:
        help_cmd ();
        break;
    case CK_History:
        /* show the history of command line widget */
        send_message (cmdline, NULL, MSG_ACTION, CK_History, NULL);
        break;
    case CK_PanelInfo:
        if (sender == WIDGET (the_menubar))
            info_cmd ();        /* menu */
        else
            info_cmd_no_menu ();        /* shortcut or buttonbar */
        break;
#ifdef ENABLE_BACKGROUND
    case CK_Jobs:
        jobs_cmd ();
        break;
#endif
    case CK_OptionsLayout:
        layout_box ();
        break;
    case CK_LearnKeys:
        learn_keys ();
        break;
    case CK_Link:
        link_cmd (LINK_HARDLINK);
        break;
    case CK_PanelListing:
        listing_cmd ();
        break;
#ifdef LISTMODE_EDITOR
    case CK_ListMode:
        listmode_cmd ();
        break;
#endif
    case CK_Menu:
        menu_cmd ();
        break;
    case CK_MenuLastSelected:
        menu_last_selected_cmd ();
        break;
    case CK_MakeDir:
        mkdir_cmd ();
        break;
    case CK_OptionsPanel:
        panel_options_box ();
        break;
#ifdef HAVE_CHARSET
    case CK_SelectCodepage:
        encoding_cmd ();
        break;
#endif
    case CK_CdQuick:
        quick_cd_cmd ();
        break;
    case CK_HotList:
        hotlist_cmd ();
        break;
    case CK_PanelQuickView:
        if (sender == WIDGET (the_menubar))
            quick_view_cmd ();  /* menu */
        else
            quick_cmd_no_menu ();       /* shortcut or buttonabr */
        break;
    case CK_QuitQuiet:
        quiet_quit_cmd ();
        break;
    case CK_Quit:
        quit_cmd ();
        break;
    case CK_LinkSymbolicRelative:
        link_cmd (LINK_SYMLINK_RELATIVE);
        break;
    case CK_Move:
        rename_cmd ();
        break;
    case CK_Reread:
        reread_cmd ();
        break;
#ifdef ENABLE_VFS
    case CK_VfsList:
        vfs_list ();
        break;
#endif
    case CK_SelectInvert:
        select_invert_cmd ();
        break;
    case CK_SaveSetup:
        save_setup_cmd ();
        break;
    case CK_Select:
        select_cmd ();
        break;
    case CK_Shell:
        view_other_cmd ();
        break;
    case CK_DirSize:
        smart_dirsize_cmd ();
        break;
    case CK_Sort:
        sort_cmd ();
        break;
    case CK_ExtendedKeyMap:
        ctl_x_cmd ();
        break;
    case CK_Suspend:
        mc_event_raise (MCEVENT_GROUP_CORE, "suspend", NULL);
        break;
    case CK_Swap:
        swap_cmd ();
        break;
    case CK_LinkSymbolic:
        link_cmd (LINK_SYMLINK_ABSOLUTE);
        break;
    case CK_PanelListingSwitch:
        toggle_listing_cmd ();
        break;
    case CK_ShowHidden:
        toggle_show_hidden ();
        break;
    case CK_SplitVertHoriz:
        toggle_panels_split ();
        break;
    case CK_SplitEqual:
        panels_split_equal ();
        break;
    case CK_SplitMore:
        panels_split_more ();
        break;
    case CK_SplitLess:
        panels_split_less ();
        break;
    case CK_PanelTree:
        panel_tree_cmd ();
        break;
    case CK_Tree:
        treebox_cmd ();
        break;
#ifdef ENABLE_VFS_UNDELFS
    case CK_Undelete:
        undelete_cmd ();
        break;
#endif
    case CK_Unselect:
        unselect_cmd ();
        break;
    case CK_UserMenu:
        user_file_menu_cmd ();
        break;
    case CK_View:
        view_cmd ();
        break;
    case CK_ViewFile:
        view_file_cmd ();
        break;
    case CK_Cancel:
        /* don't close panels due to SIGINT */
        break;
    default:
        res = MSG_NOT_HANDLED;
    }

    return res;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
midnight_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    unsigned long command;

    switch (msg)
    {
    case MSG_INIT:
        panel_init ();
        setup_panels ();
        return MSG_HANDLED;

    case MSG_DRAW:
        load_hint (1);
        /* We handle the special case of the output lines */
        if (mc_global.tty.console_flag != '\0' && output_lines)
            show_console_contents (output_start_y,
                                   LINES - output_lines - mc_global.keybar_visible -
                                   1, LINES - mc_global.keybar_visible - 1);
        return MSG_HANDLED;

    case MSG_RESIZE:
        setup_panels ();
        menubar_arrange (the_menubar);
        return MSG_HANDLED;

    case MSG_IDLE:
        /* We only need the first idle event to show user menu after start */
        widget_want_idle (w, FALSE);

        if (boot_current_is_left)
            dlg_select_widget (get_panel_widget (0));
        else
            dlg_select_widget (get_panel_widget (1));

        if (auto_menu)
            midnight_execute_cmd (NULL, CK_UserMenu);
        return MSG_HANDLED;

    case MSG_KEY:
        if (ctl_x_map_enabled)
        {
            ctl_x_map_enabled = FALSE;
            command = keybind_lookup_keymap_command (main_x_map, parm);
            if (command != CK_IgnoreKey)
                return midnight_execute_cmd (NULL, command);
        }

        /* FIXME: should handle all menu shortcuts before this point */
        if (the_menubar->is_active)
            return MSG_NOT_HANDLED;

        if (parm == '\t')
            input_free_completions (cmdline);

        if (parm == '\n')
        {
            size_t i;

            for (i = 0; cmdline->buffer[i] != '\0' &&
                 (cmdline->buffer[i] == ' ' || cmdline->buffer[i] == '\t'); i++)
                ;

            if (cmdline->buffer[i] != '\0')
            {
                send_message (cmdline, NULL, MSG_KEY, parm, NULL);
                return MSG_HANDLED;
            }

            input_insert (cmdline, "", FALSE);
            cmdline->point = 0;
        }

        /* Ctrl-Enter and Alt-Enter */
        if (((parm & ~(KEY_M_CTRL | KEY_M_ALT)) == '\n') && (parm & (KEY_M_CTRL | KEY_M_ALT)))
        {
            put_prog_name ();
            return MSG_HANDLED;
        }

        /* Ctrl-Shift-Enter */
        if (parm == (KEY_M_CTRL | KEY_M_SHIFT | '\n'))
        {
            put_current_path ();
            put_prog_name ();
            return MSG_HANDLED;
        }

        if ((!mc_global.tty.alternate_plus_minus
             || !(mc_global.tty.console_flag != '\0' || mc_global.tty.xterm_flag)) && !quote
            && !current_panel->searching)
        {
            if (!only_leading_plus_minus)
            {
                /* Special treatement, since the input line will eat them */
                if (parm == '+')
                {
                    select_cmd ();
                    return MSG_HANDLED;
                }

                if (parm == '\\' || parm == '-')
                {
                    unselect_cmd ();
                    return MSG_HANDLED;
                }

                if (parm == '*')
                {
                    select_invert_cmd ();
                    return MSG_HANDLED;
                }
            }
            else if (!command_prompt || !cmdline->buffer[0])
            {
                /* Special treatement '+', '-', '\', '*' only when this is
                 * first char on input line
                 */

                if (parm == '+')
                {
                    select_cmd ();
                    return MSG_HANDLED;
                }

                if (parm == '\\' || parm == '-')
                {
                    unselect_cmd ();
                    return MSG_HANDLED;
                }

                if (parm == '*')
                {
                    select_invert_cmd ();
                    return MSG_HANDLED;
                }
            }
        }
        return MSG_NOT_HANDLED;

    case MSG_HOTKEY_HANDLED:
        if ((get_current_type () == view_listing) && current_panel->searching)
        {
            current_panel->dirty = 1;   /* FIXME: unneeded? */
            send_message (current_panel, NULL, MSG_ACTION, CK_SearchStop, NULL);
        }
        return MSG_HANDLED;

    case MSG_UNHANDLED_KEY:
        {
            cb_ret_t v = MSG_NOT_HANDLED;

            if (ctl_x_map_enabled)
            {
                ctl_x_map_enabled = FALSE;
                command = keybind_lookup_keymap_command (main_x_map, parm);
            }
            else
                command = keybind_lookup_keymap_command (main_map, parm);

            if (command != CK_IgnoreKey)
                v = midnight_execute_cmd (NULL, command);

            if (v == MSG_NOT_HANDLED && command_prompt)
                v = send_message (cmdline, NULL, MSG_KEY, parm, NULL);

            return v;
        }

    case MSG_POST_KEY:
        if (!the_menubar->is_active)
            update_dirty_panels ();
        return MSG_HANDLED;

    case MSG_ACTION:
        /* shortcut */
        if (sender == NULL)
            return midnight_execute_cmd (NULL, parm);
        /* message from menu */
        if (sender == WIDGET (the_menubar))
            return midnight_execute_cmd (sender, parm);
        /* message from buttonbar */
        if (sender == WIDGET (the_bar))
        {
            if (data != NULL)
                return send_message (data, NULL, MSG_ACTION, parm, NULL);
            return midnight_execute_cmd (sender, parm);
        }
        return MSG_NOT_HANDLED;

    case MSG_END:
        panel_deinit ();
        return MSG_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
midnight_event (Gpm_Event * event, void *data)
{
    Widget *wh = WIDGET (data);
    int ret = MOU_UNHANDLED;

    if (event->y == wh->y + 1)
    {
        /* menubar */
        if (menubar_visible || the_menubar->is_active)
            ret = WIDGET (the_menubar)->mouse (event, the_menubar);
        else
        {
            Widget *w;

            w = get_panel_widget (0);
            if (w->mouse != NULL)
                ret = w->mouse (event, w);

            if (ret == MOU_UNHANDLED)
            {
                w = get_panel_widget (1);
                if (w->mouse != NULL)
                    ret = w->mouse (event, w);
            }

            if (ret == MOU_UNHANDLED)
                ret = WIDGET (the_menubar)->mouse (event, the_menubar);
        }
    }

    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
update_menu (void)
{
    menu_set_name (left_menu, panels_layout.horizontal_split ? _("&Above") : _("&Left"));
    menu_set_name (right_menu, panels_layout.horizontal_split ? _("&Below") : _("&Right"));
    menubar_arrange (the_menubar);
    menubar_set_visible (the_menubar, menubar_visible);
}

/* --------------------------------------------------------------------------------------------- */

void
midnight_set_buttonbar (WButtonBar * b)
{
    buttonbar_set_label (b, 1, Q_ ("ButtonBar|Help"), main_map, NULL);
    buttonbar_set_label (b, 2, Q_ ("ButtonBar|Menu"), main_map, NULL);
    buttonbar_set_label (b, 3, Q_ ("ButtonBar|View"), main_map, NULL);
    buttonbar_set_label (b, 4, Q_ ("ButtonBar|Edit"), main_map, NULL);
    buttonbar_set_label (b, 5, Q_ ("ButtonBar|Copy"), main_map, NULL);
    buttonbar_set_label (b, 6, Q_ ("ButtonBar|RenMov"), main_map, NULL);
    buttonbar_set_label (b, 7, Q_ ("ButtonBar|Mkdir"), main_map, NULL);
    buttonbar_set_label (b, 8, Q_ ("ButtonBar|Delete"), main_map, NULL);
    buttonbar_set_label (b, 9, Q_ ("ButtonBar|PullDn"), main_map, NULL);
    buttonbar_set_label (b, 10, Q_ ("ButtonBar|Quit"), main_map, NULL);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Load new hint and display it.
 * IF force is not 0, ignore the timeout.
 */

void
load_hint (gboolean force)
{
    char *hint;

    if (WIDGET (the_hint)->owner == NULL)
        return;

    if (!mc_global.message_visible)
    {
        label_set_text (the_hint, NULL);
        return;
    }

    hint = get_random_hint (force);

    if (hint != NULL)
    {
        if (*hint != '\0')
            set_hintbar (hint);
        g_free (hint);
    }
    else
    {
        char text[BUF_SMALL];

        g_snprintf (text, sizeof (text), _("GNU Midnight Commander %s\n"), VERSION);
        set_hintbar (text);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
change_panel (void)
{
    input_free_completions (cmdline);
    dlg_one_down (midnight_dlg);
}

/* --------------------------------------------------------------------------------------------- */

/** Save current stat of directories to avoid reloading the panels
 * when no modifications have taken place
 */
void
save_cwds_stat (void)
{
    if (panels_options.fast_reload)
    {
        mc_stat (current_panel->cwd_vpath, &(current_panel->dir_stat));
        if (get_other_type () == view_listing)
            mc_stat (other_panel->cwd_vpath, &(other_panel->dir_stat));
    }
}

/* --------------------------------------------------------------------------------------------- */

gboolean
quiet_quit_cmd (void)
{
    print_last_revert = TRUE;
    return quit_cmd_internal (1);
}

/* --------------------------------------------------------------------------------------------- */

/** Run the main dialog that occupies the whole screen */
gboolean
do_nc (void)
{
    gboolean ret;

    dlg_colors_t midnight_colors;

    midnight_colors[DLG_COLOR_NORMAL] = mc_skin_color_get ("dialog", "_default_");
    midnight_colors[DLG_COLOR_FOCUS] = mc_skin_color_get ("dialog", "focus");
    midnight_colors[DLG_COLOR_HOT_NORMAL] = mc_skin_color_get ("dialog", "hotnormal");
    midnight_colors[DLG_COLOR_HOT_FOCUS] = mc_skin_color_get ("dialog", "hotfocus");
    midnight_colors[DLG_COLOR_TITLE] = mc_skin_color_get ("dialog", "title");

#ifdef USE_INTERNAL_EDIT
    edit_stack_init ();
#endif

    midnight_dlg = create_dlg (FALSE, 0, 0, LINES, COLS, midnight_colors, midnight_callback,
                               midnight_event, "[main]", NULL, DLG_NONE);

    /* Check if we were invoked as an editor or file viewer */
    if (mc_global.mc_run_mode != MC_RUN_FULL)
    {
        setup_dummy_mc ();
        ret = mc_maybe_editor_or_viewer ();
    }
    else
    {
        /* We only need the first idle event to show user menu after start */
        widget_want_idle (WIDGET (midnight_dlg), TRUE);

        setup_mc ();
        mc_filehighlight = mc_fhl_new (TRUE);
        create_panels_and_run_mc ();
        mc_fhl_free (&mc_filehighlight);

        ret = TRUE;

        /* destroy_dlg destroys even current_panel->cwd_vpath, so we have to save a copy :) */
        if (mc_args__last_wd_file != NULL && vfs_current_is_local ())
            last_wd_string = vfs_path_to_str (current_panel->cwd_vpath);

        /* don't handle VFS timestamps for dirs opened in panels */
        mc_event_destroy (MCEVENT_GROUP_CORE, "vfs_timestamp");

        clean_dir (&panelized_panel.list, panelized_panel.count);
    }

    /* Program end */
    mc_global.midnight_shutdown = TRUE;
    dialog_switch_shutdown ();
    done_mc ();
    destroy_dlg (midnight_dlg);
    current_panel = NULL;

#ifdef USE_INTERNAL_EDIT
    edit_stack_free ();
#endif

    if ((quit & SUBSHELL_EXIT) == 0)
        clr_scr ();

    return ret;
}

/* --------------------------------------------------------------------------------------------- */
