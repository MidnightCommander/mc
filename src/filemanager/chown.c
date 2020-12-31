/*
   Chown command -- for the Midnight Commander

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

/** \file chown.c
 *  \brief Source: chown command
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
#include "lib/skin.h"
#include "lib/vfs/vfs.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/widget.h"

#include "src/setup.h"          /* panels_options */

#include "cmd.h"                /* chown_cmd() */

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define GH 12
#define GW 21

#define BUTTONS 5

#define B_SETALL        B_USER
#define B_SETUSR        (B_USER + 1)
#define B_SETGRP        (B_USER + 2)

#define LABELS 5

#define chown_label(n,txt) label_set_text (chown_label [n].l, txt)

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/


static struct
{
    int ret_cmd;
    button_flags_t flags;
    int y;
    int len;
    const char *text;
} chown_but[BUTTONS] =
{
    /* *INDENT-OFF* */
    { B_SETALL,  NORMAL_BUTTON, 5, 0, N_("Set &all")    },
    { B_SETGRP,  NORMAL_BUTTON, 5, 0, N_("Set &groups") },
    { B_SETUSR,  NORMAL_BUTTON, 5, 0, N_("Set &users")  },
    { B_ENTER,  DEFPUSH_BUTTON, 3, 0, N_("&Set")        },
    { B_CANCEL,  NORMAL_BUTTON, 3, 0, N_("&Cancel")     }
    /* *INDENT-ON* */
};

/* summary length of three buttons */
static int blen = 0;

static struct
{
    int y;
    WLabel *l;
} chown_label[LABELS] =
{
    /* *INDENT-OFF* */
    {  4, NULL },
    {  6, NULL },
    {  8, NULL },
    { 10, NULL },
    { 12, NULL }
    /* *INDENT-ON* */
};

static int current_file;
static gboolean ignore_all;

static WListbox *l_user, *l_group;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
chown_init (void)
{
    static gboolean i18n = FALSE;
    int i;

    if (i18n)
        return;

    i18n = TRUE;

#ifdef ENABLE_NLS
    for (i = 0; i < BUTTONS; i++)
        chown_but[i].text = _(chown_but[i].text);
#endif /* ENABLE_NLS */

    for (i = 0; i < BUTTONS; i++)
    {
        chown_but[i].len = str_term_width1 (chown_but[i].text) + 3;     /* [], spaces and w/o & */
        if (chown_but[i].flags == DEFPUSH_BUTTON)
            chown_but[i].len += 2;      /* <> */

        if (i < BUTTONS - 2)
            blen += chown_but[i].len;
    }

    blen += 2;
}

/* --------------------------------------------------------------------------------------------- */

static void
chown_refresh (const Widget * h)
{
    int y = 3;
    int x = 7 + GW * 2;

    tty_setcolor (COLOR_NORMAL);

    widget_gotoyx (h, y + 0, x);
    tty_print_string (_("Name"));
    widget_gotoyx (h, y + 2, x);
    tty_print_string (_("Owner name"));
    widget_gotoyx (h, y + 4, x);
    tty_print_string (_("Group name"));
    widget_gotoyx (h, y + 6, x);
    tty_print_string (_("Size"));
    widget_gotoyx (h, y + 8, x);
    tty_print_string (_("Permission"));
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
chown_bg_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_DRAW:
        frame_callback (w, NULL, MSG_DRAW, 0, NULL);
        chown_refresh (WIDGET (w->owner));
        return MSG_HANDLED;

    default:
        return frame_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static WDialog *
chown_dlg_create (WPanel * panel)
{
    int single_set;
    WDialog *ch_dlg;
    WGroup *g;
    int lines, cols;
    int i, y;
    struct passwd *l_pass;
    struct group *l_grp;

    single_set = (panel->marked < 2) ? 3 : 0;
    lines = GH + 4 + (single_set != 0 ? 2 : 4);
    cols = GW * 3 + 2 + 6;

    ch_dlg =
        dlg_create (TRUE, 0, 0, lines, cols, WPOS_CENTER, FALSE, dialog_colors, NULL, NULL,
                    "[Chown]", _("Chown command"));
    g = GROUP (ch_dlg);

    /* draw background */
    ch_dlg->bg->callback = chown_bg_callback;

    group_add_widget (g, groupbox_new (2, 3, GH, GW, _("User name")));
    l_user = listbox_new (3, 4, GH - 2, GW - 2, FALSE, NULL);
    group_add_widget (g, l_user);
    /* add field for unknown names (numbers) */
    listbox_add_item (l_user, LISTBOX_APPEND_AT_END, 0, _("<Unknown user>"), NULL, FALSE);
    /* get and put user names in the listbox */
    setpwent ();
    while ((l_pass = getpwent ()) != NULL)
        listbox_add_item (l_user, LISTBOX_APPEND_SORTED, 0, l_pass->pw_name, NULL, FALSE);
    endpwent ();

    group_add_widget (g, groupbox_new (2, 4 + GW, GH, GW, _("Group name")));
    l_group = listbox_new (3, 5 + GW, GH - 2, GW - 2, FALSE, NULL);
    group_add_widget (g, l_group);
    /* add field for unknown names (numbers) */
    listbox_add_item (l_group, LISTBOX_APPEND_AT_END, 0, _("<Unknown group>"), NULL, FALSE);
    /* get and put group names in the listbox */
    setgrent ();
    while ((l_grp = getgrent ()) != NULL)
        listbox_add_item (l_group, LISTBOX_APPEND_SORTED, 0, l_grp->gr_name, NULL, FALSE);
    endgrent ();

    group_add_widget (g, groupbox_new (2, 5 + GW * 2, GH, GW, _("File")));
    /* add widgets for the file information */
    for (i = 0; i < LABELS; i++)
    {
        chown_label[i].l = label_new (chown_label[i].y, 7 + GW * 2, "");
        group_add_widget (g, chown_label[i].l);
    }

    if (single_set == 0)
    {
        int x;

        group_add_widget (g, hline_new (lines - chown_but[0].y - 1, -1, -1));

        y = lines - chown_but[0].y;
        x = (cols - blen) / 2;

        for (i = 0; i < BUTTONS - 2; i++)
        {
            group_add_widget (g, button_new (y, x, chown_but[i].ret_cmd, chown_but[i].flags,
                                             chown_but[i].text, NULL));
            x += chown_but[i].len + 1;
        }
    }

    i = BUTTONS - 2;
    y = lines - chown_but[i].y;
    group_add_widget (g, hline_new (y - 1, -1, -1));
    group_add_widget (g, button_new (y, WIDGET (ch_dlg)->cols / 2 - chown_but[i].len,
                                     chown_but[i].ret_cmd, chown_but[i].flags, chown_but[i].text,
                                     NULL));
    i++;
    group_add_widget (g, button_new (y, WIDGET (ch_dlg)->cols / 2 + 1, chown_but[i].ret_cmd,
                                     chown_but[i].flags, chown_but[i].text, NULL));

    /* select first listbox */
    widget_select (WIDGET (l_user));

    return ch_dlg;
}

/* --------------------------------------------------------------------------------------------- */

static void
chown_done (gboolean need_update)
{
    if (need_update)
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

static const char *
next_file (const WPanel * panel)
{
    while (!panel->dir.list[current_file].f.marked)
        current_file++;

    return panel->dir.list[current_file].fname;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
try_chown (const vfs_path_t * p, uid_t u, gid_t g)
{
    while (mc_chown (p, u, g) == -1 && !ignore_all)
    {
        int my_errno = errno;
        int result;
        char *msg;

        msg =
            g_strdup_printf (_("Cannot chown \"%s\"\n%s"), x_basename (vfs_path_as_str (p)),
                             unix_error_string (my_errno));
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
            /* retry this file */
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
do_chown (WPanel * panel, const vfs_path_t * p, uid_t u, gid_t g)
{
    gboolean ret;

    ret = try_chown (p, u, g);

    do_file_mark (panel, current_file, 0);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static void
apply_chowns (WPanel * panel, vfs_path_t * vpath, uid_t u, gid_t g)
{
    gboolean ok;

    if (!do_chown (panel, vpath, u, g))
        return;

    do
    {
        const char *fname;
        struct stat sf;

        fname = next_file (panel);
        vpath = vfs_path_from_str (fname);
        ok = (mc_stat (vpath, &sf) == 0);

        if (!ok)
        {
            /* if current file was deleted outside mc -- try next file */
            /* decrease panel->marked */
            do_file_mark (panel, current_file, 0);

            /* try next file */
            ok = TRUE;
        }
        else
            ok = do_chown (panel, vpath, u, g);

        vfs_path_free (vpath);
    }
    while (ok && panel->marked != 0);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
chown_cmd (WPanel * panel)
{
    gboolean need_update;
    gboolean end_chown;

    chown_init ();

    current_file = 0;
    ignore_all = FALSE;

    do
    {                           /* do while any files remaining */
        vfs_path_t *vpath;
        WDialog *ch_dlg;
        struct stat sf_stat;
        const char *fname;
        int result;
        char buffer[BUF_TINY];
        uid_t new_user = (uid_t) (-1);
        gid_t new_group = (gid_t) (-1);

        do_refresh ();

        need_update = FALSE;
        end_chown = FALSE;

        if (panel->marked != 0)
            fname = next_file (panel);  /* next marked file */
        else
            fname = selection (panel)->fname;   /* single file */

        vpath = vfs_path_from_str (fname);

        if (mc_stat (vpath, &sf_stat) != 0)
        {
            vfs_path_free (vpath);
            break;
        }

        ch_dlg = chown_dlg_create (panel);

        /* select in listboxes */
        listbox_select_entry (l_user, listbox_search_text (l_user, get_owner (sf_stat.st_uid)));
        listbox_select_entry (l_group, listbox_search_text (l_group, get_group (sf_stat.st_gid)));

        chown_label (0, str_trunc (fname, GW - 4));
        chown_label (1, str_trunc (get_owner (sf_stat.st_uid), GW - 4));
        chown_label (2, str_trunc (get_group (sf_stat.st_gid), GW - 4));
        size_trunc_len (buffer, GW - 4, sf_stat.st_size, 0, panels_options.kilobyte_si);
        chown_label (3, buffer);
        chown_label (4, string_perm (sf_stat.st_mode));

        result = dlg_run (ch_dlg);

        switch (result)
        {
        case B_CANCEL:
            end_chown = TRUE;
            break;

        case B_ENTER:
        case B_SETALL:
            {
                struct group *grp;
                struct passwd *user;
                char *text;

                listbox_get_current (l_group, &text, NULL);
                grp = getgrnam (text);
                if (grp != NULL)
                    new_group = grp->gr_gid;
                listbox_get_current (l_user, &text, NULL);
                user = getpwnam (text);
                if (user != NULL)
                    new_user = user->pw_uid;
                if (result == B_ENTER)
                {
                    if (panel->marked <= 1)
                    {
                        /* single or last file */
                        if (mc_chown (vpath, new_user, new_group) == -1)
                            message (D_ERROR, MSG_ERROR, _("Cannot chown \"%s\"\n%s"),
                                     fname, unix_error_string (errno));
                        end_chown = TRUE;
                    }
                    else if (!try_chown (vpath, new_user, new_group))
                    {
                        /* stop multiple files processing */
                        result = B_CANCEL;
                        end_chown = TRUE;
                    }
                }
                else
                {
                    apply_chowns (panel, vpath, new_user, new_group);
                    end_chown = TRUE;
                }

                need_update = TRUE;
                break;
            }

        case B_SETUSR:
            {
                struct passwd *user;
                char *text;

                listbox_get_current (l_user, &text, NULL);
                user = getpwnam (text);
                if (user != NULL)
                {
                    new_user = user->pw_uid;
                    apply_chowns (panel, vpath, new_user, new_group);
                    need_update = TRUE;
                    end_chown = TRUE;
                }
                break;
            }

        case B_SETGRP:
            {
                struct group *grp;
                char *text;

                listbox_get_current (l_group, &text, NULL);
                grp = getgrnam (text);
                if (grp != NULL)
                {
                    new_group = grp->gr_gid;
                    apply_chowns (panel, vpath, new_user, new_group);
                    need_update = TRUE;
                    end_chown = TRUE;
                }
                break;
            }

        default:
            break;
        }

        if (panel->marked != 0 && result != B_CANCEL)
        {
            do_file_mark (panel, current_file, 0);
            need_update = TRUE;
        }

        vfs_path_free (vpath);

        dlg_destroy (ch_dlg);
    }
    while (panel->marked != 0 && !end_chown);

    chown_done (need_update);
}

/* --------------------------------------------------------------------------------------------- */
