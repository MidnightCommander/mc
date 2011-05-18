/* Chmod command -- for the Midnight Commander
   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2007,
   2008, 2009, 2010, 2011 Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  
 */

/** \file chmod.c
 *  \brief Source: chmod command
 */

#include <config.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/skin.h"
#include "lib/vfs/mc-vfs/vfs.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/widget.h"
#include "lib/keybind.h"        /* CK_DialogCancel */

#include "midnight.h"           /* current_panel */
#include "layout.h"             /* repaint_screen() */
#include "chmod.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define PX 5
#define PY 2

#define FX 40
#define FY 2

#define BX 6
#define BY 17

#define TX 40
#define TY 12

#define PERMISSIONS 12
#define BUTTONS 6

#define B_MARKED B_USER
#define B_ALL    (B_USER + 1)
#define B_SETMRK (B_USER + 2)
#define B_CLRMRK (B_USER + 3)

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static int single_set;

static gboolean mode_change, need_update, end_chmod;
static int c_file;

static mode_t and_mask, or_mask, c_stat;

static WLabel *statl;

static struct
{
    mode_t mode;
    const char *text;
    gboolean selected;
    WCheck *check;
} check_perm[PERMISSIONS] =
{
    /* *INDENT-OFF* */
    { S_IXOTH, N_("execute/search by others"), FALSE, NULL },
    { S_IWOTH, N_("write by others"), FALSE, NULL },
    { S_IROTH, N_("read by others"), FALSE, NULL },
    { S_IXGRP, N_("execute/search by group"), FALSE, NULL },
    { S_IWGRP, N_("write by group"), FALSE, NULL },
    { S_IRGRP, N_("read by group"), FALSE, NULL },
    { S_IXUSR, N_("execute/search by owner"), FALSE, NULL },
    { S_IWUSR, N_("write by owner"), FALSE, NULL },
    { S_IRUSR, N_("read by owner"), FALSE, NULL },
    { S_ISVTX, N_("sticky bit"), FALSE, NULL },
    { S_ISGID, N_("set group ID on execution"), FALSE, NULL },
    { S_ISUID, N_("set user ID on execution"), FALSE, NULL }
    /* *INDENT-ON* */
};

static struct
{
    int ret_cmd;
    int flags;
    int y;
    int x;
    const char *text;
} chmod_but[BUTTONS] =
{
    /* *INDENT-OFF* */
    { B_CANCEL, NORMAL_BUTTON, 2, 33, N_("&Cancel") },
    { B_ENTER, DEFPUSH_BUTTON, 2, 17, N_("&Set") },
    { B_CLRMRK, NORMAL_BUTTON, 0, 42, N_("C&lear marked") },
    { B_SETMRK, NORMAL_BUTTON, 0, 27, N_("S&et marked") },
    { B_MARKED, NORMAL_BUTTON, 0, 12, N_("&Marked all") },
    { B_ALL,    NORMAL_BUTTON, 0, 0, N_("Set &all") }
    /* *INDENT-ON* */
};

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
chmod_toggle_select (Dlg_head * h, int Id)
{
    tty_setcolor (COLOR_NORMAL);
    check_perm[Id].selected = !check_perm[Id].selected;

    dlg_move (h, PY + PERMISSIONS - Id, PX + 1);
    tty_print_char (check_perm[Id].selected ? '*' : ' ');
    dlg_move (h, PY + PERMISSIONS - Id, PX + 3);
}

/* --------------------------------------------------------------------------------------------- */

static void
chmod_refresh (Dlg_head * h)
{
    common_dialog_repaint (h);

    tty_setcolor (COLOR_NORMAL);

    dlg_move (h, FY + 1, FX + 2);
    tty_print_string (_("Name"));
    dlg_move (h, FY + 3, FX + 2);
    tty_print_string (_("Permissions (Octal)"));
    dlg_move (h, FY + 5, FX + 2);
    tty_print_string (_("Owner name"));
    dlg_move (h, FY + 7, FX + 2);
    tty_print_string (_("Group name"));
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
chmod_callback (Dlg_head * h, Widget * sender, dlg_msg_t msg, int parm, void *data)
{
    char buffer[BUF_TINY];
    int id;

    id = dlg_get_current_widget_id (h) - BUTTONS + single_set * 2 - 1;

    switch (msg)
    {
    case DLG_ACTION:
        /* close dialog due to SIGINT (ctrl-g) */
        if (sender == NULL && parm == CK_DialogCancel)
            return MSG_NOT_HANDLED;

        /* handle checkboxes */
        if (id >= 0)
        {
            c_stat ^= check_perm[id].mode;
            g_snprintf (buffer, sizeof (buffer), "%o", (unsigned int) c_stat);
            label_set_text (statl, buffer);
            chmod_toggle_select (h, id);
            mode_change = TRUE;
            return MSG_HANDLED;
        }

        return MSG_NOT_HANDLED;

    case DLG_KEY:
        if ((parm == 'T' || parm == 't' || parm == KEY_IC) && id > 0)
        {
            chmod_toggle_select (h, id);
            if (parm == KEY_IC)
                dlg_one_down (h);
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    case DLG_DRAW:
        chmod_refresh (h);
        return MSG_HANDLED;

    default:
        return default_dlg_callback (h, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static Dlg_head *
init_chmod (void)
{
    unsigned int i;
    Dlg_head *ch_dlg;

    do_refresh ();

    need_update = FALSE;
    end_chmod = FALSE;
    c_file = 0;
    single_set = (current_panel->marked < 2) ? 2 : 0;

    ch_dlg =
        create_dlg (TRUE, 0, 0, 22 - single_set, 70, dialog_colors,
                    chmod_callback, "[Chmod]", _("Chmod command"), DLG_CENTER | DLG_REVERSE);

    for (i = 0; i < BUTTONS; i++)
    {
        if (i == 2 && single_set != 0)
            break;

        add_widget (ch_dlg,
                    button_new (BY + chmod_but[i].y - single_set,
                                BX + chmod_but[i].x,
                                chmod_but[i].ret_cmd,
                                chmod_but[i].flags, _(chmod_but[i].text), 0));
    }

    add_widget (ch_dlg, groupbox_new (FY, FX, 10, 25, _("File")));

    for (i = 0; i < PERMISSIONS; i++)
    {
        check_perm[i].check = check_new (PY + (PERMISSIONS - i), PX + 2, 0, _(check_perm[i].text));
        add_widget (ch_dlg, check_perm[i].check);
    }

    add_widget (ch_dlg, groupbox_new (PY, PX, PERMISSIONS + 2, 33, _("Permission")));

    return ch_dlg;
}

/* --------------------------------------------------------------------------------------------- */

static void
chmod_done (void)
{
    if (need_update)
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

static char *
next_file (void)
{
    while (!current_panel->dir.list[c_file].f.marked)
        c_file++;

    return current_panel->dir.list[c_file].fname;
}

/* --------------------------------------------------------------------------------------------- */

static void
do_chmod (struct stat *sf)
{
    sf->st_mode &= and_mask;
    sf->st_mode |= or_mask;

    if (mc_chmod (current_panel->dir.list[c_file].fname, sf->st_mode) == -1)
        message (D_ERROR, MSG_ERROR, _("Cannot chmod \"%s\"\n%s"),
                 current_panel->dir.list[c_file].fname, unix_error_string (errno));

    do_file_mark (current_panel, c_file, 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
apply_mask (struct stat *sf)
{
    need_update = TRUE;
    end_chmod = TRUE;

    do_chmod (sf);

    do
    {
        char *fname;

        fname = next_file ();
        if (mc_stat (fname, sf) != 0)
            return;

        c_stat = sf->st_mode;

        do_chmod (sf);
    }
    while (current_panel->marked != 0);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
chmod_cmd (void)
{
    char *fname;
    struct stat sf_stat;
    char buffer[BUF_TINY];
    unsigned int i;

    do
    {                           /* do while any files remaining */
        Dlg_head *ch_dlg;
        int result;
        const char *c_fname, *c_fown, *c_fgrp;

        ch_dlg = init_chmod ();

        if (current_panel->marked != 0)
            fname = next_file ();       /* next marked file */
        else
            fname = selection (current_panel)->fname;   /* single file */

        if (mc_stat (fname, &sf_stat) != 0)
        {                       /* get status of file */
            destroy_dlg (ch_dlg);
            break;
        }

        c_stat = sf_stat.st_mode;
        mode_change = FALSE;        /* clear changes flag */

        /* set check buttons */
        for (i = 0; i < PERMISSIONS; i++)
            check_perm[i].check->state = (c_stat & check_perm[i].mode) != 0 ? 1 : 0;

        /* Set the labels */
        c_fname = str_trunc (fname, 21);
        add_widget (ch_dlg, label_new (FY + 2, FX + 2, c_fname));
        c_fown = str_trunc (get_owner (sf_stat.st_uid), 21);
        add_widget (ch_dlg, label_new (FY + 6, FX + 2, c_fown));
        c_fgrp = str_trunc (get_group (sf_stat.st_gid), 21);
        add_widget (ch_dlg, label_new (FY + 8, FX + 2, c_fgrp));
        g_snprintf (buffer, sizeof (buffer), "%o", (unsigned int) c_stat);
        statl = label_new (FY + 4, FX + 2, buffer);
        add_widget (ch_dlg, statl);

        /* do action */
        result = run_dlg (ch_dlg);

        switch (result)
        {
        case B_ENTER:
            if (mode_change && mc_chmod (fname, c_stat) == -1)
                message (D_ERROR, MSG_ERROR, _("Cannot chmod \"%s\"\n%s"),
                         fname, unix_error_string (errno));
            need_update = TRUE;
            break;

        case B_CANCEL:
            end_chmod = TRUE;
            break;

        case B_ALL:
        case B_MARKED:
            and_mask = or_mask = 0;
            and_mask = ~and_mask;

            for (i = 0; i < PERMISSIONS; i++)
                if (check_perm[i].selected || result == B_ALL)
                {
                    if (check_perm[i].check->state & C_BOOL)
                        or_mask |= check_perm[i].mode;
                    else
                        and_mask &= ~check_perm[i].mode;
                }

            apply_mask (&sf_stat);
            break;

        case B_SETMRK:
            and_mask = or_mask = 0;
            and_mask = ~and_mask;

            for (i = 0; i < PERMISSIONS; i++)
                if (check_perm[i].selected)
                    or_mask |= check_perm[i].mode;

            apply_mask (&sf_stat);
            break;

        case B_CLRMRK:
            and_mask = or_mask = 0;
            and_mask = ~and_mask;

            for (i = 0; i < PERMISSIONS; i++)
                if (check_perm[i].selected)
                    and_mask &= ~check_perm[i].mode;

            apply_mask (&sf_stat);
            break;
        }

        if (current_panel->marked != 0 && result != B_CANCEL)
        {
            do_file_mark (current_panel, c_file, 0);
            need_update = TRUE;
        }

        destroy_dlg (ch_dlg);
    }
    while (current_panel->marked != 0 && !end_chmod);

    chmod_done ();
}

/* --------------------------------------------------------------------------------------------- */
