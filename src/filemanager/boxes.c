/*
   Some misc dialog boxes for the program.

   Copyright (C) 1994, 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2009, 2010, 2011
   The Free Software Foundation, Inc.

   Written by:
   Miguel de Icaza, 1994, 1995
   Jakub Jelinek, 1995

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

#include "boxes.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#ifdef ENABLE_VFS
#define VFSX 56

#ifdef ENABLE_VFS_FTP
#define VFSY 17
#else
#define VFSY 8
#endif /* ENABLE_VFS_FTP */
#endif /* ENABLE_VFS */

#ifdef ENABLE_BACKGROUND
#define B_STOP   (B_USER+1)
#define B_RESUME (B_USER+2)
#define B_KILL   (B_USER+3)
#define JOBS_Y 15
#endif /* ENABLE_BACKGROUND */

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static WRadio *display_radio;
static WInput *display_user_format;
static WInput *display_mini_status;
static WCheck *display_check_status;
static char **displays_status;
static int display_user_hotkey = 'u';

#ifdef HAVE_CHARSET
static int new_display_codepage;
static WLabel *cplabel;
static WCheck *inpcheck;
#endif /* HAVE_CHARSET */

#ifdef ENABLE_VFS
static char *ret_timeout;
#ifdef ENABLE_VFS_FTP
static char *ret_ftp_proxy;
static char *ret_passwd;
static char *ret_directory_timeout;
#endif /* ENABLE_VFS_FTP */
#endif /* ENABLE_VFS */

#ifdef ENABLE_BACKGROUND
static int JOBS_X = 60;
static WListbox *bg_list;
static Dlg_head *jobs_dlg;

static int task_cb (WButton * button, int action);

static struct
{
    const char *name;
    int xpos;
    int value;
    bcback_fn callback;
}
job_buttons[] =
{
    /* *INDENT-OFF* */
    { N_("&Stop"), 3, B_STOP, task_cb },
    { N_("&Resume"), 12, B_RESUME, task_cb },
    { N_("&Kill"), 23, B_KILL, task_cb },
    { N_("&OK"), 35, B_CANCEL, NULL }
    /* *INDENT-ON* */
};

#endif /* ENABLE_BACKGROUND */

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
display_callback (Dlg_head * h, Widget * sender, dlg_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case DLG_KEY:
        if (parm == '\n')
        {
            if (dlg_widget_active (display_radio))
            {
                input_assign_text (display_mini_status, displays_status[display_radio->sel]);
                dlg_stop (h);
                return MSG_HANDLED;
            }

            if (dlg_widget_active (display_user_format))
            {
                h->ret_value = B_USER + 6;
                dlg_stop (h);
                return MSG_HANDLED;
            }

            if (dlg_widget_active (display_mini_status))
            {
                h->ret_value = B_USER + 7;
                dlg_stop (h);
                return MSG_HANDLED;
            }
        }

        if ((g_ascii_tolower (parm) == display_user_hotkey)
            && dlg_widget_active (display_user_format) && dlg_widget_active (display_mini_status))
        {
            display_radio->pos = display_radio->sel = 3;
            dlg_select_widget (display_radio);  /* force redraw */
            h->callback (h, WIDGET (display_radio), DLG_ACTION, 0, NULL);
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    case DLG_ACTION:
        if (sender == WIDGET (display_radio))
        {
            if (!(display_check_status->state & C_BOOL))
                input_assign_text (display_mini_status, displays_status[display_radio->sel]);
            input_update (display_mini_status, FALSE);
            input_update (display_user_format, FALSE);
            widget_disable (WIDGET (display_user_format), display_radio->sel != 3);
            return MSG_HANDLED;
        }

        if (sender == WIDGET (display_check_status))
        {
            if (display_check_status->state & C_BOOL)
            {
                widget_disable (WIDGET (display_mini_status), FALSE);
                input_assign_text (display_mini_status, displays_status[3]);
            }
            else
            {
                widget_disable (WIDGET (display_mini_status), TRUE);
                input_assign_text (display_mini_status, displays_status[display_radio->sel]);
            }
            input_update (display_mini_status, FALSE);
            return MSG_HANDLED;
        }

        return MSG_NOT_HANDLED;

    default:
        return default_dlg_callback (h, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static Dlg_head *
display_init (int radio_sel, char *init_text, int _check_status, char **_status)
{
    int dlg_width = 48, dlg_height = 15;
    Dlg_head *dd;

    /* Controls whether the array strings have been translated */
    const char *displays[LIST_TYPES] = {
        N_("&Full file list"),
        N_("&Brief file list"),
        N_("&Long file list"),
        N_("&User defined:")
    };

    /* Index in displays[] for "user defined" */
    const int user_type_idx = 3;

    const char *display_title = N_("Listing mode");
    const char *user_mini_status = N_("User &mini status");
    const char *ok_name = N_("&OK");
    const char *cancel_name = N_("&Cancel");

    WButton *ok_button, *cancel_button;

    {
        int i, maxlen = 0;
        const char *cp;
        int ok_len, cancel_len, b_len, gap;

#ifdef ENABLE_NLS
        display_title = _(display_title);
        user_mini_status = _(user_mini_status);
        ok_name = _(ok_name);
        cancel_name = _(cancel_name);

        for (i = 0; i < LIST_TYPES; i++)
            displays[i] = _(displays[i]);
#endif

        /* get hotkey of user-defined format string */
        cp = strchr (displays[user_type_idx], '&');
        if (cp != NULL && *++cp != '\0')
            display_user_hotkey = g_ascii_tolower (*cp);

        /* xpos will be fixed later */
        ok_button = button_new (dlg_height - 3, 0, B_ENTER, DEFPUSH_BUTTON, ok_name, 0);
        ok_len = button_get_len (ok_button);
        cancel_button = button_new (dlg_height - 3, 0, B_CANCEL, NORMAL_BUTTON, cancel_name, 0);
        cancel_len = button_get_len (cancel_button);
        b_len = ok_len + cancel_len + 2;

        dlg_width = max (dlg_width, str_term_width1 (display_title) + 10);
        /* calculate max width of radiobutons */
        for (i = 0; i < LIST_TYPES; i++)
            maxlen = max (maxlen, str_term_width1 (displays[i]));
        dlg_width = max (dlg_width, maxlen);
        dlg_width = max (dlg_width, str_term_width1 (user_mini_status) + 13);

        /* buttons */
        dlg_width = max (dlg_width, b_len + 6);
        gap = (dlg_width - 6 - b_len) / 3;
        WIDGET (ok_button)->x = 3 + gap;
        WIDGET (cancel_button)->x = WIDGET (ok_button)->x + ok_len + gap + 2;
    }

    displays_status = _status;

    dd = create_dlg (TRUE, 0, 0, dlg_height, dlg_width, dialog_colors,
                     display_callback, NULL, "[Listing Mode...]", display_title,
                     DLG_CENTER | DLG_REVERSE);

    add_widget (dd, cancel_button);
    add_widget (dd, ok_button);

    display_mini_status =
        input_new (10, 8, input_get_default_colors (), dlg_width - 12, _status[radio_sel],
                   "mini-input", INPUT_COMPLETE_DEFAULT);
    add_widget (dd, display_mini_status);

    display_check_status = check_new (9, 4, _check_status, user_mini_status);
    add_widget (dd, display_check_status);

    display_user_format = input_new (7, 8, input_get_default_colors (), dlg_width - 12, init_text,
                                     "user-fmt-input", INPUT_COMPLETE_DEFAULT);
    add_widget (dd, display_user_format);

    display_radio = radio_new (3, 4, LIST_TYPES, displays);
    display_radio->sel = display_radio->pos = radio_sel;
    add_widget (dd, display_radio);

    return dd;
}

/* --------------------------------------------------------------------------------------------- */
#ifdef HAVE_CHARSET

static int
sel_charset_button (WButton * button, int action)
{
    int new_dcp;

    (void) button;
    (void) action;

    new_dcp = select_charset (-1, -1, new_display_codepage, TRUE);

    if (new_dcp != SELECT_CHARSET_CANCEL)
    {
        const char *cpname;
        char buf[BUF_TINY];

        new_display_codepage = new_dcp;
        cpname = (new_display_codepage == SELECT_CHARSET_OTHER_8BIT) ?
            _("Other 8 bit") :
            ((codepage_desc *) g_ptr_array_index (codepages, new_display_codepage))->name;
        if (cpname != NULL)
            mc_global.utf8_display = str_isutf8 (cpname);
        /* avoid strange bug with label repainting */
        g_snprintf (buf, sizeof (buf), "%-27s", cpname);
        label_set_text (cplabel, buf);
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static Dlg_head *
init_disp_bits_box (void)
{
    /* dialog sizes */
    const int DISPY = 11;
    const int DISPX = 46;

    const char *cpname;
    Dlg_head *dbits_dlg;

    do_refresh ();

    dbits_dlg =
        create_dlg (TRUE, 0, 0, DISPY, DISPX, dialog_colors, NULL, NULL,
                    "[Display bits]", _("Display bits"), DLG_CENTER | DLG_REVERSE);

    add_widget (dbits_dlg, label_new (3, 4, _("Input / display codepage:")));

    cpname = (new_display_codepage < 0) ? _("Other 8 bit")
        : ((codepage_desc *) g_ptr_array_index (codepages, new_display_codepage))->name;
    cplabel = label_new (4, 4, cpname);
    add_widget (dbits_dlg, cplabel);

    add_widget (dbits_dlg,
                button_new (DISPY - 3, DISPX / 2 + 3, B_CANCEL, NORMAL_BUTTON, _("&Cancel"), 0));
    add_widget (dbits_dlg, button_new (DISPY - 3, 7, B_ENTER, NORMAL_BUTTON, _("&OK"), 0));

    inpcheck = check_new (6, 4, !use_8th_bit_as_meta, _("F&ull 8 bits input"));
    add_widget (dbits_dlg, inpcheck);

    cpname = _("&Select");
    add_widget (dbits_dlg,
                button_new (4, DISPX - 7 - str_term_width1 (cpname), B_USER,
                            NORMAL_BUTTON, cpname, sel_charset_button));

    return dbits_dlg;
}
#endif /* HAVE_CHARSET */

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
tree_callback (Dlg_head * h, Widget * sender, dlg_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case DLG_POST_KEY:
        /* The enter key will be processed by the tree widget */
        if (parm == '\n')
        {
            h->ret_value = B_ENTER;
            dlg_stop (h);
        }
        return MSG_HANDLED;

    case DLG_ACTION:
        return send_message (WIDGET (find_tree (h)), NULL, WIDGET_COMMAND, parm, NULL);

    default:
        return default_dlg_callback (h, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_VFS
#ifdef ENABLE_VFS_FTP

static cb_ret_t
confvfs_callback (Dlg_head * h, Widget * sender, dlg_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case DLG_ACTION:
        /* message from "Always use ftp proxy" checkbutton */
        if (sender != NULL && sender->id == 6)
        {
            const gboolean not_use = !(((WCheck *) sender)->state & C_BOOL);
            Widget *w;

            /* input */
            w = dlg_find_by_id (h, sender->id - 1);
            widget_disable (w, not_use);

            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    default:
        return default_dlg_callback (h, sender, msg, parm, data);
    }
}
#endif /* ENABLE_VFS_FTP */
#endif /* ENABLE_VFS */

#ifdef ENABLE_BACKGROUND
static void
jobs_fill_listbox (void)
{
    static const char *state_str[2];
    TaskList *tl = task_list;

    if (!state_str[0])
    {
        state_str[0] = _("Running");
        state_str[1] = _("Stopped");
    }

    while (tl)
    {
        char *s;

        s = g_strconcat (state_str[tl->state], " ", tl->info, (char *) NULL);
        listbox_add_item (bg_list, LISTBOX_APPEND_AT_END, 0, s, (void *) tl);
        g_free (s);
        tl = tl->next;
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
    {
        sig = SIGKILL;
    }

    if (sig == SIGKILL)
        unregister_task_running (tl->pid, tl->fd);

    kill (tl->pid, sig);
    listbox_remove_list (bg_list);
    jobs_fill_listbox ();

    /* This can be optimized to just redraw this widget :-) */
    dlg_redraw (jobs_dlg);

    return 0;
}
#endif /* ENABLE_BACKGROUND */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* return list type */
int
display_box (WPanel * panel, char **userp, char **minip, int *use_msformat, int num)
{
    int result = -1;
    Dlg_head *dd;
    char *section = NULL;
    size_t i;

    if (panel == NULL)
    {
        const char *p = get_nth_panel_name (num);
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

    dd = display_init (panel->list_type, panel->user_format,
                       panel->user_mini_status, panel->user_status_format);

    if (run_dlg (dd) != B_CANCEL)
    {
        result = display_radio->sel;
        *userp = g_strdup (display_user_format->buffer);
        *minip = g_strdup (display_mini_status->buffer);
        *use_msformat = display_check_status->state & C_BOOL;
    }

    if (section != NULL)
    {
        g_free (panel->user_format);
        for (i = 0; i < LIST_TYPES; i++)
            g_free (panel->user_status_format[i]);
        g_free (panel);
    }

    destroy_dlg (dd);

    return result;
}

/* --------------------------------------------------------------------------------------------- */

const panel_field_t *
sort_box (panel_sort_info_t * info)
{
    int dlg_width = 40, dlg_height = 7;

    const char **sort_orders_names;
    gsize sort_names_num;

    int sort_idx = 0;

    const panel_field_t *result = info->sort_field;

    sort_orders_names = panel_get_sortable_fields (&sort_names_num);
    dlg_height += sort_names_num;

    {
        int max_radio = 0, max_check = 0;
        int ok_len, cancel_len;
        gsize i;

        QuickWidget quick_widgets[] = {
            /* 0 */
            QUICK_BUTTON (0, dlg_width, dlg_height - 3, dlg_height, N_("&Cancel"), B_CANCEL, NULL),
            /* 1 */
            QUICK_BUTTON (0, dlg_width, dlg_height - 3, dlg_height, N_("&OK"), B_ENTER, NULL),
            /* 2 */
            QUICK_CHECKBOX (0, dlg_width, 5, dlg_height, N_("&Reverse"), &info->reverse),
            /* 3 */
            QUICK_CHECKBOX (0, dlg_width, 4, dlg_height, N_("Case sensi&tive"),
                            &info->case_sensitive),
            /* 4 */
            QUICK_CHECKBOX (0, dlg_width, 3, dlg_height, N_("Executable &first"),
                            &info->exec_first),
            /* 5 */
            QUICK_RADIO (4, dlg_width, 3, dlg_height, 0, NULL, &sort_idx),
            QUICK_END
        };

        QuickDialog quick_dlg = {
            dlg_width, dlg_height, -1, -1,
            N_("Sort order"), "[Sort Order...]",
            quick_widgets, NULL, NULL, TRUE
        };

        quick_widgets[5].u.radio.items = sort_orders_names;
        quick_widgets[5].u.radio.count = sort_names_num;

        for (i = 0; i < sort_names_num; i++)
            if (strcmp (sort_orders_names[i], _(info->sort_field->title_hotkey)) == 0)
            {
                sort_idx = i;
                break;
            }

#ifdef ENABLE_NLS
        quick_dlg.title = _(quick_dlg.title);
        /* buttons */
        for (i = 0; i < 2; i++)
            quick_widgets[i].u.button.text = _(quick_widgets[i].u.button.text);
        /* checkboxes */
        for (i = 2; i < 5; i++)
            quick_widgets[i].u.checkbox.text = _(quick_widgets[i].u.checkbox.text);
#endif /* ENABLE_NlS */

        /* buttons */
        cancel_len = str_term_width1 (quick_widgets[0].u.button.text) + 4;
        ok_len = str_term_width1 (quick_widgets[1].u.button.text) + 6;
        /* checkboxes */
        for (i = 2; i < 5; i++)
            max_check = max (max_check, str_term_width1 (quick_widgets[i].u.checkbox.text) + 4);
        /* radiobuttons */
        for (i = 0; i < sort_names_num; i++)
            max_radio = max (max_radio, str_term_width1 (sort_orders_names[i]) + 4);

        /* dialog width */
        dlg_width = max (dlg_width, str_term_width1 (quick_dlg.title) + 8);
        dlg_width = max (dlg_width, ok_len + cancel_len + 8);
        dlg_width = max (dlg_width, 2 * max (max_radio, max_check) + 8);

        /* fix widget and dialog parameters */
        /* dialog */
        quick_dlg.xlen = dlg_width;
        /* widgets */
        for (i = 0; (size_t) i < sizeof (quick_widgets) / sizeof (quick_widgets[0]) - 1; i++)
            quick_widgets[i].x_divisions = dlg_width;
        /* buttons */
        quick_widgets[0].relative_x = dlg_width * 2 / 3 - cancel_len / 2;
        quick_widgets[1].relative_x = dlg_width / 3 - ok_len / 2;
        /* checkboxes */
        for (i = 2; i < 5; i++)
            quick_widgets[i].relative_x = dlg_width / 2 + 2;

        if (quick_dialog (&quick_dlg) != B_CANCEL)
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
    const char *title = _("Confirmation");

    QuickWidget conf_widgets[] = {
        /* 0 */ QUICK_BUTTON (29, 46, 10, 13, N_("&Cancel"), B_CANCEL, NULL),
        /* 1 */ QUICK_BUTTON (12, 46, 10, 13, N_("&OK"), B_ENTER, NULL),
        /* TRANSLATORS: no need to translate 'Confirmation', it's just a context prefix */
        /* 2 */ QUICK_CHECKBOX (3, 46, 8, 13, N_("Confirmation|&History cleanup"),
                                &mc_global.widget.confirm_history_cleanup),
        /* 3 */ QUICK_CHECKBOX (3, 46, 7, 13, N_("Confirmation|Di&rectory hotlist delete"),
                                &confirm_directory_hotlist_delete),
        /* 4 */ QUICK_CHECKBOX (3, 46, 6, 13, N_("Confirmation|E&xit"), &confirm_exit),
        /* 5 */ QUICK_CHECKBOX (3, 46, 5, 13, N_("Confirmation|&Execute"), &confirm_execute),
        /* 6 */ QUICK_CHECKBOX (3, 46, 4, 13, N_("Confirmation|O&verwrite"), &confirm_overwrite),
        /* 7 */ QUICK_CHECKBOX (3, 46, 3, 13, N_("Confirmation|&Delete"), &confirm_delete),
        QUICK_END
    };

    const size_t w_num = sizeof (conf_widgets) / sizeof (conf_widgets[0]) - 1;

    /* dialog sizes */
    int dlg_width = 46;
    int dlg_height = w_num + 5;

    size_t i;
    int maxlen = 0;
    int cancel_len, ok_len, blen;

#ifdef ENABLE_NLS
    title = _(title);

    for (i = 0; i < 2; i++)
        conf_widgets[i].u.button.text = _(conf_widgets[i].u.button.text);
#endif /* ENABLE_NLS */

    for (i = 2; i < w_num; i++)
        conf_widgets[i].u.checkbox.text = Q_ (conf_widgets[i].u.checkbox.text);

    /* maximum length of checkboxes */
    for (i = 2; i < w_num; i++)
        maxlen = max (maxlen, str_term_width1 (conf_widgets[i].u.checkbox.text) + 4);

    /* length of buttons */
    cancel_len = str_term_width1 (conf_widgets[0].u.button.text) + 3;
    ok_len = str_term_width1 (conf_widgets[1].u.button.text) + 5;       /* default button */

    blen = cancel_len + ok_len + 2;

    dlg_width = max (maxlen, blen) + 6;
    dlg_width = max (dlg_width, str_term_width1 (title) + 4);

    /* correct widget parameters */
    for (i = 0; i < w_num; i++)
    {
        conf_widgets[i].x_divisions = dlg_width;
        conf_widgets[i].y_divisions = dlg_height;
    }

    conf_widgets[1].relative_x = dlg_width / 2 - blen / 2;
    conf_widgets[0].relative_x = conf_widgets[1].relative_x + ok_len + 2;

    {
        QuickDialog confirmation = {
            dlg_width, dlg_height, -1, -1, title,
            "[Confirmation]", conf_widgets, NULL, NULL, TRUE
        };

        (void) quick_dialog (&confirmation);
    }
}

/* --------------------------------------------------------------------------------------------- */

#ifndef HAVE_CHARSET
void
display_bits_box (void)         /* AB:FIXME: test dialog */
{
    /* dialog sizes */
    const int DISPY = 13;
    const int DISPX = 46;

    int new_meta = 0;
    int current_mode;

    const char *display_bits_str[] = {
        N_("UTF-8 output"),
        N_("Full 8 bits output"),
        N_("ISO 8859-1"),
        N_("7 bits")
    };

    QuickWidget display_widgets[] = {
        /* 0 */ QUICK_BUTTON (15, DISPX, DISPY - 3, DISPY, N_("&Cancel"), B_CANCEL, NULL),
        /* 1 */ QUICK_BUTTON (29, DISPX, DISPY - 3, DISPY, N_("&OK"), B_ENTER, NULL),
        /* 2 */ QUICK_CHECKBOX (3, DISPX, 8, DISPY, N_("F&ull 8 bits input"), &new_meta),
        /* 3 */ QUICK_RADIO (3, DISPX, 3, DISPY, 4, display_bits_str, &current_mode),
        QUICK_END
    };

    QuickDialog display_bits = {
        DISPX, DISPY, -1, -1, _("Display bits"),
        "[Display bits]", display_widgets, NULL, NULL, TRUE
    };

    int i;
    int l1, maxlen = 0;
    int ok_len, cancel_len;

#ifdef ENABLE_NLS
    static gboolean i18n_flag = FALSE;

    if (!i18n_flag)
    {
        for (i = 0; i < 3; i++)
        {
            display_bits_str[i] = _(display_bits_str[i]);
        }

        display_widgets[0].u.button.text = _(display_widgets[0].u.button.text);
        display_widgets[1].u.button.text = _(display_widgets[1].u.button.text);
        display_widgets[2].u.checkbox.text = _(display_widgets[2].u.checkbox.text);

        i18n_flag = TRUE;
    }
#endif /* ENABLE_NLS */

    /* radiobuttons */
    for (i = 0; i < 3; i++)
        maxlen = max (maxlen, str_term_width1 (display_bits_str[i]));

    /* buttons */
    cancel_len = str_term_width1 (display_widgets[0].u.button.text) + 2;
    ok_len = str_term_width1 (display_widgets[1].u.button.text) + 4;    /* default button */

    l1 = max (cancel_len, ok_len);

    display_bits.xlen = max (maxlen, l1) + 20;

    for (i = 0; i < 4; i++)
        display_widgets[i].x_divisions = display_bits.xlen;

    display_widgets[0].relative_x = display_bits.xlen * 2 / 3 - cancel_len / 2;
    display_widgets[1].relative_x = display_bits.xlen / 3 - ok_len / 2;

    if (mc_global.full_eight_bits)
        current_mode = 0;
    else if (mc_global.eight_bit_clean)
        current_mode = 1;
    else
        current_mode = 2;

    new_meta = !use_8th_bit_as_meta;

    if (quick_dialog (&display_bits) != B_CANCEL)
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
    Dlg_head *dbits_dlg;
    new_display_codepage = mc_global.display_codepage;

    application_keypad_mode ();
    dbits_dlg = init_disp_bits_box ();

    run_dlg (dbits_dlg);

    if (dbits_dlg->ret_value == B_ENTER)
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
        use_8th_bit_as_meta = !(inpcheck->state & C_BOOL);
    }
    destroy_dlg (dbits_dlg);
    repaint_screen ();
}
#endif /* HAVE_CHARSET */

/* --------------------------------------------------------------------------------------------- */
/** Show tree in a box, not on a panel */

char *
tree_box (const char *current_dir)
{
    WTree *mytree;
    Dlg_head *dlg;
    Widget *wd;
    char *val = NULL;
    WButtonBar *bar;

    (void) current_dir;

    /* Create the components */
    dlg = create_dlg (TRUE, 0, 0, LINES - 9, COLS - 20, dialog_colors,
                      tree_callback, NULL, "[Directory Tree]",
                      _("Directory tree"), DLG_CENTER | DLG_REVERSE);
    wd = WIDGET (dlg);

    mytree = tree_new (2, 2, wd->lines - 6, wd->cols - 5, FALSE);
    add_widget (dlg, mytree);
    add_widget (dlg, hline_new (wd->lines - 4, 1, -1));
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
#endif

    QuickWidget confvfs_widgets[] = {
        /*  0 */ QUICK_BUTTON (30, VFSX, VFSY - 3, VFSY, N_("&Cancel"), B_CANCEL, NULL),
        /*  1 */ QUICK_BUTTON (12, VFSX, VFSY - 3, VFSY, N_("&OK"), B_ENTER, NULL),
#ifdef ENABLE_VFS_FTP
        /*  2 */ QUICK_CHECKBOX (4, VFSX, 12, VFSY, N_("Use passive mode over pro&xy"),
                                 &ftpfs_use_passive_connections_over_proxy),
        /*  3 */ QUICK_CHECKBOX (4, VFSX, 11, VFSY, N_("Use &passive mode"),
                                 &ftpfs_use_passive_connections),
        /*  4 */ QUICK_CHECKBOX (4, VFSX, 10, VFSY, N_("&Use ~/.netrc"), &ftpfs_use_netrc),
        /*  5 */ QUICK_INPUT (4, VFSX, 9, VFSY, ftpfs_proxy_host, 48, 0, "input-ftp-proxy",
                              &ret_ftp_proxy),
        /*  6 */ QUICK_CHECKBOX (4, VFSX, 8, VFSY, N_("&Always use ftp proxy"),
                                 &ftpfs_always_use_proxy),
        /*  7 */ QUICK_LABEL (49, VFSX, 7, VFSY, N_("sec")),
        /*  8 */ QUICK_INPUT (38, VFSX, 7, VFSY, buffer3, 10, 0, "input-timeout",
                              &ret_directory_timeout),
        /*  9 */ QUICK_LABEL (4, VFSX, 7, VFSY, N_("ftpfs directory cache timeout:")),
        /* 10 */ QUICK_INPUT (4, VFSX, 6, VFSY, ftpfs_anonymous_passwd, 48, 0, "input-passwd",
                              &ret_passwd),
        /* 11 */ QUICK_LABEL (4, VFSX, 5, VFSY, N_("ftp anonymous password:")),
#endif /* ENABLE_VFS_FTP */
        /* 12 */ QUICK_LABEL (49, VFSX, 3, VFSY, N_("sec")),
        /* 13 */ QUICK_INPUT (38, VFSX, 3, VFSY, buffer2, 10, 0, "input-timo-vfs", &ret_timeout),
        /* 14 */ QUICK_LABEL (4, VFSX, 3, VFSY, N_("Timeout for freeing VFSs:")),
        QUICK_END
    };

    QuickDialog confvfs_dlg = {
        VFSX, VFSY, -1, -1, N_("Virtual File System Setting"),
        "[Virtual FS]", confvfs_widgets,
#ifdef ENABLE_VFS_FTP
        confvfs_callback,
#else
        NULL,
#endif
        NULL,
        FALSE
    };

#ifdef ENABLE_VFS_FTP
    g_snprintf (buffer3, sizeof (buffer3), "%i", ftpfs_directory_timeout);

    if (!ftpfs_always_use_proxy)
        confvfs_widgets[5].options = W_DISABLED;
#endif

    g_snprintf (buffer2, sizeof (buffer2), "%i", vfs_timeout);

    if (quick_dialog (&confvfs_dlg) != B_CANCEL)
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

#undef VFSX
#undef VFSY
}

#endif /* ENABLE_VFS */

/* --------------------------------------------------------------------------------------------- */

char *
cd_dialog (void)
{
    const char *label = N_("cd");
    const int ylen = 5;
    const int xlen = 57;

    int len;

#ifdef ENABLE_NLS
    label = _(label);
#endif

    len = str_term_width1 (label);

    {
        char *my_str;

        QuickWidget quick_widgets[] = {
            /* 0 */ QUICK_INPUT (4 + len, xlen, 2, ylen, "", xlen - 7 - len, 2, "input", &my_str),
            /* 1 */ QUICK_LABEL (3, xlen, 2, ylen, label),
            QUICK_END
        };

        QuickDialog Quick_input = {
            xlen, ylen, 2, LINES - 2 - ylen, _("Quick cd"),
            "[Quick cd]", quick_widgets, NULL, NULL, TRUE
        };

        return (quick_dialog (&Quick_input) != B_CANCEL) ? my_str : NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
symlink_dialog (const vfs_path_t * existing_vpath, const vfs_path_t * new_vpath,
                char **ret_existing, char **ret_new)
{
    char *existing = vfs_path_to_str (existing_vpath);
    char *new = vfs_path_to_str (new_vpath);

    QuickWidget quick_widgets[] = {
        /* 0 */ QUICK_BUTTON (50, 80, 6, 8, N_("&Cancel"), B_CANCEL, NULL),
        /* 1 */ QUICK_BUTTON (16, 80, 6, 8, N_("&OK"), B_ENTER, NULL),
        /* 2 */ QUICK_INPUT (4, 80, 5, 8, new, 58, 0, "input-1", ret_new),
        /* 3 */ QUICK_LABEL (4, 80, 4, 8, N_("Symbolic link filename:")),
        /* 4 */ QUICK_INPUT (4, 80, 3, 8, existing, 58, 0, "input-2", ret_existing),
        /* 5 */ QUICK_LABEL (4, 80, 2, 8,
                             N_("Existing filename (filename symlink will point to):")),
        QUICK_END
    };

    QuickDialog Quick_input = {
        64, 12, -1, -1, N_("Symbolic link"),
        "[File Menu]", quick_widgets, NULL, NULL, FALSE
    };

    if (quick_dialog (&Quick_input) == B_CANCEL)
    {
        *ret_new = NULL;
        *ret_existing = NULL;
    }
    g_free (existing);
    g_free (new);
}

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_BACKGROUND
void
jobs_cmd (void)
{
    register int i;
    int n_buttons = sizeof (job_buttons) / sizeof (job_buttons[0]);

#ifdef ENABLE_NLS
    static int i18n_flag = 0;
    if (!i18n_flag)
    {
        int startx = job_buttons[0].xpos;
        int len;

        for (i = 0; i < n_buttons; i++)
        {
            job_buttons[i].name = _(job_buttons[i].name);

            len = str_term_width1 (job_buttons[i].name) + 4;
            JOBS_X = max (JOBS_X, startx + len + 3);

            job_buttons[i].xpos = startx;
            startx += len;
        }

        /* Last button - Ok a.k.a. Cancel :) */
        job_buttons[n_buttons - 1].xpos =
            JOBS_X - str_term_width1 (job_buttons[n_buttons - 1].name) - 7;

        i18n_flag = 1;
    }
#endif /* ENABLE_NLS */

    jobs_dlg = create_dlg (TRUE, 0, 0, JOBS_Y, JOBS_X, dialog_colors, NULL, NULL,
                           "[Background jobs]", _("Background Jobs"), DLG_CENTER | DLG_REVERSE);

    bg_list = listbox_new (2, 3, JOBS_Y - 9, JOBS_X - 7, FALSE, NULL);
    add_widget (jobs_dlg, bg_list);

    i = n_buttons;
    while (i--)
    {
        add_widget (jobs_dlg, button_new (JOBS_Y - 4,
                                          job_buttons[i].xpos, job_buttons[i].value,
                                          NORMAL_BUTTON, job_buttons[i].name,
                                          job_buttons[i].callback));
    }

    /* Insert all of task information in the list */
    jobs_fill_listbox ();
    run_dlg (jobs_dlg);

    destroy_dlg (jobs_dlg);
}
#endif /* ENABLE_BACKGROUND */

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_VFS_SMB
struct smb_authinfo *
vfs_smb_get_authinfo (const char *host, const char *share, const char *domain, const char *user)
{
    static int dialog_x = 44;
    int b0 = 3, dialog_y = 12;
    static const char *lc_labs[] = { N_("Domain:"), N_("Username:"), N_("Password:") };
    static const char *buts[] = { N_("&OK"), N_("&Cancel") };
    static int ilen = 30, istart = 14;
    static int b2 = 30;
    char *title;
    WInput *in_password;
    WInput *in_user;
    WInput *in_domain;
    Dlg_head *auth_dlg;
    struct smb_authinfo *return_value = NULL;

#ifdef ENABLE_NLS
    static int i18n_flag = 0;

    if (!i18n_flag)
    {
        register int i = sizeof (lc_labs) / sizeof (lc_labs[0]);
        int l1, maxlen = 0;

        while (i--)
        {
            l1 = str_term_width1 (lc_labs[i] = _(lc_labs[i]));
            if (l1 > maxlen)
                maxlen = l1;
        }
        i = maxlen + ilen + 7;
        if (i > dialog_x)
            dialog_x = i;

        for (i = sizeof (buts) / sizeof (buts[0]), l1 = 0; i--;)
        {
            l1 += str_term_width1 (buts[i] = _(buts[i]));
        }
        l1 += 15;
        if (l1 > dialog_x)
            dialog_x = l1;

        ilen = dialog_x - 7 - maxlen;   /* for the case of very long buttons :) */
        istart = dialog_x - 3 - ilen;

        b2 = dialog_x - (str_term_width1 (buts[1]) + 6);

        i18n_flag = 1;
    }

#endif /* ENABLE_NLS */

    if (!domain)
        domain = "";
    if (!user)
        user = "";

    title = g_strdup_printf (_("Password for \\\\%s\\%s"), host, share);

    auth_dlg = create_dlg (TRUE, 0, 0, dialog_y, dialog_x, dialog_colors, NULL, NULL,
                           "[Smb Authinfo]", title, DLG_CENTER | DLG_REVERSE);

    g_free (title);

    in_user =
        input_new (5, istart, input_get_default_colors (), ilen, user, "auth_name",
                   INPUT_COMPLETE_DEFAULT);
    add_widget (auth_dlg, in_user);

    in_domain =
        input_new (3, istart, input_get_default_colors (), ilen, domain, "auth_domain",
                   INPUT_COMPLETE_DEFAULT);

    add_widget (auth_dlg, in_domain);
    add_widget (auth_dlg, button_new (9, b2, B_CANCEL, NORMAL_BUTTON, buts[1], 0));
    add_widget (auth_dlg, button_new (9, b0, B_ENTER, DEFPUSH_BUTTON, buts[0], 0));

    in_password =
        input_new (7, istart, input_get_default_colors (), ilen, "", "auth_password",
                   INPUT_COMPLETE_DEFAULT);

    in_password->completion_flags = 0;
    in_password->is_password = 1;
    add_widget (auth_dlg, in_password);

    add_widget (auth_dlg, label_new (7, 3, lc_labs[2]));
    add_widget (auth_dlg, label_new (5, 3, lc_labs[1]));
    add_widget (auth_dlg, label_new (3, 3, lc_labs[0]));

    if (run_dlg (auth_dlg) != B_CANCEL)
        return_value = vfs_smb_authinfo_new (host, share, in_domain->buffer, in_user->buffer,
                                             in_password->buffer);

    destroy_dlg (auth_dlg);

    return return_value;
}
#endif /* ENABLE_VFS_SMB */

/* --------------------------------------------------------------------------------------------- */
