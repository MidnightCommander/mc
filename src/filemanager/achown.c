/*
   Chown-advanced command -- for the Midnight Commander

   Copyright (C) 1994, 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2007, 2011
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

/** \file achown.c
 *  \brief Source: Contains functions for advanced chowning
 */

#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/key.h"        /* XCTRL and ALT macros */
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/vfs/vfs.h"
#include "lib/util.h"
#include "lib/widget.h"

#include "dir.h"
#include "midnight.h"           /* current_panel */
#include "chmod.h"

#include "achown.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define BX              5
#define BY              6

#define TX              50
#define TY              2

#define BUTTONS         9

#define B_SETALL        B_USER
#define B_SKIP          (B_USER + 1)

#define B_OWN           (B_USER + 3)
#define B_GRP           (B_USER + 4)
#define B_OTH           (B_USER + 5)
#define B_OUSER         (B_USER + 6)
#define B_OGROUP        (B_USER + 7)

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static struct Dlg_head *ch_dlg;

static struct
{
    int ret_cmd, flags, y, x;
    const char *text;
} chown_advanced_but[BUTTONS] =
{
    /* *INDENT-OFF* */
    { B_CANCEL, NORMAL_BUTTON, 4, 53, N_("&Cancel") },
    { B_ENTER,  DEFPUSH_BUTTON,4, 40, N_("&Set") },
    { B_SKIP,   NORMAL_BUTTON, 4, 23, N_("S&kip") },
    { B_SETALL, NORMAL_BUTTON, 4,  0, N_("Set &all")},
    { B_ENTER,  NARROW_BUTTON, 0, 47, ""},
    { B_ENTER,  NARROW_BUTTON, 0, 29, ""},
    { B_ENTER,  NARROW_BUTTON, 0, 19, "   "},
    { B_ENTER,  NARROW_BUTTON, 0, 11, "   "},
    { B_ENTER,  NARROW_BUTTON, 0,  3, "   "}
    /* *INDENT-ON* */
};

static WButton *b_att[3];       /* permission */
static WButton *b_user, *b_group;       /* owner */

static int files_on_begin;      /* Number of files at startup */
static int flag_pos;
static int x_toggle;
static char ch_flags[11];
static const char ch_perm[] = "rwx";
static mode_t ch_cmode;
static struct stat *sf_stat;
static int need_update;
static int end_chown;
static int current_file;
static int single_set;
static char *fname;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
update_ownership (void)
{
    button_set_text (b_user, get_owner (sf_stat->st_uid));
    button_set_text (b_group, get_group (sf_stat->st_gid));
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
inc_flag_pos (int f_pos)
{
    if (flag_pos == 10)
    {
        flag_pos = 0;
        return MSG_NOT_HANDLED;
    }
    flag_pos++;
    if (!(flag_pos % 3) || f_pos > 2)
        return MSG_NOT_HANDLED;
    return MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
dec_flag_pos (int f_pos)
{
    if (!flag_pos)
    {
        flag_pos = 10;
        return MSG_NOT_HANDLED;
    }
    flag_pos--;
    if (!((flag_pos + 1) % 3) || f_pos > 2)
        return MSG_NOT_HANDLED;
    return MSG_HANDLED;
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
            s[i] = (ch_cmode & (1 << (8 - f_p - i))) ? ch_perm[i] : '-';
    }
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

static mode_t
get_perm (char *s, int base)
{
    mode_t m;

    m = 0;
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
print_flags (void)
{
    int i;

    tty_setcolor (COLOR_NORMAL);

    for (i = 0; i < 3; i++)
    {
        widget_move (ch_dlg, BY + 1, 9 + i);
        tty_print_char (ch_flags[i]);
    }

    for (i = 0; i < 3; i++)
    {
        widget_move (ch_dlg, BY + 1, 17 + i);
        tty_print_char (ch_flags[i + 3]);
    }

    for (i = 0; i < 3; i++)
    {
        widget_move (ch_dlg, BY + 1, 25 + i);
        tty_print_char (ch_flags[i + 6]);
    }

    update_permissions ();

    for (i = 0; i < 15; i++)
    {
        widget_move (ch_dlg, BY + 1, 35 + i);
        tty_print_char (ch_flags[9]);
    }
    for (i = 0; i < 15; i++)
    {
        widget_move (ch_dlg, BY + 1, 53 + i);
        tty_print_char (ch_flags[10]);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
update_mode (Dlg_head * h)
{
    print_flags ();
    tty_setcolor (COLOR_NORMAL);
    widget_move (h, BY + 2, 9);
    tty_printf ("%12o", get_mode ());
    send_message (WIDGET (h->current->data), NULL, WIDGET_FOCUS, 0, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
chl_callback (Dlg_head * h, Widget * sender, dlg_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case DLG_KEY:
        switch (parm)
        {
        case KEY_LEFT:
        case KEY_RIGHT:
            h->ret_value = parm;
            dlg_stop (h);
        }

    default:
        return default_dlg_callback (h, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
do_enter_key (Dlg_head * h, int f_pos)
{
    Dlg_head *chl_dlg;
    WListbox *chl_list;
    struct passwd *chl_pass;
    struct group *chl_grp;
    int fe;
    int lxx, lyy, chl_end, b_pos;
    int is_owner;
    const char *title;

    do
    {
        is_owner = (f_pos == 3);
        title = is_owner ? _("owner") : _("group");

        lxx = (COLS - 74) / 2 + (is_owner ? 35 : 53);
        lyy = (LINES - 13) / 2;
        chl_end = 0;

        chl_dlg =
            create_dlg (TRUE, lyy, lxx, 13, 17, dialog_colors, chl_callback, NULL,
                        "[Advanced Chown]", title, DLG_COMPACT | DLG_REVERSE);

        /* get new listboxes */
        chl_list = listbox_new (1, 1, 11, 15, FALSE, NULL);

        listbox_add_item (chl_list, LISTBOX_APPEND_AT_END, 0, "<Unknown>", NULL);

        if (is_owner)
        {
            /* get and put user names in the listbox */
            setpwent ();
            while ((chl_pass = getpwent ()))
            {
                listbox_add_item (chl_list, LISTBOX_APPEND_SORTED, 0, chl_pass->pw_name, NULL);
            }
            endpwent ();
            fe = listbox_search_text (chl_list, get_owner (sf_stat->st_uid));
        }
        else
        {
            /* get and put group names in the listbox */
            setgrent ();
            while ((chl_grp = getgrent ()))
            {
                listbox_add_item (chl_list, LISTBOX_APPEND_SORTED, 0, chl_grp->gr_name, NULL);
            }
            endgrent ();
            fe = listbox_search_text (chl_list, get_group (sf_stat->st_gid));
        }

        listbox_select_entry (chl_list, fe);

        b_pos = chl_list->pos;
        add_widget (chl_dlg, chl_list);

        run_dlg (chl_dlg);

        if (b_pos != chl_list->pos)
        {
            int ok = 0;
            char *text;

            listbox_get_current (chl_list, &text, NULL);
            if (is_owner)
            {
                chl_pass = getpwnam (text);
                if (chl_pass)
                {
                    ok = 1;
                    sf_stat->st_uid = chl_pass->pw_uid;
                }
            }
            else
            {
                chl_grp = getgrnam (text);
                if (chl_grp)
                {
                    sf_stat->st_gid = chl_grp->gr_gid;
                    ok = 1;
                }
            }
            if (ok)
            {
                ch_flags[f_pos + 6] = '+';
                update_ownership ();
            }
            dlg_focus (h);
            if (ok)
                print_flags ();
        }
        if (chl_dlg->ret_value == KEY_LEFT)
        {
            if (!is_owner)
                chl_end = 1;
            dlg_one_up (ch_dlg);
            f_pos--;
        }
        else if (chl_dlg->ret_value == KEY_RIGHT)
        {
            if (is_owner)
                chl_end = 1;
            dlg_one_down (ch_dlg);
            f_pos++;
        }
        /* Here we used to redraw the window */
        destroy_dlg (chl_dlg);
    }
    while (chl_end);
}

/* --------------------------------------------------------------------------------------------- */

static void
chown_refresh (void)
{
    common_dialog_repaint (ch_dlg);

    tty_setcolor (COLOR_NORMAL);

    widget_move (ch_dlg, BY - 1, 8);
    tty_print_string (_("owner"));
    widget_move (ch_dlg, BY - 1, 16);
    tty_print_string (_("group"));
    widget_move (ch_dlg, BY - 1, 24);
    tty_print_string (_("other"));

    widget_move (ch_dlg, BY - 1, 35);
    tty_print_string (_("owner"));
    widget_move (ch_dlg, BY - 1, 53);
    tty_print_string (_("group"));

    widget_move (ch_dlg, 3, 4);
    tty_print_string (_("On"));
    widget_move (ch_dlg, BY + 1, 4);
    tty_print_string (_("Flag"));
    widget_move (ch_dlg, BY + 2, 4);
    tty_print_string (_("Mode"));

    if (!single_set)
    {
        widget_move (ch_dlg, 3, 54);
        tty_printf (_("%6d of %d"), files_on_begin - (current_panel->marked) + 1, files_on_begin);
    }

    print_flags ();
}

/* --------------------------------------------------------------------------------------------- */

static void
chown_info_update (void)
{
    /* display file info */
    tty_setcolor (COLOR_NORMAL);

    /* name && mode */
    widget_move (ch_dlg, 3, 8);
    tty_print_string (str_fit_to_term (fname, 45, J_LEFT_FIT));
    widget_move (ch_dlg, BY + 2, 9);
    tty_printf ("%12o", get_mode ());

    /* permissions */
    update_permissions ();
}

/* --------------------------------------------------------------------------------------------- */

static void
b_setpos (int f_pos)
{
    b_att[0]->hotpos = -1;
    b_att[1]->hotpos = -1;
    b_att[2]->hotpos = -1;
    b_att[f_pos]->hotpos = (flag_pos % 3);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
advanced_chown_callback (Dlg_head * h, Widget * sender, dlg_msg_t msg, int parm, void *data)
{
    int i = 0, f_pos = BUTTONS - dlg_get_current_widget_id (h) - single_set - 1;

    switch (msg)
    {
    case DLG_DRAW:
        chown_refresh ();
        chown_info_update ();
        return MSG_HANDLED;

    case DLG_POST_KEY:
        if (f_pos < 3)
            b_setpos (f_pos);
        return MSG_HANDLED;

    case DLG_FOCUS:
        if (f_pos < 3)
        {
            if ((flag_pos / 3) != f_pos)
                flag_pos = f_pos * 3;
            b_setpos (f_pos);
        }
        else if (f_pos < 5)
            flag_pos = f_pos + 6;
        return MSG_HANDLED;

    case DLG_KEY:
        switch (parm)
        {

        case XCTRL ('b'):
        case KEY_LEFT:
            if (f_pos < 5)
                return (dec_flag_pos (f_pos));
            break;

        case XCTRL ('f'):
        case KEY_RIGHT:
            if (f_pos < 5)
                return (inc_flag_pos (f_pos));
            break;

        case ' ':
            if (f_pos < 3)
                return MSG_HANDLED;
            break;

        case '\n':
        case KEY_ENTER:
            if (f_pos <= 2 || f_pos >= 5)
                break;
            do_enter_key (h, f_pos);
            return MSG_HANDLED;

        case ALT ('x'):
            i++;

        case ALT ('w'):
            i++;

        case ALT ('r'):
            parm = i + 3;
            for (i = 0; i < 3; i++)
                ch_flags[i * 3 + parm - 3] = (x_toggle & (1 << parm)) ? '-' : '+';
            x_toggle ^= (1 << parm);
            update_mode (h);
            dlg_broadcast_msg (h, WIDGET_DRAW, FALSE);
            send_message (WIDGET (h->current->data), NULL, WIDGET_FOCUS, 0, NULL);
            break;

        case XCTRL ('x'):
            i++;

        case XCTRL ('w'):
            i++;

        case XCTRL ('r'):
            parm = i;
            for (i = 0; i < 3; i++)
                ch_flags[i * 3 + parm] = (x_toggle & (1 << parm)) ? '-' : '+';
            x_toggle ^= (1 << parm);
            update_mode (h);
            dlg_broadcast_msg (h, WIDGET_DRAW, FALSE);
            send_message (WIDGET (h->current->data), NULL, WIDGET_FOCUS, 0, NULL);
            break;

        case 'x':
            i++;

        case 'w':
            i++;

        case 'r':
            if (f_pos > 2)
                break;
            flag_pos = f_pos * 3 + i;   /* (strchr(ch_perm,parm)-ch_perm); */
            if (((WButton *) h->current->data)->text.start[(flag_pos % 3)] == '-')
                ch_flags[flag_pos] = '+';
            else
                ch_flags[flag_pos] = '-';
            update_mode (h);
            break;

        case '4':
            i++;

        case '2':
            i++;

        case '1':
            if (f_pos > 2)
                break;
            flag_pos = i + f_pos * 3;
            ch_flags[flag_pos] = '=';
            update_mode (h);
            break;

        case '-':
            if (f_pos > 2)
                break;

        case '*':
            if (parm == '*')
                parm = '=';

        case '=':
        case '+':
            if (f_pos <= 4)
            {
                ch_flags[flag_pos] = parm;
                update_mode (h);
                advanced_chown_callback (h, sender, DLG_KEY, KEY_RIGHT, NULL);
                if (flag_pos > 8 || !(flag_pos % 3))
                    dlg_one_down (h);
            }
            break;
        }
        return MSG_NOT_HANDLED;

    default:
        return default_dlg_callback (h, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
init_chown_advanced (void)
{
    int i;
    enum
    { dlg_h = 13, dlg_w = 74, n_elem = 4 };
#ifdef ENABLE_NLS
    static int i18n_len = 0;

    if (i18n_len == 0)
    {
        int dx, cx;
        for (i = 0; i < n_elem; i++)
        {
            chown_advanced_but[i].text = _(chown_advanced_but[i].text);
            i18n_len += str_term_width1 (chown_advanced_but[i].text) + 3;
            if (DEFPUSH_BUTTON == chown_advanced_but[i].flags)
                i18n_len += 2;  /* "<>" */
        }
        cx = dx = (dlg_w - i18n_len - 2) / (n_elem + 1);

        /* Reversed order */
        for (i = n_elem - 1; i >= 0; i--)
        {
            chown_advanced_but[i].x = cx;
            cx += str_term_width1 (chown_advanced_but[i].text) + 3 + dx;
        }
    }
#endif /* ENABLE_NLS */

    sf_stat = g_new (struct stat, 1);
    do_refresh ();
    end_chown = need_update = current_file = 0;
    single_set = (current_panel->marked < 2) ? 2 : 0;
    memset (ch_flags, '=', 11);
    flag_pos = 0;
    x_toggle = 070;

    ch_dlg =
        create_dlg (TRUE, 0, 0, dlg_h, dlg_w, dialog_colors, advanced_chown_callback, NULL,
                    "[Advanced Chown]", _("Chown advanced command"), DLG_CENTER | DLG_REVERSE);

#define XTRACT(i) BY+chown_advanced_but[i].y, BX+chown_advanced_but[i].x, \
        chown_advanced_but[i].ret_cmd, chown_advanced_but[i].flags, \
        (chown_advanced_but[i].text), 0

    for (i = 0; i < BUTTONS - 5; i++)
        if (!single_set || i < 2)
            add_widget (ch_dlg, button_new (XTRACT (i)));

    b_att[0] = button_new (XTRACT (8));
    b_att[1] = button_new (XTRACT (7));
    b_att[2] = button_new (XTRACT (6));
    b_user = button_new (XTRACT (5));
    b_group = button_new (XTRACT (4));

    add_widget (ch_dlg, b_group);
    add_widget (ch_dlg, b_user);
    add_widget (ch_dlg, b_att[2]);
    add_widget (ch_dlg, b_att[1]);
    add_widget (ch_dlg, b_att[0]);
}

/* --------------------------------------------------------------------------------------------- */

static void
chown_advanced_done (void)
{
    g_free (sf_stat);
    if (need_update)
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

#if 0
static void
do_chown (uid_t u, gid_t g)
{
    chown (current_panel->dir.list[current_file].fname, u, g);
    file_mark (current_panel, current_file, 0);
}
#endif

/* --------------------------------------------------------------------------------------------- */

static char *
next_file (void)
{
    while (!current_panel->dir.list[current_file].f.marked)
        current_file++;

    return current_panel->dir.list[current_file].fname;
}

/* --------------------------------------------------------------------------------------------- */

static void
apply_advanced_chowns (struct stat *sf)
{
    vfs_path_t *vpath;
    char *lc_fname;
    gid_t a_gid = sf->st_gid;
    uid_t a_uid = sf->st_uid;

    lc_fname = current_panel->dir.list[current_file].fname;
    vpath = vfs_path_from_str (lc_fname);
    need_update = end_chown = 1;
    if (mc_chmod (vpath, get_mode ()) == -1)
        message (D_ERROR, MSG_ERROR, _("Cannot chmod \"%s\"\n%s"),
                 lc_fname, unix_error_string (errno));
    /* call mc_chown only, if mc_chmod didn't fail */
    else if (mc_chown (vpath, (ch_flags[9] == '+') ? sf->st_uid : (uid_t) - 1,
                       (ch_flags[10] == '+') ? sf->st_gid : (gid_t) - 1) == -1)
        message (D_ERROR, MSG_ERROR, _("Cannot chown \"%s\"\n%s"),
                 lc_fname, unix_error_string (errno));
    do_file_mark (current_panel, current_file, 0);
    vfs_path_free (vpath);

    do
    {
        lc_fname = next_file ();
        vpath = vfs_path_from_str (lc_fname);

        if (mc_stat (vpath, sf) != 0)
        {
            vfs_path_free (vpath);
            break;
        }
        ch_cmode = sf->st_mode;
        if (mc_chmod (vpath, get_mode ()) == -1)
            message (D_ERROR, MSG_ERROR, _("Cannot chmod \"%s\"\n%s"),
                     lc_fname, unix_error_string (errno));
        /* call mc_chown only, if mc_chmod didn't fail */
        else if (mc_chown (vpath, (ch_flags[9] == '+') ? a_uid : (uid_t) - 1,
                           (ch_flags[10] == '+') ? a_gid : (gid_t) - 1) == -1)
            message (D_ERROR, MSG_ERROR, _("Cannot chown \"%s\"\n%s"),
                     lc_fname, unix_error_string (errno));

        do_file_mark (current_panel, current_file, 0);
        vfs_path_free (vpath);
    }
    while (current_panel->marked);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
chown_advanced_cmd (void)
{
    files_on_begin = current_panel->marked;

    do
    {                           /* do while any files remaining */
        vfs_path_t *vpath;
        init_chown_advanced ();

        if (current_panel->marked)
            fname = next_file ();       /* next marked file */
        else
            fname = selection (current_panel)->fname;   /* single file */
        vpath = vfs_path_from_str (fname);

        if (mc_stat (vpath, sf_stat) != 0)
        {                       /* get status of file */
            destroy_dlg (ch_dlg);
            vfs_path_free (vpath);
            break;
        }
        ch_cmode = sf_stat->st_mode;

        chown_refresh ();

        update_ownership ();

        /* game can begin */
        run_dlg (ch_dlg);

        switch (ch_dlg->ret_value)
        {
        case B_CANCEL:
            end_chown = 1;
            break;

        case B_ENTER:
            {
                vfs_path_t *fname_vpath;

                fname_vpath = vfs_path_from_str (fname);
                need_update = 1;
                if (mc_chmod (fname_vpath, get_mode ()) == -1)
                    message (D_ERROR, MSG_ERROR, _("Cannot chmod \"%s\"\n%s"),
                             fname, unix_error_string (errno));
                /* call mc_chown only, if mc_chmod didn't fail */
                else if (mc_chown
                         (fname_vpath, (ch_flags[9] == '+') ? sf_stat->st_uid : (uid_t) - 1,
                          (ch_flags[10] == '+') ? sf_stat->st_gid : (gid_t) - 1) == -1)
                    message (D_ERROR, MSG_ERROR, _("Cannot chown \"%s\"\n%s"), fname,
                             unix_error_string (errno));
                vfs_path_free (fname_vpath);
            }
            break;
        case B_SETALL:
            apply_advanced_chowns (sf_stat);
            break;

        case B_SKIP:
            break;

        }

        if (current_panel->marked && ch_dlg->ret_value != B_CANCEL)
        {
            do_file_mark (current_panel, current_file, 0);
            need_update = 1;
        }
        destroy_dlg (ch_dlg);
        vfs_path_free (vpath);

    }
    while (current_panel->marked && !end_chown);

    chown_advanced_done ();
}

/* --------------------------------------------------------------------------------------------- */
