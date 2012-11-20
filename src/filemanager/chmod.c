/*
   Chmod command -- for the Midnight Commander

   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2007,
   2008, 2009, 2010, 2011
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
#include "lib/vfs/vfs.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/widget.h"
#include "lib/keybind.h"        /* CK_Cancel */

#include "midnight.h"           /* current_panel */
#include "chmod.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define PX 3
#define PY 2

#define B_MARKED B_USER
#define B_ALL    (B_USER + 1)
#define B_SETMRK (B_USER + 2)
#define B_CLRMRK (B_USER + 3)

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static gboolean single_set;

static gboolean mode_change, need_update, end_chmod;
static int c_file;

static mode_t and_mask, or_mask, c_stat;

static WLabel *statl;
static WGroupbox *file_gb;

static struct
{
    mode_t mode;
    const char *text;
    gboolean selected;
    WCheck *check;
} check_perm[] =
{
    /* *INDENT-OFF* */
    { S_ISUID, N_("set &user ID on execution"), FALSE, NULL },
    { S_ISGID, N_("set &group ID on execution"), FALSE, NULL },
    { S_ISVTX, N_("stick&y bit"), FALSE, NULL },
    { S_IRUSR, N_("&read by owner"), FALSE, NULL },
    { S_IWUSR, N_("&write by owner"), FALSE, NULL },
    { S_IXUSR, N_("e&xecute/search by owner"), FALSE, NULL },
    { S_IRGRP, N_("rea&d by group"), FALSE, NULL },
    { S_IWGRP, N_("write by grou&p"), FALSE, NULL },
    { S_IXGRP, N_("execu&te/search by group"), FALSE, NULL },
    { S_IROTH, N_("read &by others"), FALSE, NULL },
    { S_IWOTH, N_("wr&ite by others"), FALSE, NULL },
    { S_IXOTH, N_("execute/searc&h by others"), FALSE, NULL }
    /* *INDENT-ON* */
};

static const unsigned int check_perm_num = G_N_ELEMENTS (check_perm);
static int check_perm_len = 0;

static const char *file_info_labels[] = {
    N_("Name:"),
    N_("Permissions (octal):"),
    N_("Owner name:"),
    N_("Group name:")
};

static const unsigned int file_info_labels_num = G_N_ELEMENTS (file_info_labels);
static int file_info_labels_len = 0;

static struct
{
    int ret_cmd;
    int flags;
    int y;                      /* vertical position relatively to dialog bottom boundary */
    int len;
    const char *text;
} chmod_but[] =
{
    /* *INDENT-OFF* */
    { B_ALL,    NORMAL_BUTTON, 6, 0, N_("Set &all") },
    { B_MARKED, NORMAL_BUTTON, 6, 0, N_("&Marked all") },
    { B_SETMRK, NORMAL_BUTTON, 5, 0, N_("S&et marked") },
    { B_CLRMRK, NORMAL_BUTTON, 5, 0, N_("C&lear marked") },
    { B_ENTER, DEFPUSH_BUTTON, 3, 0, N_("&Set") },
    { B_CANCEL, NORMAL_BUTTON, 3, 0, N_("&Cancel") }
    /* *INDENT-ON* */
};

static const unsigned int chmod_but_num = G_N_ELEMENTS (chmod_but);

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
chmod_i18n (void)
{
    static gboolean i18n = FALSE;
    unsigned int i;
    int len;

    if (i18n)
        return;

    i18n = TRUE;

#ifdef ENABLE_NLS
    for (i = 0; i < check_perm_num; i++)
        check_perm[i].text = _(check_perm[i].text);

    for (i = 0; i < file_info_labels_num; i++)
        file_info_labels[i] = _(file_info_labels[i]);

    for (i = 0; i < chmod_but_num; i++)
        chmod_but[i].text = _(chmod_but[i].text);
#endif /* ENABLE_NLS */

    for (i = 0; i < check_perm_num; i++)
    {
        len = str_term_width1 (check_perm[i].text);
        check_perm_len = max (check_perm_len, len);
    }

    check_perm_len += 1 + 3 + 1;        /* mark, [x] and space */

    for (i = 0; i < file_info_labels_num; i++)
    {
        len = str_term_width1 (file_info_labels[i]) + 2;        /* spaces around */
        file_info_labels_len = max (file_info_labels_len, len);
    }

    for (i = 0; i < chmod_but_num; i++)
    {
        chmod_but[i].len = str_term_width1 (chmod_but[i].text) + 3;     /* [], spaces and w/o & */
        if (chmod_but[i].flags == DEFPUSH_BUTTON)
            chmod_but[i].len += 2;      /* <> */
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
chmod_toggle_select (WDialog * h, int Id)
{
    tty_setcolor (COLOR_NORMAL);
    check_perm[Id].selected = !check_perm[Id].selected;

    widget_move (h, PY + Id + 1, PX + 1);
    tty_print_char (check_perm[Id].selected ? '*' : ' ');
    widget_move (h, PY + Id + 1, PX + 3);
}

/* --------------------------------------------------------------------------------------------- */

static void
chmod_refresh (WDialog * h)
{
    int y = WIDGET (file_gb)->y + 1;
    int x = WIDGET (file_gb)->x + 2;

    dlg_default_repaint (h);

    tty_setcolor (COLOR_NORMAL);

    tty_gotoyx (y, x);
    tty_print_string (file_info_labels[0]);
    tty_gotoyx (y + 2, x);
    tty_print_string (file_info_labels[1]);
    tty_gotoyx (y + 4, x);
    tty_print_string (file_info_labels[2]);
    tty_gotoyx (y + 6, x);
    tty_print_string (file_info_labels[3]);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
chmod_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WDialog *h = DIALOG (w);

    switch (msg)
    {
    case MSG_ACTION:
        {
            /* handle checkboxes */
            unsigned int i;

            /* close dialog due to SIGINT (ctrl-g) */
            if (sender == NULL && parm == CK_Cancel)
                return MSG_NOT_HANDLED;

            /* whether action was sent by checkbox? */
            for (i = 0; i < check_perm_num; i++)
                if (sender == WIDGET (check_perm[i].check))
                    break;

            if (i < check_perm_num)
            {
                char buffer[BUF_TINY];

                c_stat ^= check_perm[i].mode;
                g_snprintf (buffer, sizeof (buffer), "%o", (unsigned int) c_stat);
                label_set_text (statl, buffer);
                chmod_toggle_select (h, i);
                mode_change = TRUE;
                return MSG_HANDLED;
            }
        }

        return MSG_NOT_HANDLED;

    case MSG_KEY:
        if (parm == 'T' || parm == 't' || parm == KEY_IC)
        {
            unsigned int i;
            unsigned long id;

            id = dlg_get_current_widget_id (h);
            for (i = 0; i < check_perm_num; i++)
                if (id == WIDGET (check_perm[i].check)->id)
                    break;

            if (i < check_perm_num)
            {
                chmod_toggle_select (h, i);
                if (parm == KEY_IC)
                    dlg_one_down (h);
                return MSG_HANDLED;
            }
        }
        return MSG_NOT_HANDLED;

    case MSG_DRAW:
        chmod_refresh (h);
        return MSG_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static WDialog *
init_chmod (const char *fname, const struct stat *sf_stat)
{
    WDialog *ch_dlg;
    int lines, cols;
    int y;
    int perm_gb_len;
    int file_gb_len;
    unsigned int i;
    const char *c_fname, *c_fown, *c_fgrp;
    char buffer[BUF_TINY];

    single_set = (current_panel->marked < 2);
    perm_gb_len = check_perm_len + 2;
    file_gb_len = file_info_labels_len + 2;
    cols = str_term_width1 (fname) + 2 + 1;
    file_gb_len = max (file_gb_len, cols);

    lines = single_set ? 20 : 23;
    cols = perm_gb_len + file_gb_len + 1 + 6;

    if (cols > COLS)
    {
        /* shrink the right groupbox */
        cols = COLS;
        file_gb_len = cols - (perm_gb_len + 1 + 6);
    }

    ch_dlg =
        create_dlg (TRUE, 0, 0, lines, cols, dialog_colors,
                    chmod_callback, NULL, "[Chmod]", _("Chmod command"), DLG_CENTER);

    add_widget (ch_dlg, groupbox_new (PY, PX, check_perm_num + 2, perm_gb_len, _("Permission")));

    for (i = 0; i < check_perm_num; i++)
    {
        check_perm[i].check = check_new (PY + i + 1, PX + 2,
                                         (c_stat & check_perm[i].mode) != 0 ? 1 : 0,
                                         check_perm[i].text);
        add_widget (ch_dlg, check_perm[i].check);
    }

    file_gb = groupbox_new (PY, PX + perm_gb_len + 1, check_perm_num + 2, file_gb_len, _("File"));
    add_widget (ch_dlg, file_gb);

    /* Set the labels */
    y = PY + 2;
    cols = PX + perm_gb_len + 3;
    c_fname = str_trunc (fname, file_gb_len - 3);
    add_widget (ch_dlg, label_new (y, cols, c_fname));
    g_snprintf (buffer, sizeof (buffer), "%o", (unsigned int) c_stat);
    statl = label_new (y + 2, cols, buffer);
    add_widget (ch_dlg, statl);
    c_fown = str_trunc (get_owner (sf_stat->st_uid), file_gb_len - 3);
    add_widget (ch_dlg, label_new (y + 4, cols, c_fown));
    c_fgrp = str_trunc (get_group (sf_stat->st_gid), file_gb_len - 3);
    add_widget (ch_dlg, label_new (y + 6, cols, c_fgrp));

    if (!single_set)
    {
        i = 0;
        add_widget (ch_dlg, hline_new (lines - chmod_but[i].y - 1, -1, -1));
        for (; i < chmod_but_num - 2; i++)
        {
            y = lines - chmod_but[i].y;
            add_widget (ch_dlg,
                        button_new (y, WIDGET (ch_dlg)->cols / 2 - chmod_but[i].len,
                                    chmod_but[i].ret_cmd, chmod_but[i].flags, chmod_but[i].text,
                                    NULL));
            i++;
            add_widget (ch_dlg,
                        button_new (y, WIDGET (ch_dlg)->cols / 2 + 1,
                                    chmod_but[i].ret_cmd, chmod_but[i].flags, chmod_but[i].text,
                                    NULL));
        }
    }

    i = chmod_but_num - 2;
    y = lines - chmod_but[i].y;
    add_widget (ch_dlg, hline_new (y - 1, -1, -1));
    add_widget (ch_dlg,
                button_new (y, WIDGET (ch_dlg)->cols / 2 - chmod_but[i].len, chmod_but[i].ret_cmd,
                            chmod_but[i].flags, chmod_but[i].text, NULL));
    i++;
    add_widget (ch_dlg,
                button_new (y, WIDGET (ch_dlg)->cols / 2 + 1, chmod_but[i].ret_cmd,
                            chmod_but[i].flags, chmod_but[i].text, NULL));

    /* select first checkbox */
    dlg_select_widget (check_perm[0].check);

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
    vfs_path_t *vpath;
    sf->st_mode &= and_mask;
    sf->st_mode |= or_mask;

    vpath = vfs_path_from_str (current_panel->dir.list[c_file].fname);
    if (mc_chmod (vpath, sf->st_mode) == -1)
        message (D_ERROR, MSG_ERROR, _("Cannot chmod \"%s\"\n%s"),
                 current_panel->dir.list[c_file].fname, unix_error_string (errno));

    vfs_path_free (vpath);
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
        vfs_path_t *vpath;
        gboolean ok;

        fname = next_file ();
        vpath = vfs_path_from_str (fname);
        ok = (mc_stat (vpath, sf) == 0);
        vfs_path_free (vpath);
        if (!ok)
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
    chmod_i18n ();

    do
    {                           /* do while any files remaining */
        vfs_path_t *vpath;
        WDialog *ch_dlg;
        struct stat sf_stat;
        char *fname;
        int result;
        unsigned int i;

        do_refresh ();

        mode_change = FALSE;
        need_update = FALSE;
        end_chmod = FALSE;
        c_file = 0;

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

        c_stat = sf_stat.st_mode;

        ch_dlg = init_chmod (fname, &sf_stat);

        /* do action */
        result = run_dlg (ch_dlg);

        switch (result)
        {
        case B_ENTER:
            if (mode_change && mc_chmod (vpath, c_stat) == -1)
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

            for (i = 0; i < check_perm_num; i++)
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

            for (i = 0; i < check_perm_num; i++)
                if (check_perm[i].selected)
                    or_mask |= check_perm[i].mode;

            apply_mask (&sf_stat);
            break;

        case B_CLRMRK:
            and_mask = or_mask = 0;
            and_mask = ~and_mask;

            for (i = 0; i < check_perm_num; i++)
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

        vfs_path_free (vpath);

        destroy_dlg (ch_dlg);
    }
    while (current_panel->marked != 0 && !end_chmod);

    chmod_done ();
}

/* --------------------------------------------------------------------------------------------- */
