/*
   Chmod command -- for the Midnight Commander

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

#include "filemanager.h"        /* current_panel */

#include "cmd.h"                /* chmod_cmd() */

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define PX 3
#define PY 2

#define B_MARKED B_USER
#define B_SETALL (B_USER + 1)
#define B_SETMRK (B_USER + 2)
#define B_CLRMRK (B_USER + 3)

#define BUTTONS      6
#define BUTTONS_PERM 12
#define LABELS       4

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static struct
{
    mode_t mode;
    const char *text;
    gboolean selected;
    WCheck *check;
} check_perm[BUTTONS_PERM] =
{
    /* *INDENT-OFF* */
    { S_ISUID, N_("set &user ID on execution"),  FALSE, NULL },
    { S_ISGID, N_("set &group ID on execution"), FALSE, NULL },
    { S_ISVTX, N_("stick&y bit"),                FALSE, NULL },
    { S_IRUSR, N_("&read by owner"),             FALSE, NULL },
    { S_IWUSR, N_("&write by owner"),            FALSE, NULL },
    { S_IXUSR, N_("e&xecute/search by owner"),   FALSE, NULL },
    { S_IRGRP, N_("rea&d by group"),             FALSE, NULL },
    { S_IWGRP, N_("write by grou&p"),            FALSE, NULL },
    { S_IXGRP, N_("execu&te/search by group"),   FALSE, NULL },
    { S_IROTH, N_("read &by others"),            FALSE, NULL },
    { S_IWOTH, N_("wr&ite by others"),           FALSE, NULL },
    { S_IXOTH, N_("execute/searc&h by others"),  FALSE, NULL }
    /* *INDENT-ON* */
};

static int check_perm_len = 0;

static const char *file_info_labels[LABELS] = {
    N_("Name:"),
    N_("Permissions (octal):"),
    N_("Owner name:"),
    N_("Group name:")
};

static int file_info_labels_len = 0;

static struct
{
    int ret_cmd;
    button_flags_t flags;
    int y;                      /* vertical position relatively to dialog bottom boundary */
    int len;
    const char *text;
} chmod_but[BUTTONS] =
{
    /* *INDENT-OFF* */
    { B_SETALL, NORMAL_BUTTON, 6, 0, N_("Set &all")      },
    { B_MARKED, NORMAL_BUTTON, 6, 0, N_("&Marked all")   },
    { B_SETMRK, NORMAL_BUTTON, 5, 0, N_("S&et marked")   },
    { B_CLRMRK, NORMAL_BUTTON, 5, 0, N_("C&lear marked") },
    { B_ENTER, DEFPUSH_BUTTON, 3, 0, N_("&Set")          },
    { B_CANCEL, NORMAL_BUTTON, 3, 0, N_("&Cancel")       }
    /* *INDENT-ON* */
};

static gboolean mode_change;
static int current_file;
static gboolean ignore_all;

static mode_t and_mask, or_mask, ch_mode;

static WLabel *statl;
static WGroupbox *file_gb;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
chmod_init (void)
{
    static gboolean i18n = FALSE;
    int i, len;

    for (i = 0; i < BUTTONS_PERM; i++)
        check_perm[i].selected = FALSE;

    if (i18n)
        return;

    i18n = TRUE;

#ifdef ENABLE_NLS
    for (i = 0; i < BUTTONS_PERM; i++)
        check_perm[i].text = _(check_perm[i].text);

    for (i = 0; i < LABELS; i++)
        file_info_labels[i] = _(file_info_labels[i]);

    for (i = 0; i < BUTTONS; i++)
        chmod_but[i].text = _(chmod_but[i].text);
#endif /* ENABLE_NLS */

    for (i = 0; i < BUTTONS_PERM; i++)
    {
        len = str_term_width1 (check_perm[i].text);
        check_perm_len = MAX (check_perm_len, len);
    }

    check_perm_len += 1 + 3 + 1;        /* mark, [x] and space */

    for (i = 0; i < LABELS; i++)
    {
        len = str_term_width1 (file_info_labels[i]) + 2;        /* spaces around */
        file_info_labels_len = MAX (file_info_labels_len, len);
    }

    for (i = 0; i < BUTTONS; i++)
    {
        chmod_but[i].len = str_term_width1 (chmod_but[i].text) + 3;     /* [], spaces and w/o & */
        if (chmod_but[i].flags == DEFPUSH_BUTTON)
            chmod_but[i].len += 2;      /* <> */
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
chmod_draw_select (const WDialog * h, int Id)
{
    widget_gotoyx (h, PY + Id + 1, PX + 1);
    tty_print_char (check_perm[Id].selected ? '*' : ' ');
    widget_gotoyx (h, PY + Id + 1, PX + 3);
}

/* --------------------------------------------------------------------------------------------- */

static void
chmod_toggle_select (const WDialog * h, int Id)
{
    check_perm[Id].selected = !check_perm[Id].selected;
    tty_setcolor (COLOR_NORMAL);
    chmod_draw_select (h, Id);
}

/* --------------------------------------------------------------------------------------------- */

static void
chmod_refresh (const WDialog * h)
{
    int i;
    int y, x;

    tty_setcolor (COLOR_NORMAL);

    for (i = 0; i < BUTTONS_PERM; i++)
        chmod_draw_select (h, i);

    y = WIDGET (file_gb)->y + 1;
    x = WIDGET (file_gb)->x + 2;

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
chmod_bg_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_DRAW:
        frame_callback (w, NULL, MSG_DRAW, 0, NULL);
        chmod_refresh (CONST_DIALOG (w->owner));
        return MSG_HANDLED;

    default:
        return frame_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
chmod_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WGroup *g = GROUP (w);
    WDialog *h = DIALOG (w);

    switch (msg)
    {
    case MSG_NOTIFY:
        {
            /* handle checkboxes */
            int i;

            /* whether notification was sent by checkbox? */
            for (i = 0; i < BUTTONS_PERM; i++)
                if (sender == WIDGET (check_perm[i].check))
                    break;

            if (i < BUTTONS_PERM)
            {
                ch_mode ^= check_perm[i].mode;
                label_set_textv (statl, "%o", (unsigned int) ch_mode);
                chmod_toggle_select (h, i);
                mode_change = TRUE;
                return MSG_HANDLED;
            }
        }

        return MSG_NOT_HANDLED;

    case MSG_KEY:
        if (parm == 'T' || parm == 't' || parm == KEY_IC)
        {
            int i;
            unsigned long id;

            id = group_get_current_widget_id (g);
            for (i = 0; i < BUTTONS_PERM; i++)
                if (id == WIDGET (check_perm[i].check)->id)
                    break;

            if (i < BUTTONS_PERM)
            {
                chmod_toggle_select (h, i);
                if (parm == KEY_IC)
                    group_select_next_widget (g);
                return MSG_HANDLED;
            }
        }
        return MSG_NOT_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static WDialog *
chmod_dlg_create (const char *fname, const struct stat *sf_stat)
{
    gboolean single_set;
    WDialog *ch_dlg;
    WGroup *g;
    int lines, cols;
    int i, y;
    int perm_gb_len;
    int file_gb_len;
    const char *c_fname, *c_fown, *c_fgrp;
    char buffer[BUF_TINY];

    mode_change = FALSE;

    single_set = (current_panel->marked < 2);
    perm_gb_len = check_perm_len + 2;
    file_gb_len = file_info_labels_len + 2;
    cols = str_term_width1 (fname) + 2 + 1;
    file_gb_len = MAX (file_gb_len, cols);

    lines = single_set ? 20 : 23;
    cols = perm_gb_len + file_gb_len + 1 + 6;

    if (cols > COLS)
    {
        /* shrink the right groupbox */
        cols = COLS;
        file_gb_len = cols - (perm_gb_len + 1 + 6);
    }

    ch_dlg =
        dlg_create (TRUE, 0, 0, lines, cols, WPOS_CENTER, FALSE, dialog_colors,
                    chmod_callback, NULL, "[Chmod]", _("Chmod command"));
    g = GROUP (ch_dlg);

    /* draw background */
    ch_dlg->bg->callback = chmod_bg_callback;

    group_add_widget (g, groupbox_new (PY, PX, BUTTONS_PERM + 2, perm_gb_len, _("Permission")));

    for (i = 0; i < BUTTONS_PERM; i++)
    {
        check_perm[i].check = check_new (PY + i + 1, PX + 2, (ch_mode & check_perm[i].mode) != 0,
                                         check_perm[i].text);
        group_add_widget (g, check_perm[i].check);
    }

    file_gb = groupbox_new (PY, PX + perm_gb_len + 1, BUTTONS_PERM + 2, file_gb_len, _("File"));
    group_add_widget (g, file_gb);

    /* Set the labels */
    y = PY + 2;
    cols = PX + perm_gb_len + 3;
    c_fname = str_trunc (fname, file_gb_len - 3);
    group_add_widget (g, label_new (y, cols, c_fname));
    g_snprintf (buffer, sizeof (buffer), "%o", (unsigned int) ch_mode);
    statl = label_new (y + 2, cols, buffer);
    group_add_widget (g, statl);
    c_fown = str_trunc (get_owner (sf_stat->st_uid), file_gb_len - 3);
    group_add_widget (g, label_new (y + 4, cols, c_fown));
    c_fgrp = str_trunc (get_group (sf_stat->st_gid), file_gb_len - 3);
    group_add_widget (g, label_new (y + 6, cols, c_fgrp));

    if (!single_set)
    {
        i = 0;

        group_add_widget (g, hline_new (lines - chmod_but[i].y - 1, -1, -1));

        for (; i < BUTTONS - 2; i++)
        {
            y = lines - chmod_but[i].y;
            group_add_widget (g, button_new (y, WIDGET (ch_dlg)->cols / 2 - chmod_but[i].len,
                                             chmod_but[i].ret_cmd, chmod_but[i].flags,
                                             chmod_but[i].text, NULL));
            i++;
            group_add_widget (g, button_new (y, WIDGET (ch_dlg)->cols / 2 + 1,
                                             chmod_but[i].ret_cmd, chmod_but[i].flags,
                                             chmod_but[i].text, NULL));
        }
    }

    i = BUTTONS - 2;
    y = lines - chmod_but[i].y;
    group_add_widget (g, hline_new (y - 1, -1, -1));
    group_add_widget (g, button_new (y, WIDGET (ch_dlg)->cols / 2 - chmod_but[i].len,
                                     chmod_but[i].ret_cmd, chmod_but[i].flags, chmod_but[i].text,
                                     NULL));
    i++;
    group_add_widget (g, button_new (y, WIDGET (ch_dlg)->cols / 2 + 1, chmod_but[i].ret_cmd,
                                     chmod_but[i].flags, chmod_but[i].text, NULL));

    /* select first checkbox */
    widget_select (WIDGET (check_perm[0].check));

    return ch_dlg;
}

/* --------------------------------------------------------------------------------------------- */

static void
chmod_done (gboolean need_update)
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
try_chmod (const vfs_path_t * p, mode_t m)
{
    while (mc_chmod (p, m) == -1 && !ignore_all)
    {
        int my_errno = errno;
        int result;
        char *msg;

        msg =
            g_strdup_printf (_("Cannot chmod \"%s\"\n%s"), x_basename (vfs_path_as_str (p)),
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
do_chmod (const vfs_path_t * p, struct stat *sf)
{
    gboolean ret;

    sf->st_mode &= and_mask;
    sf->st_mode |= or_mask;

    ret = try_chmod (p, sf->st_mode);

    do_file_mark (current_panel, current_file, 0);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static void
apply_mask (vfs_path_t * vpath, struct stat *sf)
{
    gboolean ok;

    if (!do_chmod (vpath, sf))
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
            ch_mode = sf->st_mode;

            ok = do_chmod (vpath, sf);
        }

        vfs_path_free (vpath);
    }
    while (ok && current_panel->marked != 0);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
chmod_cmd (void)
{
    gboolean need_update;
    gboolean end_chmod;

    chmod_init ();

    current_file = 0;
    ignore_all = FALSE;

    do
    {                           /* do while any files remaining */
        vfs_path_t *vpath;
        WDialog *ch_dlg;
        struct stat sf_stat;
        const char *fname;
        int i, result;

        do_refresh ();

        need_update = FALSE;
        end_chmod = FALSE;

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

        ch_mode = sf_stat.st_mode;

        ch_dlg = chmod_dlg_create (fname, &sf_stat);
        result = dlg_run (ch_dlg);

        switch (result)
        {
        case B_CANCEL:
            end_chmod = TRUE;
            break;

        case B_ENTER:
            if (mode_change)
            {
                if (current_panel->marked <= 1)
                {
                    /* single or last file */
                    if (mc_chmod (vpath, ch_mode) == -1 && !ignore_all)
                        message (D_ERROR, MSG_ERROR, _("Cannot chmod \"%s\"\n%s"), fname,
                                 unix_error_string (errno));
                    end_chmod = TRUE;
                }
                else if (!try_chmod (vpath, ch_mode))
                {
                    /* stop multiple files processing */
                    result = B_CANCEL;
                    end_chmod = TRUE;
                }
            }

            need_update = TRUE;
            break;

        case B_SETALL:
        case B_MARKED:
            and_mask = or_mask = 0;
            and_mask = ~and_mask;

            for (i = 0; i < BUTTONS_PERM; i++)
                if (check_perm[i].selected || result == B_SETALL)
                {
                    if (check_perm[i].check->state)
                        or_mask |= check_perm[i].mode;
                    else
                        and_mask &= ~check_perm[i].mode;
                }

            apply_mask (vpath, &sf_stat);
            need_update = TRUE;
            end_chmod = TRUE;
            break;

        case B_SETMRK:
            and_mask = or_mask = 0;
            and_mask = ~and_mask;

            for (i = 0; i < BUTTONS_PERM; i++)
                if (check_perm[i].selected)
                    or_mask |= check_perm[i].mode;

            apply_mask (vpath, &sf_stat);
            need_update = TRUE;
            end_chmod = TRUE;
            break;

        case B_CLRMRK:
            and_mask = or_mask = 0;
            and_mask = ~and_mask;

            for (i = 0; i < BUTTONS_PERM; i++)
                if (check_perm[i].selected)
                    and_mask &= ~check_perm[i].mode;

            apply_mask (vpath, &sf_stat);
            need_update = TRUE;
            end_chmod = TRUE;
            break;

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
    while (current_panel->marked != 0 && !end_chmod);

    chmod_done (need_update);
}

/* --------------------------------------------------------------------------------------------- */
