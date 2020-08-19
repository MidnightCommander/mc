/*
   Chown-advanced command -- for the Midnight Commander

   Copyright (C) 1994-2020
   Free Software Foundation, Inc.

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

/** \file achown.c
 *  \brief Source: Contains functions for advanced chowning
 */

#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/key.h"        /* XCTRL and ALT macros */
#include "lib/skin.h"
#include "lib/vfs/vfs.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/widget.h"

#include "filemanager.h"        /* current_panel */

#include "cmd.h"                /* advanced_chown_cmd() */

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define BX 5
#define BY 5

#define BUTTONS      9
#define BUTTONS_PERM 5

#define B_SETALL B_USER
#define B_SKIP   (B_USER + 1)

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static struct
{
    unsigned long id;
    int ret_cmd;
    button_flags_t flags;
    int x;
    int len;
    const char *text;
} advanced_chown_but[BUTTONS] =
{
    /* *INDENT-OFF* */
    { 0, B_ENTER,   NARROW_BUTTON,  3, 0, "   " },
    { 0, B_ENTER,   NARROW_BUTTON, 11, 0, "   " },
    { 0, B_ENTER,   NARROW_BUTTON, 19, 0, "   " },
    { 0, B_ENTER,   NARROW_BUTTON, 29, 0, ""    },
    { 0, B_ENTER,   NARROW_BUTTON, 47, 0, ""    },

    { 0, B_SETALL,  NORMAL_BUTTON,  0, 0, N_("Set &all") },
    { 0, B_SKIP,    NORMAL_BUTTON,  0, 0, N_("S&kip")    },
    { 0, B_ENTER,  DEFPUSH_BUTTON,  0, 0, N_("&Set")     },
    { 0, B_CANCEL,  NORMAL_BUTTON,  0, 0, N_("&Cancel")  }
    /* *INDENT-ON* */
};

static int current_file;
static gboolean ignore_all;

static WButton *b_att[3];       /* permission */
static WButton *b_user, *b_group;       /* owner */
static WLabel *l_filename;
static WLabel *l_mode;

static int flag_pos;
static int x_toggle;
static char ch_flags[11];
static const char ch_perm[] = "rwx";
static mode_t ch_cmode;
static struct stat sf_stat;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
advanced_chown_init (void)
{
    static gboolean i18n = FALSE;
    int i;

    if (i18n)
        return;

    i18n = TRUE;

    for (i = BUTTONS_PERM; i < BUTTONS; i++)
    {
#ifdef ENABLE_NLS
        advanced_chown_but[i].text = _(advanced_chown_but[i].text);
#endif /* ENABLE_NLS */

        advanced_chown_but[i].len = str_term_width1 (advanced_chown_but[i].text) + 3;
        if (advanced_chown_but[i].flags == DEFPUSH_BUTTON)
            advanced_chown_but[i].len += 2;     /* "<>" */
    }

}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
inc_flag_pos (void)
{
    if (flag_pos == 10)
    {
        flag_pos = 0;
        return MSG_NOT_HANDLED;
    }

    flag_pos++;

    return flag_pos % 3 == 0 ? MSG_NOT_HANDLED : MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
dec_flag_pos (void)
{
    if (flag_pos == 0)
    {
        flag_pos = 10;
        return MSG_NOT_HANDLED;
    }

    flag_pos--;

    return (flag_pos + 1) % 3 == 0 ? MSG_NOT_HANDLED : MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static void
set_perm_by_flags (char *s, int f_p)
{
    int i;

    for (i = 0; i < 3; i++)
    {
        if (ch_flags[f_p + i] == '+')
            s[i] = ch_perm[i];
        else if (ch_flags[f_p + i] == '-')
            s[i] = '-';
        else
            s[i] = (ch_cmode & (1 << (8 - f_p - i))) != 0 ? ch_perm[i] : '-';
    }
}

/* --------------------------------------------------------------------------------------------- */

static mode_t
get_perm (char *s, int base)
{
    mode_t m = 0;

    m |= (s[0] == '-') ? 0 :
        ((s[0] == '+') ? (mode_t) (1 << (base + 2)) : (1 << (base + 2)) & ch_cmode);

    m |= (s[1] == '-') ? 0 :
        ((s[1] == '+') ? (mode_t) (1 << (base + 1)) : (1 << (base + 1)) & ch_cmode);

    m |= (s[2] == '-') ? 0 : ((s[2] == '+') ? (mode_t) (1 << base) : (1 << base) & ch_cmode);

    return m;
}

/* --------------------------------------------------------------------------------------------- */

static mode_t
get_mode (void)
{
    mode_t m;

    m = ch_cmode ^ (ch_cmode & 0777);
    m |= get_perm (ch_flags, 6);
    m |= get_perm (ch_flags + 3, 3);
    m |= get_perm (ch_flags + 6, 0);

    return m;
}

/* --------------------------------------------------------------------------------------------- */

static void
update_permissions (void)
{
    set_perm_by_flags (b_att[0]->text.start, 0);
    set_perm_by_flags (b_att[1]->text.start, 3);
    set_perm_by_flags (b_att[2]->text.start, 6);
}

/* --------------------------------------------------------------------------------------------- */

static void
update_ownership (void)
{
    button_set_text (b_user, get_owner (sf_stat.st_uid));
    button_set_text (b_group, get_group (sf_stat.st_gid));
}

/* --------------------------------------------------------------------------------------------- */

static void
print_flags (const WDialog * h)
{
    int i;

    tty_setcolor (COLOR_NORMAL);

    for (i = 0; i < 3; i++)
    {
        widget_gotoyx (h, BY + 1, advanced_chown_but[0].x + 6 + i);
        tty_print_char (ch_flags[i]);
    }

    for (i = 0; i < 3; i++)
    {
        widget_gotoyx (h, BY + 1, advanced_chown_but[1].x + 6 + i);
        tty_print_char (ch_flags[i + 3]);
    }

    for (i = 0; i < 3; i++)
    {
        widget_gotoyx (h, BY + 1, advanced_chown_but[2].x + 6 + i);
        tty_print_char (ch_flags[i + 6]);
    }

    update_permissions ();

    for (i = 0; i < 15; i++)
    {
        widget_gotoyx (h, BY + 1, advanced_chown_but[3].x + 6 + i);
        tty_print_char (ch_flags[9]);
    }
    for (i = 0; i < 15; i++)
    {
        widget_gotoyx (h, BY + 1, advanced_chown_but[4].x + 6 + i);
        tty_print_char (ch_flags[10]);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
advanced_chown_refresh (WDialog * h)
{
    tty_setcolor (COLOR_NORMAL);

    widget_gotoyx (h, BY - 1, advanced_chown_but[0].x + 5);
    tty_print_string (_("owner"));
    widget_gotoyx (h, BY - 1, advanced_chown_but[1].x + 5);
    tty_print_string (_("group"));
    widget_gotoyx (h, BY - 1, advanced_chown_but[2].x + 5);
    tty_print_string (_("other"));

    widget_gotoyx (h, BY - 1, advanced_chown_but[3].x + 5);
    tty_print_string (_("owner"));
    widget_gotoyx (h, BY - 1, advanced_chown_but[4].x + 5);
    tty_print_string (_("group"));

    widget_gotoyx (h, BY + 1, 3);
    tty_print_string (_("Flag"));
    print_flags (h);
}

/* --------------------------------------------------------------------------------------------- */

static void
advanced_chown_info_update (void)
{
    /* mode */
    label_set_textv (l_mode, _("Permissions (octal): %o"), get_mode ());

    /* permissions */
    update_permissions ();
}

/* --------------------------------------------------------------------------------------------- */

static void
update_mode (WGroup * g)
{
    print_flags (DIALOG (g));
    advanced_chown_info_update ();
    widget_set_state (WIDGET (g->current->data), WST_FOCUSED, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
perm_button_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WButton *b = BUTTON (w);
    WGroup *g = w->owner;
    int i = 0;
    int f_pos;

    /* one of permission buttons */
    if (b == b_att[0])
        f_pos = 0;
    else if (b == b_att[1])
        f_pos = 1;
    else                        /* if (w == b_att [1] */
        f_pos = 2;

    switch (msg)
    {
    case MSG_FOCUS:
        if (b->hotpos == -1)
            b->hotpos = 0;

        flag_pos = f_pos * 3 + b->hotpos;
        return MSG_HANDLED;

    case MSG_KEY:
        switch (parm)
        {
        case '*':
            parm = '=';
            MC_FALLTHROUGH;

        case '-':
        case '=':
        case '+':
            flag_pos = f_pos * 3 + b->hotpos;
            ch_flags[flag_pos] = parm;
            update_mode (g);
            send_message (w, NULL, MSG_KEY, KEY_RIGHT, NULL);
            if (b->hotpos == 2)
                group_select_next_widget (g);
            break;

        case XCTRL ('f'):
        case KEY_RIGHT:
            {
                cb_ret_t ret;

                ret = inc_flag_pos ();
                b->hotpos = flag_pos % 3;
                return ret;
            }

        case XCTRL ('b'):
        case KEY_LEFT:
            {
                cb_ret_t ret;

                ret = dec_flag_pos ();
                b->hotpos = flag_pos % 3;
                return ret;
            }

        case 'x':
            i++;
            MC_FALLTHROUGH;

        case 'w':
            i++;
            MC_FALLTHROUGH;

        case 'r':
            b->hotpos = i;
            MC_FALLTHROUGH;

        case ' ':
            i = b->hotpos;

            flag_pos = f_pos * 3 + i;
            if (b->text.start[flag_pos % 3] == '-')
                ch_flags[flag_pos] = '+';
            else
                ch_flags[flag_pos] = '-';
            update_mode (w->owner);
            break;

        case '4':
            i++;
            MC_FALLTHROUGH;

        case '2':
            i++;
            MC_FALLTHROUGH;

        case '1':
            b->hotpos = i;
            flag_pos = f_pos * 3 + i;
            ch_flags[flag_pos] = '=';
            update_mode (g);
            break;

        default:
            break;
        }
        /* continue key handling in the dialog level */
        return MSG_NOT_HANDLED;

    default:
        return button_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
perm_button_mouse_callback (Widget * w, mouse_msg_t msg, mouse_event_t * event)
{
    switch (msg)
    {
    case MSG_MOUSE_DOWN:
        /* place cursor on flag that is being modified */
        BUTTON (w)->hotpos = CLAMP (event->x - 1, 0, 2);
        MC_FALLTHROUGH;

    default:
        button_mouse_default_callback (w, msg, event);
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */

static WButton *
perm_button_new (int y, int x, int action, button_flags_t flags, const char *text,
                 bcback_fn callback)
{
    WButton *b;
    Widget *w;

    /* create base button using native API */
    b = button_new (y, x, action, flags, text, callback);
    w = WIDGET (b);

    /* we don't want HOTKEY */
    widget_want_hotkey (w, FALSE);

    w->callback = perm_button_callback;
    w->mouse_callback = perm_button_mouse_callback;

    return b;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
chl_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_KEY:
        switch (parm)
        {
        case KEY_LEFT:
        case KEY_RIGHT:
            {
                WDialog *h = DIALOG (w);

                h->ret_value = parm;
                dlg_stop (h);
            }
        default:
            break;
        }
        MC_FALLTHROUGH;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
user_group_button_cb (WButton * button, int action)
{
    Widget *w = WIDGET (button);
    int f_pos;
    gboolean chl_end;

    (void) action;

    if (button == b_user)
        f_pos = BUTTONS_PERM - 2;
    else if (button == b_group)
        f_pos = BUTTONS_PERM - 1;
    else
        return 0;               /* do nothing */

    do
    {
        WGroup *g = w->owner;
        WDialog *h = DIALOG (g);
        Widget *wh = WIDGET (h);

        gboolean is_owner = (f_pos == BUTTONS_PERM - 2);
        const char *title;
        int lxx, b_pos;
        WDialog *chl_dlg;
        WListbox *chl_list;
        int result;
        int fe;
        struct passwd *chl_pass;
        struct group *chl_grp;

        chl_end = FALSE;

        if (is_owner)
        {
            title = _("owner");
            lxx = WIDGET (b_user)->x + 1;
        }
        else
        {
            title = _("group");
            lxx = WIDGET (b_group)->x + 1;
        }

        chl_dlg =
            dlg_create (TRUE, wh->y - 1, lxx, wh->lines + 2, 17, WPOS_KEEP_DEFAULT, TRUE,
                        dialog_colors, chl_callback, NULL, "[Advanced Chown]", title);

        /* get new listboxes */
        chl_list =
            listbox_new (1, 1, WIDGET (chl_dlg)->lines - 2, WIDGET (chl_dlg)->cols - 2, FALSE,
                         NULL);
        listbox_add_item (chl_list, LISTBOX_APPEND_AT_END, 0, "<Unknown>", NULL, FALSE);
        if (is_owner)
        {
            /* get and put user names in the listbox */
            setpwent ();
            while ((chl_pass = getpwent ()) != NULL)
                listbox_add_item (chl_list, LISTBOX_APPEND_SORTED, 0, chl_pass->pw_name, NULL,
                                  FALSE);
            endpwent ();
            fe = listbox_search_text (chl_list, get_owner (sf_stat.st_uid));
        }
        else
        {
            /* get and put group names in the listbox */
            setgrent ();
            while ((chl_grp = getgrent ()) != NULL)
                listbox_add_item (chl_list, LISTBOX_APPEND_SORTED, 0, chl_grp->gr_name, NULL,
                                  FALSE);
            endgrent ();
            fe = listbox_search_text (chl_list, get_group (sf_stat.st_gid));
        }

        listbox_select_entry (chl_list, fe);

        b_pos = chl_list->pos;
        group_add_widget (GROUP (chl_dlg), chl_list);

        result = dlg_run (chl_dlg);

        if (result != B_CANCEL)
        {
            if (b_pos != chl_list->pos)
            {
                gboolean ok = FALSE;
                char *text;

                listbox_get_current (chl_list, &text, NULL);
                if (is_owner)
                {
                    chl_pass = getpwnam (text);
                    if (chl_pass != NULL)
                    {
                        sf_stat.st_uid = chl_pass->pw_uid;
                        ok = TRUE;
                    }
                }
                else
                {
                    chl_grp = getgrnam (text);
                    if (chl_grp != NULL)
                    {
                        sf_stat.st_gid = chl_grp->gr_gid;
                        ok = TRUE;
                    }
                }

                if (!ok)
                    group_select_current_widget (g);
                else
                {
                    ch_flags[f_pos + 6] = '+';
                    update_ownership ();
                    group_select_current_widget (g);
                    print_flags (h);
                }
            }

            if (result == KEY_LEFT)
            {
                if (!is_owner)
                    chl_end = TRUE;
                group_select_prev_widget (g);
                f_pos--;
            }
            else if (result == KEY_RIGHT)
            {
                if (is_owner)
                    chl_end = TRUE;
                group_select_next_widget (g);
                f_pos++;
            }
        }

        /* Here we used to redraw the window */
        dlg_destroy (chl_dlg);
    }
    while (chl_end);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
advanced_chown_bg_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_DRAW:
        frame_callback (w, NULL, MSG_DRAW, 0, NULL);
        advanced_chown_refresh (DIALOG (w->owner));
        advanced_chown_info_update ();
        return MSG_HANDLED;

    default:
        return frame_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
advanced_chown_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WGroup *g = GROUP (w);
    int i = 0;

    switch (msg)
    {
    case MSG_KEY:
        switch (parm)
        {
        case ALT ('x'):
            i++;
            MC_FALLTHROUGH;

        case ALT ('w'):
            i++;
            MC_FALLTHROUGH;

        case ALT ('r'):
            parm = i + 3;
            for (i = 0; i < 3; i++)
                ch_flags[i * 3 + parm - 3] = (x_toggle & (1 << parm)) ? '-' : '+';
            x_toggle ^= (1 << parm);
            update_mode (g);
            widget_draw (w);
            break;

        case XCTRL ('x'):
            i++;
            MC_FALLTHROUGH;

        case XCTRL ('w'):
            i++;
            MC_FALLTHROUGH;

        case XCTRL ('r'):
            parm = i;
            for (i = 0; i < 3; i++)
                ch_flags[i * 3 + parm] = (x_toggle & (1 << parm)) ? '-' : '+';
            x_toggle ^= (1 << parm);
            update_mode (g);
            widget_draw (w);
            break;

        default:
            break;
        }
        return MSG_NOT_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static WDialog *
advanced_chown_dlg_create (void)
{
    gboolean single_set;
    WDialog *ch_dlg;
    WGroup *ch_grp;
    int lines = 12;
    int cols = 74;
    int i;
    int y;

    memset (ch_flags, '=', 11);
    flag_pos = 0;
    x_toggle = 070;

    single_set = (current_panel->marked < 2);
    if (!single_set)
        lines += 2;

    ch_dlg =
        dlg_create (TRUE, 0, 0, lines, cols, WPOS_CENTER, FALSE, dialog_colors,
                    advanced_chown_callback, NULL, "[Advanced Chown]", _("Chown advanced command"));
    ch_grp = GROUP (ch_dlg);

    /* draw background */
    ch_dlg->bg->callback = advanced_chown_bg_callback;

    l_filename = label_new (2, 3, "");
    group_add_widget (ch_grp, l_filename);

    group_add_widget (ch_grp, hline_new (3, -1, -1));

#define XTRACT(i,y,cb) y, BX+advanced_chown_but[i].x, \
        advanced_chown_but[i].ret_cmd, advanced_chown_but[i].flags, \
        (advanced_chown_but[i].text), cb
    b_att[0] = perm_button_new (XTRACT (0, BY, NULL));
    advanced_chown_but[0].id = group_add_widget (ch_grp, b_att[0]);
    b_att[1] = perm_button_new (XTRACT (1, BY, NULL));
    advanced_chown_but[1].id = group_add_widget (ch_grp, b_att[1]);
    b_att[2] = perm_button_new (XTRACT (2, BY, NULL));
    advanced_chown_but[2].id = group_add_widget (ch_grp, b_att[2]);
    b_user = button_new (XTRACT (3, BY, user_group_button_cb));
    advanced_chown_but[3].id = group_add_widget (ch_grp, b_user);
    b_group = button_new (XTRACT (4, BY, user_group_button_cb));
    advanced_chown_but[4].id = group_add_widget (ch_grp, b_group);

    l_mode = label_new (BY + 2, 3, "");
    group_add_widget (ch_grp, l_mode);

    y = BY + 3;
    if (!single_set)
    {
        i = BUTTONS_PERM;
        group_add_widget (ch_grp, hline_new (y++, -1, -1));
        advanced_chown_but[i].id = group_add_widget (ch_grp,
                                                     button_new (y,
                                                                 WIDGET (ch_dlg)->cols / 2 -
                                                                 advanced_chown_but[i].len,
                                                                 advanced_chown_but[i].ret_cmd,
                                                                 advanced_chown_but[i].flags,
                                                                 advanced_chown_but[i].text, NULL));
        i++;
        advanced_chown_but[i].id = group_add_widget (ch_grp,
                                                     button_new (y, WIDGET (ch_dlg)->cols / 2 + 1,
                                                                 advanced_chown_but[i].ret_cmd,
                                                                 advanced_chown_but[i].flags,
                                                                 advanced_chown_but[i].text, NULL));
        y++;
    }

    i = BUTTONS_PERM + 2;
    group_add_widget (ch_grp, hline_new (y++, -1, -1));
    advanced_chown_but[i].id = group_add_widget (ch_grp,
                                                 button_new (y,
                                                             WIDGET (ch_dlg)->cols / 2 -
                                                             advanced_chown_but[i].len,
                                                             advanced_chown_but[i].ret_cmd,
                                                             advanced_chown_but[i].flags,
                                                             advanced_chown_but[i].text, NULL));
    i++;
    advanced_chown_but[i].id = group_add_widget (ch_grp,
                                                 button_new (y, WIDGET (ch_dlg)->cols / 2 + 1,
                                                             advanced_chown_but[i].ret_cmd,
                                                             advanced_chown_but[i].flags,
                                                             advanced_chown_but[i].text, NULL));

    widget_select (WIDGET (b_att[0]));

    return ch_dlg;
}

/* --------------------------------------------------------------------------------------------- */

static void
advanced_chown_done (gboolean need_update)
{
    if (need_update)
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

static const char *
next_file (void)
{
    while (!current_panel->dir.list[current_file].f.marked)
        current_file++;

    return current_panel->dir.list[current_file].fname;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
try_advanced_chown (const vfs_path_t * p, mode_t m, uid_t u, gid_t g)
{
    int chmod_result;
    const char *fname;

    fname = x_basename (vfs_path_as_str (p));

    while ((chmod_result = mc_chmod (p, m)) == -1 && !ignore_all)
    {
        int my_errno = errno;
        int result;
        char *msg;

        msg = g_strdup_printf (_("Cannot chmod \"%s\"\n%s"), fname, unix_error_string (my_errno));
        result =
            query_dialog (MSG_ERROR, msg, D_ERROR, 4, _("&Ignore"), _("Ignore &all"), _("&Retry"),
                          _("&Cancel"));
        g_free (msg);

        switch (result)
        {
        case 0:
            /* call mc_chown() only, if mc_chmod() didn't fail */
            return TRUE;

        case 1:
            ignore_all = TRUE;
            /* call mc_chown() only, if mc_chmod() didn't fail */
            return TRUE;

        case 2:
            /* retry chmod of this file */
            break;

        case 3:
        default:
            /* stop remain files processing */
            return FALSE;
        }
    }

    /* call mc_chown() only, if mc_chmod didn't fail */
    while (chmod_result != -1 && mc_chown (p, u, g) == -1 && !ignore_all)
    {
        int my_errno = errno;
        int result;
        char *msg;

        msg = g_strdup_printf (_("Cannot chown \"%s\"\n%s"), fname, unix_error_string (my_errno));
        result =
            query_dialog (MSG_ERROR, msg, D_ERROR, 4, _("&Ignore"), _("Ignore &all"), _("&Retry"),
                          _("&Cancel"));
        g_free (msg);

        switch (result)
        {
        case 0:
            /* try next file */
            return TRUE;

        case 1:
            ignore_all = TRUE;
            /* try next file */
            return TRUE;

        case 2:
            /* retry chown of this file */
            break;

        case 3:
        default:
            /* stop remain files processing */
            return FALSE;
        }
    }

    return TRUE;

}

/* --------------------------------------------------------------------------------------------- */

static gboolean
do_advanced_chown (const vfs_path_t * p, mode_t m, uid_t u, gid_t g)
{
    gboolean ret;

    ret = try_advanced_chown (p, m, u, g);

    do_file_mark (current_panel, current_file, 0);

    return ret;
}

 /* --------------------------------------------------------------------------------------------- */

static void
apply_advanced_chowns (vfs_path_t * vpath, struct stat *sf)
{
    gid_t a_gid = sf->st_gid;
    uid_t a_uid = sf->st_uid;
    gboolean ok;

    if (!do_advanced_chown
        (vpath, get_mode (), (ch_flags[9] == '+') ? a_uid : (uid_t) (-1),
         (ch_flags[10] == '+') ? a_gid : (gid_t) (-1)))
        return;

    do
    {
        const char *fname;

        fname = next_file ();
        vpath = vfs_path_from_str (fname);
        ok = (mc_stat (vpath, sf) == 0);

        if (!ok)
        {
            /* if current file was deleted outside mc -- try next file */
            /* decrease current_panel->marked */
            do_file_mark (current_panel, current_file, 0);

            /* try next file */
            ok = TRUE;
        }
        else
        {
            ch_cmode = sf->st_mode;

            ok = do_advanced_chown (vpath, get_mode (),
                                    (ch_flags[9] == '+') ? a_uid : (uid_t) (-1),
                                    (ch_flags[10] == '+') ? a_gid : (gid_t) (-1));
        }

        vfs_path_free (vpath);
    }
    while (ok && current_panel->marked != 0);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
advanced_chown_cmd (void)
{
    gboolean need_update;
    gboolean end_chown;

    /* Number of files at startup */
    int files_on_begin;

    files_on_begin = MAX (1, current_panel->marked);

    advanced_chown_init ();

    current_file = 0;
    ignore_all = FALSE;

    do
    {                           /* do while any files remaining */
        vfs_path_t *vpath;
        WDialog *ch_dlg;
        const char *fname;
        int result;
        int file_idx;

        do_refresh ();

        need_update = FALSE;
        end_chown = FALSE;

        if (current_panel->marked != 0)
            fname = next_file ();       /* next marked file */
        else
            fname = selection (current_panel)->fname;   /* single file */

        vpath = vfs_path_from_str (fname);

        if (mc_stat (vpath, &sf_stat) != 0)
        {
            vfs_path_free (vpath);
            break;
        }

        ch_cmode = sf_stat.st_mode;

        ch_dlg = advanced_chown_dlg_create ();

        file_idx = files_on_begin == 1 ? 1 : (files_on_begin - current_panel->marked + 1);
        label_set_textv (l_filename, "%s (%d/%d)",
                         str_fit_to_term (fname, WIDGET (ch_dlg)->cols - 20, J_LEFT_FIT),
                         file_idx, files_on_begin);
        update_ownership ();

        result = dlg_run (ch_dlg);

        switch (result)
        {
        case B_CANCEL:
            end_chown = TRUE;
            break;

        case B_ENTER:
            if (current_panel->marked <= 1)
            {
                /* single or last file */
                if (mc_chmod (vpath, get_mode ()) == -1)
                    message (D_ERROR, MSG_ERROR, _("Cannot chmod \"%s\"\n%s"),
                             fname, unix_error_string (errno));
                /* call mc_chown only, if mc_chmod didn't fail */
                else if (mc_chown
                         (vpath, (ch_flags[9] == '+') ? sf_stat.st_uid : (uid_t) (-1),
                          (ch_flags[10] == '+') ? sf_stat.st_gid : (gid_t) (-1)) == -1)
                    message (D_ERROR, MSG_ERROR, _("Cannot chown \"%s\"\n%s"), fname,
                             unix_error_string (errno));

                end_chown = TRUE;
            }
            else if (!try_advanced_chown
                     (vpath, get_mode (), (ch_flags[9] == '+') ? sf_stat.st_uid : (uid_t) (-1),
                      (ch_flags[10] == '+') ? sf_stat.st_gid : (gid_t) (-1)))
            {
                /* stop multiple files processing */
                result = B_CANCEL;
                end_chown = TRUE;
            }

            need_update = TRUE;
            break;

        case B_SETALL:
            apply_advanced_chowns (vpath, &sf_stat);
            need_update = TRUE;
            end_chown = TRUE;
            break;

        case B_SKIP:
        default:
            break;
        }

        if (current_panel->marked != 0 && result != B_CANCEL)
        {
            do_file_mark (current_panel, current_file, 0);
            need_update = TRUE;
        }

        vfs_path_free (vpath);

        dlg_destroy (ch_dlg);
    }
    while (current_panel->marked != 0 && !end_chown);

    advanced_chown_done (need_update);
}

/* --------------------------------------------------------------------------------------------- */
