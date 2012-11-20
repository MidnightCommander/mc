/*
   Chown command -- for the Midnight Commander

   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007, 2011, 2012
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

static int need_update, end_chown;
static int current_file;
static int single_set;
static WListbox *l_user, *l_group;

/* *INDENT-OFF* */
static struct
{
    int ret_cmd, flags, y, len;
    const char *text;
} chown_but[BUTTONS] = {
    { B_SETALL, NORMAL_BUTTON,  5, 0, N_("Set &all") },
    { B_SETGRP, NORMAL_BUTTON,  5, 0, N_("Set &groups") },
    { B_SETUSR, NORMAL_BUTTON,  5, 0, N_("Set &users") },
    { B_ENTER,  DEFPUSH_BUTTON, 3, 0, N_("&Set") },
    { B_CANCEL, NORMAL_BUTTON,  3, 0, N_("&Cancel") },
};

/* summary length of three buttons */
static unsigned int blen = 0;

static struct {
    int y;
    WLabel *l;
} chown_label [LABELS] = {
    { 4 , NULL },
    { 6 , NULL },
    { 8 , NULL },
    { 10 , NULL },
    { 12, NULL }
};
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
chown_i18n (void)
{
    static gboolean i18n = FALSE;
    unsigned int i;

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
chown_refresh (WDialog * h)
{
    const int y = 3;
    const int x = 7 + GW * 2;

    dlg_default_repaint (h);

    tty_setcolor (COLOR_NORMAL);

    widget_move (h, y + 0, x);
    tty_print_string (_("Name"));
    widget_move (h, y + 2, x);
    tty_print_string (_("Owner name"));
    widget_move (h, y + 4, x);
    tty_print_string (_("Group name"));
    widget_move (h, y + 6, x);
    tty_print_string (_("Size"));
    widget_move (h, y + 8, x);
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
chown_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_DRAW:
        chown_refresh (DIALOG (w));
        return MSG_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static WDialog *
init_chown (void)
{
    int lines, cols;
    int i;
    int y;
    struct passwd *l_pass;
    struct group *l_grp;
    WDialog *ch_dlg;

    do_refresh ();

    end_chown = need_update = current_file = 0;
    single_set = (current_panel->marked < 2) ? 3 : 0;

    cols = GW * 3 + 2 + 6;
    lines = GH + 4 + (single_set ? 2 : 4);

    ch_dlg =
        create_dlg (TRUE, 0, 0, lines, cols, dialog_colors, chown_callback, NULL, "[Chown]",
                    _("Chown command"), DLG_CENTER);

    add_widget (ch_dlg, groupbox_new (2, 3, GH, GW, _("User name")));
    l_user = listbox_new (3, 4, GH - 2, GW - 2, FALSE, NULL);
    add_widget (ch_dlg, l_user);
    /* add field for unknown names (numbers) */
    listbox_add_item (l_user, LISTBOX_APPEND_AT_END, 0, _("<Unknown user>"), NULL);
    /* get and put user names in the listbox */
    setpwent ();
    while ((l_pass = getpwent ()) != NULL)
        listbox_add_item (l_user, LISTBOX_APPEND_SORTED, 0, l_pass->pw_name, NULL);
    endpwent ();

    add_widget (ch_dlg, groupbox_new (2, 4 + GW, GH, GW, _("Group name")));
    l_group = listbox_new (3, 5 + GW, GH - 2, GW - 2, FALSE, NULL);
    add_widget (ch_dlg, l_group);
    /* add field for unknown names (numbers) */
    listbox_add_item (l_group, LISTBOX_APPEND_AT_END, 0, _("<Unknown group>"), NULL);
    /* get and put group names in the listbox */
    setgrent ();
    while ((l_grp = getgrent ()) != NULL)
        listbox_add_item (l_group, LISTBOX_APPEND_SORTED, 0, l_grp->gr_name, NULL);
    endgrent ();

    add_widget (ch_dlg, groupbox_new (2, 5 + GW * 2, GH, GW, _("File")));
    /* add widgets for the file information */
    for (i = 0; i < LABELS; i++)
    {
        chown_label[i].l = label_new (chown_label[i].y, 7 + GW * 2, "");
        add_widget (ch_dlg, chown_label[i].l);
    }

    if (!single_set)
    {
        int x;

        add_widget (ch_dlg, hline_new (lines - chown_but[0].y - 1, -1, -1));

        y = lines - chown_but[0].y;
        x = (cols - blen) / 2;

        for (i = 0; i < BUTTONS - 2; i++)
        {
            add_widget (ch_dlg,
                        button_new (y, x, chown_but[i].ret_cmd, chown_but[i].flags,
                                    chown_but[i].text, NULL));
            x += chown_but[i].len + 1;
        }
    }

    i = BUTTONS - 2;
    y = lines - chown_but[i].y;
    add_widget (ch_dlg, hline_new (y - 1, -1, -1));
    add_widget (ch_dlg,
                button_new (y, WIDGET (ch_dlg)->cols / 2 - chown_but[i].len, chown_but[i].ret_cmd,
                            chown_but[i].flags, chown_but[i].text, NULL));
    i++;
    add_widget (ch_dlg,
                button_new (y, WIDGET (ch_dlg)->cols / 2 + 1, chown_but[i].ret_cmd,
                            chown_but[i].flags, chown_but[i].text, NULL));

    /* select first listbox */
    dlg_select_widget (l_user);

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
    WDialog *ch_dlg;
    uid_t new_user;
    gid_t new_group;
    char buffer[BUF_TINY];

    chown_i18n ();

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

        chown_label (0, str_trunc (fname, GW - 4));
        chown_label (1, str_trunc (get_owner (sf_stat.st_uid), GW - 4));
        chown_label (2, str_trunc (get_group (sf_stat.st_gid), GW - 4));
        size_trunc_len (buffer, GW - 4, sf_stat.st_size, 0, panels_options.kilobyte_si);
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
