/*
   Directory hotlist -- for the Midnight Commander

   Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
   2003, 2004, 2005, 2006, 2007, 2008, 2011
   The Free Software Foundation, Inc.

   Written by:
   Radek Doulik, 1994
   Janne Kukonlehto, 1995
   Andrej Borsenkow, 1996
   Norbert Warmuth, 1997

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

#include "midnight.h"           /* current_panel */
#include "command.h"            /* cmdline */

#include "hotlist.h"

/*** global variables ****************************************************************************/

int hotlist_has_dot_dot = 1;

/*** file scope macro definitions ****************************************************************/

#define UX 5
#define UY 2

#define BX UX
#define BY (LINES - 6)

#define BUTTONS (sizeof(hotlist_but)/sizeof(struct _hotlist_but))
#define LABELS          3
#define B_ADD_CURRENT   B_USER
#define B_REMOVE        (B_USER + 1)
#define B_NEW_GROUP     (B_USER + 2)
#define B_NEW_ENTRY     (B_USER + 3)
#define B_UP_GROUP      (B_USER + 4)
#define B_INSERT        (B_USER + 5)
#define B_APPEND        (B_USER + 6)
#define B_MOVE          (B_USER + 7)

#ifdef ENABLE_VFS
#define B_FREE_ALL_VFS (B_USER + 8)
#define B_REFRESH_VFS (B_USER + 9)
#endif

#define new_hotlist() g_new0(struct hotlist, 1)

#define CHECK_BUFFER \
do \
{ \
    size_t i; \
    i = strlen (current->label); \
    if (i + 3 > buflen) { \
      g_free (buf); \
      buflen = 1024 * (i/1024 + 1); \
      buf = g_malloc (buflen); \
    } \
    buf[0] = '\0'; \
} while (0)

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
    hotlist_state.readonly = 1; \
    hotlist_state.file_error = 1; \
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
     * these parameters are intended to be user configurable
     */
    int expanded;               /* expanded view of all groups at startup */

    /*
     * these reflect run time state
     */

    int loaded;                 /* hotlist is loaded */
    int readonly;               /* hotlist readonly */
    int file_error;             /* parse error while reading file */
    int running;                /* we are running dlg (and have to
                                   update listbox */
    int moving;                 /* we are in moving hotlist currently */
    int modified;               /* hotlist was modified */
    int type;                   /* LIST_HOTLIST || LIST_VFSLIST */
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

static WListbox *l_hotlist;
static WListbox *l_movelist;

static Dlg_head *hotlist_dlg;
static Dlg_head *movelist_dlg;

static WLabel *pname, *pname_group, *movelist_group;

static struct _hotlist_but
{
    int ret_cmd, flags, y, x;
    const char *text;
    int type;
    widget_pos_flags_t pos_flags;
} hotlist_but[] =
{
    /* *INDENT-OFF* */
    { B_MOVE, NORMAL_BUTTON, 1, 42, N_("&Move"), LIST_HOTLIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
    { B_REMOVE, NORMAL_BUTTON, 1, 30, N_("&Remove"),
            LIST_HOTLIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
    { B_APPEND, NORMAL_BUTTON, 1, 15, N_("&Append"),
            LIST_MOVELIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
    { B_INSERT, NORMAL_BUTTON, 1, 0, N_("&Insert"),
            LIST_MOVELIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
    { B_NEW_ENTRY, NORMAL_BUTTON, 1, 15, N_("New &entry"),
            LIST_HOTLIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
    { B_NEW_GROUP, NORMAL_BUTTON, 1, 0, N_("New &group"),
            LIST_HOTLIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
    { B_CANCEL, NORMAL_BUTTON, 0, 53, N_("&Cancel"),
            LIST_HOTLIST | LIST_VFSLIST | LIST_MOVELIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
    { B_UP_GROUP, NORMAL_BUTTON, 0, 42, N_("&Up"),
            LIST_HOTLIST | LIST_MOVELIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
    { B_ADD_CURRENT, NORMAL_BUTTON, 0, 20, N_("&Add current"),
            LIST_HOTLIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
#ifdef ENABLE_VFS
    { B_REFRESH_VFS, NORMAL_BUTTON, 0, 43, N_("&Refresh"),
            LIST_VFSLIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
    { B_FREE_ALL_VFS, NORMAL_BUTTON, 0, 20, N_("Fr&ee VFSs now"),
            LIST_VFSLIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM },
#endif
    { B_ENTER, DEFPUSH_BUTTON, 0, 0, N_("Change &to"),
            LIST_HOTLIST | LIST_VFSLIST | LIST_MOVELIST, WPOS_KEEP_LEFT | WPOS_KEEP_BOTTOM }
    /* *INDENT-ON* */
};

static struct hotlist *hotlist = NULL;

static struct hotlist *current_group;

static GString *tkn_buf = NULL;

static char *hotlist_file_name;
static FILE *hotlist_file;
static time_t hotlist_file_mtime;

static int list_level = 0;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void init_movelist (int, struct hotlist *);
static void add_new_group_cmd (void);
static void add_new_entry_cmd (void);
static void remove_from_hotlist (struct hotlist *entry);
static void load_hotlist (void);
static void add_dotdot_to_list (void);

/* --------------------------------------------------------------------------------------------- */

static void
hotlist_refresh (Dlg_head * h)
{
    Widget *wh = WIDGET (h);

    /* TODO: use groupboxes here */
    common_dialog_repaint (h);
    tty_setcolor (COLOR_NORMAL);
    draw_box (h, 2, 5, wh->lines - (hotlist_state.moving ? 6 : 10), wh->cols - (UX * 2), TRUE);
    if (!hotlist_state.moving)
        draw_box (h, wh->lines - 8, 5, 3, wh->cols - (UX * 2), TRUE);
}

/* --------------------------------------------------------------------------------------------- */
/** If current->data is 0, then we are dealing with a VFS pathname */

static void
update_path_name (void)
{
    const char *text = "";
    char *p;
    WListbox *list = hotlist_state.moving ? l_movelist : l_hotlist;
    Dlg_head *owner = WIDGET (list)->owner;
    Widget *wo = WIDGET (owner);

    if (list->count != 0)
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
    if (!hotlist_state.moving)
        label_set_text (pname, str_trunc (text, wo->cols - (UX * 2 + 4)));

    p = g_strconcat (" ", current_group->label, " ", (char *) NULL);
    if (!hotlist_state.moving)
        label_set_text (pname_group, str_trunc (p, wo->cols - (UX * 2 + 4)));
    else
        label_set_text (movelist_group, str_trunc (p, wo->cols - (UX * 2 + 4)));
    g_free (p);

    dlg_redraw (owner);
}

/* --------------------------------------------------------------------------------------------- */

static void
fill_listbox (void)
{
    struct hotlist *current = current_group->head;
    GString *buff = g_string_new ("");

    while (current)
    {
        switch (current->type)
        {
        case HL_TYPE_GROUP:
            {
                /* buff clean up */
                g_string_truncate (buff, 0);
                g_string_append (buff, "->");
                g_string_append (buff, current->label);
                if (hotlist_state.moving)
                    listbox_add_item (l_movelist, LISTBOX_APPEND_AT_END, 0, buff->str, current);
                else
                    listbox_add_item (l_hotlist, LISTBOX_APPEND_AT_END, 0, buff->str, current);
            }
            break;
        case HL_TYPE_DOTDOT:
        case HL_TYPE_ENTRY:
            if (hotlist_state.moving)
                listbox_add_item (l_movelist, LISTBOX_APPEND_AT_END, 0, current->label, current);
            else
                listbox_add_item (l_hotlist, LISTBOX_APPEND_AT_END, 0, current->label, current);
            break;
        default:
            break;
        }
        current = current->next;
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
        while (current && current->next != entry)
            current = current->next;
        if (current)
            current->next = entry->next;
    }
    entry->next = entry->up = 0;
}

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_VFS
static void
add_name_to_list (const char *path)
{
    listbox_add_item (l_hotlist, LISTBOX_APPEND_AT_END, 0, path, 0);
}
#endif /* !ENABLE_VFS */

/* --------------------------------------------------------------------------------------------- */

static int
hotlist_button_callback (WButton * button, int action)
{
    (void) button;

    switch (action)
    {
    case B_MOVE:
        {
            struct hotlist *saved = current_group;
            struct hotlist *item = NULL;
            struct hotlist *moveto_item = NULL;
            struct hotlist *moveto_group = NULL;
            int ret;

            if (l_hotlist->count == 0)
                return MSG_NOT_HANDLED; /* empty group - nothing to do */

            listbox_get_current (l_hotlist, NULL, (void **) &item);
            hotlist_state.moving = 1;
            init_movelist (LIST_MOVELIST, item);

            ret = run_dlg (movelist_dlg);

            hotlist_state.moving = 0;
            listbox_get_current (l_movelist, NULL, (void **) &moveto_item);
            moveto_group = current_group;
            destroy_dlg (movelist_dlg);
            current_group = saved;
            if (ret == B_CANCEL)
                return MSG_NOT_HANDLED;
            if (moveto_item == item)
                return MSG_NOT_HANDLED; /* If we insert/append a before/after a
                                           it hardly changes anything ;) */
            unlink_entry (item);
            listbox_remove_current (l_hotlist);
            item->up = moveto_group;
            if (!moveto_group->head)
                moveto_group->head = item;
            else if (!moveto_item)
            {                   /* we have group with just comments */
                struct hotlist *p = moveto_group->head;

                /* skip comments */
                while (p->next)
                    p = p->next;
                p->next = item;
            }
            else if (ret == B_ENTER || ret == B_APPEND)
                if (!moveto_item->next)
                    moveto_item->next = item;
                else
                {
                    item->next = moveto_item->next;
                    moveto_item->next = item;
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
            fill_listbox ();
            repaint_screen ();
            hotlist_state.modified = 1;
            return MSG_NOT_HANDLED;
        }
    case B_REMOVE:
        {
            struct hotlist *entry = NULL;
            listbox_get_current (l_hotlist, NULL, (void **) &entry);
            remove_from_hotlist (entry);
        }
        return MSG_NOT_HANDLED;

    case B_NEW_GROUP:
        add_new_group_cmd ();
        return MSG_NOT_HANDLED;

    case B_ADD_CURRENT:
        add2hotlist_cmd ();
        return MSG_NOT_HANDLED;

    case B_NEW_ENTRY:
        add_new_entry_cmd ();
        return MSG_NOT_HANDLED;

    case B_ENTER:
        {
            WListbox *list;
            void *data;
            struct hotlist *hlp;

            list = hotlist_state.moving ? l_movelist : l_hotlist;
            listbox_get_current (list, NULL, &data);

            if (data == NULL)
                return MSG_HANDLED;

            hlp = (struct hotlist *) data;

            if (hlp->type == HL_TYPE_ENTRY)
                return MSG_HANDLED;
            if (hlp->type != HL_TYPE_DOTDOT)
            {
                listbox_remove_list (list);
                current_group = hlp;
                fill_listbox ();
                return MSG_NOT_HANDLED;
            }
            /* Fall through - go up */
        }
        /* Fall through if list empty - just go up */

    case B_UP_GROUP:
        {
            WListbox *list = hotlist_state.moving ? l_movelist : l_hotlist;
            listbox_remove_list (list);
            current_group = current_group->up;
            fill_listbox ();
            return MSG_NOT_HANDLED;
        }

#ifdef	ENABLE_VFS
    case B_FREE_ALL_VFS:
        vfs_expire (TRUE);
        /* fall through */

    case B_REFRESH_VFS:
        listbox_remove_list (l_hotlist);
        listbox_add_item (l_hotlist, LISTBOX_APPEND_AT_END, 0, mc_config_get_home_dir (), 0);
        vfs_fill_names (add_name_to_list);
        return MSG_NOT_HANDLED;
#endif /* ENABLE_VFS */

    default:
        return MSG_HANDLED;
    }
}

/* --------------------------------------------------------------------------------------------- */

static inline cb_ret_t
hotlist_handle_key (Dlg_head * h, int key)
{
    switch (key)
    {
    case KEY_M_CTRL | '\n':
        goto l1;

    case '\n':
    case KEY_ENTER:
    case KEY_RIGHT:
        if (hotlist_button_callback (NULL, B_ENTER))
        {
            h->ret_value = B_ENTER;
            dlg_stop (h);
        }
        return MSG_HANDLED;

    case KEY_LEFT:
        if (hotlist_state.type != LIST_VFSLIST)
            return !hotlist_button_callback (NULL, B_UP_GROUP);
        else
            return MSG_NOT_HANDLED;

    case KEY_DC:
        if (hotlist_state.moving)
            return MSG_NOT_HANDLED;
        else
        {
            hotlist_button_callback (NULL, B_REMOVE);
            return MSG_HANDLED;
        }

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
hotlist_callback (Dlg_head * h, Widget * sender, dlg_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case DLG_DRAW:
        hotlist_refresh (h);
        return MSG_HANDLED;

    case DLG_UNHANDLED_KEY:
        return hotlist_handle_key (h, parm);

    case DLG_POST_KEY:
        if (hotlist_state.moving)
            dlg_select_widget (l_movelist);
        else
            dlg_select_widget (l_hotlist);
        /* always stay on hotlist */
        /* fall through */

    case DLG_INIT:
        tty_setcolor (MENU_ENTRY_COLOR);
        update_path_name ();
        return MSG_HANDLED;

    case DLG_RESIZE:
        /* simply call dlg_set_size() with new size */
        dlg_set_size (h, LINES - 2, COLS - 6);
        return MSG_HANDLED;

    default:
        return default_dlg_callback (h, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static lcback_ret_t
l_call (WListbox * list)
{
    Dlg_head *dlg = WIDGET (list)->owner;

    if (list->count != 0)
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
                hotlist_callback (dlg, NULL, DLG_POST_KEY, '\n', NULL);
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
    hotlist_callback (dlg, NULL, DLG_POST_KEY, 'u', NULL);
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
    register int i;
    static const char *cancel_but = N_("&Cancel");

#ifdef ENABLE_NLS
    static int hotlist_i18n_flag = 0;

    if (!hotlist_i18n_flag)
    {
        i = sizeof (hotlist_but) / sizeof (hotlist_but[0]);
        while (i--)
            hotlist_but[i].text = _(hotlist_but[i].text);

        cancel_but = _(cancel_but);
        hotlist_i18n_flag = 1;
    }
#endif /* ENABLE_NLS */

    /* Dynamic resizing of buttonbars */
    {
        int len[2], count[2];   /* at most two lines of buttons */
        int cur_x[2], row;

        i = sizeof (hotlist_but) / sizeof (hotlist_but[0]);
        len[0] = len[1] = count[0] = count[1] = 0;

        /* Count len of buttonbars, assuming 2 extra space between buttons */
        while (i--)
        {
            if (!(hotlist_but[i].type & list_type))
                continue;

            row = hotlist_but[i].y;
            ++count[row];
            len[row] += str_term_width1 (hotlist_but[i].text) + 5;
            if (hotlist_but[i].flags == DEFPUSH_BUTTON)
                len[row] += 2;
        }
        len[0] -= 2;
        len[1] -= 2;

        cols = max (cols, max (len[0], len[1]));

        /* arrange buttons */

        cur_x[0] = cur_x[1] = 0;
        i = sizeof (hotlist_but) / sizeof (hotlist_but[0]);
        while (i--)
        {
            if (!(hotlist_but[i].type & list_type))
                continue;

            row = hotlist_but[i].y;

            if (hotlist_but[i].x != 0)
            {
                /* not first int the row */
                if (!strcmp (hotlist_but[i].text, cancel_but))
                    hotlist_but[i].x = cols - str_term_width1 (hotlist_but[i].text) - 13;
                else
                    hotlist_but[i].x = cur_x[row];
            }

            cur_x[row] += str_term_width1 (hotlist_but[i].text) + 2
                + (hotlist_but[i].flags == DEFPUSH_BUTTON ? 5 : 3);
        }
    }

    return cols;
}

/* --------------------------------------------------------------------------------------------- */

static void
init_hotlist (int list_type)
{
    size_t i;
    const char *title, *help_node;
    int hotlist_cols;

    hotlist_cols = init_i18n_stuff (list_type, COLS - 6);

    do_refresh ();

    hotlist_state.expanded =
        mc_config_get_int (mc_main_config, "HotlistConfig", "expanded_view_of_groups", 0);

    if (list_type == LIST_VFSLIST)
    {
        title = _("Active VFS directories");
        help_node = "[vfshot]"; /* FIXME - no such node */
    }
    else
    {
        title = _("Directory hotlist");
        help_node = "[Hotlist]";
    }

    hotlist_dlg =
        create_dlg (TRUE, 0, 0, LINES - 2, hotlist_cols, dialog_colors,
                    hotlist_callback, NULL, help_node, title, DLG_CENTER | DLG_REVERSE);

    for (i = 0; i < BUTTONS; i++)
    {
        if (hotlist_but[i].type & list_type)
            add_widget_autopos (hotlist_dlg,
                                button_new (BY + hotlist_but[i].y,
                                            BX + hotlist_but[i].x,
                                            hotlist_but[i].ret_cmd,
                                            hotlist_but[i].flags,
                                            hotlist_but[i].text,
                                            hotlist_button_callback), hotlist_but[i].pos_flags,
                                NULL);
    }

    /* We add the labels.
     *    pname       will hold entry's pathname;
     *    pname_group will hold name of current group
     */
    pname = label_new (UY - 11 + LINES, UX + 2, "");
    add_widget_autopos (hotlist_dlg, pname, WPOS_KEEP_BOTTOM | WPOS_KEEP_LEFT, NULL);
    if (!hotlist_state.moving)
    {
        char label_text[BUF_TINY];

        g_snprintf (label_text, sizeof (label_text), " %s ", _("Directory path"));
        add_widget_autopos (hotlist_dlg,
                            label_new (UY - 12 + LINES, UX + 2,
                                       label_text), WPOS_KEEP_BOTTOM | WPOS_KEEP_LEFT, NULL);

        /* This one holds the displayed pathname */
        pname_group = label_new (UY, UX + 2, _("Directory label"));
        add_widget (hotlist_dlg, pname_group);
    }
    /* get new listbox */
    l_hotlist = listbox_new (UY + 1, UX + 1, LINES - 14, COLS - 2 * UX - 8, FALSE, l_call);

    /* Fill the hotlist with the active VFS or the hotlist */
#ifdef ENABLE_VFS
    if (list_type == LIST_VFSLIST)
    {
        listbox_add_item (l_hotlist, LISTBOX_APPEND_AT_END, 0, mc_config_get_home_dir (), 0);
        vfs_fill_names (add_name_to_list);
    }
    else
#endif /* !ENABLE_VFS */
        fill_listbox ();

    add_widget_autopos (hotlist_dlg, l_hotlist, WPOS_KEEP_ALL, NULL);
    /* add listbox to the dialogs */
}

/* --------------------------------------------------------------------------------------------- */

static void
init_movelist (int list_type, struct hotlist *item)
{
    size_t i;
    char *hdr = g_strdup_printf (_("Moving %s"), item->label);
    int movelist_cols = init_i18n_stuff (list_type, COLS - 6);

    do_refresh ();

    movelist_dlg =
        create_dlg (TRUE, 0, 0, LINES - 6, movelist_cols, dialog_colors,
                    hotlist_callback, NULL, "[Hotlist]", hdr, DLG_CENTER | DLG_REVERSE);
    g_free (hdr);

    for (i = 0; i < BUTTONS; i++)
    {
        if (hotlist_but[i].type & list_type)
            add_widget (movelist_dlg,
                        button_new (BY - 4 + hotlist_but[i].y,
                                    BX + hotlist_but[i].x,
                                    hotlist_but[i].ret_cmd,
                                    hotlist_but[i].flags,
                                    hotlist_but[i].text, hotlist_button_callback));
    }

    /* We add the labels.  We are interested in the last one,
     * that one will hold the path name label
     */
    movelist_group = label_new (UY, UX + 2, _("Directory label"));
    add_widget (movelist_dlg, movelist_group);
    /* get new listbox */
    l_movelist =
        listbox_new (UY + 1, UX + 1, WIDGET (movelist_dlg)->lines - 8,
                     WIDGET (movelist_dlg)->cols - 2 * UX - 2, FALSE, l_call);

    fill_listbox ();

    add_widget (movelist_dlg, l_movelist);
    /* add listbox to the dialogs */
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Destroy the list dialog.
 * Don't confuse with done_hotlist() for the list in memory.
 */

static void
hotlist_done (void)
{
    destroy_dlg (hotlist_dlg);
    l_hotlist = NULL;
    if (0)
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
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
    if (!current_group)
        load_hotlist ();

    listbox_get_current (l_hotlist, NULL, (void **) &current);

    /* Make sure `..' stays at the top of the list. */
    if ((current != NULL) && (current->type == HL_TYPE_DOTDOT))
        pos = LISTBOX_APPEND_AFTER;

    new = new_hotlist ();

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

    if (!current_group->head)
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

        while (p->next)
            p = p->next;

        p->next = new;
    }

    if (hotlist_state.running && type != HL_TYPE_COMMENT && type != HL_TYPE_DOTDOT)
    {
        if (type == HL_TYPE_GROUP)
        {
            char *lbl = g_strconcat ("->", new->label, (char *) NULL);

            listbox_add_item (l_hotlist, pos, 0, lbl, new);
            g_free (lbl);
        }
        else
            listbox_add_item (l_hotlist, pos, 0, new->label, new);
        listbox_select_entry (l_hotlist, l_hotlist->pos);
    }
    return new;

}

/* --------------------------------------------------------------------------------------------- */
/**
 * Support routine for add_new_entry_input()/add_new_group_input()
 * Change positions of buttons (first three widgets).
 *
 * This is just a quick hack. Accurate procedure must take care of
 * internationalized label lengths and total buttonbar length...assume
 * 64 is longer anyway.
 */

#ifdef ENABLE_NLS
static void
add_widgets_i18n (QuickWidget * qw, int len)
{
    int i, l[3], space, cur_x;

    for (i = 0; i < 3; i++)
    {
        qw[i].u.button.text = _(qw[i].u.button.text);
        l[i] = str_term_width1 (qw[i].u.button.text) + 3;
    }
    space = (len - 4 - l[0] - l[1] - l[2]) / 4;

    for (cur_x = 2 + space, i = 3; i--; cur_x += l[i] + space)
    {
        qw[i].relative_x = cur_x;
        qw[i].x_divisions = len;
    }
}
#endif /* ENABLE_NLS */

/* --------------------------------------------------------------------------------------------- */

static int
add_new_entry_input (const char *header, const char *text1, const char *text2,
                     const char *help, char **r1, char **r2)
{
#define RELATIVE_Y_BUTTONS   4
#define RELATIVE_Y_LABEL_PTH 3
#define RELATIVE_Y_INPUT_PTH 4

    QuickWidget quick_widgets[] = {
        /* 0 */ QUICK_BUTTON (55, 80, RELATIVE_Y_BUTTONS, 0, N_("&Cancel"), B_CANCEL, NULL),
        /* 1 */ QUICK_BUTTON (30, 80, RELATIVE_Y_BUTTONS, 0, N_("&Insert"), B_INSERT, NULL),
        /* 2 */ QUICK_BUTTON (10, 80, RELATIVE_Y_BUTTONS, 0, N_("&Append"), B_APPEND, NULL),
        /* 3 */ QUICK_INPUT (4, 80, RELATIVE_Y_INPUT_PTH, 0, *r2, 58, 2, "input-pth", r2),
        /* 4 */ QUICK_LABEL (4, 80, 3, 0, text2),
        /* 5 */ QUICK_INPUT (4, 80, 3, 0, *r1, 58, 0, "input-lbl", r1),
        /* 6 */ QUICK_LABEL (4, 80, 2, 0, text1),
        QUICK_END
    };

    int len;
    int i;
    int lines1, lines2;
    int cols1, cols2;

#ifdef ENABLE_NLS
    static gboolean i18n_flag = FALSE;
#endif /* ENABLE_NLS */

    len = str_term_width1 (header);
    str_msg_term_size (text1, &lines1, &cols1);
    str_msg_term_size (text2, &lines2, &cols2);
    len = max (len, cols1);
    len = max (max (len, cols2) + 4, 64);

#ifdef ENABLE_NLS
    if (!i18n_flag)
    {
        add_widgets_i18n (quick_widgets, len);
        i18n_flag = TRUE;
    }
#endif /* ENABLE_NLS */

    {
        QuickDialog Quick_input = {
            len, lines1 + lines2 + 7, -1, -1, header,
            help, quick_widgets, NULL, NULL, FALSE
        };

        for (i = 0; i < 7; i++)
            quick_widgets[i].y_divisions = Quick_input.ylen;

        quick_widgets[0].relative_y = RELATIVE_Y_BUTTONS + (lines1 + lines2);
        quick_widgets[1].relative_y = RELATIVE_Y_BUTTONS + (lines1 + lines2);
        quick_widgets[2].relative_y = RELATIVE_Y_BUTTONS + (lines1 + lines2);
        quick_widgets[3].relative_y = RELATIVE_Y_INPUT_PTH + (lines1);
        quick_widgets[4].relative_y = RELATIVE_Y_LABEL_PTH + (lines1);

        i = quick_dialog (&Quick_input);
    }

    return (i != B_CANCEL) ? i : 0;

#undef RELATIVE_Y_BUTTONS
#undef RELATIVE_Y_LABEL_PTH
#undef RELATIVE_Y_INPUT_PTH
}

/* --------------------------------------------------------------------------------------------- */

static void
add_new_entry_cmd (void)
{
    char *title, *url, *to_free;
    int ret;

    /* Take current directory as default value for input fields */
    to_free = title = url = vfs_path_to_str_flags (current_panel->cwd_vpath, 0, VPF_STRIP_PASSWORD);

    ret = add_new_entry_input (_("New hotlist entry"), _("Directory label:"),
                               _("Directory path:"), "[Hotlist]", &title, &url);
    g_free (to_free);

    if (!ret)
        return;
    if (!title || !*title || !url || !*url)
    {
        g_free (title);
        g_free (url);
        return;
    }

    if (ret == B_ENTER || ret == B_APPEND)
        add2hotlist (title, url, HL_TYPE_ENTRY, LISTBOX_APPEND_AFTER);
    else
        add2hotlist (title, url, HL_TYPE_ENTRY, LISTBOX_APPEND_BEFORE);

    hotlist_state.modified = 1;
}

/* --------------------------------------------------------------------------------------------- */

static int
add_new_group_input (const char *header, const char *label, char **result)
{
    QuickWidget quick_widgets[] = {
        /* 0 */ QUICK_BUTTON (55, 80, 1, 0, N_("&Cancel"), B_CANCEL, NULL),
        /* 1 */ QUICK_BUTTON (30, 80, 1, 0, N_("&Insert"), B_INSERT, NULL),
        /* 2 */ QUICK_BUTTON (10, 80, 1, 0, N_("&Append"), B_APPEND, NULL),
        /* 3 */ QUICK_INPUT (4, 80, 0, 0, "", 58, 0, "input", result),
        /* 4 */ QUICK_LABEL (4, 80, 2, 0, label),
        QUICK_END
    };

    int len;
    int i;
    int lines, cols;
    int ret;

#ifdef ENABLE_NLS
    static gboolean i18n_flag = FALSE;
#endif /* ENABLE_NLS */

    len = str_term_width1 (header);
    str_msg_term_size (label, &lines, &cols);
    len = max (max (len, cols) + 4, 64);

#ifdef ENABLE_NLS
    if (!i18n_flag)
    {
        add_widgets_i18n (quick_widgets, len);
        i18n_flag = TRUE;
    }
#endif /* ENABLE_NLS */

    {
        QuickDialog Quick_input = {
            len, lines + 6, -1, -1, header,
            "[Hotlist]", quick_widgets, NULL, NULL, FALSE
        };

        int relative_y[] = { 1, 1, 1, 0, 2 };   /* the relative_x component from the
                                                   quick_widgets variable above */

        for (i = 0; i < 5; i++)
            quick_widgets[i].y_divisions = Quick_input.ylen;

        for (i = 0; i < 4; i++)
            quick_widgets[i].relative_y = relative_y[i] + 2 + lines;

        ret = quick_dialog (&Quick_input);
    }

    return (ret != B_CANCEL) ? ret : 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
add_new_group_cmd (void)
{
    char *label;
    int ret;

    ret = add_new_group_input (_("New hotlist group"), _("Name of new group:"), &label);
    if (!ret || !label || !*label)
        return;

    if (ret == B_ENTER || ret == B_APPEND)
        add2hotlist (label, 0, HL_TYPE_GROUP, LISTBOX_APPEND_AFTER);
    else
        add2hotlist (label, 0, HL_TYPE_GROUP, LISTBOX_APPEND_BEFORE);

    hotlist_state.modified = 1;
}

/* --------------------------------------------------------------------------------------------- */

static void
remove_group (struct hotlist *grp)
{
    struct hotlist *current = grp->head;

    while (current)
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
    hotlist_state.modified = 1;
}

/* --------------------------------------------------------------------------------------------- */

static void
load_group (struct hotlist *grp)
{
    gchar **profile_keys, **keys;
    gsize len;
    char *group_section;
    struct hotlist *current = 0;

    group_section = find_group_section (grp);

    profile_keys = keys = mc_config_get_keys (mc_main_config, group_section, &len);

    current_group = grp;

    while (*profile_keys)
    {
        add2hotlist (mc_config_get_string (mc_main_config, group_section, *profile_keys, ""),
                     g_strdup (*profile_keys), HL_TYPE_GROUP, LISTBOX_APPEND_AT_END);
        profile_keys++;
    }
    g_free (group_section);
    g_strfreev (keys);

    profile_keys = keys = mc_config_get_keys (mc_main_config, grp->directory, &len);

    while (*profile_keys)
    {
        add2hotlist (mc_config_get_string (mc_main_config, group_section, *profile_keys, ""),
                     g_strdup (*profile_keys), HL_TYPE_ENTRY, LISTBOX_APPEND_AT_END);
        profile_keys++;
    }
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
        {
            g_string_append_c (tkn_buf, c);
        }
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
        if (c == EOF)
            ret = TKN_EOF;
        else
            ret = TKN_STRING;
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

        /* fall through; it is taken as normal character */

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
            hotlist_state.readonly = 1;
            hotlist_state.file_error = 1;
            return;
            break;
        case TKN_EOL:
            /* skip empty lines */
            break;
        default:
            hotlist_state.readonly = 1;
            hotlist_state.file_error = 1;
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
            hotlist_state.readonly = 1;
            hotlist_state.file_error = 1;
            SKIP_TO_EOL;
            break;
        }
}

/* --------------------------------------------------------------------------------------------- */

static void
clean_up_hotlist_groups (const char *section)
{
    char *grp_section;
    gchar **profile_keys, **keys;
    gsize len;

    grp_section = g_strconcat (section, ".Group", (char *) NULL);
    if (mc_config_has_group (mc_main_config, section))
        mc_config_del_group (mc_main_config, section);

    if (mc_config_has_group (mc_main_config, grp_section))
    {
        profile_keys = keys = mc_config_get_keys (mc_main_config, grp_section, &len);

        while (*profile_keys)
        {
            clean_up_hotlist_groups (*profile_keys);
            profile_keys++;
        }
        g_strfreev (keys);
        mc_config_del_group (mc_main_config, grp_section);
    }
    g_free (grp_section);
}

/* --------------------------------------------------------------------------------------------- */

static void
load_hotlist (void)
{
    int remove_old_list = 0;
    struct stat stat_buf;

    if (hotlist_state.loaded)
    {
        stat (hotlist_file_name, &stat_buf);
        if (hotlist_file_mtime < stat_buf.st_mtime)
            done_hotlist ();
        else
            return;
    }

    if (!hotlist_file_name)
        hotlist_file_name = mc_config_get_full_path (MC_HOTLIST_FILE);

    hotlist = new_hotlist ();
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
        hotlist_state.loaded = 1;
        /*
         * just to be sure we got copy
         */
        hotlist_state.modified = 1;
        result = save_hotlist ();
        hotlist_state.modified = 0;
        if (result)
            remove_old_list = 1;
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
        hotlist_state.loaded = 1;
    }

    if (remove_old_list)
    {
        GError *error = NULL;
        clean_up_hotlist_groups ("Hotlist");
        if (!mc_config_save_file (mc_main_config, &error))
            setup_save_config_show_error (mc_main_config->ini_path, &error);
    }

    stat (hotlist_file_name, &stat_buf);
    hotlist_file_mtime = stat_buf.st_mtime;
    current_group = hotlist;
}

/* --------------------------------------------------------------------------------------------- */

static void
hot_save_group (struct hotlist *grp)
{
    struct hotlist *current = grp->head;
    int i;
    char *s;

#define INDENT(n) \
do { \
    for (i = 0; i < n; i++) \
        putc (' ', hotlist_file); \
} while (0)

    for (; current; current = current->next)
        switch (current->type)
        {
        case HL_TYPE_GROUP:
            INDENT (list_level);
            fputs ("GROUP \"", hotlist_file);
            for (s = current->label; *s; s++)
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
            for (s = current->label; *s; s++)
            {
                if (*s == '"' || *s == '\\')
                    putc ('\\', hotlist_file);
                putc (*s, hotlist_file);
            }
            fputs ("\" URL \"", hotlist_file);
            for (s = current->directory; *s; s++)
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
        }
}

/* --------------------------------------------------------------------------------------------- */

static void
add_dotdot_to_list (void)
{
    if (current_group != hotlist)
    {
        if (hotlist_has_dot_dot != 0)
            add2hotlist (g_strdup (".."), g_strdup (".."), HL_TYPE_DOTDOT, LISTBOX_APPEND_AT_END);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
add2hotlist_cmd (void)
{
    char *lc_prompt;
    const char *cp = N_("Label for \"%s\":");
    int l;
    char *label_string, *label;

#ifdef ENABLE_NLS
    cp = _(cp);
#endif

    l = str_term_width1 (cp);
    label_string = vfs_path_to_str_flags (current_panel->cwd_vpath, 0, VPF_STRIP_PASSWORD);
    lc_prompt = g_strdup_printf (cp, str_trunc (label_string, COLS - 2 * UX - (l + 8)));
    label = input_dialog (_("Add to hotlist"), lc_prompt, MC_HISTORY_HOTLIST_ADD, label_string);
    g_free (lc_prompt);

    if (!label || !*label)
    {
        g_free (label_string);
        g_free (label);
        return;
    }
    add2hotlist (label, label_string, HL_TYPE_ENTRY, LISTBOX_APPEND_AT_END);
    hotlist_state.modified = 1;
}

/* --------------------------------------------------------------------------------------------- */

char *
hotlist_show (int vfs_or_hotlist)
{
    char *target = NULL;

    hotlist_state.type = vfs_or_hotlist;
    load_hotlist ();

    init_hotlist (vfs_or_hotlist);

    /* display file info */
    tty_setcolor (SELECTED_COLOR);

    hotlist_state.running = 1;
    run_dlg (hotlist_dlg);
    hotlist_state.running = 0;
    save_hotlist ();

    switch (hotlist_dlg->ret_value)
    {
    default:
    case B_CANCEL:
        break;

    case B_ENTER:
        {
            char *text = NULL;
            struct hotlist *hlp = NULL;

            listbox_get_current (l_hotlist, &text, (void **) &hlp);
            target = g_strdup (hlp != NULL ? hlp->directory : text);
            break;
        }
    }                           /* switch */

    hotlist_done ();
    return target;
}

/* --------------------------------------------------------------------------------------------- */

int
save_hotlist (void)
{
    int saved = 0;
    struct stat stat_buf;

    if (!hotlist_state.readonly && hotlist_state.modified && hotlist_file_name)
    {
        mc_util_make_backup_if_possible (hotlist_file_name, ".bak");

        hotlist_file = fopen (hotlist_file_name, "w");
        if (hotlist_file != NULL)
        {
            hot_save_group (hotlist);
            fclose (hotlist_file);
            stat (hotlist_file_name, &stat_buf);
            hotlist_file_mtime = stat_buf.st_mtime;
            saved = 1;
            hotlist_state.modified = 0;
        }
        else
            mc_util_restore_from_backup_if_possible (hotlist_file_name, ".bak");
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
    if (hotlist)
    {
        remove_group (hotlist);
        g_free (hotlist->label);
        g_free (hotlist->directory);
        g_free (hotlist);
        hotlist = 0;
    }

    hotlist_state.loaded = 0;

    g_free (hotlist_file_name);
    hotlist_file_name = 0;
    l_hotlist = 0;
    current_group = 0;

    if (tkn_buf)
    {
        g_string_free (tkn_buf, TRUE);
        tkn_buf = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */
