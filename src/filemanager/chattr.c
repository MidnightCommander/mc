/*
   Chattr command -- for the Midnight Commander

   Copyright (C) 2020
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2020

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

/** \file chattr.c
 *  \brief Source: chattr command
 */

/* TODO: change attributes recursively (ticket #3109) */

#include <config.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <e2p/e2p.h>
#include <ext2fs/ext2_fs.h>

#include "lib/global.h"

#include "lib/tty/tty.h"        /* tty_print*() */
#include "lib/tty/color.h"      /* tty_setcolor() */
#include "lib/skin.h"           /* COLOR_NORMAL, DISABLED_COLOR */
#include "lib/vfs/vfs.h"
#include "lib/widget.h"

#include "src/keybind-defaults.h"       /* chattr_map */

#include "cmd.h"                /* chattr_cmd(), chattr_get_as_str() */

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define B_MARKED B_USER
#define B_SETALL (B_USER + 1)
#define B_SETMRK (B_USER + 2)
#define B_CLRMRK (B_USER + 3)

#define BUTTONS  6

#define CHATTRBOXES(x) ((WChattrBoxes *)(x))

/*** file scope type declarations ****************************************************************/

typedef struct WFileAttrText WFileAttrText;

struct WFileAttrText
{
    Widget widget;              /* base class */

    char *filename;
    int filename_width;         /* cached width of file name */
    char attrs[32 + 1];         /* 32 bits in attributes (unsigned long) */
};

typedef struct WChattrBoxes WChattrBoxes;

struct WChattrBoxes
{
    WGroup base;                /* base class */

    int pos;                    /* The current checkbox selected */
    int top;                    /* The first flag displayed */
};

/*** file scope variables ************************************************************************/

/* see /usr/include/ext2fs/ext2_fs.h
 *
 * EXT2_SECRM_FL            0x00000001 -- Secure deletion
 * EXT2_UNRM_FL             0x00000002 -- Undelete
 * EXT2_COMPR_FL            0x00000004 -- Compress file
 * EXT2_SYNC_FL             0x00000008 -- Synchronous updates
 * EXT2_IMMUTABLE_FL        0x00000010 -- Immutable file
 * EXT2_APPEND_FL           0x00000020 -- writes to file may only append
 * EXT2_NODUMP_FL           0x00000040 -- do not dump file
 * EXT2_NOATIME_FL          0x00000080 -- do not update atime
 * * Reserved for compression usage...
 * EXT2_DIRTY_FL            0x00000100
 * EXT2_COMPRBLK_FL         0x00000200 -- One or more compressed clusters
 * EXT2_NOCOMPR_FL          0x00000400 -- Access raw compressed data
 * * nb: was previously EXT2_ECOMPR_FL
 * EXT4_ENCRYPT_FL          0x00000800 -- encrypted inode
 * * End compression flags --- maybe not all used
 * EXT2_BTREE_FL            0x00001000 -- btree format dir
 * EXT2_INDEX_FL            0x00001000 -- hash-indexed directory
 * EXT2_IMAGIC_FL           0x00002000
 * EXT3_JOURNAL_DATA_FL     0x00004000 -- file data should be journaled
 * EXT2_NOTAIL_FL           0x00008000 -- file tail should not be merged
 * EXT2_DIRSYNC_FL          0x00010000 -- Synchronous directory modifications
 * EXT2_TOPDIR_FL           0x00020000 -- Top of directory hierarchies
 * EXT4_HUGE_FILE_FL        0x00040000 -- Set to each huge file
 * EXT4_EXTENTS_FL          0x00080000 -- Inode uses extents
 * EXT4_VERITY_FL           0x00100000 -- Verity protected inode
 * EXT4_EA_INODE_FL         0x00200000 -- Inode used for large EA
 * EXT4_EOFBLOCKS_FL        0x00400000 was here, unused
 * FS_NOCOW_FL              0x00800000 -- Do not cow file
 * EXT4_SNAPFILE_FL         0x01000000 -- Inode is a snapshot
 * FS_DAX_FL                0x02000000 -- Inode is DAX
 * EXT4_SNAPFILE_DELETED_FL 0x04000000 -- Snapshot is being deleted
 * EXT4_SNAPFILE_SHRUNK_FL  0x08000000 -- Snapshot shrink has completed
 * EXT4_INLINE_DATA_FL      0x10000000 -- Inode has inline data
 * EXT4_PROJINHERIT_FL      0x20000000 -- Create with parents projid
 * EXT4_CASEFOLD_FL         0x40000000 -- Casefolded file
 *                          0x80000000 -- unused yet
 */

static struct
{
    unsigned long flags;
    char attr;
    const char *text;
    gboolean selected;
    gboolean state;             /* state of checkboxes */
} check_attr[] =
{
    /* *INDENT-OFF* */
    { EXT2_SECRM_FL,        's', N_("Secure deletion"),               FALSE, FALSE },
    { EXT2_UNRM_FL,         'u', N_("Undelete"),                      FALSE, FALSE },
    { EXT2_SYNC_FL,         'S', N_("Synchronous updates"),           FALSE, FALSE },
    { EXT2_DIRSYNC_FL,      'D', N_("Synchronous directory updates"), FALSE, FALSE },
    { EXT2_IMMUTABLE_FL,    'i', N_("Immutable"),                     FALSE, FALSE },
    { EXT2_APPEND_FL,       'a', N_("Append only"),                   FALSE, FALSE },
    { EXT2_NODUMP_FL,       'd', N_("No dump"),                       FALSE, FALSE },
    { EXT2_NOATIME_FL,      'A', N_("No update atime"),               FALSE, FALSE },
    { EXT2_COMPR_FL,        'c', N_("Compress"),                      FALSE, FALSE },
#ifdef EXT2_COMPRBLK_FL
    /* removed in v1.43-WIP-2015-05-18
       ext2fsprogs 4a05268cf86f7138c78d80a53f7e162f32128a3d 2015-04-12 */
    { EXT2_COMPRBLK_FL,     'B', N_("Compressed clusters"),           FALSE, FALSE },
#endif
#ifdef EXT2_DIRTY_FL
    /* removed in v1.43-WIP-2015-05-18
       ext2fsprogs 4a05268cf86f7138c78d80a53f7e162f32128a3d 2015-04-12 */
    { EXT2_DIRTY_FL,        'Z', N_("Compressed dirty file"),         FALSE, FALSE },
#endif
#ifdef EXT2_NOCOMPR_FL
    /* removed in v1.43-WIP-2015-05-18
       ext2fsprogs 4a05268cf86f7138c78d80a53f7e162f32128a3d 2015-04-12 */
    { EXT2_NOCOMPR_FL,      'X', N_("Compression raw access"),        FALSE, FALSE },
#endif
#ifdef EXT4_ENCRYPT_FL
    { EXT4_ENCRYPT_FL,      'E', N_("Encrypted inode"),               FALSE, FALSE },
#endif
    { EXT3_JOURNAL_DATA_FL, 'j', N_("Journaled data"),                FALSE, FALSE },
    { EXT2_INDEX_FL,        'I', N_("Indexed directory"),             FALSE, FALSE },
    { EXT2_NOTAIL_FL,       't', N_("No tail merging"),               FALSE, FALSE },
    { EXT2_TOPDIR_FL,       'T', N_("Top of directory hierarchies"),  FALSE, FALSE },
    { EXT4_EXTENTS_FL,      'e', N_("Inode uses extents"),            FALSE, FALSE },
#ifdef EXT4_HUGE_FILE_FL
    /* removed in v1.43.9
       ext2fsprogs 4825daeb0228e556444d199274b08c499ac3706c 2018-02-06 */
    { EXT4_HUGE_FILE_FL,    'h', N_("Huge_file"),                     FALSE, FALSE },
#endif
    { FS_NOCOW_FL,          'C', N_("No COW"),                        FALSE, FALSE },
#ifdef FS_DAX_FL
    /* added in v1.45.7
       TODO: clarify version after ext2fsprogs release
       ext2fsprogs 1dd48bc23c3776df76459aff0c7723fff850ea45 2020-07-28 */
    { FS_DAX_FL,            'x', N_("Direct access for files"),       FALSE, FALSE },
#endif
#ifdef EXT4_CASEFOLD_FL
    /* added in v1.45.0
       ext2fsprogs 1378bb6515e98a27f0f5c220381d49d20544204e 2018-12-01 */
    { EXT4_CASEFOLD_FL,     'F', N_("Casefolded file"),               FALSE, FALSE },
#endif
#ifdef EXT4_INLINE_DATA_FL
    { EXT4_INLINE_DATA_FL,  'N', N_("Inode has inline data"),         FALSE, FALSE },
#endif
#ifdef EXT4_PROJINHERIT_FL
    /* added in v1.43-WIP-2016-05-12
       ext2fsprogs e1cec4464bdaf93ea609de43c5cdeb6a1f553483 2016-03-07
                   97d7e2fdb2ebec70c3124c1a6370d28ec02efad0 2016-05-09 */
    { EXT4_PROJINHERIT_FL,  'P', N_("Project hierarchy"),             FALSE, FALSE },
#endif
#ifdef EXT4_VERITY_FL
    /* added in v1.44.4
       ext2fsprogs faae7aa00df0abe7c6151fc4947aa6501b981ee1 2018-08-14
       v1.44.5
       ext2fsprogs 7e5a95e3d59719361661086ec7188ca6e674f139 2018-08-21 */
    { EXT4_VERITY_FL,       'V', N_("Verity protected inode"),        FALSE, FALSE }
#endif
    /* *INDENT-ON* */
};

/* number of attributes */
static const size_t check_attr_num = G_N_ELEMENTS (check_attr);

/* modifable attribute numbers */
static int check_attr_mod[32];
static int check_attr_mod_num = 0;      /* 0..31 */

/* maximum width of attribute text */
static int check_attr_width = 0;

static struct
{
    int ret_cmd;
    button_flags_t flags;
    int width;
    const char *text;
    Widget *button;
} chattr_but[BUTTONS] =
{
    /* *INDENT-OFF* */
    /* 0 */ { B_SETALL, NORMAL_BUTTON, 0, N_("Set &all"),      NULL },
    /* 1 */ { B_MARKED, NORMAL_BUTTON, 0, N_("&Marked all"),   NULL },
    /* 2 */ { B_SETMRK, NORMAL_BUTTON, 0, N_("S&et marked"),   NULL },
    /* 3 */ { B_CLRMRK, NORMAL_BUTTON, 0, N_("C&lear marked"), NULL },
    /* 4 */ { B_ENTER, DEFPUSH_BUTTON, 0, N_("&Set"),          NULL },
    /* 5 */ { B_CANCEL, NORMAL_BUTTON, 0, N_("&Cancel"),       NULL }
    /* *INDENT-ON* */
};

static gboolean flags_changed;
static int current_file;
static gboolean ignore_all;

static unsigned long and_mask, or_mask, flags;

static WFileAttrText *file_attr;

/* x-coord of widget in the dialog */
static const int wx = 3;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static inline gboolean
chattr_is_modifiable (size_t i)
{
    return ((check_attr[i].flags & EXT2_FL_USER_MODIFIABLE) != 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
chattr_fill_str (unsigned long attr, char *str)
{
    size_t i;

    for (i = 0; i < check_attr_num; i++)
        str[i] = (attr & check_attr[i].flags) != 0 ? check_attr[i].attr : '-';

    str[check_attr_num] = '\0';
}

/* --------------------------------------------------------------------------------------------- */

static void
fileattrtext_fill (WFileAttrText * fat, unsigned long attr)
{
    chattr_fill_str (attr, fat->attrs);
    widget_draw (WIDGET (fat));
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
fileattrtext_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WFileAttrText *fat = (WFileAttrText *) w;

    switch (msg)
    {
    case MSG_DRAW:
        {
            int color;
            size_t i;

            color = COLOR_NORMAL;
            tty_setcolor (color);

            if (w->cols > fat->filename_width)
            {
                widget_gotoyx (w, 0, (w->cols - fat->filename_width) / 2);
                tty_print_string (fat->filename);
            }
            else
            {
                widget_gotoyx (w, 0, 0);
                tty_print_string (str_trunc (fat->filename, w->cols));
            }

            /* hope that w->cols is greater than check_attr_num */
            widget_gotoyx (w, 1, (w->cols - check_attr_num) / 2);
            for (i = 0; i < check_attr_num; i++)
            {
                /* Do not set new color for each symbol. Try to use previous color. */
                if (chattr_is_modifiable (i))
                {
                    if (color == DISABLED_COLOR)
                    {
                        color = COLOR_NORMAL;
                        tty_setcolor (color);
                    }
                }
                else
                {
                    if (color != DISABLED_COLOR)
                    {
                        color = DISABLED_COLOR;
                        tty_setcolor (color);
                    }
                }

                tty_print_char (fat->attrs[i]);
            }
            return MSG_HANDLED;
        }

    case MSG_RESIZE:
        {
            Widget *wo = WIDGET (w->owner);

            widget_default_callback (w, sender, msg, parm, data);
            /* intially file name may be wider than screen */
            if (fat->filename_width > wo->cols - wx * 2)
            {
                w->x = wo->x + wx;
                w->cols = wo->cols - wx * 2;
            }
            return MSG_HANDLED;
        }

    case MSG_DESTROY:
        g_free (fat->filename);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static WFileAttrText *
fileattrtext_new (int y, int x, const char *filename, unsigned long attr)
{
    WFileAttrText *fat;
    int width, cols;

    width = str_term_width1 (filename);
    cols = MAX (width, (int) check_attr_num);

    fat = g_new (WFileAttrText, 1);
    widget_init (WIDGET (fat), y, x, 2, cols, fileattrtext_callback, NULL);

    fat->filename = g_strdup (filename);
    fat->filename_width = width;
    fileattrtext_fill (fat, attr);

    return fat;
}

/* --------------------------------------------------------------------------------------------- */

static void
chattr_draw_select (const Widget * w, gboolean selected)
{
    widget_gotoyx (w, 0, -1);
    tty_print_char (selected ? '*' : ' ');
    widget_gotoyx (w, 0, 1);
}

/* --------------------------------------------------------------------------------------------- */

static void
chattr_toggle_select (const WChattrBoxes * cb, int Id)
{
    Widget *w;

    /* find checkbox */
    w = WIDGET (g_list_nth_data (GROUP (cb)->widgets, Id - cb->top));

    check_attr[Id].selected = !check_attr[Id].selected;

    tty_setcolor (COLOR_NORMAL);
    chattr_draw_select (w, check_attr[Id].selected);
}

/* --------------------------------------------------------------------------------------------- */

static inline void
chattrboxes_draw_scrollbar (const WChattrBoxes * cb)
{
    const Widget *w = CONST_WIDGET (cb);
    int max_line;
    int line;
    int i;

    /* Are we at the top? */
    widget_gotoyx (w, 0, w->cols);
    if (cb->top == 0)
        tty_print_one_vline (TRUE);
    else
        tty_print_char ('^');

    max_line = w->lines - 1;

    /* Are we at the bottom? */
    widget_gotoyx (w, max_line, w->cols);
    if (cb->top + w->lines == check_attr_mod_num || w->lines >= check_attr_mod_num)
        tty_print_one_vline (TRUE);
    else
        tty_print_char ('v');

    /* Now draw the nice relative pointer */
    line = 1 + (cb->pos * (w->lines - 2)) / check_attr_mod_num;

    for (i = 1; i < max_line; i++)
    {
        widget_gotoyx (w, i, w->cols);
        if (i != line)
            tty_print_one_vline (TRUE);
        else
            tty_print_char ('*');
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
chattrboxes_draw (WChattrBoxes * cb)
{
    Widget *w = WIDGET (cb);
    int i;
    GList *l;
    const int *colors;

    colors = widget_get_colors (w);
    tty_setcolor (colors[DLG_COLOR_NORMAL]);
    tty_fill_region (w->y, w->x - 1, w->lines, w->cols + 1, ' ');

    /* redraw checkboxes */
    group_default_callback (w, NULL, MSG_DRAW, 0, NULL);

    /* draw scrollbar */
    tty_setcolor (colors[DLG_COLOR_NORMAL]);
    if (!mc_global.tty.slow_terminal && check_attr_mod_num > w->lines)
        chattrboxes_draw_scrollbar (cb);

    /* mark selected checkboxes */
    for (i = cb->top, l = GROUP (cb)->widgets; l != NULL; i++, l = g_list_next (l))
        chattr_draw_select (WIDGET (l->data), check_attr[i].selected);
}

/* --------------------------------------------------------------------------------------------- */

static void
chattrboxes_rename (WChattrBoxes * cb)
{
    Widget *w = WIDGET (cb);
    gboolean active;
    int i;
    GList *l;
    char btext[BUF_SMALL];      /* FIXME: is 128 bytes enough? */

    active = widget_get_state (w, WST_ACTIVE);

    /* lock the group to avoid redraw of checkboxes individually */
    if (active)
        widget_set_state (w, WST_SUSPENDED, TRUE);

    for (i = cb->top, l = GROUP (cb)->widgets; l != NULL; i++, l = g_list_next (l))
    {
        WCheck *c = CHECK (l->data);
        int m;

        m = check_attr_mod[i];
        g_snprintf (btext, sizeof (btext), "(%c) %s", check_attr[m].attr, check_attr[m].text);
        check_set_text (c, btext);
        c->state = check_attr[m].state;
    }

    /* unlock */
    if (active)
        widget_set_state (w, WST_ACTIVE, TRUE);

    widget_draw (w);
}

/* --------------------------------------------------------------------------------------------- */

static void
checkboxes_save_state (const WChattrBoxes * cb)
{
    int i;
    GList *l;

    for (i = cb->top, l = GROUP (cb)->widgets; l != NULL; i++, l = g_list_next (l))
    {
        int m;

        m = check_attr_mod[i];
        check_attr[m].state = CHECK (l->data)->state;
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
chattrboxes_down (WChattrBoxes * cb)
{
    if (cb->pos == cb->top + WIDGET (cb)->lines - 1)
    {
        /* We are on the last checkbox.
           Keep this position. */

        if (cb->pos == check_attr_mod_num - 1)
            /* get out of widget */
            return MSG_NOT_HANDLED;

        /* emulate scroll of checkboxes */
        checkboxes_save_state (cb);
        cb->pos++;
        cb->top++;
        chattrboxes_rename (cb);
    }
    else                        /* cb->pos > cb-top */
    {
        GList *l;

        /* select next checkbox */
        cb->pos++;
        l = g_list_next (GROUP (cb)->current);
        widget_select (WIDGET (l->data));
    }

    return MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
chattrboxes_page_down (WChattrBoxes * cb)
{
    WGroup *g = GROUP (cb);
    GList *l;

    if (cb->pos == check_attr_mod_num - 1)
    {
        /* We are on the last checkbox.
           Keep this position.
           Do nothing. */
        l = g_list_last (g->widgets);
    }
    else
    {
        int i = WIDGET (cb)->lines;

        checkboxes_save_state (cb);

        if (cb->top > check_attr_mod_num - 2 * i)
            i = check_attr_mod_num - i - cb->top;
        if (cb->top + i < 0)
            i = -cb->top;
        if (i == 0)
        {
            cb->pos = check_attr_mod_num - 1;
            cb->top += i;
            l = g_list_last (g->widgets);
        }
        else
        {
            cb->pos += i;
            cb->top += i;
            l = g_list_nth (g->widgets, cb->pos - cb->top);
        }

        chattrboxes_rename (cb);
    }

    widget_select (WIDGET (l->data));

    return MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
chattrboxes_end (WChattrBoxes * cb)
{
    GList *l;

    checkboxes_save_state (cb);
    cb->pos = check_attr_mod_num - 1;
    cb->top = cb->pos - WIDGET (cb)->lines + 1;
    l = g_list_last (GROUP (cb)->widgets);
    chattrboxes_rename (cb);
    widget_select (WIDGET (l->data));

    return MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
chattrboxes_up (WChattrBoxes * cb)
{
    if (cb->pos == cb->top)
    {
        /* We are on the first checkbox.
           Keep this position. */

        if (cb->top == 0)
            /* get out of widget */
            return MSG_NOT_HANDLED;

        /* emulate scroll of checkboxes */
        checkboxes_save_state (cb);
        cb->pos--;
        cb->top--;
        chattrboxes_rename (cb);
    }
    else                        /* cb->pos > cb-top */
    {
        GList *l;

        /* select previous checkbox */
        cb->pos--;
        l = g_list_previous (GROUP (cb)->current);
        widget_select (WIDGET (l->data));
    }

    return MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
chattrboxes_page_up (WChattrBoxes * cb)
{
    WGroup *g = GROUP (cb);
    GList *l;

    if (cb->pos == 0 && cb->top == 0)
    {
        /* We are on the first checkbox.
           Keep this position.
           Do nothing. */
        l = g_list_first (g->widgets);
    }
    else
    {
        int i = WIDGET (cb)->lines;

        checkboxes_save_state (cb);

        if (cb->top < i)
            i = cb->top;
        if (i == 0)
        {
            cb->pos = 0;
            cb->top -= i;
            l = g_list_first (g->widgets);
        }
        else
        {
            cb->pos -= i;
            cb->top -= i;
            l = g_list_nth (g->widgets, cb->pos - cb->top);
        }

        chattrboxes_rename (cb);
    }

    widget_select (WIDGET (l->data));

    return MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
chattrboxes_home (WChattrBoxes * cb)
{
    GList *l;

    checkboxes_save_state (cb);
    cb->pos = 0;
    cb->top = 0;
    l = g_list_first (GROUP (cb)->widgets);
    chattrboxes_rename (cb);
    widget_select (WIDGET (l->data));

    return MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
chattrboxes_execute_cmd (WChattrBoxes * cb, long command)
{
    switch (command)
    {
    case CK_Down:
        return chattrboxes_down (cb);

    case CK_PageDown:
        return chattrboxes_page_down (cb);

    case CK_Bottom:
        return chattrboxes_end (cb);

    case CK_Up:
        return chattrboxes_up (cb);

    case CK_PageUp:
        return chattrboxes_page_up (cb);

    case CK_Top:
        return chattrboxes_home (cb);

    case CK_Mark:
    case CK_MarkAndDown:
        {
            chattr_toggle_select (cb, cb->pos); /* FIXME */
            if (command == CK_MarkAndDown)
                chattrboxes_down (cb);

            return MSG_HANDLED;
        }

    default:
        return MSG_NOT_HANDLED;
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
chattrboxes_key (WChattrBoxes * cb, int key)
{
    long command;

    command = widget_lookup_key (WIDGET (cb), key);
    if (command == CK_IgnoreKey)
        return MSG_NOT_HANDLED;
    return chattrboxes_execute_cmd (cb, command);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
chattrboxes_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WChattrBoxes *cb = CHATTRBOXES (w);
    WGroup *g = GROUP (w);

    switch (msg)
    {
    case MSG_DRAW:
        chattrboxes_draw (cb);
        return MSG_HANDLED;

    case MSG_NOTIFY:
        {
            /* handle checkboxes */
            int i;

            i = g_list_index (g->widgets, sender);
            if (i >= 0)
            {
                int m;

                i += cb->top;
                m = check_attr_mod[i];
                flags ^= check_attr[m].flags;
                fileattrtext_fill (file_attr, flags);
                chattr_toggle_select (cb, i);
                flags_changed = TRUE;
                return MSG_HANDLED;
            }
        }
        return MSG_NOT_HANDLED;

    case MSG_CHANGED_FOCUS:
        /* sender is one of chattr checkboxes */
        if (widget_get_state (sender, WST_FOCUSED))
        {
            int i;

            i = g_list_index (g->widgets, sender);
            cb->pos = cb->top + i;
        }
        return MSG_HANDLED;

    case MSG_KEY:
        {
            cb_ret_t ret;

            ret = chattrboxes_key (cb, parm);
            if (ret != MSG_HANDLED)
                ret = group_default_callback (w, NULL, MSG_KEY, parm, NULL);

            return ret;
        }

    case MSG_ACTION:
        return chattrboxes_execute_cmd (cb, parm);

    case MSG_DESTROY:
        /* save all states */
        checkboxes_save_state (cb);
        MC_FALLTHROUGH;

    default:
        return group_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
chattrboxes_handle_mouse_event (Widget * w, Gpm_Event * event)
{
    int mou;

    mou = mouse_handle_event (w, event);
    if (mou == MOU_UNHANDLED)
        mou = group_handle_mouse_event (w, event);

    return mou;
}

/* --------------------------------------------------------------------------------------------- */

static void
chattrboxes_mouse_callback (Widget * w, mouse_msg_t msg, mouse_event_t * event)
{
    WChattrBoxes *cb = CHATTRBOXES (w);

    (void) event;

    switch (msg)
    {
    case MSG_MOUSE_SCROLL_UP:
        chattrboxes_up (cb);
        break;

    case MSG_MOUSE_SCROLL_DOWN:
        chattrboxes_down (cb);
        break;

    default:
        /* return MOU_UNHANDLED */
        event->result.abort = TRUE;
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */

static WChattrBoxes *
chattrboxes_new (int y, int x, int height, int width)
{
    WChattrBoxes *cb;
    Widget *w;

    if (height <= 0)
        height = 1;

    cb = g_new0 (WChattrBoxes, 1);
    w = WIDGET (cb);
    group_init (GROUP (cb), y, x, height, width, chattrboxes_callback, chattrboxes_mouse_callback);
    w->options |= WOP_SELECTABLE | WOP_WANT_CURSOR;
    w->mouse_handler = chattrboxes_handle_mouse_event;
    w->keymap = chattr_map;

    return cb;
}

/* --------------------------------------------------------------------------------------------- */

static void
chattr_init (void)
{
    static gboolean i18n = FALSE;
    size_t i;

    for (i = 0; i < check_attr_num; i++)
        check_attr[i].selected = FALSE;

    if (i18n)
        return;

    i18n = TRUE;

    for (i = 0; i < check_attr_num; i++)
        if (chattr_is_modifiable (i))
        {
            int width;

#ifdef ENABLE_NLS
            check_attr[i].text = _(check_attr[i].text);
#endif

            check_attr_mod[check_attr_mod_num++] = i;

            width = 4 + str_term_width1 (check_attr[i].text);   /* "(Q) text " */
            check_attr_width = MAX (check_attr_width, width);
        }

    check_attr_width += 1 + 3 + 1;      /* mark, [x] and space */

    for (i = 0; i < BUTTONS; i++)
    {
#ifdef ENABLE_NLS
        chattr_but[i].text = _(chattr_but[i].text);
#endif

        chattr_but[i].width = str_term_width1 (chattr_but[i].text) + 3; /* [], spaces and w/o & */
        if (chattr_but[i].flags == DEFPUSH_BUTTON)
            chattr_but[i].width += 2;   /* <> */
    }
}

/* --------------------------------------------------------------------------------------------- */

static WDialog *
chattr_dlg_create (WPanel * panel, const char *fname, unsigned long attr)
{
    const Widget *mw = CONST_WIDGET (midnight_dlg);
    gboolean single_set;
    WDialog *ch_dlg;
    int lines, cols;
    int checkboxes_lines = check_attr_mod_num;
    size_t i;
    int y;
    Widget *dw;
    WGroup *dg, *cbg;
    WChattrBoxes *cb;
    const int cb_scrollbar_width = 1;

    /* prepate to set up checkbox states */
    for (i = 0; i < check_attr_num; i++)
        check_attr[i].state = chattr_is_modifiable (i) && (attr & check_attr[i].flags) != 0;

    cols = check_attr_width + cb_scrollbar_width;

    single_set = (panel->marked < 2);

    lines = 5 + checkboxes_lines + 4;
    if (!single_set)
        lines += 3;

    if (lines >= mw->lines - 2)
    {
        int dl;

        dl = lines - (mw->lines - 2);
        lines -= dl;
        checkboxes_lines -= dl;
    }

    ch_dlg =
        dlg_create (TRUE, 0, 0, lines, cols + wx * 2, WPOS_CENTER, FALSE, dialog_colors,
                    dlg_default_callback, NULL, "[Chattr]", _("Chattr command"));
    dg = GROUP (ch_dlg);
    dw = WIDGET (ch_dlg);

    y = 2;
    file_attr = fileattrtext_new (y, wx, fname, attr);
    group_add_widget_autopos (dg, file_attr, WPOS_KEEP_TOP | WPOS_CENTER_HORZ, NULL);
    y += WIDGET (file_attr)->lines;
    group_add_widget (dg, hline_new (y++, -1, -1));

    if (cols < WIDGET (file_attr)->cols)
    {
        cols = WIDGET (file_attr)->cols;
        cols = MIN (cols, mw->cols - wx * 2);
        widget_set_size (dw, dw->y, dw->x, lines, cols + wx * 2);
    }

    cb = chattrboxes_new (y++, wx, checkboxes_lines, cols);
    cbg = GROUP (cb);
    group_add_widget_autopos (dg, cb, WPOS_KEEP_TOP | WPOS_KEEP_HORZ, NULL);

    /* create checkboxes */
    for (i = 0; i < (size_t) check_attr_mod_num && i < (size_t) checkboxes_lines; i++)
    {
        int m;
        WCheck *check;

        m = check_attr_mod[i];

        check = check_new (i, 0, check_attr[m].state, NULL);
        group_add_widget (cbg, check);
    }

    chattrboxes_rename (cb);

    y += i - 1;
    cols = 0;

    for (i = single_set ? (BUTTONS - 2) : 0; i < BUTTONS; i++)
    {
        if (i == 0 || i == BUTTONS - 2)
            group_add_widget (dg, hline_new (y++, -1, -1));

        chattr_but[i].button = WIDGET (button_new (y, dw->cols / 2 + 1 - chattr_but[i].width,
                                                   chattr_but[i].ret_cmd, chattr_but[i].flags,
                                                   chattr_but[i].text, NULL));
        group_add_widget (dg, chattr_but[i].button);

        i++;
        chattr_but[i].button = WIDGET (button_new (y++, dw->cols / 2 + 2, chattr_but[i].ret_cmd,
                                                   chattr_but[i].flags, chattr_but[i].text, NULL));
        group_add_widget (dg, chattr_but[i].button);

        /* two buttons in a row */
        cols = MAX (cols, chattr_but[i - 1].button->cols + 1 + chattr_but[i].button->cols);
    }

    /* adjust dialog size and button positions */
    cols += 6;
    if (cols > dw->cols)
    {
        widget_set_size (dw, dw->y, dw->x, lines, cols);

        /* dialog center */
        cols = dw->x + dw->cols / 2 + 1;

        for (i = single_set ? (BUTTONS - 2) : 0; i < BUTTONS; i++)
        {
            Widget *b;

            b = chattr_but[i++].button;
            widget_set_size (b, b->y, cols - b->cols, b->lines, b->cols);

            b = chattr_but[i].button;
            widget_set_size (b, b->y, cols + 1, b->lines, b->cols);
        }
    }

    /* select first checkbox */
    cbg->current = cbg->widgets;
    widget_select (WIDGET (cb));

    return ch_dlg;
}

/* --------------------------------------------------------------------------------------------- */

static void
chattr_done (gboolean need_update)
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
try_chattr (const char *p, unsigned long m)
{
    while (fsetflags (p, m) == -1 && !ignore_all)
    {
        int my_errno = errno;
        int result;
        char *msg;

        msg =
            g_strdup_printf (_("Cannot chattr \"%s\"\n%s"), x_basename (p),
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
do_chattr (WPanel * panel, const vfs_path_t * p, unsigned long m)
{
    gboolean ret;

    m &= and_mask;
    m |= or_mask;

    ret = try_chattr (vfs_path_as_str (p), m);

    do_file_mark (panel, current_file, 0);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static void
chattr_apply_mask (WPanel * panel, vfs_path_t * vpath, unsigned long m)
{
    gboolean ok;

    if (!do_chattr (panel, vpath, m))
        return;

    do
    {
        const char *fname;

        fname = next_file (panel);
        ok = (fgetflags (fname, &m) == 0);

        if (!ok)
        {
            /* if current file was deleted outside mc -- try next file */
            /* decrease panel->marked */
            do_file_mark (panel, current_file, 0);

            /* try next file */
            ok = TRUE;
        }
        else
        {
            vpath = vfs_path_from_str (fname);
            flags = m;
            ok = do_chattr (panel, vpath, m);
            vfs_path_free (vpath);
        }
    }
    while (ok && panel->marked != 0);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
chattr_cmd (WPanel * panel)
{
    gboolean need_update = FALSE;
    gboolean end_chattr = FALSE;

    chattr_init ();

    current_file = 0;
    ignore_all = FALSE;

    do
    {                           /* do while any files remaining */
        vfs_path_t *vpath;
        WDialog *ch_dlg;
        const char *fname, *fname2;
        size_t i;
        int result;

        if (!vfs_current_is_local ())
        {
            message (D_ERROR, MSG_ERROR, "%s",
                     _("Cannot change attributes on non-local filesystems"));
            break;
        }

        do_refresh ();

        need_update = FALSE;
        end_chattr = FALSE;

        if (panel->marked != 0)
            fname = next_file (panel);  /* next marked file */
        else
            fname = selection (panel)->fname;   /* single file */

        vpath = vfs_path_from_str (fname);
        fname2 = vfs_path_as_str (vpath);

        if (fgetflags (fname2, &flags) != 0)
        {
            message (D_ERROR, MSG_ERROR, _("Cannot get flags of \"%s\"\n%s"), fname,
                     unix_error_string (errno));
            vfs_path_free (vpath);
            break;
        }

        flags_changed = FALSE;

        ch_dlg = chattr_dlg_create (panel, fname, flags);
        result = dlg_run (ch_dlg);
        dlg_destroy (ch_dlg);

        switch (result)
        {
        case B_CANCEL:
            end_chattr = TRUE;
            break;

        case B_ENTER:
            if (flags_changed)
            {
                if (panel->marked <= 1)
                {
                    /* single or last file */
                    if (fsetflags (fname2, flags) == -1 && !ignore_all)
                        message (D_ERROR, MSG_ERROR, _("Cannot chattr \"%s\"\n%s"), fname,
                                 unix_error_string (errno));
                    end_chattr = TRUE;
                }
                else if (!try_chattr (fname2, flags))
                {
                    /* stop multiple files processing */
                    result = B_CANCEL;
                    end_chattr = TRUE;
                }
            }

            need_update = TRUE;
            break;

        case B_SETALL:
        case B_MARKED:
            or_mask = 0;
            and_mask = ~0;

            for (i = 0; i < check_attr_num; i++)
                if (chattr_is_modifiable (i) && (check_attr[i].selected || result == B_SETALL))
                {
                    if (check_attr[i].state)
                        or_mask |= check_attr[i].flags;
                    else
                        and_mask &= ~check_attr[i].flags;
                }

            chattr_apply_mask (panel, vpath, flags);
            need_update = TRUE;
            end_chattr = TRUE;
            break;

        case B_SETMRK:
            or_mask = 0;
            and_mask = ~0;

            for (i = 0; i < check_attr_num; i++)
                if (chattr_is_modifiable (i) && check_attr[i].selected)
                    or_mask |= check_attr[i].flags;

            chattr_apply_mask (panel, vpath, flags);
            need_update = TRUE;
            end_chattr = TRUE;
            break;

        case B_CLRMRK:
            or_mask = 0;
            and_mask = ~0;

            for (i = 0; i < check_attr_num; i++)
                if (chattr_is_modifiable (i) && check_attr[i].selected)
                    and_mask &= ~check_attr[i].flags;

            chattr_apply_mask (panel, vpath, flags);
            need_update = TRUE;
            end_chattr = TRUE;
            break;

        default:
            break;
        }

        if (panel->marked != 0 && result != B_CANCEL)
        {
            do_file_mark (panel, current_file, 0);
            need_update = TRUE;
        }

        vfs_path_free (vpath);

    }
    while (panel->marked != 0 && !end_chattr);

    chattr_done (need_update);
}

/* --------------------------------------------------------------------------------------------- */

const char *
chattr_get_as_str (unsigned long attr)
{
    static char str[32 + 1];    /* 32 bits in attributes (unsigned long) */

    chattr_fill_str (attr, str);

    return str;
}

/* --------------------------------------------------------------------------------------------- */
