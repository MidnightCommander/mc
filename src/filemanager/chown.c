/*
   Chown command -- for the Midnight Commander

   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007, 2011
   The Free Software Foundation, Inc.

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

/* Needed for the extern declarations of integer parameters */
#include "chmod.h"
#include "midnight.h"           /* current_panel */

#include "chown.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define UX 5
#define UY 2

#define GX 27
#define GY 2

#define BX 5
#define BY 15

#define TX 50
#define TY 2

#define BUTTONS 5

#define B_SETALL        B_USER
#define B_SETUSR        (B_USER + 1)
#define B_SETGRP        (B_USER + 2)

#define LABELS 5

#define chown_label(n,txt) label_set_text (chown_label [n].l, txt)

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static int need_update, end_chown;
static int current_file;
static int single_set;
static WListbox *l_user, *l_group;

/* *INDENT-OFF* */
static struct
{
    int ret_cmd, flags, y, x;
    const char *text;
} chown_but[BUTTONS] = {
    { B_CANCEL, NORMAL_BUTTON,  0, 53, N_("&Cancel") },
    { B_ENTER,  DEFPUSH_BUTTON, 0, 40, N_("&Set") },
    { B_SETUSR, NORMAL_BUTTON,  0, 25, N_("Set &users") },
    { B_SETGRP, NORMAL_BUTTON,  0, 11, N_("Set &groups") },
    { B_SETALL, NORMAL_BUTTON,  0, 0,  N_("Set &all") },
};

static struct {
    int y, x;
    WLabel *l;
} chown_label [LABELS] = {
    { TY +  2, TX + 2, NULL },
    { TY +  4, TX + 2, NULL },
    { TY +  6, TX + 2, NULL },
    { TY +  8, TX + 2, NULL },
    { TY + 10, TX + 2, NULL }
};
/* *INDENT-ON* */

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
chown_refresh (Dlg_head * h)
{
    common_dialog_repaint (h);

    tty_setcolor (COLOR_NORMAL);

    widget_move (h, TY + 1, TX + 2);
    tty_print_string (_("Name"));
    widget_move (h, TY + 3, TX + 2);
    tty_print_string (_("Owner name"));
    widget_move (h, TY + 5, TX + 2);
    tty_print_string (_("Group name"));
    widget_move (h, TY + 7, TX + 2);
    tty_print_string (_("Size"));
    widget_move (h, TY + 9, TX + 2);
    tty_print_string (_("Permission"));
}

/* --------------------------------------------------------------------------------------------- */

static char *
next_file (void)
{
    while (!current_panel->dir.list[current_file].f.marked)
        current_file++;

    return current_panel->dir.list[current_file].fname;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
chown_callback (Dlg_head * h, Widget * sender, dlg_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case DLG_DRAW:
        chown_refresh (h);
        return MSG_HANDLED;

    default:
        return default_dlg_callback (h, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static Dlg_head *
init_chown (void)
{
    int i;
    struct passwd *l_pass;
    struct group *l_grp;
    Dlg_head *ch_dlg;

    do_refresh ();
    end_chown = need_update = current_file = 0;
    single_set = (current_panel->marked < 2) ? 3 : 0;

    ch_dlg =
        create_dlg (TRUE, 0, 0, 18, 74, dialog_colors, chown_callback, NULL, "[Chown]",
                    _("Chown command"), DLG_CENTER | DLG_REVERSE);

    for (i = 0; i < BUTTONS - single_set; i++)
        add_widget (ch_dlg,
                    button_new (BY + chown_but[i].y, BX + chown_but[i].x,
                                chown_but[i].ret_cmd, chown_but[i].flags, _(chown_but[i].text), 0));

    /* Add the widgets for the file information */
    for (i = 0; i < LABELS; i++)
    {
        chown_label[i].l = label_new (chown_label[i].y, chown_label[i].x, "");
        add_widget (ch_dlg, chown_label[i].l);
    }

    /* get new listboxes */
    l_user = listbox_new (UY + 1, UX + 1, 10, 19, FALSE, NULL);
    l_group = listbox_new (GY + 1, GX + 1, 10, 19, FALSE, NULL);

    /* add fields for unknown names (numbers) */
    listbox_add_item (l_user, LISTBOX_APPEND_AT_END, 0, _("<Unknown user>"), NULL);
    listbox_add_item (l_group, LISTBOX_APPEND_AT_END, 0, _("<Unknown group>"), NULL);

    /* get and put user names in the listbox */
    setpwent ();
    while ((l_pass = getpwent ()) != NULL)
    {
        listbox_add_item (l_user, LISTBOX_APPEND_SORTED, 0, l_pass->pw_name, NULL);
    }
    endpwent ();

    /* get and put group names in the listbox */
    setgrent ();
    while ((l_grp = getgrent ()) != NULL)
    {
        listbox_add_item (l_group, LISTBOX_APPEND_SORTED, 0, l_grp->gr_name, NULL);
    }
    endgrent ();

    add_widget (ch_dlg, groupbox_new (TY, TX, 12, 19, _("File")));

    /* add listboxes to the dialogs */
    add_widget (ch_dlg, l_group);
    add_widget (ch_dlg, groupbox_new (GY, GX, 12, 21, _("Group name")));
    add_widget (ch_dlg, l_user);
    add_widget (ch_dlg, groupbox_new (UY, UX, 12, 21, _("User name")));

    return ch_dlg;
}

/* --------------------------------------------------------------------------------------------- */

static void
chown_done (void)
{
    if (need_update)
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

static void
do_chown (uid_t u, gid_t g)
{
    vfs_path_t *vpath;

    vpath = vfs_path_from_str (current_panel->dir.list[current_file].fname);
    if (mc_chown (vpath, u, g) == -1)
        message (D_ERROR, MSG_ERROR, _("Cannot chown \"%s\"\n%s"),
                 current_panel->dir.list[current_file].fname, unix_error_string (errno));

    vfs_path_free (vpath);
    do_file_mark (current_panel, current_file, 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
apply_chowns (uid_t u, gid_t g)
{

    need_update = end_chown = 1;
    do_chown (u, g);

    do
    {
        next_file ();
        do_chown (u, g);
    }
    while (current_panel->marked);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
chown_cmd (void)
{
    char *fname;
    struct stat sf_stat;
    Dlg_head *ch_dlg;
    uid_t new_user;
    gid_t new_group;
    char buffer[BUF_TINY];

    do
    {                           /* do while any files remaining */
        vfs_path_t *vpath;

        ch_dlg = init_chown ();
        new_user = new_group = -1;

        if (current_panel->marked)
            fname = next_file ();       /* next marked file */
        else
            fname = selection (current_panel)->fname;   /* single file */

        vpath = vfs_path_from_str (fname);
        if (mc_stat (vpath, &sf_stat) != 0)
        {                       /* get status of file */
            destroy_dlg (ch_dlg);
            vfs_path_free (vpath);
            break;
        }
        vfs_path_free (vpath);

        /* select in listboxes */
        listbox_select_entry (l_user, listbox_search_text (l_user, get_owner (sf_stat.st_uid)));
        listbox_select_entry (l_group, listbox_search_text (l_group, get_group (sf_stat.st_gid)));

        chown_label (0, str_trunc (fname, 15));
        chown_label (1, str_trunc (get_owner (sf_stat.st_uid), 15));
        chown_label (2, str_trunc (get_group (sf_stat.st_gid), 15));
        size_trunc_len (buffer, 15, sf_stat.st_size, 0, panels_options.kilobyte_si);
        chown_label (3, buffer);
        chown_label (4, string_perm (sf_stat.st_mode));

        switch (run_dlg (ch_dlg))
        {
        case B_CANCEL:
            end_chown = 1;
            break;

        case B_SETUSR:
            {
                struct passwd *user;
                char *text;

                listbox_get_current (l_user, &text, NULL);
                user = getpwnam (text);
                if (user)
                {
                    new_user = user->pw_uid;
                    apply_chowns (new_user, new_group);
                }
                break;
            }

        case B_SETGRP:
            {
                struct group *grp;
                char *text;

                listbox_get_current (l_group, &text, NULL);
                grp = getgrnam (text);
                if (grp)
                {
                    new_group = grp->gr_gid;
                    apply_chowns (new_user, new_group);
                }
                break;
            }

        case B_SETALL:
        case B_ENTER:
            {
                struct group *grp;
                struct passwd *user;
                char *text;

                listbox_get_current (l_group, &text, NULL);
                grp = getgrnam (text);
                if (grp)
                    new_group = grp->gr_gid;
                listbox_get_current (l_user, &text, NULL);
                user = getpwnam (text);
                if (user)
                    new_user = user->pw_uid;
                if (ch_dlg->ret_value == B_ENTER)
                {
                    vfs_path_t *fname_vpath;

                    fname_vpath = vfs_path_from_str (fname);
                    need_update = 1;
                    if (mc_chown (fname_vpath, new_user, new_group) == -1)
                        message (D_ERROR, MSG_ERROR, _("Cannot chown \"%s\"\n%s"),
                                 fname, unix_error_string (errno));
                    vfs_path_free (fname_vpath);
                }
                else
                    apply_chowns (new_user, new_group);
                break;
            }
        }                       /* switch */

        if (current_panel->marked && ch_dlg->ret_value != B_CANCEL)
        {
            do_file_mark (current_panel, current_file, 0);
            need_update = 1;
        }

        destroy_dlg (ch_dlg);
    }
    while (current_panel->marked && !end_chown);

    chown_done ();
}

/* --------------------------------------------------------------------------------------------- */
