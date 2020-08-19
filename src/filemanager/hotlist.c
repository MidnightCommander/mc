/*
   Directory hotlist -- for the Midnight Commander

   Copyright (C) 1994-2020
   Free Software Foundation, Inc.

   Written by:
   Radek Doulik, 1994
   Janne Kukonlehto, 1995
   Andrej Borsenkow, 1996
   Norbert Warmuth, 1997
   Andrew Borodin <aborodin@vmail.ru>, 2012, 2013

   Janne did the original Hotlist code, Andrej made the groupable
   hotlist; the move hotlist and revamped the file format and made
   it stronger.

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

/** \file hotlist.c
 *  \brief Source: directory hotlist
 */

#include <config.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"

#include "lib/tty/tty.h"        /* COLS */
#include "lib/tty/key.h"        /* KEY_M_CTRL */
#include "lib/skin.h"           /* colors */
#include "lib/mcconfig.h"       /* Load/save directories hotlist */
#include "lib/fileloc.h"
#include "lib/strutil.h"
#include "lib/vfs/vfs.h"
#include "lib/util.h"
#include "lib/widget.h"

#include "src/setup.h"          /* For profile_bname */
#include "src/history.h"

#include "command.h"            /* cmdline */

#include "hotlist.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define UX 3
#define UY 2

#define B_ADD_CURRENT   B_USER
#define B_REMOVE        (B_USER + 1)
#define B_NEW_GROUP     (B_USER + 2)
#define B_NEW_ENTRY     (B_USER + 3)
#define B_ENTER_GROUP   (B_USER + 4)
#define B_UP_GROUP      (B_USER + 5)
#define B_INSERT        (B_USER + 6)
#define B_APPEND        (B_USER + 7)
#define B_MOVE          (B_USER + 8)
#ifdef ENABLE_VFS
#define B_FREE_ALL_VFS  (B_USER + 9)
#define B_REFRESH_VFS   (B_USER + 10)
#endif

#define TKN_GROUP   0
#define TKN_ENTRY   1
#define TKN_STRING  2
#define TKN_URL     3
#define TKN_ENDGROUP 4
#define TKN_COMMENT  5
#define TKN_EOL      125
#define TKN_EOF      126
#define TKN_UNKNOWN  127

#define SKIP_TO_EOL \
{ \
    int _tkn; \
    while ((_tkn = hot_next_token ()) != TKN_EOF && _tkn != TKN_EOL) ; \
}

#define CHECK_TOKEN(_TKN_) \
tkn = hot_next_token (); \
if (tkn != _TKN_) \
{ \
    hotlist_state.readonly = TRUE; \
    hotlist_state.file_error = TRUE; \
    while (tkn != TKN_EOL && tkn != TKN_EOF) \
        tkn = hot_next_token (); \
    break; \
}

/*** file scope type declarations ****************************************************************/

enum HotListType
{
    HL_TYPE_GROUP,
    HL_TYPE_ENTRY,
    HL_TYPE_COMMENT,
    HL_TYPE_DOTDOT
};

static struct
{
    /*
     * these reflect run time state
     */

    gboolean loaded;            /* hotlist is loaded */
    gboolean readonly;          /* hotlist readonly */
    gboolean file_error;        /* parse error while reading file */
    gboolean running;           /* we are running dlg (and have to
                                   update listbox */
    gboolean moving;            /* we are in moving hotlist currently */
    gboolean modified;          /* hotlist was modified */
    hotlist_t type;             /* LIST_HOTLIST || LIST_VFSLIST */
} hotlist_state;

/* Directory hotlist */
struct hotlist
{
    enum HotListType type;
    char *directory;
    char *label;
    struct hotlist *head;
    struct hotlist *up;
    struct hotlist *next;
};

/*** file scope variables ************************************************************************/

static WPanel *our_panel;

static gboolean hotlist_has_dot_dot = TRUE;

static WDialog *hotlist_dlg, *movelist_dlg;
static WGroupbox *hotlist_group, *movelist_group;
static WListbox *l_hotlist, *l_movelist;
static WLabel *pname;

static struct
{
    int ret_cmd, flags, y, x, len;
    const char *text;
    int type;
    widget_pos_flags_t pos_flags;
} hotlist_but[] =
{
    /* *INDENT-OFF* */
    { B_ENTER, DEFPUSH_BUTTON, 0, 0, 0, N_("Change &to"),
            LIST_HOTLIST | LIST_VFSLIST | LIST_MOVELIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
#ifdef ENABLE_VFS
    { B_FREE_ALL_VFS, NORMAL_BUTTON, 0, 20, 0, N_("&Free VFSs now"),
            LIST_VFSLIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
    { B_REFRESH_VFS, NORMAL_BUTTON, 0, 43, 0, N_("&Refresh"),
            LIST_VFSLIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
#endif
    { B_ADD_CURRENT, NORMAL_BUTTON, 0, 20, 0, N_("&Add current"),
            LIST_HOTLIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
    { B_UP_GROUP, NORMAL_BUTTON, 0, 42, 0, N_("&Up"),
            LIST_HOTLIST | LIST_MOVELIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
    { B_CANCEL, NORMAL_BUTTON, 0, 53, 0, N_("&Cancel"),
            LIST_HOTLIST | LIST_VFSLIST | LIST_MOVELIST, WPOS_KEEP_RIGHT | WPOS_KEEP_BOTTOM },
    { B_NEW_GROUP, NORMAL_BUTTON, 1, 0, 0, N_("New &group"),
            LIST_HOTLIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
    { B_NEW_ENTRY, NORMAL_BUTTON, 1, 15, 0, N_("New &entry"),
            LIST_HOTLIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
    { B_INSERT, NORMAL_BUTTON, 1, 0, 0, N_("&Insert"),
            LIST_MOVELIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
    { B_APPEND, NORMAL_BUTTON, 1, 15, 0, N_("A&ppend"),
            LIST_MOVELIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
    { B_REMOVE, NORMAL_BUTTON, 1, 30, 0, N_("&Remove"),
            LIST_HOTLIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
    { B_MOVE, NORMAL_BUTTON, 1, 42, 0, N_("&Move"),
            LIST_HOTLIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM }
    /* *INDENT-ON* */
};

static const size_t hotlist_but_num = G_N_ELEMENTS (hotlist_but);

static struct hotlist *hotlist = NULL;

static struct hotlist *current_group;

static GString *tkn_buf = NULL;

static char *hotlist_file_name;
static FILE *hotlist_file;
static time_t hotlist_file_mtime;

static int list_level = 0;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void init_movelist (struct hotlist *item);
static void add_new_group_cmd (void);
static void add_new_entry_cmd (WPanel * panel);
static void remove_from_hotlist (struct hotlist *entry);
static void load_hotlist (void);
static void add_dotdot_to_list (void);

/* --------------------------------------------------------------------------------------------- */
/** If current->data is 0, then we are dealing with a VFS pathname */

static void
update_path_name (void)
{
    const char *text = "";
    char *p;
    WListbox *list = hotlist_state.moving ? l_movelist : l_hotlist;
    Widget *w = WIDGET (list);

    if (!listbox_is_empty (list))
    {
        char *ctext = NULL;
        void *cdata = NULL;

        listbox_get_current (list, &ctext, &cdata);
        if (cdata == NULL)
            text = ctext;
        else
        {
            struct hotlist *hlp = (struct hotlist *) cdata;

            if (hlp->type == HL_TYPE_ENTRY || hlp->type == HL_TYPE_DOTDOT)
                text = hlp->directory;
            else if (hlp->type == HL_TYPE_GROUP)
                text = _("Subgroup - press ENTER to see list");
        }
    }

    p = g_strconcat (" ", current_group->label, " ", (char *) NULL);
    if (hotlist_state.moving)
        groupbox_set_title (movelist_group, str_trunc (p, w->cols - 2));
    else
    {
        groupbox_set_title (hotlist_group, str_trunc (p, w->cols - 2));
        label_set_text (pname, str_trunc (text, w->cols));
    }
    g_free (p);
}

/* --------------------------------------------------------------------------------------------- */

static void
fill_listbox (WListbox * list)
{
    struct hotlist *current;
    GString *buff;

    buff = g_string_new ("");

    for (current = current_group->head; current != NULL; current = current->next)
        switch (current->type)
        {
        case HL_TYPE_GROUP:
            {
                /* buff clean up */
                g_string_truncate (buff, 0);
                g_string_append (buff, "->");
                g_string_append (buff, current->label);
                listbox_add_item (list, LISTBOX_APPEND_AT_END, 0, buff->str, current, FALSE);
            }
            break;
        case HL_TYPE_DOTDOT:
        case HL_TYPE_ENTRY:
            listbox_add_item (list, LISTBOX_APPEND_AT_END, 0, current->label, current, FALSE);
        default:
            break;
        }

    g_string_free (buff, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static void
unlink_entry (struct hotlist *entry)
{
    struct hotlist *current = current_group->head;

    if (current == entry)
        current_group->head = entry->next;
    else
    {
        while (current != NULL && current->next != entry)
            current = current->next;
        if (current != NULL)
            current->next = entry->next;
    }
    entry->next = entry->up = NULL;
}

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_VFS
static void
add_name_to_list (const char *path)
{
    listbox_add_item (l_hotlist, LISTBOX_APPEND_AT_END, 0, path, NULL, FALSE);
}
#endif /* !ENABLE_VFS */

/* --------------------------------------------------------------------------------------------- */

static int
hotlist_run_cmd (int action)
{
    switch (action)
    {
    case B_MOVE:
        {
            struct hotlist *saved = current_group;
            struct hotlist *item = NULL;
            struct hotlist *moveto_item = NULL;
            struct hotlist *moveto_group = NULL;
            int ret;

            if (listbox_is_empty (l_hotlist))
                return 0;       /* empty group - nothing to do */

            listbox_get_current (l_hotlist, NULL, (void **) &item);
            init_movelist (item);
            hotlist_state.moving = TRUE;
            ret = dlg_run (movelist_dlg);
            hotlist_state.moving = FALSE;
            listbox_get_current (l_movelist, NULL, (void **) &moveto_item);
            moveto_group = current_group;
            dlg_destroy (movelist_dlg);
            current_group = saved;
            if (ret == B_CANCEL)
                return 0;
            if (moveto_item == item)
                return 0;       /* If we insert/append a before/after a
                                   it hardly changes anything ;) */
            unlink_entry (item);
            listbox_remove_current (l_hotlist);
            item->up = moveto_group;
            if (moveto_group->head == NULL)
                moveto_group->head = item;
            else if (moveto_item == NULL)
            {                   /* we have group with just comments */
                struct hotlist *p = moveto_group->head;

                /* skip comments */
                while (p->next != NULL)
                    p = p->next;
                p->next = item;
            }
            else if (ret == B_ENTER || ret == B_APPEND)
            {
                if (moveto_item->next == NULL)
                    moveto_item->next = item;
                else
                {
                    item->next = moveto_item->next;
                    moveto_item->next = item;
                }
            }
            else if (moveto_group->head == moveto_item)
            {
                moveto_group->head = item;
                item->next = moveto_item;
            }
            else
            {
                struct hotlist *p = moveto_group->head;

                while (p->next != moveto_item)
                    p = p->next;
                item->next = p->next;
                p->next = item;
            }
            listbox_remove_list (l_hotlist);
            fill_listbox (l_hotlist);
            repaint_screen ();
            hotlist_state.modified = TRUE;
            return 0;
        }
    case B_REMOVE:
        {
            struct hotlist *entry = NULL;

            listbox_get_current (l_hotlist, NULL, (void **) &entry);
            remove_from_hotlist (entry);
        }
        return 0;

    case B_NEW_GROUP:
        add_new_group_cmd ();
        return 0;

    case B_ADD_CURRENT:
        add2hotlist_cmd (our_panel);
        return 0;

    case B_NEW_ENTRY:
        add_new_entry_cmd (our_panel);
        return 0;

    case B_ENTER:
    case B_ENTER_GROUP:
        {
            WListbox *list;
            void *data;
            struct hotlist *hlp;

            list = hotlist_state.moving ? l_movelist : l_hotlist;
            listbox_get_current (list, NULL, &data);

            if (data == NULL)
                return 1;

            hlp = (struct hotlist *) data;

            if (hlp->type == HL_TYPE_ENTRY)
                return (action == B_ENTER ? 1 : 0);
            if (hlp->type != HL_TYPE_DOTDOT)
            {
                listbox_remove_list (list);
                current_group = hlp;
                fill_listbox (list);
                return 0;
            }
            MC_FALLTHROUGH;     /* go up */
        }
        MC_FALLTHROUGH;         /* if list empty - just go up */

    case B_UP_GROUP:
        {
            WListbox *list = hotlist_state.moving ? l_movelist : l_hotlist;

            listbox_remove_list (list);
            current_group = current_group->up;
            fill_listbox (list);
            return 0;
        }

#ifdef ENABLE_VFS
    case B_FREE_ALL_VFS:
        vfs_expire (TRUE);
        MC_FALLTHROUGH;

    case B_REFRESH_VFS:
        listbox_remove_list (l_hotlist);
        listbox_add_item (l_hotlist, LISTBOX_APPEND_AT_END, 0, mc_config_get_home_dir (), NULL,
                          FALSE);
        vfs_fill_names (add_name_to_list);
        return 0;
#endif /* ENABLE_VFS */

    default:
        return 1;
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
hotlist_button_callback (WButton * button, int action)
{
    int ret;

    (void) button;
    ret = hotlist_run_cmd (action);
    update_path_name ();
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static inline cb_ret_t
hotlist_handle_key (WDialog * h, int key)
{
    switch (key)
    {
    case KEY_M_CTRL | '\n':
        goto l1;

    case '\n':
    case KEY_ENTER:
        if (hotlist_button_callback (NULL, B_ENTER) != 0)
        {
            h->ret_value = B_ENTER;
            dlg_stop (h);
        }
        return MSG_HANDLED;

    case KEY_RIGHT:
        /* enter to the group */
        if (hotlist_state.type == LIST_VFSLIST)
            return MSG_NOT_HANDLED;
        return hotlist_button_callback (NULL, B_ENTER_GROUP) == 0 ? MSG_HANDLED : MSG_NOT_HANDLED;

    case KEY_LEFT:
        /* leave the group */
        if (hotlist_state.type == LIST_VFSLIST)
            return MSG_NOT_HANDLED;
        return hotlist_button_callback (NULL, B_UP_GROUP) == 0 ? MSG_HANDLED : MSG_NOT_HANDLED;

    case KEY_DC:
        if (hotlist_state.moving)
            return MSG_NOT_HANDLED;
        hotlist_button_callback (NULL, B_REMOVE);
        return MSG_HANDLED;

      l1:
    case ALT ('\n'):
    case ALT ('\r'):
        if (!hotlist_state.moving)
        {
            void *ldata = NULL;

            listbox_get_current (l_hotlist, NULL, &ldata);

            if (ldata != NULL)
            {
                struct hotlist *hlp = (struct hotlist *) ldata;

                if (hlp->type == HL_TYPE_ENTRY)
                {
                    char *tmp;

                    tmp = g_strconcat ("cd ", hlp->directory, (char *) NULL);
                    input_insert (cmdline, tmp, FALSE);
                    g_free (tmp);
                    h->ret_value = B_CANCEL;
                    dlg_stop (h);
                }
            }
        }
        return MSG_HANDLED;     /* ignore key */

    default:
        return MSG_NOT_HANDLED;
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
hotlist_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WDialog *h = DIALOG (w);

    switch (msg)
    {
    case MSG_INIT:
    case MSG_NOTIFY:           /* MSG_NOTIFY is fired by the listbox to tell us the item has changed. */
        update_path_name ();
        return MSG_HANDLED;

    case MSG_UNHANDLED_KEY:
        return hotlist_handle_key (h, parm);

    case MSG_POST_KEY:
        /*
         * The code here has two purposes:
         *
         * (1) Always stay on the hotlist.
         *
         * Activating a button using its hotkey (and even pressing ENTER, as
         * there's a "default button") moves the focus to the button. But we
         * want to stay on the hotlist, to be able to use the usual keys (up,
         * down, etc.). So we do `widget_select (lst)`.
         *
         * (2) Refresh the hotlist.
         *
         * We may have run a command that changed the contents of the list.
         * We therefore need to refresh it. So we do `widget_draw (lst)`.
         */
        {
            Widget *lst;

            lst = WIDGET (h == hotlist_dlg ? l_hotlist : l_movelist);

            /* widget_select() already redraws the widget, but since it's a
             * no-op if the widget is already selected ("focused"), we have
             * to call widget_draw() separately. */
            if (!widget_get_state (lst, WST_FOCUSED))
                widget_select (lst);
            else
                widget_draw (lst);
        }
        return MSG_HANDLED;

    case MSG_RESIZE:
        {
            WRect r;

            rect_init (&r, w->y, w->x, LINES - (h == hotlist_dlg ? 2 : 6), COLS - 6);

            return dlg_default_callback (w, NULL, MSG_RESIZE, 0, &r);
        }

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static lcback_ret_t
hotlist_listbox_callback (WListbox * list)
{
    WDialog *dlg = DIALOG (WIDGET (list)->owner);

    if (!listbox_is_empty (list))
    {
        void *data = NULL;

        listbox_get_current (list, NULL, &data);

        if (data != NULL)
        {
            struct hotlist *hlp = (struct hotlist *) data;

            if (hlp->type == HL_TYPE_ENTRY)
            {
                dlg->ret_value = B_ENTER;
                dlg_stop (dlg);
                return LISTBOX_DONE;
            }
            else
            {
                hotlist_button_callback (NULL, B_ENTER);
                send_message (dlg, NULL, MSG_POST_KEY, '\n', NULL);
                return LISTBOX_CONT;
            }
        }
        else
        {
            dlg->ret_value = B_ENTER;
            dlg_stop (dlg);
            return LISTBOX_DONE;
        }
    }

    hotlist_button_callback (NULL, B_UP_GROUP);
    send_message (dlg, NULL, MSG_POST_KEY, 'u', NULL);
    return LISTBOX_CONT;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Expands all button names (once) and recalculates button positions.
 * returns number of columns in the dialog box, which is 10 chars longer
 * then buttonbar.
 *
 * If common width of the window (i.e. in xterm) is less than returned
 * width - sorry :)  (anyway this did not handled in previous version too)
 */

static int
init_i18n_stuff (int list_type, int cols)
{
    size_t i;

    static gboolean i18n_flag = FALSE;

    if (!i18n_flag)
    {
        for (i = 0; i < hotlist_but_num; i++)
        {
#ifdef ENABLE_NLS
            hotlist_but[i].text = _(hotlist_but[i].text);
#endif /* ENABLE_NLS */
            hotlist_but[i].len = str_term_width1 (hotlist_but[i].text) + 3;
            if (hotlist_but[i].flags == DEFPUSH_BUTTON)
                hotlist_but[i].len += 2;
        }

        i18n_flag = TRUE;
    }

    /* Dynamic resizing of buttonbars */
    {
        int len[2], count[2];   /* at most two lines of buttons */
        int cur_x[2];

        len[0] = len[1] = 0;
        count[0] = count[1] = 0;
        cur_x[0] = cur_x[1] = 0;

        /* Count len of buttonbars, assuming 1 extra space between buttons */
        for (i = 0; i < hotlist_but_num; i++)
            if ((hotlist_but[i].type & list_type) != 0)
            {
                int row;

                row = hotlist_but[i].y;
                ++count[row];
                len[row] += hotlist_but[i].len + 1;
            }

        (len[0])--;
        (len[1])--;

        cols = MAX (cols, MAX (len[0], len[1]));

        /* arrange buttons */
        for (i = 0; i < hotlist_but_num; i++)
            if ((hotlist_but[i].type & list_type) != 0)
            {
                int row;

                row = hotlist_but[i].y;

                if (hotlist_but[i].x != 0)
                {
                    /* not first int the row */
                    if (hotlist_but[i].ret_cmd == B_CANCEL)
                        hotlist_but[i].x = cols - hotlist_but[i].len - 6;
                    else
                        hotlist_but[i].x = cur_x[row];
                }

                cur_x[row] += hotlist_but[i].len + 1;
            }
    }

    return cols;
}

/* --------------------------------------------------------------------------------------------- */

static void
init_hotlist (hotlist_t list_type)
{
    size_t i;
    const char *title, *help_node;
    int lines, cols;
    int y;
    int dh = 0;
    WGroup *g;
    WGroupbox *path_box;
    Widget *hotlist_widget;

    do_refresh ();

    lines = LINES - 2;
    cols = init_i18n_stuff (list_type, COLS - 6);

#ifdef ENABLE_VFS
    if (list_type == LIST_VFSLIST)
    {
        title = _("Active VFS directories");
        help_node = "[vfshot]"; /* FIXME - no such node */
        dh = 1;
    }
    else
#endif /* !ENABLE_VFS */
    {
        title = _("Directory hotlist");
        help_node = "[Hotlist]";
    }

    hotlist_dlg =
        dlg_create (TRUE, 0, 0, lines, cols, WPOS_CENTER, FALSE, dialog_colors, hotlist_callback,
                    NULL, help_node, title);
    g = GROUP (hotlist_dlg);

    y = UY;
    hotlist_group = groupbox_new (y, UX, lines - 10 + dh, cols - 2 * UX, _("Top level group"));
    hotlist_widget = WIDGET (hotlist_group);
    group_add_widget_autopos (g, hotlist_widget, WPOS_KEEP_ALL, NULL);

    l_hotlist =
        listbox_new (y + 1, UX + 1, hotlist_widget->lines - 2, hotlist_widget->cols - 2, FALSE,
                     hotlist_listbox_callback);

    /* Fill the hotlist with the active VFS or the hotlist */
#ifdef ENABLE_VFS
    if (list_type == LIST_VFSLIST)
    {
        listbox_add_item (l_hotlist, LISTBOX_APPEND_AT_END, 0, mc_config_get_home_dir (), NULL,
                          FALSE);
        vfs_fill_names (add_name_to_list);
    }
    else
#endif /* !ENABLE_VFS */
        fill_listbox (l_hotlist);

    /* insert before groupbox to view scrollbar */
    group_add_widget_autopos (g, l_hotlist, WPOS_KEEP_ALL, NULL);

    y += hotlist_widget->lines;

    path_box = groupbox_new (y, UX, 3, hotlist_widget->cols, _("Directory path"));
    group_add_widget_autopos (g, path_box, WPOS_KEEP_BOTTOM | WPOS_KEEP_HORZ, NULL);

    pname = label_new (y + 1, UX + 2, "");
    group_add_widget_autopos (g, pname, WPOS_KEEP_BOTTOM | WPOS_KEEP_LEFT, NULL);
    y += WIDGET (path_box)->lines;

    group_add_widget_autopos (g, hline_new (y++, -1, -1), WPOS_KEEP_BOTTOM, NULL);

    for (i = 0; i < hotlist_but_num; i++)
        if ((hotlist_but[i].type & list_type) != 0)
            group_add_widget_autopos (g,
                                      button_new (y + hotlist_but[i].y, UX + hotlist_but[i].x,
                                                  hotlist_but[i].ret_cmd, hotlist_but[i].flags,
                                                  hotlist_but[i].text, hotlist_button_callback),
                                      hotlist_but[i].pos_flags, NULL);

    widget_select (WIDGET (l_hotlist));
}

/* --------------------------------------------------------------------------------------------- */

static void
init_movelist (struct hotlist *item)
{
    size_t i;
    char *hdr;
    int lines, cols;
    int y;
    WGroup *g;
    Widget *movelist_widget;

    do_refresh ();

    lines = LINES - 6;
    cols = init_i18n_stuff (LIST_MOVELIST, COLS - 6);

    hdr = g_strdup_printf (_("Moving %s"), item->label);

    movelist_dlg =
        dlg_create (TRUE, 0, 0, lines, cols, WPOS_CENTER, FALSE, dialog_colors, hotlist_callback,
                    NULL, "[Hotlist]", hdr);
    g = GROUP (movelist_dlg);

    g_free (hdr);

    y = UY;
    movelist_group = groupbox_new (y, UX, lines - 7, cols - 2 * UX, _("Directory label"));
    movelist_widget = WIDGET (movelist_group);
    group_add_widget_autopos (g, movelist_widget, WPOS_KEEP_ALL, NULL);

    l_movelist =
        listbox_new (y + 1, UX + 1, movelist_widget->lines - 2, movelist_widget->cols - 2, FALSE,
                     hotlist_listbox_callback);
    fill_listbox (l_movelist);
    /* insert before groupbox to view scrollbar */
    group_add_widget_autopos (g, l_movelist, WPOS_KEEP_ALL, NULL);

    y += movelist_widget->lines;

    group_add_widget_autopos (g, hline_new (y++, -1, -1), WPOS_KEEP_BOTTOM, NULL);

    for (i = 0; i < hotlist_but_num; i++)
        if ((hotlist_but[i].type & LIST_MOVELIST) != 0)
            group_add_widget_autopos (g,
                                      button_new (y + hotlist_but[i].y, UX + hotlist_but[i].x,
                                                  hotlist_but[i].ret_cmd, hotlist_but[i].flags,
                                                  hotlist_but[i].text, hotlist_button_callback),
                                      hotlist_but[i].pos_flags, NULL);

    widget_select (WIDGET (l_movelist));
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Destroy the list dialog.
 * Don't confuse with done_hotlist() for the list in memory.
 */

static void
hotlist_done (void)
{
    dlg_destroy (hotlist_dlg);
    l_hotlist = NULL;
#if 0
    update_panels (UP_OPTIMIZE, UP_KEEPSEL);
#endif
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

static inline char *
find_group_section (struct hotlist *grp)
{
    return g_strconcat (grp->directory, ".Group", (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

static struct hotlist *
add2hotlist (char *label, char *directory, enum HotListType type, listbox_append_t pos)
{
    struct hotlist *new;
    struct hotlist *current = NULL;

    /*
     * Hotlist is neither loaded nor loading.
     * Must be called by "Ctrl-x a" before using hotlist.
     */
    if (current_group == NULL)
        load_hotlist ();

    listbox_get_current (l_hotlist, NULL, (void **) &current);

    /* Make sure '..' stays at the top of the list. */
    if ((current != NULL) && (current->type == HL_TYPE_DOTDOT))
        pos = LISTBOX_APPEND_AFTER;

    new = g_new0 (struct hotlist, 1);

    new->type = type;
    new->label = label;
    new->directory = directory;
    new->up = current_group;

    if (type == HL_TYPE_GROUP)
    {
        current_group = new;
        add_dotdot_to_list ();
        current_group = new->up;
    }

    if (current_group->head == NULL)
    {
        /* first element in group */
        current_group->head = new;
    }
    else if (pos == LISTBOX_APPEND_AFTER)
    {
        new->next = current->next;
        current->next = new;
    }
    else if (pos == LISTBOX_APPEND_BEFORE && current == current_group->head)
    {
        /* should be inserted before first item */
        new->next = current;
        current_group->head = new;
    }
    else if (pos == LISTBOX_APPEND_BEFORE)
    {
        struct hotlist *p = current_group->head;

        while (p->next != current)
            p = p->next;

        new->next = current;
        p->next = new;
    }
    else
    {                           /* append at the end */
        struct hotlist *p = current_group->head;

        while (p->next != NULL)
            p = p->next;

        p->next = new;
    }

    if (hotlist_state.running && type != HL_TYPE_COMMENT && type != HL_TYPE_DOTDOT)
    {
        if (type == HL_TYPE_GROUP)
        {
            char *lbl;

            lbl = g_strconcat ("->", new->label, (char *) NULL);
            listbox_add_item (l_hotlist, pos, 0, lbl, new, FALSE);
            g_free (lbl);
        }
        else
            listbox_add_item (l_hotlist, pos, 0, new->label, new, FALSE);
        listbox_select_entry (l_hotlist, l_hotlist->pos);
    }

    return new;
}

/* --------------------------------------------------------------------------------------------- */

static int
add_new_entry_input (const char *header, const char *text1, const char *text2,
                     const char *help, char **r1, char **r2)
{
    quick_widget_t quick_widgets[] = {
        /* *INDENT-OFF* */
        QUICK_LABELED_INPUT (text1, input_label_above, *r1, "input-lbl", r1, NULL,
                             FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_SEPARATOR (FALSE),
        QUICK_LABELED_INPUT (text2, input_label_above, *r2, "input-lbl", r2, NULL,
                             FALSE, FALSE, INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_CD),
        QUICK_START_BUTTONS (TRUE, TRUE),
            QUICK_BUTTON (N_("&Append"), B_APPEND, NULL, NULL),
            QUICK_BUTTON (N_("&Insert"), B_INSERT, NULL, NULL),
            QUICK_BUTTON (N_("&Cancel"), B_CANCEL, NULL, NULL),
        QUICK_END
        /* *INDENT-ON* */
    };

    quick_dialog_t qdlg = {
        -1, -1, 64,
        header, help,
        quick_widgets, NULL, NULL
    };

    int ret;

    ret = quick_dialog (&qdlg);

    return (ret != B_CANCEL) ? ret : 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
add_new_entry_cmd (WPanel * panel)
{
    char *title, *url, *to_free;
    int ret;

    /* Take current directory as default value for input fields */
    to_free = title = url = vfs_path_to_str_flags (panel->cwd_vpath, 0, VPF_STRIP_PASSWORD);

    ret = add_new_entry_input (_("New hotlist entry"), _("Directory label:"),
                               _("Directory path:"), "[Hotlist]", &title, &url);
    g_free (to_free);

    if (ret == 0)
        return;
    if (title == NULL || *title == '\0' || url == NULL || *url == '\0')
    {
        g_free (title);
        g_free (url);
        return;
    }

    if (ret == B_ENTER || ret == B_APPEND)
        add2hotlist (title, url, HL_TYPE_ENTRY, LISTBOX_APPEND_AFTER);
    else
        add2hotlist (title, url, HL_TYPE_ENTRY, LISTBOX_APPEND_BEFORE);

    hotlist_state.modified = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static int
add_new_group_input (const char *header, const char *label, char **result)
{
    quick_widget_t quick_widgets[] = {
        /* *INDENT-OFF* */
        QUICK_LABELED_INPUT (label, input_label_above, "", "input", result, NULL,
                             FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_START_BUTTONS (TRUE, TRUE),
            QUICK_BUTTON (N_("&Append"), B_APPEND, NULL, NULL),
            QUICK_BUTTON (N_("&Insert"), B_INSERT, NULL, NULL),
            QUICK_BUTTON (N_("&Cancel"), B_CANCEL, NULL, NULL),
        QUICK_END
        /* *INDENT-ON* */
    };

    quick_dialog_t qdlg = {
        -1, -1, 64,
        header, "[Hotlist]",
        quick_widgets, NULL, NULL
    };

    int ret;

    ret = quick_dialog (&qdlg);

    return (ret != B_CANCEL) ? ret : 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
add_new_group_cmd (void)
{
    char *label;
    int ret;

    ret = add_new_group_input (_("New hotlist group"), _("Name of new group:"), &label);
    if (ret == 0 || label == NULL || *label == '\0')
        return;

    if (ret == B_ENTER || ret == B_APPEND)
        add2hotlist (label, 0, HL_TYPE_GROUP, LISTBOX_APPEND_AFTER);
    else
        add2hotlist (label, 0, HL_TYPE_GROUP, LISTBOX_APPEND_BEFORE);

    hotlist_state.modified = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
remove_group (struct hotlist *grp)
{
    struct hotlist *current = grp->head;

    while (current != NULL)
    {
        struct hotlist *next = current->next;

        if (current->type == HL_TYPE_GROUP)
            remove_group (current);

        g_free (current->label);
        g_free (current->directory);
        g_free (current);

        current = next;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
remove_from_hotlist (struct hotlist *entry)
{
    if (entry == NULL)
        return;

    if (entry->type == HL_TYPE_DOTDOT)
        return;

    if (confirm_directory_hotlist_delete)
    {
        char text[BUF_MEDIUM];
        int result;

        if (safe_delete)
            query_set_sel (1);

        g_snprintf (text, sizeof (text), _("Are you sure you want to remove entry \"%s\"?"),
                    str_trunc (entry->label, 30));
        result = query_dialog (Q_ ("DialogTitle|Delete"), text, D_ERROR | D_CENTER, 2,
                               _("&Yes"), _("&No"));
        if (result != 0)
            return;
    }

    if (entry->type == HL_TYPE_GROUP)
    {
        struct hotlist *head = entry->head;

        if (head != NULL && (head->type != HL_TYPE_DOTDOT || head->next != NULL))
        {
            char text[BUF_MEDIUM];
            int result;

            g_snprintf (text, sizeof (text), _("Group \"%s\" is not empty.\nRemove it?"),
                        str_trunc (entry->label, 30));
            result = query_dialog (Q_ ("DialogTitle|Delete"), text, D_ERROR | D_CENTER, 2,
                                   _("&Yes"), _("&No"));
            if (result != 0)
                return;
        }

        remove_group (entry);
    }

    unlink_entry (entry);

    g_free (entry->label);
    g_free (entry->directory);
    g_free (entry);
    /* now remove list entry from screen */
    listbox_remove_current (l_hotlist);
    hotlist_state.modified = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
load_group (struct hotlist *grp)
{
    gchar **profile_keys, **keys;
    char *group_section;
    struct hotlist *current = 0;

    group_section = find_group_section (grp);

    keys = mc_config_get_keys (mc_global.main_config, group_section, NULL);

    current_group = grp;

    for (profile_keys = keys; *profile_keys != NULL; profile_keys++)
        add2hotlist (mc_config_get_string (mc_global.main_config, group_section, *profile_keys, ""),
                     g_strdup (*profile_keys), HL_TYPE_GROUP, LISTBOX_APPEND_AT_END);

    g_free (group_section);
    g_strfreev (keys);

    keys = mc_config_get_keys (mc_global.main_config, grp->directory, NULL);

    for (profile_keys = keys; *profile_keys != NULL; profile_keys++)
        add2hotlist (mc_config_get_string (mc_global.main_config, group_section, *profile_keys, ""),
                     g_strdup (*profile_keys), HL_TYPE_ENTRY, LISTBOX_APPEND_AT_END);

    g_strfreev (keys);

    for (current = grp->head; current; current = current->next)
        load_group (current);
}

/* --------------------------------------------------------------------------------------------- */

static int
hot_skip_blanks (void)
{
    int c;

    while ((c = getc (hotlist_file)) != EOF && c != '\n' && g_ascii_isspace (c))
        ;
    return c;
}

/* --------------------------------------------------------------------------------------------- */

static int
hot_next_token (void)
{
    int c, ret = 0;
    size_t l;

    if (tkn_buf == NULL)
        tkn_buf = g_string_new ("");
    g_string_set_size (tkn_buf, 0);

  again:
    c = hot_skip_blanks ();
    switch (c)
    {
    case EOF:
        ret = TKN_EOF;
        break;
    case '\n':
        ret = TKN_EOL;
        break;
    case '#':
        while ((c = getc (hotlist_file)) != EOF && c != '\n')
            g_string_append_c (tkn_buf, c);
        ret = TKN_COMMENT;
        break;
    case '"':
        while ((c = getc (hotlist_file)) != EOF && c != '"')
        {
            if (c == '\\')
            {
                c = getc (hotlist_file);
                if (c == EOF)
                {
                    g_string_free (tkn_buf, TRUE);
                    return TKN_EOF;
                }
            }
            g_string_append_c (tkn_buf, c == '\n' ? ' ' : c);
        }
        ret = (c == EOF) ? TKN_EOF : TKN_STRING;
        break;
    case '\\':
        c = getc (hotlist_file);
        if (c == EOF)
        {
            g_string_free (tkn_buf, TRUE);
            return TKN_EOF;
        }
        if (c == '\n')
            goto again;

        MC_FALLTHROUGH;         /* it is taken as normal character */

    default:
        do
        {
            g_string_append_c (tkn_buf, g_ascii_toupper (c));
        }
        while ((c = fgetc (hotlist_file)) != EOF && (g_ascii_isalnum (c) || !isascii (c)));
        if (c != EOF)
            ungetc (c, hotlist_file);
        l = tkn_buf->len;
        if (strncmp (tkn_buf->str, "GROUP", l) == 0)
            ret = TKN_GROUP;
        else if (strncmp (tkn_buf->str, "ENTRY", l) == 0)
            ret = TKN_ENTRY;
        else if (strncmp (tkn_buf->str, "ENDGROUP", l) == 0)
            ret = TKN_ENDGROUP;
        else if (strncmp (tkn_buf->str, "URL", l) == 0)
            ret = TKN_URL;
        else
            ret = TKN_UNKNOWN;
        break;
    }
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static void
hot_load_group (struct hotlist *grp)
{
    int tkn;
    struct hotlist *new_grp;
    char *label, *url;

    current_group = grp;

    while ((tkn = hot_next_token ()) != TKN_ENDGROUP)
        switch (tkn)
        {
        case TKN_GROUP:
            CHECK_TOKEN (TKN_STRING);
            new_grp =
                add2hotlist (g_strndup (tkn_buf->str, tkn_buf->len), 0, HL_TYPE_GROUP,
                             LISTBOX_APPEND_AT_END);
            SKIP_TO_EOL;
            hot_load_group (new_grp);
            current_group = grp;
            break;
        case TKN_ENTRY:
            {
                CHECK_TOKEN (TKN_STRING);
                label = g_strndup (tkn_buf->str, tkn_buf->len);
                CHECK_TOKEN (TKN_URL);
                CHECK_TOKEN (TKN_STRING);
                url = tilde_expand (tkn_buf->str);
                add2hotlist (label, url, HL_TYPE_ENTRY, LISTBOX_APPEND_AT_END);
                SKIP_TO_EOL;
            }
            break;
        case TKN_COMMENT:
            label = g_strndup (tkn_buf->str, tkn_buf->len);
            add2hotlist (label, 0, HL_TYPE_COMMENT, LISTBOX_APPEND_AT_END);
            break;
        case TKN_EOF:
            hotlist_state.readonly = TRUE;
            hotlist_state.file_error = TRUE;
            return;
        case TKN_EOL:
            /* skip empty lines */
            break;
        default:
            hotlist_state.readonly = TRUE;
            hotlist_state.file_error = TRUE;
            SKIP_TO_EOL;
            break;
        }
    SKIP_TO_EOL;
}

/* --------------------------------------------------------------------------------------------- */

static void
hot_load_file (struct hotlist *grp)
{
    int tkn;
    struct hotlist *new_grp;
    char *label, *url;

    current_group = grp;

    while ((tkn = hot_next_token ()) != TKN_EOF)
        switch (tkn)
        {
        case TKN_GROUP:
            CHECK_TOKEN (TKN_STRING);
            new_grp =
                add2hotlist (g_strndup (tkn_buf->str, tkn_buf->len), 0, HL_TYPE_GROUP,
                             LISTBOX_APPEND_AT_END);
            SKIP_TO_EOL;
            hot_load_group (new_grp);
            current_group = grp;
            break;
        case TKN_ENTRY:
            {
                CHECK_TOKEN (TKN_STRING);
                label = g_strndup (tkn_buf->str, tkn_buf->len);
                CHECK_TOKEN (TKN_URL);
                CHECK_TOKEN (TKN_STRING);
                url = tilde_expand (tkn_buf->str);
                add2hotlist (label, url, HL_TYPE_ENTRY, LISTBOX_APPEND_AT_END);
                SKIP_TO_EOL;
            }
            break;
        case TKN_COMMENT:
            label = g_strndup (tkn_buf->str, tkn_buf->len);
            add2hotlist (label, 0, HL_TYPE_COMMENT, LISTBOX_APPEND_AT_END);
            break;
        case TKN_EOL:
            /* skip empty lines */
            break;
        default:
            hotlist_state.readonly = TRUE;
            hotlist_state.file_error = TRUE;
            SKIP_TO_EOL;
            break;
        }
}

/* --------------------------------------------------------------------------------------------- */

static void
clean_up_hotlist_groups (const char *section)
{
    char *grp_section;

    grp_section = g_strconcat (section, ".Group", (char *) NULL);
    if (mc_config_has_group (mc_global.main_config, section))
        mc_config_del_group (mc_global.main_config, section);

    if (mc_config_has_group (mc_global.main_config, grp_section))
    {
        char **profile_keys, **keys;

        keys = mc_config_get_keys (mc_global.main_config, grp_section, NULL);

        for (profile_keys = keys; *profile_keys != NULL; profile_keys++)
            clean_up_hotlist_groups (*profile_keys);

        g_strfreev (keys);
        mc_config_del_group (mc_global.main_config, grp_section);
    }
    g_free (grp_section);
}

/* --------------------------------------------------------------------------------------------- */

static void
load_hotlist (void)
{
    gboolean remove_old_list = FALSE;
    struct stat stat_buf;

    if (hotlist_state.loaded)
    {
        stat (hotlist_file_name, &stat_buf);
        if (hotlist_file_mtime < stat_buf.st_mtime)
            done_hotlist ();
        else
            return;
    }

    if (hotlist_file_name == NULL)
        hotlist_file_name = mc_config_get_full_path (MC_HOTLIST_FILE);

    hotlist = g_new0 (struct hotlist, 1);
    hotlist->type = HL_TYPE_GROUP;
    hotlist->label = g_strdup (_("Top level group"));
    hotlist->up = hotlist;
    /*
     * compatibility :-(
     */
    hotlist->directory = g_strdup ("Hotlist");

    hotlist_file = fopen (hotlist_file_name, "r");
    if (hotlist_file == NULL)
    {
        int result;

        load_group (hotlist);
        hotlist_state.loaded = TRUE;
        /*
         * just to be sure we got copy
         */
        hotlist_state.modified = TRUE;
        result = save_hotlist ();
        hotlist_state.modified = FALSE;
        if (result != 0)
            remove_old_list = TRUE;
        else
            message (D_ERROR, _("Hotlist Load"),
                     _
                     ("MC was unable to write %s file,\nyour old hotlist entries were not deleted"),
                     MC_USERCONF_DIR PATH_SEP_STR MC_HOTLIST_FILE);
    }
    else
    {
        hot_load_file (hotlist);
        fclose (hotlist_file);
        hotlist_state.loaded = TRUE;
    }

    if (remove_old_list)
    {
        GError *mcerror = NULL;

        clean_up_hotlist_groups ("Hotlist");
        if (!mc_config_save_file (mc_global.main_config, &mcerror))
            setup_save_config_show_error (mc_global.main_config->ini_path, &mcerror);

        mc_error_message (&mcerror, NULL);
    }

    stat (hotlist_file_name, &stat_buf);
    hotlist_file_mtime = stat_buf.st_mtime;
    current_group = hotlist;
}

/* --------------------------------------------------------------------------------------------- */

static void
hot_save_group (struct hotlist *grp)
{
    struct hotlist *current;
    int i;
    char *s;

#define INDENT(n) \
do { \
    for (i = 0; i < n; i++) \
        putc (' ', hotlist_file); \
} while (0)

    for (current = grp->head; current != NULL; current = current->next)
        switch (current->type)
        {
        case HL_TYPE_GROUP:
            INDENT (list_level);
            fputs ("GROUP \"", hotlist_file);
            for (s = current->label; *s != '\0'; s++)
            {
                if (*s == '"' || *s == '\\')
                    putc ('\\', hotlist_file);
                putc (*s, hotlist_file);
            }
            fputs ("\"\n", hotlist_file);
            list_level += 2;
            hot_save_group (current);
            list_level -= 2;
            INDENT (list_level);
            fputs ("ENDGROUP\n", hotlist_file);
            break;
        case HL_TYPE_ENTRY:
            INDENT (list_level);
            fputs ("ENTRY \"", hotlist_file);
            for (s = current->label; *s != '\0'; s++)
            {
                if (*s == '"' || *s == '\\')
                    putc ('\\', hotlist_file);
                putc (*s, hotlist_file);
            }
            fputs ("\" URL \"", hotlist_file);
            for (s = current->directory; *s != '\0'; s++)
            {
                if (*s == '"' || *s == '\\')
                    putc ('\\', hotlist_file);
                putc (*s, hotlist_file);
            }
            fputs ("\"\n", hotlist_file);
            break;
        case HL_TYPE_COMMENT:
            fprintf (hotlist_file, "#%s\n", current->label);
            break;
        case HL_TYPE_DOTDOT:
            /* do nothing */
            break;
        default:
            break;
        }
}

/* --------------------------------------------------------------------------------------------- */

static void
add_dotdot_to_list (void)
{
    if (current_group != hotlist && hotlist_has_dot_dot)
        add2hotlist (g_strdup (".."), g_strdup (".."), HL_TYPE_DOTDOT, LISTBOX_APPEND_AT_END);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
add2hotlist_cmd (WPanel * panel)
{
    char *lc_prompt;
    const char *cp = N_("Label for \"%s\":");
    int l;
    char *label_string, *label;

#ifdef ENABLE_NLS
    cp = _(cp);
#endif

    /* extra variable to use it in the button callback */
    our_panel = panel;

    l = str_term_width1 (cp);
    label_string = vfs_path_to_str_flags (panel->cwd_vpath, 0, VPF_STRIP_PASSWORD);
    lc_prompt = g_strdup_printf (cp, str_trunc (label_string, COLS - 2 * UX - (l + 8)));
    label =
        input_dialog (_("Add to hotlist"), lc_prompt, MC_HISTORY_HOTLIST_ADD, label_string,
                      INPUT_COMPLETE_NONE);
    g_free (lc_prompt);

    if (label == NULL || *label == '\0')
    {
        g_free (label_string);
        g_free (label);
    }
    else
    {
        add2hotlist (label, label_string, HL_TYPE_ENTRY, LISTBOX_APPEND_AT_END);
        hotlist_state.modified = TRUE;
    }
}

/* --------------------------------------------------------------------------------------------- */

char *
hotlist_show (hotlist_t list_type)
{
    char *target = NULL;
    int res;

    hotlist_state.type = list_type;
    load_hotlist ();

    init_hotlist (list_type);

    /* display file info */
    tty_setcolor (SELECTED_COLOR);

    hotlist_state.running = TRUE;
    res = dlg_run (hotlist_dlg);
    hotlist_state.running = FALSE;
    save_hotlist ();

    if (res == B_ENTER)
    {
        char *text = NULL;
        struct hotlist *hlp = NULL;

        listbox_get_current (l_hotlist, &text, (void **) &hlp);
        target = g_strdup (hlp != NULL ? hlp->directory : text);
    }

    hotlist_done ();
    return target;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
save_hotlist (void)
{
    gboolean saved = FALSE;
    struct stat stat_buf;

    if (!hotlist_state.readonly && hotlist_state.modified && hotlist_file_name != NULL)
    {
        mc_util_make_backup_if_possible (hotlist_file_name, ".bak");

        hotlist_file = fopen (hotlist_file_name, "w");
        if (hotlist_file == NULL)
            mc_util_restore_from_backup_if_possible (hotlist_file_name, ".bak");
        else
        {
            hot_save_group (hotlist);
            fclose (hotlist_file);
            stat (hotlist_file_name, &stat_buf);
            hotlist_file_mtime = stat_buf.st_mtime;
            hotlist_state.modified = FALSE;
            saved = TRUE;
        }
    }

    return saved;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Unload list from memory.
 * Don't confuse with hotlist_done() for GUI.
 */

void
done_hotlist (void)
{
    if (hotlist != NULL)
    {
        remove_group (hotlist);
        g_free (hotlist->label);
        g_free (hotlist->directory);
        MC_PTR_FREE (hotlist);
    }

    hotlist_state.loaded = FALSE;

    MC_PTR_FREE (hotlist_file_name);
    l_hotlist = NULL;
    current_group = NULL;

    if (tkn_buf != NULL)
    {
        g_string_free (tkn_buf, TRUE);
        tkn_buf = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */
