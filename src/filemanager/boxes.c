/*
   Some misc dialog boxes for the program.

   Copyright (C) 1994, 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2009, 2010, 2011, 2012, 2013
   The Free Software Foundation, Inc.

   Written by:
   Miguel de Icaza, 1994, 1995
   Jakub Jelinek, 1995
   Andrew Borodin <aborodin@vmail.ru>, 2009, 2010, 2011, 2012, 2013

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

/** \file boxes.c
 *  \brief Source: Some misc dialog boxes for the program
 */

#include <config.h>

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/key.h"        /* XCTRL and ALT macros  */
#include "lib/skin.h"           /* INPUT_COLOR */
#include "lib/mcconfig.h"       /* Load/save user formats */
#include "lib/strutil.h"

#include "lib/vfs/vfs.h"
#ifdef ENABLE_VFS_FTP
#include "src/vfs/ftpfs/ftpfs.h"
#endif /* ENABLE_VFS_FTP */
#ifdef ENABLE_VFS_SMB
#include "src/vfs/smbfs/smbfs.h"
#endif /* ENABLE_VFS_SMB */

#include "lib/util.h"           /* Q_() */
#include "lib/widget.h"

#include "src/setup.h"          /* For profile_name */
#include "src/history.h"        /* MC_HISTORY_ESC_TIMEOUT */
#include "src/execute.h"        /* pause_after_run */
#ifdef ENABLE_BACKGROUND
#include "src/background.h"     /* task_list */
#endif

#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#include "src/selcodepage.h"
#endif

#include "command.h"            /* For cmdline */
#include "dir.h"
#include "panel.h"              /* LIST_TYPES */
#include "tree.h"
#include "layout.h"             /* for get_nth_panel_name proto */
#include "midnight.h"           /* current_panel */

#include "boxes.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#ifdef ENABLE_BACKGROUND
#define B_STOP   (B_USER+1)
#define B_RESUME (B_USER+2)
#define B_KILL   (B_USER+3)
#endif /* ENABLE_BACKGROUND */

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

unsigned long configure_old_esc_mode_id, configure_time_out_id;

/* Index in list_types[] for "user defined" */
static const int panel_listing_user_idx = 3;

static char **status_format;
static int listing_user_hotkey = 'u';
static unsigned long panel_listing_types_id, panel_user_format_id;
static unsigned long mini_user_status_id, mini_user_format_id;

#ifdef HAVE_CHARSET
static int new_display_codepage;
static unsigned long disp_bits_name_id;
#endif /* HAVE_CHARSET */

#if defined(ENABLE_VFS) && defined(ENABLE_VFS_FTP)
static unsigned long ftpfs_always_use_proxy_id, ftpfs_proxy_host_id;
#endif /* ENABLE_VFS && ENABLE_VFS_FTP */

#ifdef ENABLE_BACKGROUND
static WListbox *bg_list = NULL;
#endif /* ENABLE_BACKGROUND */

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
configure_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_ACTION:
        /* message from "Single press" checkbutton */
        if (sender != NULL && sender->id == configure_old_esc_mode_id)
        {
            const gboolean not_single = !(CHECK (sender)->state & C_BOOL);
            Widget *ww;

            /* input line */
            ww = dlg_find_by_id (DIALOG (w), configure_time_out_id);
            widget_disable (ww, not_single);

            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
panel_listing_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WDialog *h = DIALOG (w);

    switch (msg)
    {
    case MSG_KEY:
        if (parm == '\n')
        {
            Widget *wi;

            wi = dlg_find_by_id (h, panel_listing_types_id);
            if (dlg_widget_active (wi))
            {
                WInput *in;

                in = INPUT (dlg_find_by_id (h, mini_user_format_id));
                input_assign_text (in, status_format[RADIO (wi)->sel]);
                dlg_stop (h);
                return MSG_HANDLED;
            }

            wi = dlg_find_by_id (h, panel_user_format_id);
            if (dlg_widget_active (wi))
            {
                h->ret_value = B_USER + 6;
                dlg_stop (h);
                return MSG_HANDLED;
            }

            wi = dlg_find_by_id (h, mini_user_format_id);
            if (dlg_widget_active (wi))
            {
                h->ret_value = B_USER + 7;
                dlg_stop (h);
                return MSG_HANDLED;
            }
        }

        if (g_ascii_tolower (parm) == listing_user_hotkey)
        {
            Widget *wi;

            wi = dlg_find_by_id (h, panel_user_format_id);
            if (dlg_widget_active (wi))
            {
                wi = dlg_find_by_id (h, mini_user_format_id);
                if (dlg_widget_active (wi))
                {
                    WRadio *r;

                    r = RADIO (dlg_find_by_id (h, panel_listing_types_id));
                    r->pos = r->sel = panel_listing_user_idx;
                    dlg_select_widget (WIDGET (r));     /* force redraw */
                    send_message (h, r, MSG_ACTION, 0, NULL);
                    return MSG_HANDLED;
                }
            }
        }
        return MSG_NOT_HANDLED;

    case MSG_ACTION:
        if (sender != NULL && sender->id == panel_listing_types_id)
        {
            WCheck *ch;
            WInput *in1, *in2;

            in1 = INPUT (dlg_find_by_id (h, panel_user_format_id));
            ch = CHECK (dlg_find_by_id (h, mini_user_status_id));
            in2 = INPUT (dlg_find_by_id (h, mini_user_format_id));

            if (!(ch->state & C_BOOL))
                input_assign_text (in2, status_format[RADIO (sender)->sel]);
            input_update (in2, FALSE);
            input_update (in1, FALSE);
            widget_disable (WIDGET (in1), RADIO (sender)->sel != panel_listing_user_idx);
            return MSG_HANDLED;
        }

        if (sender != NULL && sender->id == mini_user_status_id)
        {
            WInput *in;

            in = INPUT (dlg_find_by_id (h, mini_user_format_id));

            if (CHECK (sender)->state & C_BOOL)
            {
                widget_disable (WIDGET (in), FALSE);
                input_assign_text (in, status_format[3]);
            }
            else
            {
                WRadio *r;

                r = RADIO (dlg_find_by_id (h, panel_listing_types_id));
                widget_disable (WIDGET (in), TRUE);
                input_assign_text (in, status_format[r->sel]);
            }
            /* input_update (in, FALSE); */
            return MSG_HANDLED;
        }

        return MSG_NOT_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_CHARSET
static int
sel_charset_button (WButton * button, int action)
{
    int new_dcp;

    (void) action;

    new_dcp = select_charset (-1, -1, new_display_codepage, TRUE);

    if (new_dcp != SELECT_CHARSET_CANCEL)
    {
        const char *cpname;
        char buf[BUF_TINY];
        Widget *w;

        new_display_codepage = new_dcp;
        cpname = (new_display_codepage == SELECT_CHARSET_OTHER_8BIT) ?
            _("Other 8 bit") :
            ((codepage_desc *) g_ptr_array_index (codepages, new_display_codepage))->name;
        if (cpname != NULL)
            mc_global.utf8_display = str_isutf8 (cpname);
        /* avoid strange bug with label repainting */
        g_snprintf (buf, sizeof (buf), "%-27s", cpname);
        w = dlg_find_by_id (WIDGET (button)->owner, disp_bits_name_id);
        label_set_text (LABEL (w), buf);
    }

    return 0;
}
#endif /* HAVE_CHARSET */

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
tree_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WDialog *h = DIALOG (w);

    switch (msg)
    {
    case MSG_POST_KEY:
        /* The enter key will be processed by the tree widget */
        if (parm == '\n')
        {
            h->ret_value = B_ENTER;
            dlg_stop (h);
        }
        return MSG_HANDLED;

    case MSG_RESIZE:
        {
            Widget *bar;

            /* simply call dlg_set_size() with new size */
            dlg_set_size (h, LINES - 9, COLS - 20);
            bar = WIDGET (find_buttonbar (h));
            bar->x = 0;
            bar->y = LINES - 1;
            return MSG_HANDLED;
        }

    case MSG_ACTION:
        return send_message (find_tree (h), NULL, MSG_ACTION, parm, NULL);

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

#if defined(ENABLE_VFS) && defined (ENABLE_VFS_FTP)
static cb_ret_t
confvfs_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_ACTION:
        /* message from "Always use ftp proxy" checkbutton */
        if (sender != NULL && sender->id == ftpfs_always_use_proxy_id)
        {
            const gboolean not_use = !(CHECK (sender)->state & C_BOOL);
            Widget *wi;

            /* input */
            wi = dlg_find_by_id (DIALOG (w), ftpfs_proxy_host_id);
            widget_disable (wi, not_use);
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}
#endif /* ENABLE_VFS && ENABLE_VFS_FTP */

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_BACKGROUND
static void
jobs_fill_listbox (WListbox * list)
{
    static const char *state_str[2] = { "", "" };
    TaskList *tl;

    if (state_str[0] == '\0')
    {
        state_str[0] = _("Running");
        state_str[1] = _("Stopped");
    }

    for (tl = task_list; tl != NULL; tl = tl->next)
    {
        char *s;

        s = g_strconcat (state_str[tl->state], " ", tl->info, (char *) NULL);
        listbox_add_item (list, LISTBOX_APPEND_AT_END, 0, s, (void *) tl);
        g_free (s);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
task_cb (WButton * button, int action)
{
    TaskList *tl;
    int sig = 0;

    (void) button;

    if (bg_list->list == NULL)
        return 0;

    /* Get this instance information */
    listbox_get_current (bg_list, NULL, (void **) &tl);

#ifdef SIGTSTP
    if (action == B_STOP)
    {
        sig = SIGSTOP;
        tl->state = Task_Stopped;
    }
    else if (action == B_RESUME)
    {
        sig = SIGCONT;
        tl->state = Task_Running;
    }
    else
#endif
    if (action == B_KILL)
        sig = SIGKILL;

    if (sig == SIGKILL)
        unregister_task_running (tl->pid, tl->fd);

    kill (tl->pid, sig);
    listbox_remove_list (bg_list);
    jobs_fill_listbox (bg_list);

    /* This can be optimized to just redraw this widget :-) */
    dlg_redraw (WIDGET (button)->owner);

    return 0;
}
#endif /* ENABLE_BACKGROUND */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
configure_box (void)
{
    const char *pause_options[] = {
        N_("&Never"),
        N_("On dum&b terminals"),
        N_("Alwa&ys")
    };

    int pause_options_num;

    pause_options_num = G_N_ELEMENTS (pause_options);

    {
        char time_out[BUF_TINY] = "";
        char *time_out_new;

        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_START_COLUMNS,
                QUICK_START_GROUPBOX (N_("File operations")),
                    QUICK_CHECKBOX (N_("&Verbose operation"), &verbose, NULL),
                    QUICK_CHECKBOX (N_("Compute tota&ls"), &file_op_compute_totals, NULL),
                    QUICK_CHECKBOX (N_("Classic pro&gressbar"), &classic_progressbar, NULL),
                    QUICK_CHECKBOX (N_("Mkdi&r autoname"), &auto_fill_mkdir_name, NULL),
                    QUICK_CHECKBOX (N_("&Preallocate space"), &mc_global.vfs.preallocate_space,
                                    NULL),
                QUICK_STOP_GROUPBOX,
                QUICK_START_GROUPBOX (N_("Esc key mode")),
                    QUICK_CHECKBOX (N_("S&ingle press"), &old_esc_mode, &configure_old_esc_mode_id),
                    QUICK_LABELED_INPUT (N_("Timeout:"), input_label_left,
                                         (const char *) time_out, MC_HISTORY_ESC_TIMEOUT,
                                         &time_out_new, &configure_time_out_id, FALSE, FALSE,
                                         INPUT_COMPLETE_NONE),
                QUICK_STOP_GROUPBOX,
                QUICK_START_GROUPBOX (N_("Pause after run")),
                    QUICK_RADIO (pause_options_num, pause_options, &pause_after_run, NULL),
                QUICK_STOP_GROUPBOX,
            QUICK_NEXT_COLUMN,
                QUICK_START_GROUPBOX (N_("Other options")),
                    QUICK_CHECKBOX (N_("Use internal edi&t"), &use_internal_edit, NULL),
                    QUICK_CHECKBOX (N_("Use internal vie&w"), &use_internal_view, NULL),
                    QUICK_CHECKBOX (N_("A&sk new file name"),
                                    &editor_ask_filename_before_edit, NULL),
                    QUICK_CHECKBOX (N_("Auto m&enus"), &auto_menu, NULL),
                    QUICK_CHECKBOX (N_("&Drop down menus"), &drop_menus, NULL),
                    QUICK_CHECKBOX (N_("S&hell patterns"), &easy_patterns, NULL),
                    QUICK_CHECKBOX (N_("Co&mplete: show all"),
                                    &mc_global.widget.show_all_if_ambiguous, NULL),
                    QUICK_CHECKBOX (N_("Rotating d&ash"), &nice_rotating_dash, NULL),
                    QUICK_CHECKBOX (N_("Cd follows lin&ks"), &mc_global.vfs.cd_symlinks, NULL),
                    QUICK_CHECKBOX (N_("Sa&fe delete"), &safe_delete, NULL),
                    QUICK_CHECKBOX (N_("A&uto save setup"), &auto_save_setup, NULL),
                    QUICK_SEPARATOR (FALSE),
                    QUICK_SEPARATOR (FALSE),
                    QUICK_SEPARATOR (FALSE),
                QUICK_STOP_GROUPBOX,
            QUICK_STOP_COLUMNS,
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 60,
            N_("Configure options"), "[Configuration]",
            quick_widgets, configure_callback, NULL
        };

        g_snprintf (time_out, sizeof (time_out), "%d", old_esc_mode_timeout);

#ifndef USE_INTERNAL_EDIT
        quick_widgets[17].options = W_DISABLED;
#endif

        if (!old_esc_mode)
            quick_widgets[10].options = quick_widgets[11].options = W_DISABLED;

#ifndef HAVE_POSIX_FALLOCATE
        mc_global.vfs.preallocate_space = FALSE;
        quick_widgets[7].options = W_DISABLED;
#endif

        if (quick_dialog (&qdlg) == B_ENTER)
            old_esc_mode_timeout = atoi (time_out_new);

        g_free (time_out_new);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
panel_options_box (void)
{
    const char *qsearch_options[] = {
        N_("Case &insensitive"),
        N_("Cas&e sensitive"),
        N_("Use panel sort mo&de")
    };

    int simple_swap;

    simple_swap = mc_config_get_bool (mc_main_config, CONFIG_PANELS_SECTION,
                                      "simple_swap", FALSE) ? 1 : 0;

    {
        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_START_COLUMNS,
                QUICK_START_GROUPBOX (N_("Main options")),
                    QUICK_CHECKBOX (N_("Show mi&ni-status"), &panels_options.show_mini_info, NULL),
                    QUICK_CHECKBOX (N_("Use SI si&ze units"), &panels_options.kilobyte_si, NULL),
                    QUICK_CHECKBOX (N_("Mi&x all files"), &panels_options.mix_all_files, NULL),
                    QUICK_CHECKBOX (N_("Show &backup files"), &panels_options.show_backups, NULL),
                    QUICK_CHECKBOX (N_("Show &hidden files"), &panels_options.show_dot_files, NULL),
                    QUICK_CHECKBOX (N_("&Fast dir reload"), &panels_options.fast_reload, NULL),
                    QUICK_CHECKBOX (N_("Ma&rk moves down"), &panels_options.mark_moves_down, NULL),
                    QUICK_CHECKBOX (N_("Re&verse files only"), &panels_options.reverse_files_only,
                                    NULL),
                    QUICK_CHECKBOX (N_("Simple s&wap"), &simple_swap, NULL),
                    QUICK_CHECKBOX (N_("A&uto save panels setup"), &panels_options.auto_save_setup,
                                    NULL),
                    QUICK_SEPARATOR (FALSE),
                    QUICK_SEPARATOR (FALSE),
                QUICK_STOP_GROUPBOX,
            QUICK_NEXT_COLUMN,
                QUICK_START_GROUPBOX (N_("Navigation")),
                    QUICK_CHECKBOX (N_("L&ynx-like motion"), &panels_options.navigate_with_arrows,
                                    NULL),
                    QUICK_CHECKBOX (N_("Pa&ge scrolling"), &panels_options.scroll_pages, NULL),
                    QUICK_CHECKBOX (N_("&Mouse page scrolling"), &panels_options.mouse_move_pages,
                                    NULL),
                QUICK_STOP_GROUPBOX,
                QUICK_START_GROUPBOX (N_("File highlight")),
                    QUICK_CHECKBOX (N_("File &types"), &panels_options.filetype_mode, NULL),
                    QUICK_CHECKBOX (N_("&Permissions"), &panels_options.permission_mode, NULL),
                QUICK_STOP_GROUPBOX,
                QUICK_START_GROUPBOX (N_("Quick search")),
                    QUICK_RADIO (QSEARCH_NUM, qsearch_options, (int *) &panels_options.qsearch_mode,
                                 NULL),
                QUICK_STOP_GROUPBOX,
            QUICK_STOP_COLUMNS,
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 60,
            N_("Panel options"), "[Panel options]",
            quick_widgets, NULL, NULL
        };

        if (quick_dialog (&qdlg) != B_ENTER)
            return;
    }

    mc_config_set_bool (mc_main_config, CONFIG_PANELS_SECTION,
                        "simple_swap", (gboolean) (simple_swap & C_BOOL));

    if (!panels_options.fast_reload_msg_shown && panels_options.fast_reload)
    {
        message (D_NORMAL, _("Information"),
                 _("Using the fast reload option may not reflect the exact\n"
                   "directory contents. In this case you'll need to do a\n"
                   "manual reload of the directory. See the man page for\n" "the details."));
        panels_options.fast_reload_msg_shown = TRUE;
    }

    update_panels (UP_RELOAD, UP_KEEPSEL);
}

/* --------------------------------------------------------------------------------------------- */

/* return list type */
int
panel_listing_box (WPanel * panel, char **userp, char **minip, int *use_msformat, int num)
{
    int result = -1;
    char *section = NULL;

    if (panel == NULL)
    {
        const char *p;
        size_t i;

        p = get_nth_panel_name (num);
        panel = g_new (WPanel, 1);
        panel->list_type = list_full;
        panel->user_format = g_strdup (DEFAULT_USER_FORMAT);
        panel->user_mini_status = 0;
        for (i = 0; i < LIST_TYPES; i++)
            panel->user_status_format[i] = g_strdup (DEFAULT_USER_FORMAT);
        section = g_strconcat ("Temporal:", p, (char *) NULL);
        if (!mc_config_has_group (mc_main_config, section))
        {
            g_free (section);
            section = g_strdup (p);
        }
        panel_load_setup (panel, section);
        g_free (section);
    }

    {
        int mini_user_status;
        char *panel_user_format;
        char *mini_user_format;
        const char *cp;

        /* Controls whether the array strings have been translated */
        const char *list_types[LIST_TYPES] = {
            N_("&Full file list"),
            N_("&Brief file list"),
            N_("&Long file list"),
            N_("&User defined:")
        };

        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_RADIO (LIST_TYPES, list_types, &result, &panel_listing_types_id),
            QUICK_INPUT (panel->user_format, "user-fmt-input", &panel_user_format,
                         &panel_user_format_id, FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_SEPARATOR (TRUE),
            QUICK_CHECKBOX (N_("User &mini status"), &mini_user_status, &mini_user_status_id),
            QUICK_INPUT (panel->user_status_format[panel->list_type], "mini_input",
                         &mini_user_format, &mini_user_format_id, FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 48,
            N_("Listing mode"), "[Listing Mode...]",
            quick_widgets, panel_listing_callback, NULL
        };

        /* get hotkey of user-defined format string */
        cp = strchr (_(list_types[panel_listing_user_idx]), '&');
        if (cp != NULL && *++cp != '\0')
            listing_user_hotkey = g_ascii_tolower (*cp);

        mini_user_status = panel->user_mini_status;
        result = panel->list_type;
        status_format = panel->user_status_format;

        if (panel->list_type != panel_listing_user_idx)
            quick_widgets[1].options = W_DISABLED;

        if (!mini_user_status)
            quick_widgets[4].options = W_DISABLED;

        if (quick_dialog (&qdlg) == B_CANCEL)
            result = -1;
        else
        {
            *userp = panel_user_format;
            *minip = mini_user_format;
            *use_msformat = mini_user_status;
        }
    }

    if (section != NULL)
    {
        int i;

        g_free (panel->user_format);
        for (i = 0; i < LIST_TYPES; i++)
            g_free (panel->user_status_format[i]);
        g_free (panel);
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */

const panel_field_t *
sort_box (panel_sort_info_t * info)
{
    const char **sort_orders_names;
    gsize sort_names_num, i;
    int sort_idx = 0;
    const panel_field_t *result = info->sort_field;

    sort_orders_names = panel_get_sortable_fields (&sort_names_num);

    for (i = 0; i < sort_names_num; i++)
        if (strcmp (sort_orders_names[i], _(info->sort_field->title_hotkey)) == 0)
        {
            sort_idx = i;
            break;
        }

    {
        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_START_COLUMNS,
                QUICK_RADIO (sort_names_num, sort_orders_names, &sort_idx, NULL),
            QUICK_NEXT_COLUMN,
                QUICK_CHECKBOX (N_("Executable &first"), &info->exec_first, NULL),
                QUICK_CHECKBOX (N_("Cas&e sensitive"), &info->case_sensitive, NULL),
                QUICK_CHECKBOX (N_("&Reverse"), &info->reverse, NULL),
            QUICK_STOP_COLUMNS,
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 40,
            N_("Sort order"), "[Sort Order...]",
            quick_widgets, NULL, NULL
        };

        if (quick_dialog (&qdlg) != B_CANCEL)
            result = panel_get_field_by_title_hotkey (sort_orders_names[sort_idx]);

        if (result == NULL)
            result = info->sort_field;
    }

    g_strfreev ((gchar **) sort_orders_names);

    return result;
}

/* --------------------------------------------------------------------------------------------- */

void
confirm_box (void)
{
    quick_widget_t quick_widgets[] = {
        /* *INDENT-OFF* */
        /* TRANSLATORS: no need to translate 'Confirmation', it's just a context prefix */
        QUICK_CHECKBOX (N_("Confirmation|&Delete"), &confirm_delete, NULL),
        QUICK_CHECKBOX (N_("Confirmation|O&verwrite"), &confirm_overwrite, NULL),
        QUICK_CHECKBOX (N_("Confirmation|&Execute"), &confirm_execute, NULL),
        QUICK_CHECKBOX (N_("Confirmation|E&xit"), &confirm_exit, NULL),
        QUICK_CHECKBOX (N_("Confirmation|Di&rectory hotlist delete"),
                        &confirm_directory_hotlist_delete, NULL),
        QUICK_CHECKBOX (N_("Confirmation|&History cleanup"),
                        &mc_global.widget.confirm_history_cleanup, NULL),
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END
        /* *INDENT-ON* */
    };

    quick_dialog_t qdlg = {
        -1, -1, 46,
        N_("Confirmation"), "[Confirmation]",
        quick_widgets, NULL, NULL
    };

    (void) quick_dialog (&qdlg);
}

/* --------------------------------------------------------------------------------------------- */

#ifndef HAVE_CHARSET
void
display_bits_box (void)
{
    int new_meta;
    int current_mode;

    const char *display_bits_str[] = {
        N_("&UTF-8 output"),
        N_("&Full 8 bits output"),
        N_("&ISO 8859-1"),
        N_("7 &bits")
    };

    quick_widget_t quick_widgets[] = {
        /* *INDENT-OFF* */
        QUICK_RADIO (4, display_bits_str, &current_mode, NULL),
        QUICK_SEPARATOR (TRUE),
        QUICK_CHECKBOX (N_("F&ull 8 bits input"), &new_meta, NULL),
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END
        /* *INDENT-ON* */
    };

    quick_dialog_t qdlg = {
        -1, -1, 46,
        _("Display bits"), "[Display bits]",
        quick_widgets, NULL, NULL
    };

    if (mc_global.full_eight_bits)
        current_mode = 0;
    else if (mc_global.eight_bit_clean)
        current_mode = 1;
    else
        current_mode = 2;

    new_meta = !use_8th_bit_as_meta;

    if (quick_dialog (&qdlg) != B_CANCEL)
    {
        mc_global.eight_bit_clean = current_mode < 3;
        mc_global.full_eight_bits = current_mode < 2;
#ifndef HAVE_SLANG
        meta (stdscr, mc_global.eight_bit_clean);
#else
        SLsmg_Display_Eight_Bit = mc_global.full_eight_bits ? 128 : 160;
#endif
        use_8th_bit_as_meta = !new_meta;
    }
}

/* --------------------------------------------------------------------------------------------- */
#else /* HAVE_CHARSET */

void
display_bits_box (void)
{
    const char *cpname;

    cpname = (new_display_codepage < 0) ? _("Other 8 bit")
        : ((codepage_desc *) g_ptr_array_index (codepages, new_display_codepage))->name;

    new_display_codepage = mc_global.display_codepage;

    {
        int new_meta;

        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_START_COLUMNS,
                QUICK_LABEL (N_("Input / display codepage:"), NULL),
            QUICK_NEXT_COLUMN,
            QUICK_STOP_COLUMNS,
            QUICK_START_COLUMNS,
                QUICK_LABEL (cpname, &disp_bits_name_id),
            QUICK_NEXT_COLUMN,
                QUICK_BUTTON (N_("&Select"), B_USER, sel_charset_button, NULL),
            QUICK_STOP_COLUMNS,
            QUICK_SEPARATOR (TRUE),
                QUICK_CHECKBOX (N_("F&ull 8 bits input"), &new_meta, NULL),
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 46,
            N_("Display bits"), "[Display bits]",
            quick_widgets, NULL, NULL
        };

        new_meta = !use_8th_bit_as_meta;
        application_keypad_mode ();

        if (quick_dialog (&qdlg) == B_ENTER)
        {
            char *errmsg;

            mc_global.display_codepage = new_display_codepage;

            errmsg = init_translation_table (mc_global.source_codepage, mc_global.display_codepage);
            if (errmsg != NULL)
            {
                message (D_ERROR, MSG_ERROR, "%s", errmsg);
                g_free (errmsg);
            }

#ifdef HAVE_SLANG
            tty_display_8bit (mc_global.display_codepage != 0 && mc_global.display_codepage != 1);
#else
            tty_display_8bit (mc_global.display_codepage != 0);
#endif
            use_8th_bit_as_meta = !new_meta;

            repaint_screen ();
        }
    }
}
#endif /* HAVE_CHARSET */

/* --------------------------------------------------------------------------------------------- */
/** Show tree in a box, not on a panel */

char *
tree_box (const char *current_dir)
{
    WTree *mytree;
    WDialog *dlg;
    Widget *wd;
    char *val = NULL;
    WButtonBar *bar;

    (void) current_dir;

    /* Create the components */
    dlg = create_dlg (TRUE, 0, 0, LINES - 9, COLS - 20, dialog_colors, tree_callback, NULL,
                      "[Directory Tree]", _("Directory tree"), DLG_CENTER);
    wd = WIDGET (dlg);

    mytree = tree_new (2, 2, wd->lines - 6, wd->cols - 5, FALSE);
    add_widget_autopos (dlg, mytree, WPOS_KEEP_ALL, NULL);
    add_widget_autopos (dlg, hline_new (wd->lines - 4, 1, -1), WPOS_KEEP_BOTTOM, NULL);
    bar = buttonbar_new (TRUE);
    add_widget (dlg, bar);
    /* restore ButtonBar coordinates after add_widget() */
    WIDGET (bar)->x = 0;
    WIDGET (bar)->y = LINES - 1;

    if (run_dlg (dlg) == B_ENTER)
        val = vfs_path_to_str (tree_selected_name (mytree));

    destroy_dlg (dlg);
    return val;
}

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_VFS
void
configure_vfs (void)
{
    char buffer2[BUF_TINY];
#ifdef ENABLE_VFS_FTP
    char buffer3[BUF_TINY];

    g_snprintf (buffer3, sizeof (buffer3), "%i", ftpfs_directory_timeout);
#endif

    g_snprintf (buffer2, sizeof (buffer2), "%i", vfs_timeout);

    {
        char *ret_timeout;
#ifdef ENABLE_VFS_FTP
        char *ret_passwd;
        char *ret_ftp_proxy;
        char *ret_directory_timeout;
#endif /* ENABLE_VFS_FTP */

        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_LABELED_INPUT (N_("Timeout for freeing VFSs (sec):"), input_label_left,
                                 buffer2, "input-timo-vfs", &ret_timeout, NULL, FALSE, FALSE,
                                 INPUT_COMPLETE_NONE),
#ifdef ENABLE_VFS_FTP
            QUICK_SEPARATOR (TRUE),
            QUICK_LABELED_INPUT (N_("FTP anonymous password:"), input_label_left,
                                 ftpfs_anonymous_passwd, "input-passwd", &ret_passwd, NULL,
                                 FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_LABELED_INPUT (N_("FTP directory cache timeout (sec):"), input_label_left,
                                 buffer3, "input-timeout", &ret_directory_timeout, NULL,
                                 FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_CHECKBOX (N_("&Always use ftp proxy:"), &ftpfs_always_use_proxy,
                            &ftpfs_always_use_proxy_id),
            QUICK_INPUT (ftpfs_proxy_host, "input-ftp-proxy", &ret_ftp_proxy,
                         &ftpfs_proxy_host_id, FALSE, FALSE, INPUT_COMPLETE_HOSTNAMES),
            QUICK_CHECKBOX (N_("&Use ~/.netrc"), &ftpfs_use_netrc, NULL),
            QUICK_CHECKBOX (N_("Use &passive mode"), &ftpfs_use_passive_connections, NULL),
            QUICK_CHECKBOX (N_("Use passive mode over pro&xy"),
                            &ftpfs_use_passive_connections_over_proxy, NULL),
#endif /* ENABLE_VFS_FTP */
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 56,
            N_("Virtual File System Setting"), "[Virtual FS]",
            quick_widgets,
#ifdef ENABLE_VFS_FTP
            confvfs_callback,
#else
            NULL,
#endif
            NULL,
        };

#ifdef ENABLE_VFS_FTP
        if (!ftpfs_always_use_proxy)
            quick_widgets[5].options = W_DISABLED;
#endif

        if (quick_dialog (&qdlg) != B_CANCEL)
        {
            vfs_timeout = atoi (ret_timeout);
            g_free (ret_timeout);

            if (vfs_timeout < 0 || vfs_timeout > 10000)
                vfs_timeout = 10;
#ifdef ENABLE_VFS_FTP
            g_free (ftpfs_anonymous_passwd);
            ftpfs_anonymous_passwd = ret_passwd;
            g_free (ftpfs_proxy_host);
            ftpfs_proxy_host = ret_ftp_proxy;
            ftpfs_directory_timeout = atoi (ret_directory_timeout);
            g_free (ret_directory_timeout);
#endif
        }
    }
}

#endif /* ENABLE_VFS */

/* --------------------------------------------------------------------------------------------- */

char *
cd_dialog (void)
{
    const Widget *w = WIDGET (current_panel);
    char *my_str;

    quick_widget_t quick_widgets[] = {
        QUICK_LABELED_INPUT (N_("cd"), input_label_left, "", "input", &my_str, NULL, FALSE, TRUE,
                             INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_CD),
        QUICK_END
    };

    quick_dialog_t qdlg = {
        w->y + w->lines - 6, w->x, w->cols,
        N_("Quick cd"), "[Quick cd]",
        quick_widgets, NULL, NULL
    };

    return (quick_dialog (&qdlg) != B_CANCEL) ? my_str : NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
symlink_dialog (const vfs_path_t * existing_vpath, const vfs_path_t * new_vpath,
                char **ret_existing, char **ret_new)
{
    char *existing;
    char *new;

    existing = vfs_path_to_str (existing_vpath);
    new = vfs_path_to_str (new_vpath);

    {
        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_LABELED_INPUT (N_("Existing filename (filename symlink will point to):"),
                                 input_label_above,
                                 existing, "input-2", ret_existing, NULL, FALSE, FALSE, INPUT_COMPLETE_FILENAMES),
            QUICK_SEPARATOR (FALSE),
            QUICK_LABELED_INPUT (N_("Symbolic link filename:"), input_label_above,
                                 new, "input-1", ret_new, NULL, FALSE, FALSE, INPUT_COMPLETE_FILENAMES),
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 64,
            N_("Symbolic link"), "[File Menu]",
            quick_widgets, NULL, NULL
        };

        if (quick_dialog (&qdlg) == B_CANCEL)
        {
            *ret_new = NULL;
            *ret_existing = NULL;
        }
    }

    g_free (existing);
    g_free (new);
}

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_BACKGROUND
void
jobs_cmd (void)
{
    struct
    {
        const char *name;
        int flags;
        int value;
        int len;
        bcback_fn callback;
    }
    job_but[] =
    {
        /* *INDENT-OFF* */
        { N_("&Stop"), NORMAL_BUTTON, B_STOP, 0, task_cb },
        { N_("&Resume"), NORMAL_BUTTON, B_RESUME, 0, task_cb },
        { N_("&Kill"), NORMAL_BUTTON, B_KILL, 0, task_cb },
        { N_("&OK"), DEFPUSH_BUTTON, B_CANCEL, 0, NULL }
        /* *INDENT-ON* */
    };

    size_t i;
    const size_t n_but = G_N_ELEMENTS (job_but);

    WDialog *jobs_dlg;
    int cols = 60;
    int lines = 15;
    int x = 0;

    for (i = 0; i < n_but; i++)
    {
#ifdef ENABLE_NLS
        job_but[i].name = _(job_but[i].name);
#endif /* ENABLE_NLS */

        job_but[i].len = str_term_width1 (job_but[i].name) + 3;
        if (job_but[i].flags == DEFPUSH_BUTTON)
            job_but[i].len += 2;
        x += job_but[i].len;
    }

    x += (int) n_but - 1;
    cols = max (cols, x + 6);

    jobs_dlg = create_dlg (TRUE, 0, 0, lines, cols, dialog_colors, NULL, NULL,
                           "[Background jobs]", _("Background jobs"), DLG_CENTER);

    bg_list = listbox_new (2, 2, lines - 6, cols - 6, FALSE, NULL);
    jobs_fill_listbox (bg_list);
    add_widget (jobs_dlg, bg_list);

    add_widget (jobs_dlg, hline_new (lines - 4, -1, -1));

    x = (cols - x) / 2;
    for (i = 0; i < n_but; i++)
    {
        add_widget (jobs_dlg,
                    button_new (lines - 3, x, job_but[i].value, job_but[i].flags, job_but[i].name,
                                job_but[i].callback));
        x += job_but[i].len + 1;
    }

    (void) run_dlg (jobs_dlg);
    destroy_dlg (jobs_dlg);
}
#endif /* ENABLE_BACKGROUND */

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_VFS_SMB
struct smb_authinfo *
vfs_smb_get_authinfo (const char *host, const char *share, const char *domain, const char *user)
{
    char *label;
    struct smb_authinfo *return_value = NULL;

    if (domain == NULL)
        domain = "";
    if (user == NULL)
        user = "";

    label = g_strdup_printf (_("Password for \\\\%s\\%s"), host, share);

    {
        char *ret_domain, *ret_user, *ret_password;

        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_LABEL (label, NULL),
            QUICK_SEPARATOR (TRUE),
            QUICK_START_COLUMNS,
                QUICK_LABEL (N_("Domain:"), NULL),
                QUICK_SEPARATOR (FALSE),
                QUICK_LABEL (N_("Username:"), NULL),
                QUICK_SEPARATOR (FALSE),
                QUICK_LABEL (N_("Password:"), NULL),
            QUICK_NEXT_COLUMN,
                QUICK_INPUT (domain, "auth_domain", &ret_domain, NULL, FALSE, FALSE, INPUT_COMPLETE_HOSTNAMES),
                QUICK_SEPARATOR (FALSE),
                QUICK_INPUT (user, "auth_name", &ret_user, NULL, FALSE, FALSE, INPUT_COMPLETE_USERNAMES),
                QUICK_SEPARATOR (FALSE),
                QUICK_INPUT ("", "auth_password", &ret_password, NULL, TRUE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_STOP_COLUMNS,
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 40,
            N_("SMB authentication"), "[Smb Authinfo]",
            quick_widgets, NULL, NULL
        };

        if (quick_dialog (&qdlg) != B_CANCEL)
        {
            return_value = vfs_smb_authinfo_new (host, share, ret_domain, ret_user, ret_password);

            g_free (ret_domain);
            g_free (ret_user);
            g_free (ret_password);
        }
    }

    g_free (label);

    return return_value;
}
#endif /* ENABLE_VFS_SMB */

/* --------------------------------------------------------------------------------------------- */
