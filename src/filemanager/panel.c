/*
   Panel managing.

   Copyright (C) 1994, 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2009, 2011
   The Free Software Foundation, Inc.

   Written by:
   Miguel de Icaza, 1995
   Timur Bakeyev, 1997, 1999

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

/** \file panel.c
 *  \brief Source: panel managin module
 */

#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/mouse.h"      /* For Gpm_Event */
#include "lib/tty/key.h"        /* XCTRL and ALT macros  */
#include "lib/skin.h"
#include "lib/strescape.h"
#include "lib/mcconfig.h"
#include "lib/vfs/vfs.h"
#include "lib/unixcompat.h"
#include "lib/timefmt.h"        /* file_date() */
#include "lib/util.h"
#include "lib/widget.h"
#ifdef HAVE_CHARSET
#include "lib/charsets.h"       /* get_codepage_id () */
#endif
#include "lib/event.h"

#include "src/setup.h"          /* For loading/saving panel options */
#include "src/execute.h"
#ifdef HAVE_CHARSET
#include "src/selcodepage.h"    /* select_charset (), SELECT_CHARSET_NO_TRANSLATE */
#endif
#include "src/keybind-defaults.h"       /* global_keymap_t */
#ifdef ENABLE_SUBSHELL
#include "src/subshell.h"       /* do_subshell_chdir() */
#endif

#include "dir.h"
#include "boxes.h"
#include "tree.h"
#include "ext.h"                /* regexp_command */
#include "layout.h"             /* Most layout variables are here */
#include "cmd.h"
#include "command.h"            /* cmdline */
#include "usermenu.h"
#include "midnight.h"
#include "mountlist.h"          /* my_statfs */

#include "panel.h"

/*** global variables ****************************************************************************/

/* The hook list for the select file function */
hook_t *select_file_hook = NULL;

/* *INDENT-OFF* */
panelized_panel_t panelized_panel = { {NULL, 0}, -1, NULL };
/* *INDENT-ON* */

static const char *string_file_name (file_entry *, int);
static const char *string_file_size (file_entry *, int);
static const char *string_file_size_brief (file_entry *, int);
static const char *string_file_type (file_entry *, int);
static const char *string_file_mtime (file_entry *, int);
static const char *string_file_atime (file_entry *, int);
static const char *string_file_ctime (file_entry *, int);
static const char *string_file_permission (file_entry *, int);
static const char *string_file_perm_octal (file_entry *, int);
static const char *string_file_nlinks (file_entry *, int);
static const char *string_inode (file_entry *, int);
static const char *string_file_nuid (file_entry *, int);
static const char *string_file_ngid (file_entry *, int);
static const char *string_file_owner (file_entry *, int);
static const char *string_file_group (file_entry *, int);
static const char *string_marked (file_entry *, int);
static const char *string_space (file_entry *, int);
static const char *string_dot (file_entry *, int);

/* *INDENT-OFF* */
panel_field_t panel_fields[] = {
    {
     "unsorted", 12, 1, J_LEFT_FIT,
     /* TRANSLATORS: one single character to represent 'unsorted' sort mode  */
     /* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
     N_("sort|u"),
     N_("&Unsorted"), TRUE, FALSE,
     string_file_name,
     (sortfn *) unsorted
    }
    ,
    {
     "name", 12, 1, J_LEFT_FIT,
     /* TRANSLATORS: one single character to represent 'name' sort mode  */
     /* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
     N_("sort|n"),
     N_("&Name"), TRUE, TRUE,
     string_file_name,
     (sortfn *) sort_name
    }
    ,
    {
     "version", 12, 1, J_LEFT_FIT,
     /* TRANSLATORS: one single character to represent 'version' sort mode  */
     /* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
     N_("sort|v"),
     N_("&Version"), TRUE, FALSE,
     string_file_name,
     (sortfn *) sort_vers
    }
    ,
    {
     "extension", 12, 1, J_LEFT_FIT,
     /* TRANSLATORS: one single character to represent 'extension' sort mode  */
     /* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
     N_("sort|e"),
     N_("E&xtension"), TRUE, FALSE,
     string_file_name,          /* TODO: string_file_ext */
     (sortfn *) sort_ext
    }
    ,
    {
     "size", 7, 0, J_RIGHT,
     /* TRANSLATORS: one single character to represent 'size' sort mode  */
     /* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
     N_("sort|s"),
     N_("&Size"), TRUE, TRUE,
     string_file_size,
     (sortfn *) sort_size
    }
    ,
    {
     "bsize", 7, 0, J_RIGHT,
     "",
     N_("Block Size"), FALSE, FALSE,
     string_file_size_brief,
     (sortfn *) sort_size
    }
    ,
    {
     "type", 1, 0, J_LEFT,
     "",
     "", FALSE, TRUE,
     string_file_type,
     NULL
    }
    ,
    {
     "mtime", 12, 0, J_RIGHT,
     /* TRANSLATORS: one single character to represent 'Modify time' sort mode  */
     /* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
     N_("sort|m"),
     N_("&Modify time"), TRUE, TRUE,
     string_file_mtime,
     (sortfn *) sort_time
    }
    ,
    {
     "atime", 12, 0, J_RIGHT,
     /* TRANSLATORS: one single character to represent 'Access time' sort mode  */
     /* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
     N_("sort|a"),
     N_("&Access time"), TRUE, TRUE,
     string_file_atime,
     (sortfn *) sort_atime
    }
    ,
    {
     "ctime", 12, 0, J_RIGHT,
     /* TRANSLATORS: one single character to represent 'Change time' sort mode  */
     /* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
     N_("sort|h"),
     N_("C&hange time"), TRUE, TRUE,
     string_file_ctime,
     (sortfn *) sort_ctime
    }
    ,
    {
     "perm", 10, 0, J_LEFT,
     "",
     N_("Permission"), FALSE, TRUE,
     string_file_permission,
     NULL
    }
    ,
    {
     "mode", 6, 0, J_RIGHT,
     "",
     N_("Perm"), FALSE, TRUE,
     string_file_perm_octal,
     NULL
    }
    ,
    {
     "nlink", 2, 0, J_RIGHT,
     "",
     N_("Nl"), FALSE, TRUE,
     string_file_nlinks, NULL
    }
    ,
    {
     "inode", 5, 0, J_RIGHT,
     /* TRANSLATORS: one single character to represent 'inode' sort mode  */
     /* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
     N_("sort|i"),
     N_("&Inode"), TRUE, TRUE,
     string_inode,
     (sortfn *) sort_inode
    }
    ,
    {
     "nuid", 5, 0, J_RIGHT,
     "",
     N_("UID"), FALSE, FALSE,
     string_file_nuid,
     NULL
    }
    ,
    {
     "ngid", 5, 0, J_RIGHT,
     "",
     N_("GID"), FALSE, FALSE,
     string_file_ngid,
     NULL
    }
    ,
    {
     "owner", 8, 0, J_LEFT_FIT,
     "",
     N_("Owner"), FALSE, TRUE,
     string_file_owner,
     NULL
    }
    ,
    {
     "group", 8, 0, J_LEFT_FIT,
     "",
     N_("Group"), FALSE, TRUE,
     string_file_group,
     NULL
    }
    ,
    {
     "mark", 1, 0, J_RIGHT,
     "",
     " ", FALSE, TRUE,
     string_marked,
     NULL
    }
    ,
    {
     "|", 1, 0, J_RIGHT,
     "",
     " ", FALSE, TRUE,
     NULL,
     NULL
    }
    ,
    {
     "space", 1, 0, J_RIGHT,
     "",
     " ", FALSE, TRUE,
     string_space,
     NULL
    }
    ,
    {
     "dot", 1, 0, J_RIGHT,
     "",
     " ", FALSE, FALSE,
     string_dot,
     NULL
    }
    ,
    {
     NULL, 0, 0, J_RIGHT, NULL, NULL, FALSE, FALSE, NULL, NULL
    }
};
/* *INDENT-ON* */

mc_fhl_t *mc_filehighlight = NULL;

extern int saving_setup;

/*** file scope macro definitions ****************************************************************/

#define ELEMENTS(arr) ( sizeof(arr) / sizeof((arr)[0]) )

#define NORMAL          0
#define SELECTED        1
#define MARKED          2
#define MARKED_SELECTED 3
#define STATUS          5

/* This macro extracts the number of available lines in a panel */
#define llines(p) (WIDGET (p)->lines - 3 - (panels_options.show_mini_info ? 2 : 0))

/*** file scope type declarations ****************************************************************/

typedef enum
{
    MARK_DONT_MOVE = 0,
    MARK_DOWN = 1,
    MARK_FORCE_DOWN = 2,
    MARK_FORCE_UP = 3
} mark_act_t;

/*
 * This describes a format item.  The parse_display_format routine parses
 * the user specified format and creates a linked list of format_e structures.
 */
typedef struct format_e
{
    struct format_e *next;
    int requested_field_len;
    int field_len;
    align_crt_t just_mode;
    int expand;
    const char *(*string_fn) (file_entry *, int len);
    char *title;
    const char *id;
} format_e;

/* File name scroll states */
typedef enum
{
    FILENAME_NOSCROLL = 1,
    FILENAME_SCROLL_LEFT = 2,
    FILENAME_SCROLL_RIGHT = 4
} filename_scroll_flag_t;

/*** file scope variables ************************************************************************/

static char *panel_sort_up_sign = NULL;
static char *panel_sort_down_sign = NULL;

static char *panel_hiddenfiles_sign_show = NULL;
static char *panel_hiddenfiles_sign_hide = NULL;
static char *panel_history_prev_item_sign = NULL;
static char *panel_history_next_item_sign = NULL;
static char *panel_history_show_list_sign = NULL;
static char *panel_filename_scroll_left_char = NULL;
static char *panel_filename_scroll_right_char = NULL;

/* Panel that selection started */
static WPanel *mouse_mark_panel = NULL;

static int mouse_marking = 0;
static int state_mark = 0;
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
set_colors (WPanel * panel)
{
    (void) panel;
    tty_set_normal_attrs ();
    tty_setcolor (NORMAL_COLOR);
}

/* --------------------------------------------------------------------------------------------- */
/** Delete format string, it is a linked list */

static void
delete_format (format_e * format)
{
    while (format != NULL)
    {
        format_e *next = format->next;
        g_free (format->title);
        g_free (format);
        format = next;
    }
}

/* --------------------------------------------------------------------------------------------- */
/** This code relies on the default justification!!! */

static void
add_permission_string (const char *dest, int width, file_entry * fe, int attr, int color,
                       int is_octal)
{
    int i, r, l;

    l = get_user_permissions (&fe->st);

    if (is_octal)
    {
        /* Place of the access bit in octal mode */
        l = width + l - 3;
        r = l + 1;
    }
    else
    {
        /* The same to the triplet in string mode */
        l = l * 3 + 1;
        r = l + 3;
    }

    for (i = 0; i < width; i++)
    {
        if (i >= l && i < r)
        {
            if (attr == SELECTED || attr == MARKED_SELECTED)
                tty_setcolor (MARKED_SELECTED_COLOR);
            else
                tty_setcolor (MARKED_COLOR);
        }
        else if (color >= 0)
            tty_setcolor (color);
        else
            tty_lowlevel_setcolor (-color);

        tty_print_char (dest[i]);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** String representations of various file attributes name */

static const char *
string_file_name (file_entry * fe, int len)
{
    static char buffer[MC_MAXPATHLEN * MB_LEN_MAX + 1];

    (void) len;
    g_strlcpy (buffer, fe->fname, sizeof (buffer));
    return buffer;
}

/* --------------------------------------------------------------------------------------------- */

static unsigned int
ilog10 (dev_t n)
{
    unsigned int digits = 0;
    do
    {
        digits++, n /= 10;
    }
    while (n != 0);
    return digits;
}

/* --------------------------------------------------------------------------------------------- */

static void
format_device_number (char *buf, size_t bufsize, dev_t dev)
{
    dev_t major_dev = major (dev);
    dev_t minor_dev = minor (dev);
    unsigned int major_digits = ilog10 (major_dev);
    unsigned int minor_digits = ilog10 (minor_dev);

    g_assert (bufsize >= 1);
    if (major_digits + 1 + minor_digits + 1 <= bufsize)
    {
        g_snprintf (buf, bufsize, "%lu,%lu", (unsigned long) major_dev, (unsigned long) minor_dev);
    }
    else
    {
        g_strlcpy (buf, _("[dev]"), bufsize);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** size */

static const char *
string_file_size (file_entry * fe, int len)
{
    static char buffer[BUF_TINY];

    /* Don't ever show size of ".." since we don't calculate it */
    if (!strcmp (fe->fname, ".."))
    {
        return _("UP--DIR");
    }

#ifdef HAVE_STRUCT_STAT_ST_RDEV
    if (S_ISBLK (fe->st.st_mode) || S_ISCHR (fe->st.st_mode))
        format_device_number (buffer, len + 1, fe->st.st_rdev);
    else
#endif
    {
        size_trunc_len (buffer, (unsigned int) len, fe->st.st_size, 0, panels_options.kilobyte_si);
    }
    return buffer;
}

/* --------------------------------------------------------------------------------------------- */
/** bsize */

static const char *
string_file_size_brief (file_entry * fe, int len)
{
    if (S_ISLNK (fe->st.st_mode) && !fe->f.link_to_dir)
    {
        return _("SYMLINK");
    }

    if ((S_ISDIR (fe->st.st_mode) || fe->f.link_to_dir) && strcmp (fe->fname, ".."))
    {
        return _("SUB-DIR");
    }

    return string_file_size (fe, len);
}

/* --------------------------------------------------------------------------------------------- */
/** This functions return a string representation of a file entry type */

static const char *
string_file_type (file_entry * fe, int len)
{
    static char buffer[2];

    (void) len;
    if (S_ISDIR (fe->st.st_mode))
        buffer[0] = PATH_SEP;
    else if (S_ISLNK (fe->st.st_mode))
    {
        if (fe->f.link_to_dir)
            buffer[0] = '~';
        else if (fe->f.stale_link)
            buffer[0] = '!';
        else
            buffer[0] = '@';
    }
    else if (S_ISCHR (fe->st.st_mode))
        buffer[0] = '-';
    else if (S_ISSOCK (fe->st.st_mode))
        buffer[0] = '=';
    else if (S_ISDOOR (fe->st.st_mode))
        buffer[0] = '>';
    else if (S_ISBLK (fe->st.st_mode))
        buffer[0] = '+';
    else if (S_ISFIFO (fe->st.st_mode))
        buffer[0] = '|';
    else if (S_ISNAM (fe->st.st_mode))
        buffer[0] = '#';
    else if (!S_ISREG (fe->st.st_mode))
        buffer[0] = '?';        /* non-regular of unknown kind */
    else if (is_exe (fe->st.st_mode))
        buffer[0] = '*';
    else
        buffer[0] = ' ';
    buffer[1] = '\0';
    return buffer;
}

/* --------------------------------------------------------------------------------------------- */
/** mtime */

static const char *
string_file_mtime (file_entry * fe, int len)
{
    (void) len;
    return file_date (fe->st.st_mtime);
}

/* --------------------------------------------------------------------------------------------- */
/** atime */

static const char *
string_file_atime (file_entry * fe, int len)
{
    (void) len;
    return file_date (fe->st.st_atime);
}

/* --------------------------------------------------------------------------------------------- */
/** ctime */

static const char *
string_file_ctime (file_entry * fe, int len)
{
    (void) len;
    return file_date (fe->st.st_ctime);
}

/* --------------------------------------------------------------------------------------------- */
/** perm */

static const char *
string_file_permission (file_entry * fe, int len)
{
    (void) len;
    return string_perm (fe->st.st_mode);
}

/* --------------------------------------------------------------------------------------------- */
/** mode */

static const char *
string_file_perm_octal (file_entry * fe, int len)
{
    static char buffer[10];

    (void) len;
    g_snprintf (buffer, sizeof (buffer), "0%06lo", (unsigned long) fe->st.st_mode);
    return buffer;
}

/* --------------------------------------------------------------------------------------------- */
/** nlink */

static const char *
string_file_nlinks (file_entry * fe, int len)
{
    static char buffer[BUF_TINY];

    (void) len;
    g_snprintf (buffer, sizeof (buffer), "%16d", (int) fe->st.st_nlink);
    return buffer;
}

/* --------------------------------------------------------------------------------------------- */
/** inode */

static const char *
string_inode (file_entry * fe, int len)
{
    static char buffer[10];

    (void) len;
    g_snprintf (buffer, sizeof (buffer), "%lu", (unsigned long) fe->st.st_ino);
    return buffer;
}

/* --------------------------------------------------------------------------------------------- */
/** nuid */

static const char *
string_file_nuid (file_entry * fe, int len)
{
    static char buffer[10];

    (void) len;
    g_snprintf (buffer, sizeof (buffer), "%lu", (unsigned long) fe->st.st_uid);
    return buffer;
}

/* --------------------------------------------------------------------------------------------- */
/** ngid */

static const char *
string_file_ngid (file_entry * fe, int len)
{
    static char buffer[10];

    (void) len;
    g_snprintf (buffer, sizeof (buffer), "%lu", (unsigned long) fe->st.st_gid);
    return buffer;
}

/* --------------------------------------------------------------------------------------------- */
/** owner */

static const char *
string_file_owner (file_entry * fe, int len)
{
    (void) len;
    return get_owner (fe->st.st_uid);
}

/* --------------------------------------------------------------------------------------------- */
/** group */

static const char *
string_file_group (file_entry * fe, int len)
{
    (void) len;
    return get_group (fe->st.st_gid);
}

/* --------------------------------------------------------------------------------------------- */
/** mark */

static const char *
string_marked (file_entry * fe, int len)
{
    (void) len;
    return fe->f.marked ? "*" : " ";
}

/* --------------------------------------------------------------------------------------------- */
/** space */

static const char *
string_space (file_entry * fe, int len)
{
    (void) fe;
    (void) len;
    return " ";
}

/* --------------------------------------------------------------------------------------------- */
/** dot */

static const char *
string_dot (file_entry * fe, int len)
{
    (void) fe;
    (void) len;
    return ".";
}

/* --------------------------------------------------------------------------------------------- */

static int
file_compute_color (int attr, file_entry * fe)
{
    switch (attr)
    {
    case SELECTED:
        return (SELECTED_COLOR);
    case MARKED:
        return (MARKED_COLOR);
    case MARKED_SELECTED:
        return (MARKED_SELECTED_COLOR);
    case STATUS:
        return (NORMAL_COLOR);
    case NORMAL:
    default:
        if (!panels_options.filetype_mode)
            return (NORMAL_COLOR);
    }

    return mc_fhl_get_color (mc_filehighlight, fe);
}

/* --------------------------------------------------------------------------------------------- */
/** Formats the file number file_index of panel in the buffer dest */

static filename_scroll_flag_t
format_file (char *dest, int limit, WPanel * panel, int file_index, int width, int attr,
             int isstatus, int *field_lenght)
{
    int color, length, empty_line;
    const char *txt;
    format_e *format, *home;
    file_entry *fe;
    filename_scroll_flag_t res = FILENAME_NOSCROLL;

    (void) dest;
    (void) limit;
    length = 0;
    empty_line = (file_index >= panel->count);
    home = (isstatus) ? panel->status_format : panel->format;
    fe = &panel->dir.list[file_index];
    *field_lenght = 0;

    if (!empty_line)
        color = file_compute_color (attr, fe);
    else
        color = NORMAL_COLOR;
    for (format = home; format; format = format->next)
    {
        if (length == width)
            break;

        if (format->string_fn)
        {
            int len, perm;
            const char *prepared_text;
            int name_offset = 0;

            if (empty_line)
                txt = " ";
            else
                txt = (*format->string_fn) (fe, format->field_len);

            len = format->field_len;
            if (len + length > width)
                len = width - length;
            if (len <= 0)
                break;

            if (!isstatus && panel->content_shift > -1 && strcmp (format->id, "name") == 0)
            {
                int str_len;
                int i;

                *field_lenght = len + 1;

                str_len = str_length (txt);
                i = max (0, str_len - len);
                panel->max_shift = max (panel->max_shift, i);
                i = min (panel->content_shift, i);

                if (i > -1)
                {
                    name_offset = str_offset_to_pos (txt, i);
                    if (str_len > len)
                    {
                        res = FILENAME_SCROLL_LEFT;
                        if (str_length (txt + name_offset) > len)
                            res |= FILENAME_SCROLL_RIGHT;
                    }
                }
            }

            perm = 0;
            if (panels_options.permission_mode)
            {
                if (!strcmp (format->id, "perm"))
                    perm = 1;
                else if (!strcmp (format->id, "mode"))
                    perm = 2;
            }

            if (color >= 0)
                tty_setcolor (color);
            else
                tty_lowlevel_setcolor (-color);

            if (!isstatus && panel->content_shift > -1)
                prepared_text =
                    str_fit_to_term (txt + name_offset, len, HIDE_FIT (format->just_mode));
            else
                prepared_text = str_fit_to_term (txt, len, format->just_mode);

            if (perm)
                add_permission_string (prepared_text, format->field_len, fe, attr, color, perm - 1);
            else
                tty_print_string (prepared_text);

            length += len;
        }
        else
        {
            if (attr == SELECTED || attr == MARKED_SELECTED)
                tty_setcolor (SELECTED_COLOR);
            else
                tty_setcolor (NORMAL_COLOR);
            tty_print_one_vline (TRUE);
            length++;
        }
    }

    if (length < width)
    {
        int y, x;

        tty_getyx (&y, &x);
        tty_draw_hline (y, x, ' ', width - length);
    }

    return res;
}

/* --------------------------------------------------------------------------------------------- */

static void
repaint_file (WPanel * panel, int file_index, int mv, int attr, int isstatus)
{
    Widget *w = WIDGET (panel);

    int second_column = 0;
    int width;
    int offset = 0;
    char buffer[BUF_MEDIUM];
    filename_scroll_flag_t ret_frm;
    int ypos = 0;
    gboolean panel_is_split = !isstatus && panel->split;
    int fln = 0;

    width = w->cols - 2;

    if (panel_is_split)
    {
        second_column = (file_index - panel->top_file) / llines (panel);
        width = width / 2 - 1;

        if (second_column != 0)
        {
            offset = 1 + width;
            width = w->cols - offset - 2;
        }
    }

    /* Nothing to paint */
    if (width <= 0)
        return;

    if (mv)
    {
        int pos = file_index - panel->top_file;

        if (panel_is_split)
            ypos = pos % llines (panel);
        else
            ypos = pos;

        ypos += 2;
        widget_move (w, ypos, offset + 1);
    }

    ret_frm = format_file (buffer, sizeof (buffer), panel, file_index, width, attr, isstatus, &fln);

    if (panel_is_split)
    {
        if (second_column)
            tty_print_char (' ');
        else
        {
            tty_setcolor (NORMAL_COLOR);
            tty_print_one_vline (TRUE);
        }
    }
    if (ret_frm != FILENAME_NOSCROLL && mv)
    {
        if (!panel_is_split && fln > 0)
        {
            if (panel->list_type == list_long)
            {
                offset = width - fln + 1;
                width = fln - 1;
            }
            else
            {
                width = fln;
            }
        }
        widget_move (w, ypos, offset);
        tty_setcolor (NORMAL_COLOR);
        tty_print_string (panel_filename_scroll_left_char);

        if ((ret_frm & FILENAME_SCROLL_RIGHT) != 0)
        {
            widget_move (w, ypos, offset + 1 + width);
            tty_setcolor (NORMAL_COLOR);
            tty_print_string (panel_filename_scroll_right_char);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
display_mini_info (WPanel * panel)
{
    Widget *w = WIDGET (panel);

    if (!panels_options.show_mini_info)
        return;

    widget_move (w, llines (panel) + 3, 1);

    if (panel->searching)
    {
        tty_setcolor (INPUT_COLOR);
        tty_print_char ('/');
        tty_print_string (str_fit_to_term (panel->search_buffer, w->cols - 3, J_LEFT));
        return;
    }

    /* Status resolves links and show them */
    set_colors (panel);

    if (S_ISLNK (panel->dir.list[panel->selected].st.st_mode))
    {
        char link_target[MC_MAXPATHLEN];
        vfs_path_t *lc_link_vpath;
        int len;

        lc_link_vpath =
            vfs_path_append_new (panel->cwd_vpath, panel->dir.list[panel->selected].fname, NULL);
        len = mc_readlink (lc_link_vpath, link_target, MC_MAXPATHLEN - 1);
        vfs_path_free (lc_link_vpath);
        if (len > 0)
        {
            link_target[len] = 0;
            tty_print_string ("-> ");
            tty_print_string (str_fit_to_term (link_target, w->cols - 5, J_LEFT_FIT));
        }
        else
            tty_print_string (str_fit_to_term (_("<readlink failed>"), w->cols - 2, J_LEFT));
    }
    else if (strcmp (panel->dir.list[panel->selected].fname, "..") == 0)
    {
        /* FIXME:
         * while loading directory (do_load_dir() and do_reload_dir()),
         * the actual stat info about ".." directory isn't got;
         * so just don't display incorrect info about ".." directory */
        tty_print_string (str_fit_to_term (_("UP--DIR"), w->cols - 2, J_LEFT));
    }
    else
        /* Default behavior */
        repaint_file (panel, panel->selected, 0, STATUS, 1);
}

/* --------------------------------------------------------------------------------------------- */

static void
paint_dir (WPanel * panel)
{
    int i;
    int color;                  /* Color value of the line */
    int items;                  /* Number of items */

    items = llines (panel) * (panel->split ? 2 : 1);
    /* reset max len of filename because we have the new max length for the new file list */
    panel->max_shift = -1;
    for (i = 0; i < items; i++)
    {
        if (i + panel->top_file >= panel->count)
            color = 0;
        else
        {
            color = 2 * (panel->dir.list[i + panel->top_file].f.marked);
            color += (panel->selected == i + panel->top_file && panel->active);
        }
        repaint_file (panel, i + panel->top_file, 1, color, 0);
    }

    tty_set_normal_attrs ();
}

/* --------------------------------------------------------------------------------------------- */

static void
display_total_marked_size (WPanel * panel, int y, int x, gboolean size_only)
{
    Widget *w = WIDGET (panel);

    char buffer[BUF_SMALL], b_bytes[BUF_SMALL], *buf;
    int cols;

    if (panel->marked <= 0)
        return;

    buf = size_only ? b_bytes : buffer;
    cols = w->cols - 2;

    /*
     * This is a trick to use two ngettext() calls in one sentence.
     * First make "N bytes", then insert it into "X in M files".
     */
    g_snprintf (b_bytes, sizeof (b_bytes),
                ngettext ("%s byte", "%s bytes", panel->total),
                size_trunc_sep (panel->total, panels_options.kilobyte_si));
    if (!size_only)
        g_snprintf (buffer, sizeof (buffer),
                    ngettext ("%s in %d file", "%s in %d files", panel->marked),
                    b_bytes, panel->marked);

    /* don't forget spaces around buffer content */
    buf = (char *) str_trunc (buf, cols - 4);

    if (x < 0)
        /* center in panel */
        x = (w->cols - str_term_width1 (buf)) / 2 - 1;

    /*
     * y == llines (panel) + 2  for mini_info_separator
     * y == w->lines - 1        for panel bottom frame
     */
    widget_move (w, y, x);
    tty_setcolor (MARKED_COLOR);
    tty_printf (" %s ", buf);
}

/* --------------------------------------------------------------------------------------------- */

static void
mini_info_separator (WPanel * panel)
{
    if (panels_options.show_mini_info)
    {
        Widget *w = WIDGET (panel);
        const int y = llines (panel) + 2;

        tty_setcolor (NORMAL_COLOR);
        tty_draw_hline (w->y + y, w->x + 1, ACS_HLINE, w->cols - 2);
        /* Status displays total marked size.
         * Centered in panel, full format. */
        display_total_marked_size (panel, y, -1, FALSE);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
show_free_space (WPanel * panel)
{
    /* Used to figure out how many free space we have */
    static struct my_statfs myfs_stats;
    /* Old current working directory for displaying free space */
    static char *old_cwd = NULL;
    char *tmp_path;

    /* Don't try to stat non-local fs */
    if (!vfs_file_is_local (panel->cwd_vpath) || !free_space)
        return;

    tmp_path = vfs_path_to_str (panel->cwd_vpath);
    if (old_cwd == NULL || strcmp (old_cwd, tmp_path) != 0)
    {
        char rpath[PATH_MAX];

        init_my_statfs ();
        g_free (old_cwd);
        old_cwd = tmp_path;
        tmp_path = NULL;

        if (mc_realpath (old_cwd, rpath) == NULL)
            return;

        my_statfs (&myfs_stats, rpath);
    }
    g_free (tmp_path);

    if (myfs_stats.avail != 0 || myfs_stats.total != 0)
    {
        Widget *w = WIDGET (panel);
        char buffer1[6], buffer2[6], tmp[BUF_SMALL];

        size_trunc_len (buffer1, sizeof (buffer1) - 1, myfs_stats.avail, 1,
                        panels_options.kilobyte_si);
        size_trunc_len (buffer2, sizeof (buffer2) - 1, myfs_stats.total, 1,
                        panels_options.kilobyte_si);
        g_snprintf (tmp, sizeof (tmp), " %s/%s (%d%%) ", buffer1, buffer2,
                    myfs_stats.total == 0 ? 0 :
                    (int) (100 * (long double) myfs_stats.avail / myfs_stats.total));
        widget_move (w, w->lines - 1, w->cols - 2 - (int) strlen (tmp));
        tty_setcolor (NORMAL_COLOR);
        tty_print_string (tmp);
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Prepare path string for showing in panel's header.
 * Passwords will removed, also home dir will replaced by ~
 *
 * @param panel WPanel object
 *
 * @return newly allocated string.
 */

static char *
panel_correct_path_to_show (WPanel * panel)
{
    vfs_path_t *last_vpath;
    const vfs_path_element_t *path_element;
    char *return_path;
    int elements_count;

    elements_count = vfs_path_elements_count (panel->cwd_vpath);

    /* get last path element */
    path_element = vfs_path_element_clone (vfs_path_get_by_index (panel->cwd_vpath, -1));


    if (elements_count > 1 && (strcmp (path_element->class->name, "cpiofs") == 0 ||
                               strcmp (path_element->class->name, "extfs") == 0 ||
                               strcmp (path_element->class->name, "tarfs") == 0))
    {
        const char *archive_name;
        const vfs_path_element_t *prev_path_element;

        /* get previous path element for catching archive name */
        prev_path_element = vfs_path_get_by_index (panel->cwd_vpath, -2);
        archive_name = strrchr (prev_path_element->path, PATH_SEP);
        if (archive_name != NULL)
        {
            last_vpath = vfs_path_from_str_flags (archive_name + 1, VPF_NO_CANON);
        }
        else
        {
            last_vpath = vfs_path_from_str_flags (prev_path_element->path, VPF_NO_CANON);
            last_vpath->relative = TRUE;
        }
    }
    else
    {
        last_vpath = vfs_path_new ();
        last_vpath->relative = TRUE;
    }

    vfs_path_add_element (last_vpath, path_element);
    return_path =
        vfs_path_to_str_flags (last_vpath, 0,
                               VPF_STRIP_HOME | VPF_STRIP_PASSWORD | VPF_HIDE_CHARSET);
    vfs_path_free (last_vpath);

    return return_path;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Get Current path element encoding
 *
 * @param panel WPanel object
 *
 * @return newly allocated string or NULL if path charset is same as system charset
 */

#ifdef HAVE_CHARSET
static char *
panel_get_encoding_info_str (WPanel * panel)
{
    char *ret_str = NULL;
    const vfs_path_element_t *path_element;

    path_element = vfs_path_get_by_index (panel->cwd_vpath, -1);
    if (path_element->encoding != NULL)
        ret_str = g_strdup_printf ("[%s]", path_element->encoding);

    return ret_str;
}
#endif

/* --------------------------------------------------------------------------------------------- */

static void
show_dir (WPanel * panel)
{
    Widget *w = WIDGET (panel);

    gchar *tmp;

    set_colors (panel);
    draw_box (w->owner, w->y, w->x, w->lines, w->cols, FALSE);

    if (panels_options.show_mini_info)
    {
        widget_move (w, llines (panel) + 2, 0);
        tty_print_alt_char (ACS_LTEE, FALSE);
        widget_move (w, llines (panel) + 2, w->cols - 1);
        tty_print_alt_char (ACS_RTEE, FALSE);
    }

    widget_move (w, 0, 1);
    tty_print_string (panel_history_prev_item_sign);

    tmp = panels_options.show_dot_files ? panel_hiddenfiles_sign_show : panel_hiddenfiles_sign_hide;
    tmp = g_strdup_printf ("%s[%s]%s", tmp, panel_history_show_list_sign,
                           panel_history_next_item_sign);

    widget_move (w, 0, w->cols - 6);
    tty_print_string (tmp);

    g_free (tmp);

    widget_move (w, 0, 3);

    if (panel->is_panelized)
        tty_printf (" %s ", _("Panelize"));
#ifdef HAVE_CHARSET
    else
    {
        tmp = panel_get_encoding_info_str (panel);
        if (tmp != NULL)
        {
            tty_printf ("%s", tmp);
            widget_move (w, 0, 3 + strlen (tmp));
            g_free (tmp);
        }
    }
#endif

    if (panel->active)
        tty_setcolor (REVERSE_COLOR);

    tmp = panel_correct_path_to_show (panel);
    tty_printf (" %s ", str_term_trim (tmp, min (max (w->cols - 12, 0), w->cols)));
    g_free (tmp);

    if (!panels_options.show_mini_info)
    {
        if (panel->marked == 0)
        {
            /* Show size of curret file in the bottom of panel */
            if (S_ISREG (panel->dir.list[panel->selected].st.st_mode))
            {
                char buffer[BUF_SMALL];

                g_snprintf (buffer, sizeof (buffer), " %s ",
                            size_trunc_sep (panel->dir.list[panel->selected].st.st_size,
                                            panels_options.kilobyte_si));
                tty_setcolor (NORMAL_COLOR);
                widget_move (w, w->lines - 1, 4);
                tty_print_string (buffer);
            }
        }
        else
        {
            /* Show total size of marked files
             * In the bottom of panel, display size only. */
            display_total_marked_size (panel, w->lines - 1, 2, TRUE);
        }
    }

    show_free_space (panel);

    if (panel->active)
        tty_set_normal_attrs ();
}

/* --------------------------------------------------------------------------------------------- */

/* Returns the number of items in the given panel */
static int
ITEMS (WPanel * p)
{
    if (p->split)
        return llines (p) * 2;
    else
        return llines (p);
}

/* --------------------------------------------------------------------------------------------- */

static void
adjust_top_file (WPanel * panel)
{
    int items = ITEMS (panel);

    if (panel->count <= items)
    {
        /* If all files fit, show them all. */
        panel->top_file = 0;
    }
    else
    {
        int i;

        /* top_file has to be in the range [selected-items+1, selected] so that
           the selected file is visible.
           top_file should be in the range [0, count-items] so that there's
           no empty space wasted.
           Within these ranges, adjust it by as little as possible. */

        if (panel->top_file < 0)
            panel->top_file = 0;

        i = panel->selected - items + 1;
        if (panel->top_file < i)
            panel->top_file = i;

        i = panel->count - items;
        if (panel->top_file > i)
            panel->top_file = i;

        if (panel->top_file > panel->selected)
            panel->top_file = panel->selected;
    }
}

/* --------------------------------------------------------------------------------------------- */
/** add "#enc:encodning" to end of path */
/* if path end width a previous #enc:, only encoding is changed no additional 
 * #enc: is appended
 * retun new string
 */

static char *
panel_save_name (WPanel * panel)
{
    /* If the program is shuting down */
    if ((mc_global.midnight_shutdown && auto_save_setup) || saving_setup)
        return g_strdup (panel->panel_name);
    else
        return g_strconcat ("Temporal:", panel->panel_name, (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
directory_history_add (struct WPanel *panel, const vfs_path_t * vpath)
{
    char *tmp;

    tmp = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_PASSWORD);
    panel->dir_history = list_append_unique (panel->dir_history, tmp);
    panel->dir_history_current = panel->dir_history;
}

/* --------------------------------------------------------------------------------------------- */

/* "history_load" event handler */
static gboolean
panel_load_history (const gchar * event_group_name, const gchar * event_name,
                    gpointer init_data, gpointer data)
{
    WPanel *p = (WPanel *) init_data;
    ev_history_load_save_t *ev = (ev_history_load_save_t *) data;

    (void) event_group_name;
    (void) event_name;

    if (ev->receiver == NULL || ev->receiver == WIDGET (p))
    {
        if (ev->cfg != NULL)
            p->dir_history = history_load (ev->cfg, p->hist_name);
        else
            p->dir_history = history_get (p->hist_name);

        directory_history_add (p, p->cwd_vpath);
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* "history_save" event handler */
static gboolean
panel_save_history (const gchar * event_group_name, const gchar * event_name,
                    gpointer init_data, gpointer data)
{
    WPanel *p = (WPanel *) init_data;

    (void) event_group_name;
    (void) event_name;

    if (p->dir_history != NULL)
    {
        ev_history_load_save_t *ev = (ev_history_load_save_t *) data;

        history_save (ev->cfg, p->hist_name, p->dir_history);
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_destroy (WPanel * p)
{
    size_t i;

    if (panels_options.auto_save_setup)
    {
        char *name;

        name = panel_save_name (p);
        panel_save_setup (p, name);
        g_free (name);
    }

    panel_clean_dir (p);

    /* clean history */
    if (p->dir_history != NULL)
    {
        /* directory history is already saved before this moment */
        p->dir_history = g_list_first (p->dir_history);
        g_list_foreach (p->dir_history, (GFunc) g_free, NULL);
        g_list_free (p->dir_history);
    }
    g_free (p->hist_name);

    delete_format (p->format);
    delete_format (p->status_format);

    g_free (p->user_format);
    for (i = 0; i < LIST_TYPES; i++)
        g_free (p->user_status_format[i]);

    g_free (p->dir.list);
    g_free (p->panel_name);

    vfs_path_free (p->lwd_vpath);
    vfs_path_free (p->cwd_vpath);
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_format_modified (WPanel * panel)
{
    panel->format_modified = 1;
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_paint_sort_info (WPanel * panel)
{
    if (*panel->sort_info.sort_field->hotkey != '\0')
    {
        const char *sort_sign =
            panel->sort_info.reverse ? panel_sort_down_sign : panel_sort_up_sign;
        char *str;

        str = g_strdup_printf ("%s%s", sort_sign, Q_ (panel->sort_info.sort_field->hotkey));
        widget_move (panel, 1, 1);
        tty_print_string (str);
        g_free (str);
    }
}

/* --------------------------------------------------------------------------------------------- */

static gchar *
panel_get_title_without_hotkey (const char *title)
{
    char *translated_title;
    char *hkey;

    if (title == NULL)
        return NULL;
    if (title[0] == '\0')
        return g_strdup ("");

    translated_title = g_strdup (_(title));

    hkey = strchr (translated_title, '&');
    if ((hkey != NULL) && (hkey[1] != '\0'))
        memmove ((void *) hkey, (void *) hkey + 1, strlen (hkey));

    return translated_title;
}

/* --------------------------------------------------------------------------------------------- */

static void
paint_frame (WPanel * panel)
{
    Widget *w = WIDGET (panel);

    int side, width;
    GString *format_txt;

    adjust_top_file (panel);

    widget_erase (w);
    show_dir (panel);

    widget_move (w, 1, 1);

    for (side = 0; side <= panel->split; side++)
    {
        format_e *format;

        if (side)
        {
            tty_setcolor (NORMAL_COLOR);
            tty_print_one_vline (TRUE);
            width = w->cols - w->cols / 2 - 1;
        }
        else if (panel->split)
            width = w->cols / 2 - 3;
        else
            width = w->cols - 2;

        format_txt = g_string_new ("");
        for (format = panel->format; format; format = format->next)
        {
            if (format->string_fn)
            {
                g_string_set_size (format_txt, 0);

                if (panel->list_type == list_long
                    && strcmp (format->id, panel->sort_info.sort_field->id) == 0)
                    g_string_append (format_txt,
                                     panel->sort_info.reverse
                                     ? panel_sort_down_sign : panel_sort_up_sign);

                g_string_append (format_txt, format->title);
                if (strcmp (format->id, "name") == 0 && panel->filter && *panel->filter)
                {
                    g_string_append (format_txt, " [");
                    g_string_append (format_txt, panel->filter);
                    g_string_append (format_txt, "]");
                }

                tty_setcolor (HEADER_COLOR);
                tty_print_string (str_fit_to_term (format_txt->str, format->field_len,
                                                   J_CENTER_LEFT));
                width -= format->field_len;
            }
            else
            {
                tty_setcolor (NORMAL_COLOR);
                tty_print_one_vline (TRUE);
                width--;
            }
        }
        g_string_free (format_txt, TRUE);

        if (width > 0)
        {
            int y, x;

            tty_getyx (&y, &x);
            tty_draw_hline (y, x, ' ', width);
        }
    }

    if (panel->list_type != list_long)
        panel_paint_sort_info (panel);
}

/* --------------------------------------------------------------------------------------------- */

static const char *
parse_panel_size (WPanel * panel, const char *format, int isstatus)
{
    panel_display_t frame = frame_half;
    format = skip_separators (format);

    if (!strncmp (format, "full", 4))
    {
        frame = frame_full;
        format += 4;
    }
    else if (!strncmp (format, "half", 4))
    {
        frame = frame_half;
        format += 4;
    }

    if (!isstatus)
    {
        panel->frame_size = frame;
        panel->split = 0;
    }

    /* Now, the optional column specifier */
    format = skip_separators (format);

    if (*format == '1' || *format == '2')
    {
        if (!isstatus)
            panel->split = *format == '2';
        format++;
    }

    if (!isstatus)
        panel_update_cols (WIDGET (panel), panel->frame_size);

    return skip_separators (format);
}

/* Format is:

   all              := panel_format? format
   panel_format     := [full|half] [1|2]
   format           := one_format_e
   | format , one_format_e

   one_format_e     := just format.id [opt_size]
   just             := [<=>]
   opt_size         := : size [opt_expand]
   size             := [0-9]+
   opt_expand       := +

 */

/* --------------------------------------------------------------------------------------------- */

static format_e *
parse_display_format (WPanel * panel, const char *format, char **error, int isstatus,
                      int *res_total_cols)
{
    format_e *darr, *old = 0, *home = 0;        /* The formats we return */
    int total_cols = 0;         /* Used columns by the format */
    int set_justify;            /* flag: set justification mode? */
    align_crt_t justify = J_LEFT;       /* Which mode. */
    size_t i;

    static size_t i18n_timelength = 0;  /* flag: check ?Time length at startup */

    *error = 0;

    if (i18n_timelength == 0)
    {
        i18n_timelength = i18n_checktimelength ();      /* Musn't be 0 */

        for (i = 0; panel_fields[i].id != NULL; i++)
            if (strcmp ("time", panel_fields[i].id + 1) == 0)
                panel_fields[i].min_size = i18n_timelength;
    }

    /*
     * This makes sure that the panel and mini status full/half mode
     * setting is equal
     */
    format = parse_panel_size (panel, format, isstatus);

    while (*format)
    {                           /* format can be an empty string */
        int found = 0;

        darr = g_new0 (format_e, 1);

        /* I'm so ugly, don't look at me :-) */
        if (!home)
            home = old = darr;

        old->next = darr;
        darr->next = 0;
        old = darr;

        format = skip_separators (format);

        if (strchr ("<=>", *format))
        {
            set_justify = 1;
            switch (*format)
            {
            case '<':
                justify = J_LEFT;
                break;
            case '=':
                justify = J_CENTER;
                break;
            case '>':
            default:
                justify = J_RIGHT;
                break;
            }
            format = skip_separators (format + 1);
        }
        else
            set_justify = 0;

        for (i = 0; panel_fields[i].id != NULL; i++)
        {
            size_t klen = strlen (panel_fields[i].id);

            if (strncmp (format, panel_fields[i].id, klen) != 0)
                continue;

            format += klen;

            darr->requested_field_len = panel_fields[i].min_size;
            darr->string_fn = panel_fields[i].string_fn;
            darr->title = panel_get_title_without_hotkey (panel_fields[i].title_hotkey);

            darr->id = panel_fields[i].id;
            darr->expand = panel_fields[i].expands;
            darr->just_mode = panel_fields[i].default_just;

            if (set_justify)
            {
                if (IS_FIT (darr->just_mode))
                    darr->just_mode = MAKE_FIT (justify);
                else
                    darr->just_mode = justify;
            }
            found = 1;

            format = skip_separators (format);

            /* If we have a size specifier */
            if (*format == ':')
            {
                int req_length;

                /* If the size was specified, we don't want
                 * auto-expansion by default
                 */
                darr->expand = 0;
                format++;
                req_length = atoi (format);
                darr->requested_field_len = req_length;

                format = skip_numbers (format);

                /* Now, if they insist on expansion */
                if (*format == '+')
                {
                    darr->expand = 1;
                    format++;
                }

            }

            break;
        }
        if (!found)
        {
            char *tmp_format = g_strdup (format);

            int pos = min (8, strlen (format));
            delete_format (home);
            tmp_format[pos] = 0;
            *error =
                g_strconcat (_("Unknown tag on display format:"), " ", tmp_format, (char *) NULL);
            g_free (tmp_format);
            return 0;
        }
        total_cols += darr->requested_field_len;
    }

    *res_total_cols = total_cols;
    return home;
}

/* --------------------------------------------------------------------------------------------- */

static format_e *
use_display_format (WPanel * panel, const char *format, char **error, int isstatus)
{
#define MAX_EXPAND 4
    int expand_top = 0;         /* Max used element in expand */
    int usable_columns;         /* Usable columns in the panel */
    int total_cols = 0;
    int i;
    format_e *darr, *home;

    if (!format)
        format = DEFAULT_USER_FORMAT;

    home = parse_display_format (panel, format, error, isstatus, &total_cols);

    if (*error)
        return 0;

    panel->dirty = 1;

    /* Status needn't to be split */
    usable_columns = ((WIDGET (panel)->cols - 2) / ((isstatus)
                                                    ? 1
                                                    : (panel->split + 1))) - (!isstatus
                                                                              && panel->split);

    /* Look for the expandable fields and set field_len based on the requested field len */
    for (darr = home; darr && expand_top < MAX_EXPAND; darr = darr->next)
    {
        darr->field_len = darr->requested_field_len;
        if (darr->expand)
            expand_top++;
    }

    /* If we used more columns than the available columns, adjust that */
    if (total_cols > usable_columns)
    {
        int pdif, dif = total_cols - usable_columns;

        while (dif)
        {
            pdif = dif;
            for (darr = home; darr; darr = darr->next)
            {
                if (dif && darr->field_len - 1)
                {
                    darr->field_len--;
                    dif--;
                }
            }

            /* avoid endless loop if num fields > 40 */
            if (pdif == dif)
                break;
        }
        total_cols = usable_columns;    /* give up, the rest should be truncated */
    }

    /* Expand the available space */
    if ((usable_columns > total_cols) && expand_top)
    {
        int spaces = (usable_columns - total_cols) / expand_top;
        int extra = (usable_columns - total_cols) % expand_top;

        for (i = 0, darr = home; darr && (i < expand_top); darr = darr->next)
            if (darr->expand)
            {
                darr->field_len += (spaces + ((i == 0) ? extra : 0));
                i++;
            }
    }
    return home;
}

/* --------------------------------------------------------------------------------------------- */
/** Given the panel->view_type returns the format string to be parsed */

static const char *
panel_format (WPanel * panel)
{
    switch (panel->list_type)
    {
    case list_long:
        return "full perm space nlink space owner space group space size space mtime space name";

    case list_brief:
        return "half 2 type name";

    case list_user:
        return panel->user_format;

    default:
    case list_full:
        return "half type name | size | mtime";
    }
}

/* --------------------------------------------------------------------------------------------- */

static const char *
mini_status_format (WPanel * panel)
{
    if (panel->user_mini_status)
        return panel->user_status_format[panel->list_type];

    switch (panel->list_type)
    {
    case list_long:
        return "full perm space nlink space owner space group space size space mtime space name";

    case list_brief:
        return "half type name space bsize space perm space";

    case list_full:
        return "half type name";

    default:
    case list_user:
        return panel->user_format;
    }
}

/*                          */
/* Panel operation commands */
/*                          */

/* --------------------------------------------------------------------------------------------- */
/** Used to emulate Lynx's entering leaving a directory with the arrow keys */

static cb_ret_t
maybe_cd (int move_up_dir)
{
    if (panels_options.navigate_with_arrows && (cmdline->buffer[0] == '\0'))
    {
        if (move_up_dir)
        {
            vfs_path_t *up_dir;

            up_dir = vfs_path_from_str ("..");
            do_cd (up_dir, cd_exact);
            vfs_path_free (up_dir);
            return MSG_HANDLED;
        }

        if (S_ISDIR (selection (current_panel)->st.st_mode)
            || link_isdir (selection (current_panel)))
        {
            vfs_path_t *vpath;

            vpath = vfs_path_from_str (selection (current_panel)->fname);
            do_cd (vpath, cd_exact);
            vfs_path_free (vpath);
            return MSG_HANDLED;
        }
    }
    return MSG_NOT_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

/* if command line is empty then do 'cd ..' */
static cb_ret_t
force_maybe_cd (void)
{
    if (cmdline->buffer[0] == '\0')
    {
        vfs_path_t *up_dir = vfs_path_from_str ("..");
        do_cd (up_dir, cd_exact);
        vfs_path_free (up_dir);
        return MSG_HANDLED;
    }
    return MSG_NOT_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static void
unselect_item (WPanel * panel)
{
    repaint_file (panel, panel->selected, 1, 2 * selection (panel)->f.marked, 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
move_down (WPanel * panel)
{
    if (panel->selected + 1 == panel->count)
        return;

    unselect_item (panel);
    panel->selected++;
    if (panels_options.scroll_pages && panel->selected - panel->top_file == ITEMS (panel))
    {
        /* Scroll window half screen */
        panel->top_file += ITEMS (panel) / 2;
        if (panel->top_file > panel->count - ITEMS (panel))
            panel->top_file = panel->count - ITEMS (panel);
        paint_dir (panel);
    }
    select_item (panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
move_up (WPanel * panel)
{
    if (panel->selected == 0)
        return;

    unselect_item (panel);
    panel->selected--;
    if (panels_options.scroll_pages && panel->selected < panel->top_file)
    {
        /* Scroll window half screen */
        panel->top_file -= ITEMS (panel) / 2;
        if (panel->top_file < 0)
            panel->top_file = 0;
        paint_dir (panel);
    }
    select_item (panel);
}

/* --------------------------------------------------------------------------------------------- */
/** Changes the selection by lines (may be negative) */

static void
move_selection (WPanel * panel, int lines)
{
    int new_pos;
    int adjust = 0;

    new_pos = panel->selected + lines;
    if (new_pos >= panel->count)
        new_pos = panel->count - 1;

    if (new_pos < 0)
        new_pos = 0;

    unselect_item (panel);
    panel->selected = new_pos;

    if (panel->selected - panel->top_file >= ITEMS (panel))
    {
        panel->top_file += lines;
        adjust = 1;
    }

    if (panel->selected - panel->top_file < 0)
    {
        panel->top_file += lines;
        adjust = 1;
    }

    if (adjust)
    {
        if (panel->top_file > panel->selected)
            panel->top_file = panel->selected;
        if (panel->top_file < 0)
            panel->top_file = 0;
        paint_dir (panel);
    }
    select_item (panel);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
move_left (WPanel * panel)
{
    if (panel->split)
    {
        move_selection (panel, -llines (panel));
        return MSG_HANDLED;
    }
    else
        return maybe_cd (1);    /* cd .. */
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
move_right (WPanel * panel)
{
    if (panel->split)
    {
        move_selection (panel, llines (panel));
        return MSG_HANDLED;
    }
    else
        return maybe_cd (0);    /* cd (selection) */
}

/* --------------------------------------------------------------------------------------------- */

static void
prev_page (WPanel * panel)
{
    int items;

    if (!panel->selected && !panel->top_file)
        return;
    unselect_item (panel);
    items = ITEMS (panel);
    if (panel->top_file < items)
        items = panel->top_file;
    if (!items)
        panel->selected = 0;
    else
        panel->selected -= items;
    panel->top_file -= items;

    select_item (panel);
    paint_dir (panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
goto_parent_dir (WPanel * panel)
{
    if (!panel->is_panelized)
    {
        vfs_path_t *up_dir;

        up_dir = vfs_path_from_str ("..");
        do_cd (up_dir, cd_exact);
        vfs_path_free (up_dir);
    }
    else
    {
        char *fname = panel->dir.list[panel->selected].fname;
        const char *bname;
        vfs_path_t *dname_vpath;

        if (g_path_is_absolute (fname))
            fname = g_strdup (fname);
        else
        {
            char *tmp_root;

            tmp_root = vfs_path_to_str (panelized_panel.root_vpath);
            fname = mc_build_filename (tmp_root, fname, (char *) NULL);
            g_free (tmp_root);
        }

        bname = x_basename (fname);

        if (bname == fname)
            dname_vpath = vfs_path_from_str (".");
        else
        {
            char *dname;

            dname = g_strndup (fname, bname - fname);
            dname_vpath = vfs_path_from_str (dname);
            g_free (dname);
        }

        do_cd (dname_vpath, cd_exact);
        try_to_select (panel, bname);

        vfs_path_free (dname_vpath);
        g_free (fname);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
next_page (WPanel * panel)
{
    int items;

    if (panel->selected == panel->count - 1)
        return;
    unselect_item (panel);
    items = ITEMS (panel);
    if (panel->top_file > panel->count - 2 * items)
        items = panel->count - items - panel->top_file;
    if (panel->top_file + items < 0)
        items = -panel->top_file;
    if (!items)
        panel->selected = panel->count - 1;
    else
        panel->selected += items;
    panel->top_file += items;

    select_item (panel);
    paint_dir (panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
goto_child_dir (WPanel * panel)
{
    if ((S_ISDIR (selection (panel)->st.st_mode) || link_isdir (selection (panel))))
    {
        vfs_path_t *vpath;

        vpath = vfs_path_from_str (selection (panel)->fname);
        do_cd (vpath, cd_exact);
        vfs_path_free (vpath);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
goto_top_file (WPanel * panel)
{
    unselect_item (panel);
    panel->selected = panel->top_file;
    select_item (panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
goto_middle_file (WPanel * panel)
{
    unselect_item (panel);
    panel->selected = panel->top_file + (ITEMS (panel) / 2);
    select_item (panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
goto_bottom_file (WPanel * panel)
{
    unselect_item (panel);
    panel->selected = panel->top_file + ITEMS (panel) - 1;
    select_item (panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
move_home (WPanel * panel)
{
    if (panel->selected == 0)
        return;

    unselect_item (panel);

    if (panels_options.torben_fj_mode)
    {
        int middle_pos = panel->top_file + (ITEMS (panel) / 2);

        if (panel->selected > middle_pos)
        {
            goto_middle_file (panel);
            return;
        }
        if (panel->selected != panel->top_file)
        {
            goto_top_file (panel);
            return;
        }
    }

    panel->top_file = 0;
    panel->selected = 0;

    paint_dir (panel);
    select_item (panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
move_end (WPanel * panel)
{
    if (panel->selected == panel->count - 1)
        return;

    unselect_item (panel);

    if (panels_options.torben_fj_mode)
    {
        int middle_pos = panel->top_file + (ITEMS (panel) / 2);

        if (panel->selected < middle_pos)
        {
            goto_middle_file (panel);
            return;
        }
        if (panel->selected != (panel->top_file + ITEMS (panel) - 1))
        {
            goto_bottom_file (panel);
            return;
        }
    }

    panel->selected = panel->count - 1;
    paint_dir (panel);
    select_item (panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
do_mark_file (WPanel * panel, mark_act_t do_move)
{
    do_file_mark (panel, panel->selected, selection (panel)->f.marked ? 0 : 1);
    if ((panels_options.mark_moves_down && do_move == MARK_DOWN) || do_move == MARK_FORCE_DOWN)
        move_down (panel);
    else if (do_move == MARK_FORCE_UP)
        move_up (panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
mark_file (WPanel * panel)
{
    do_mark_file (panel, MARK_DOWN);
}

/* --------------------------------------------------------------------------------------------- */

static void
mark_file_up (WPanel * panel)
{
    do_mark_file (panel, MARK_FORCE_UP);
}

/* --------------------------------------------------------------------------------------------- */

static void
mark_file_down (WPanel * panel)
{
    do_mark_file (panel, MARK_FORCE_DOWN);
}

/* --------------------------------------------------------------------------------------------- */

static void
mark_file_right (WPanel * panel)
{
    int lines = llines (panel);

    if (state_mark < 0)
        state_mark = selection (panel)->f.marked ? 0 : 1;

    lines = min (lines, panel->count - panel->selected - 1);
    for (; lines != 0; lines--)
    {
        do_file_mark (panel, panel->selected, state_mark);
        move_down (panel);
    }
    do_file_mark (panel, panel->selected, state_mark);
}

/* --------------------------------------------------------------------------------------------- */

static void
mark_file_left (WPanel * panel)
{
    int lines = llines (panel);

    if (state_mark < 0)
        state_mark = selection (panel)->f.marked ? 0 : 1;

    lines = min (lines, panel->selected + 1);
    for (; lines != 0; lines--)
    {
        do_file_mark (panel, panel->selected, state_mark);
        move_up (panel);
    }
    do_file_mark (panel, panel->selected, state_mark);
}

/* --------------------------------------------------------------------------------------------- */
/** Incremental search of a file name in the panel.
  * @param panel instance of WPanel structure
  * @param c_code key code
  */

static void
do_search (WPanel * panel, int c_code)
{
    size_t l;
    int i, sel;
    gboolean wrapped = FALSE;
    char *act;
    mc_search_t *search;
    char *reg_exp, *esc_str;
    gboolean is_found = FALSE;

    l = strlen (panel->search_buffer);
    if (c_code == KEY_BACKSPACE)
    {
        if (l != 0)
        {
            act = panel->search_buffer + l;
            str_prev_noncomb_char (&act, panel->search_buffer);
            act[0] = '\0';
        }
        panel->search_chpoint = 0;
    }
    else
    {
        if (c_code != 0 && (gsize) panel->search_chpoint < sizeof (panel->search_char))
        {
            panel->search_char[panel->search_chpoint] = c_code;
            panel->search_chpoint++;
        }

        if (panel->search_chpoint > 0)
        {
            switch (str_is_valid_char (panel->search_char, panel->search_chpoint))
            {
            case -2:
                return;
            case -1:
                panel->search_chpoint = 0;
                return;
            default:
                if (l + panel->search_chpoint < sizeof (panel->search_buffer))
                {
                    memcpy (panel->search_buffer + l, panel->search_char, panel->search_chpoint);
                    l += panel->search_chpoint;
                    *(panel->search_buffer + l) = '\0';
                    panel->search_chpoint = 0;
                }
            }
        }
    }

    reg_exp = g_strdup_printf ("%s*", panel->search_buffer);
    esc_str = strutils_escape (reg_exp, -1, ",|\\{}[]", TRUE);
    search = mc_search_new (esc_str, -1);
    search->search_type = MC_SEARCH_T_GLOB;
    search->is_entire_line = TRUE;
    switch (panels_options.qsearch_mode)
    {
    case QSEARCH_CASE_SENSITIVE:
        search->is_case_sensitive = TRUE;
        break;
    case QSEARCH_CASE_INSENSITIVE:
        search->is_case_sensitive = FALSE;
        break;
    default:
        search->is_case_sensitive = panel->sort_info.case_sensitive;
        break;
    }
    sel = panel->selected;
    for (i = panel->selected; !wrapped || i != panel->selected; i++)
    {
        if (i >= panel->count)
        {
            i = 0;
            if (wrapped)
                break;
            wrapped = TRUE;
        }
        if (mc_search_run (search, panel->dir.list[i].fname, 0, panel->dir.list[i].fnamelen, NULL))
        {
            sel = i;
            is_found = TRUE;
            break;
        }
    }
    if (is_found)
    {
        unselect_item (panel);
        panel->selected = sel;
        select_item (panel);
        widget_redraw (WIDGET (panel));
    }
    else if (c_code != KEY_BACKSPACE)
    {
        act = panel->search_buffer + l;
        str_prev_noncomb_char (&act, panel->search_buffer);
        act[0] = '\0';
    }
    mc_search_free (search);
    g_free (reg_exp);
    g_free (esc_str);
}

/* --------------------------------------------------------------------------------------------- */
/** Start new search.
  * @param panel instance of WPanel structure
  */

static void
start_search (WPanel * panel)
{
    if (panel->searching)
    {
        if (panel->selected + 1 == panel->count)
            panel->selected = 0;
        else
            move_down (panel);

        /* in case if there was no search string we need to recall
           previous string, with which we ended previous searching */
        if (panel->search_buffer[0] == '\0')
            g_strlcpy (panel->search_buffer, panel->prev_search_buffer,
                       sizeof (panel->search_buffer));

        do_search (panel, 0);
    }
    else
    {
        panel->searching = TRUE;
        panel->search_buffer[0] = '\0';
        panel->search_char[0] = '\0';
        panel->search_chpoint = 0;
        display_mini_info (panel);
        mc_refresh ();
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
stop_search (WPanel * panel)
{
    panel->searching = FALSE;

    /* if user had overrdied search string, we need to store it
       to the previous_search_buffer */
    if (panel->search_buffer[0] != '\0')
        g_strlcpy (panel->prev_search_buffer, panel->search_buffer,
                   sizeof (panel->prev_search_buffer));

    display_mini_info (panel);
}

/* --------------------------------------------------------------------------------------------- */
/** Return 1 if the Enter key has been processed, 0 otherwise */

static int
do_enter_on_file_entry (file_entry * fe)
{
    vfs_path_t *full_name_vpath;
    gboolean ok;

    /*
     * Directory or link to directory - change directory.
     * Try the same for the entries on which mc_lstat() has failed.
     */
    if (S_ISDIR (fe->st.st_mode) || link_isdir (fe) || (fe->st.st_mode == 0))
    {
        vfs_path_t *fname_vpath;

        fname_vpath = vfs_path_from_str (fe->fname);
        if (!do_cd (fname_vpath, cd_exact))
            message (D_ERROR, MSG_ERROR, _("Cannot change directory"));
        vfs_path_free (fname_vpath);
        return 1;
    }

    full_name_vpath = vfs_path_append_new (current_panel->cwd_vpath, fe->fname, NULL);

    /* Try associated command */
    if (regex_command (full_name_vpath, "Open") != 0)
    {
        vfs_path_free (full_name_vpath);
        return 1;
    }

    /* Check if the file is executable */
    full_name_vpath = vfs_path_append_new (current_panel->cwd_vpath, fe->fname, NULL);
    ok = (is_exe (fe->st.st_mode) && if_link_is_exe (full_name_vpath, fe));
    vfs_path_free (full_name_vpath);
    if (!ok)
        return 0;

    if (confirm_execute)
    {
        if (query_dialog
            (_("The Midnight Commander"),
             _("Do you really want to execute?"), D_NORMAL, 2, _("&Yes"), _("&No")) != 0)
            return 1;
    }

    if (!vfs_current_is_local ())
    {
        int ret;
        vfs_path_t *tmp_vpath;

        tmp_vpath = vfs_path_append_new (vfs_get_raw_current_dir (), fe->fname, NULL);
        ret = mc_setctl (tmp_vpath, VFS_SETCTL_RUN, NULL);
        vfs_path_free (tmp_vpath);
        /* We took action only if the dialog was shown or the execution
         * was successful */
        return confirm_execute || (ret == 0);
    }

    {
        char *tmp = name_quote (fe->fname, 0);
        char *cmd = g_strconcat (".", PATH_SEP_STR, tmp, (char *) NULL);
        g_free (tmp);
        shell_execute (cmd, 0);
        g_free (cmd);
    }

#ifdef HAVE_CHARSET
    mc_global.source_codepage = default_source_codepage;
#endif

    return 1;
}

/* --------------------------------------------------------------------------------------------- */

static int
do_enter (WPanel * panel)
{
    return do_enter_on_file_entry (selection (panel));
}

/* --------------------------------------------------------------------------------------------- */

static void
chdir_other_panel (WPanel * panel)
{
    const file_entry *entry = &panel->dir.list[panel->selected];

    vfs_path_t *new_dir_vpath;
    char *sel_entry = NULL;

    if (get_other_type () != view_listing)
    {
        set_display_type (get_other_index (), view_listing);
    }

    if (S_ISDIR (entry->st.st_mode) || entry->f.link_to_dir)
        new_dir_vpath = vfs_path_append_new (panel->cwd_vpath, entry->fname, NULL);
    else
    {
        new_dir_vpath = vfs_path_append_new (panel->cwd_vpath, "..", (char *) NULL);
        sel_entry = strrchr (vfs_path_get_last_path_str (panel->cwd_vpath), PATH_SEP);
    }

    change_panel ();
    do_cd (new_dir_vpath, cd_exact);
    vfs_path_free (new_dir_vpath);

    if (sel_entry)
        try_to_select (current_panel, sel_entry);
    change_panel ();

    move_down (panel);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Make the current directory of the current panel also the current
 * directory of the other panel.  Put the other panel to the listing
 * mode if needed.  If the current panel is panelized, the other panel
 * doesn't become panelized.
 */

static void
panel_sync_other (const WPanel * panel)
{
    if (get_other_type () != view_listing)
    {
        set_display_type (get_other_index (), view_listing);
    }

    do_panel_cd (other_panel, current_panel->cwd_vpath, cd_exact);

    /* try to select current filename on the other panel */
    if (!panel->is_panelized)
    {
        try_to_select (other_panel, selection (panel)->fname);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
chdir_to_readlink (WPanel * panel)
{
    vfs_path_t *new_dir_vpath;
    char buffer[MC_MAXPATHLEN], *p;
    int i;
    struct stat st;
    vfs_path_t *panel_fname_vpath;
    gboolean ok;

    if (get_other_type () != view_listing)
        return;

    if (!S_ISLNK (panel->dir.list[panel->selected].st.st_mode))
        return;

    i = readlink (selection (panel)->fname, buffer, MC_MAXPATHLEN - 1);
    if (i < 0)
        return;

    panel_fname_vpath = vfs_path_from_str (selection (panel)->fname);
    ok = (mc_stat (panel_fname_vpath, &st) >= 0);
    vfs_path_free (panel_fname_vpath);
    if (!ok)
        return;

    buffer[i] = 0;
    if (!S_ISDIR (st.st_mode))
    {
        p = strrchr (buffer, PATH_SEP);
        if (p && !p[1])
        {
            *p = 0;
            p = strrchr (buffer, PATH_SEP);
        }
        if (!p)
            return;
        p[1] = 0;
    }
    if (*buffer == PATH_SEP)
        new_dir_vpath = vfs_path_from_str (buffer);
    else
        new_dir_vpath = vfs_path_append_new (panel->cwd_vpath, buffer, NULL);

    change_panel ();
    do_cd (new_dir_vpath, cd_exact);
    vfs_path_free (new_dir_vpath);
    change_panel ();

    move_down (panel);
}

/* --------------------------------------------------------------------------------------------- */

static gsize
panel_get_format_field_count (WPanel * panel)
{
    format_e *format;
    gsize lc_index;
    for (lc_index = 0, format = panel->format; format != NULL; format = format->next, lc_index++);
    return lc_index;
}

/* --------------------------------------------------------------------------------------------- */
/**
   function return 0 if not found and REAL_INDEX+1 if found
 */

static gsize
panel_get_format_field_index_by_name (WPanel * panel, const char *name)
{
    format_e *format;
    gsize lc_index;

    for (lc_index = 1, format = panel->format;
         !(format == NULL || strcmp (format->title, name) == 0); format = format->next, lc_index++);
    if (format == NULL)
        lc_index = 0;

    return lc_index;
}

/* --------------------------------------------------------------------------------------------- */

static format_e *
panel_get_format_field_by_index (WPanel * panel, gsize lc_index)
{
    format_e *format;
    for (format = panel->format;
         !(format == NULL || lc_index == 0); format = format->next, lc_index--);
    return format;
}

/* --------------------------------------------------------------------------------------------- */

static const panel_field_t *
panel_get_sortable_field_by_format (WPanel * panel, gsize lc_index)
{
    const panel_field_t *pfield;
    format_e *format;

    format = panel_get_format_field_by_index (panel, lc_index);
    if (format == NULL)
        return NULL;
    pfield = panel_get_field_by_title (format->title);
    if (pfield == NULL)
        return NULL;
    if (pfield->sort_routine == NULL)
        return NULL;
    return pfield;
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_toggle_sort_order_prev (WPanel * panel)
{
    gsize lc_index, i;
    gchar *title;

    const panel_field_t *pfield = NULL;

    title = panel_get_title_without_hotkey (panel->sort_info.sort_field->title_hotkey);
    lc_index = panel_get_format_field_index_by_name (panel, title);
    g_free (title);

    if (lc_index > 1)
    {
        /* search for prev sortable column in panel format */
        for (i = lc_index - 1;
             i != 0 && (pfield = panel_get_sortable_field_by_format (panel, i - 1)) == NULL; i--);
    }

    if (pfield == NULL)
    {
        /* Sortable field not found. Try to search in each array */
        for (i = panel_get_format_field_count (panel);
             i != 0 && (pfield = panel_get_sortable_field_by_format (panel, i - 1)) == NULL; i--);
    }

    if (pfield != NULL)
    {
        panel->sort_info.sort_field = pfield;
        panel_set_sort_order (panel, pfield);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_toggle_sort_order_next (WPanel * panel)
{
    gsize lc_index, i;
    const panel_field_t *pfield = NULL;
    gsize format_field_count;
    gchar *title;

    format_field_count = panel_get_format_field_count (panel);
    title = panel_get_title_without_hotkey (panel->sort_info.sort_field->title_hotkey);
    lc_index = panel_get_format_field_index_by_name (panel, title);
    g_free (title);

    if (lc_index != 0 && lc_index != format_field_count)
    {
        /* search for prev sortable column in panel format */
        for (i = lc_index;
             i != format_field_count
             && (pfield = panel_get_sortable_field_by_format (panel, i)) == NULL; i++);
    }

    if (pfield == NULL)
    {
        /* Sortable field not found. Try to search in each array */
        for (i = 0;
             i != format_field_count
             && (pfield = panel_get_sortable_field_by_format (panel, i)) == NULL; i++);
    }

    if (pfield != NULL)
    {
        panel->sort_info.sort_field = pfield;
        panel_set_sort_order (panel, pfield);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_select_sort_order (WPanel * panel)
{
    const panel_field_t *sort_order;

    sort_order = sort_box (&panel->sort_info);
    if (sort_order != NULL)
    {
        panel->sort_info.sort_field = sort_order;
        panel_set_sort_order (panel, sort_order);
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * panel_content_scroll_left:
 * @param panel the pointer to the panel on which we operate
 *
 * scroll long filename to the left (decrement scroll pointer)
 *
 */

static void
panel_content_scroll_left (WPanel * panel)
{
    if (panel->content_shift > -1)
    {
        if (panel->content_shift > panel->max_shift)
            panel->content_shift = panel->max_shift;

        panel->content_shift--;
        show_dir (panel);
        paint_dir (panel);
    }

}

/* --------------------------------------------------------------------------------------------- */

/**
 * panel_content_scroll_right:
 * @param panel the pointer to the panel on which we operate
 *
 * scroll long filename to the right (increment scroll pointer)
 *
 */

static void
panel_content_scroll_right (WPanel * panel)
{
    if (panel->content_shift < 0 || panel->content_shift < panel->max_shift)
    {
        panel->content_shift++;
        show_dir (panel);
        paint_dir (panel);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_set_sort_type_by_id (WPanel * panel, const char *name)
{
    if (strcmp (panel->sort_info.sort_field->id, name) != 0)
    {
        const panel_field_t *sort_order;

        sort_order = panel_get_field_by_id (name);
        if (sort_order == NULL)
            return;
        panel->sort_info.sort_field = sort_order;
    }
    else
        panel->sort_info.reverse = !panel->sort_info.reverse;

    panel_set_sort_order (panel, panel->sort_info.sort_field);
}

/* --------------------------------------------------------------------------------------------- */
/**
 *  If we moved to the parent directory move the selection pointer to
 *  the old directory name; If we leave VFS dir, remove FS specificator.
 *
 *  You do _NOT_ want to add any vfs aware code here. <pavel@ucw.cz>
 */

static const char *
get_parent_dir_name (const char *cwd, const char *lwd)
{
    size_t llen, clen;
    const char *p;

    llen = strlen (lwd);
    clen = strlen (cwd);

    if (llen <= clen)
        return NULL;

    p = g_strrstr (lwd, VFS_PATH_URL_DELIMITER);

    if (p == NULL)
    {
        p = strrchr (lwd, PATH_SEP);

        if ((p != NULL)
            && (strncmp (cwd, lwd, (size_t) (p - lwd)) == 0)
            && (clen == (size_t) (p - lwd)
                || ((p == lwd) && (cwd[0] == PATH_SEP) && (cwd[1] == '\0'))))
            return (p + 1);

        return NULL;
    }

    /* skip VFS prefix */
    while (--p > lwd && *p != PATH_SEP)
        ;
    /* get last component */
    while (--p > lwd && *p != PATH_SEP)
        ;

    /* return last component */
    return (p != lwd || *p == PATH_SEP) ? p + 1 : p;
}

/* --------------------------------------------------------------------------------------------- */
/** Wrapper for do_subshell_chdir, check for availability of subshell */

static void
subshell_chdir (const vfs_path_t * vpath)
{
#ifdef ENABLE_SUBSHELL
    if (mc_global.tty.use_subshell && vfs_current_is_local ())
        do_subshell_chdir (vpath, FALSE, TRUE);
#else /* ENABLE_SUBSHELL */
    (void) vpath;
#endif /* ENABLE_SUBSHELL */
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Changes the current directory of the panel.
 * Don't record change in the directory history.
 */

static gboolean
_do_panel_cd (WPanel * panel, const vfs_path_t * new_dir_vpath, enum cd_enum cd_type)
{
    char *olddir;

    olddir = vfs_path_to_str (panel->cwd_vpath);

    /* Convert *new_path to a suitable pathname, handle ~user */
    if (cd_type == cd_parse_command)
    {
        const vfs_path_element_t *element;

        element = vfs_path_get_by_index (new_dir_vpath, 0);
        if (strcmp (element->path, "-") == 0)
            new_dir_vpath = panel->lwd_vpath;
    }

    if (mc_chdir (new_dir_vpath) == -1)
    {
        panel_set_cwd (panel, olddir);
        g_free (olddir);
        return FALSE;
    }

    /* Success: save previous directory, shutdown status of previous dir */
    panel_set_lwd (panel, olddir);
    input_free_completions (cmdline);

    vfs_path_free (panel->cwd_vpath);
    vfs_setup_cwd ();
    panel->cwd_vpath = vfs_path_clone (vfs_get_raw_current_dir ());

    vfs_release_path (olddir);

    subshell_chdir (panel->cwd_vpath);

    /* Reload current panel */
    panel_clean_dir (panel);

    {
        char *tmp_path;

        panel->count =
            do_load_dir (panel->cwd_vpath, &panel->dir, panel->sort_info.sort_field->sort_routine,
                         panel->sort_info.reverse, panel->sort_info.case_sensitive,
                         panel->sort_info.exec_first, panel->filter);
        tmp_path = vfs_path_to_str (panel->cwd_vpath);
        try_to_select (panel, get_parent_dir_name (tmp_path, olddir));
        g_free (tmp_path);
    }

    load_hint (0);
    panel->dirty = 1;
    update_xterm_title_path ();

    g_free (olddir);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
directory_history_next (WPanel * panel)
{
    gboolean ok;

    do
    {
        GList *next;

        ok = TRUE;
        next = g_list_next (panel->dir_history_current);
        if (next != NULL)
        {
            vfs_path_t *data_vpath;

            data_vpath = vfs_path_from_str ((char *) next->data);
            ok = _do_panel_cd (panel, data_vpath, cd_exact);
            vfs_path_free (data_vpath);
            panel->dir_history_current = next;
        }
        /* skip directories that present in history but absent in file system */
    }
    while (!ok);
}

/* --------------------------------------------------------------------------------------------- */

static void
directory_history_prev (WPanel * panel)
{
    gboolean ok;

    do
    {
        GList *prev;

        ok = TRUE;
        prev = g_list_previous (panel->dir_history_current);
        if (prev != NULL)
        {
            vfs_path_t *data_vpath;

            data_vpath = vfs_path_from_str ((char *) prev->data);
            ok = _do_panel_cd (panel, data_vpath, cd_exact);
            vfs_path_free (data_vpath);
            panel->dir_history_current = prev;
        }
        /* skip directories that present in history but absent in file system */
    }
    while (!ok);
}

/* --------------------------------------------------------------------------------------------- */

static void
directory_history_list (WPanel * panel)
{
    char *s;
    gboolean ok = FALSE;
    size_t pos;

    pos = g_list_position (panel->dir_history_current, panel->dir_history);

    s = history_show (&panel->dir_history, WIDGET (panel), pos);
    if (s != NULL)
    {
        vfs_path_t *s_vpath;

        s_vpath = vfs_path_from_str (s);
        ok = _do_panel_cd (panel, s_vpath, cd_exact);
        if (ok)
            directory_history_add (panel, panel->cwd_vpath);
        else
            message (D_ERROR, MSG_ERROR, _("Cannot change directory"));
        vfs_path_free (s_vpath);
        g_free (s);
    }

    if (!ok)
    {
        /* Since history is fully modified in history_show(), panel->dir_history actually
         * points to the invalid place. Try restore current postition here. */

        size_t i;

        panel->dir_history_current = panel->dir_history;

        for (i = 0; i <= pos; i++)
        {
            GList *prev;

            prev = g_list_previous (panel->dir_history_current);
            if (prev == NULL)
                break;

            panel->dir_history_current = prev;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
panel_execute_cmd (WPanel * panel, unsigned long command)
{
    int res = MSG_HANDLED;

    if (command != CK_Search)
        stop_search (panel);


    switch (command)
    {
    case CK_Up:
    case CK_Down:
    case CK_Left:
    case CK_Right:
    case CK_Bottom:
    case CK_Top:
    case CK_PageDown:
    case CK_PageUp:
        /* reset state of marks flag */
        state_mark = -1;
        break;
    }
    switch (command)
    {
    case CK_PanelOtherCd:
        chdir_other_panel (panel);
        break;
    case CK_PanelOtherCdLink:
        chdir_to_readlink (panel);
        break;
    case CK_CopySingle:
        copy_cmd_local ();
        break;
    case CK_DeleteSingle:
        delete_cmd_local ();
        break;
    case CK_Enter:
        do_enter (panel);
        break;
    case CK_ViewRaw:
        view_raw_cmd ();
        break;
    case CK_EditNew:
        edit_cmd_new ();
        break;
    case CK_MoveSingle:
        rename_cmd_local ();
        break;
    case CK_SelectInvert:
        select_invert_cmd ();
        break;
    case CK_Select:
        select_cmd ();
        break;
    case CK_Unselect:
        unselect_cmd ();
        break;
    case CK_PageDown:
        next_page (panel);
        break;
    case CK_PageUp:
        prev_page (panel);
        break;
    case CK_CdChild:
        goto_child_dir (panel);
        break;
    case CK_CdParent:
        goto_parent_dir (panel);
        break;
    case CK_History:
        directory_history_list (panel);
        break;
    case CK_HistoryNext:
        directory_history_next (panel);
        break;
    case CK_HistoryPrev:
        directory_history_prev (panel);
        break;
    case CK_BottomOnScreen:
        goto_bottom_file (panel);
        break;
    case CK_MiddleOnScreen:
        goto_middle_file (panel);
        break;
    case CK_TopOnScreen:
        goto_top_file (panel);
        break;
    case CK_Mark:
        mark_file (panel);
        break;
    case CK_MarkUp:
        mark_file_up (panel);
        break;
    case CK_MarkDown:
        mark_file_down (panel);
        break;
    case CK_MarkLeft:
        mark_file_left (panel);
        break;
    case CK_MarkRight:
        mark_file_right (panel);
        break;
    case CK_CdParentSmart:
        res = force_maybe_cd ();
        break;
    case CK_Up:
        move_up (panel);
        break;
    case CK_Down:
        move_down (panel);
        break;
    case CK_Left:
        res = move_left (panel);
        break;
    case CK_Right:
        res = move_right (panel);
        break;
    case CK_Bottom:
        move_end (panel);
        break;
    case CK_Top:
        move_home (panel);
        break;
#ifdef HAVE_CHARSET
    case CK_SelectCodepage:
        panel_change_encoding (panel);
        break;
#endif
    case CK_ScrollLeft:
        panel_content_scroll_left (panel);
        break;
    case CK_ScrollRight:
        panel_content_scroll_right (panel);
        break;
    case CK_Search:
        start_search (panel);
        break;
    case CK_SearchStop:
        break;
    case CK_PanelOtherSync:
        panel_sync_other (panel);
        break;
    case CK_Sort:
        panel_select_sort_order (panel);
        break;
    case CK_SortPrev:
        panel_toggle_sort_order_prev (panel);
        break;
    case CK_SortNext:
        panel_toggle_sort_order_next (panel);
        break;
    case CK_SortReverse:
        panel->sort_info.reverse = !panel->sort_info.reverse;
        panel_set_sort_order (panel, panel->sort_info.sort_field);
        break;
    case CK_SortByName:
        panel_set_sort_type_by_id (panel, "name");
        break;
    case CK_SortByExt:
        panel_set_sort_type_by_id (panel, "extension");
        break;
    case CK_SortBySize:
        panel_set_sort_type_by_id (panel, "size");
        break;
    case CK_SortByMTime:
        panel_set_sort_type_by_id (panel, "mtime");
        break;
    }
    return res;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
panel_key (WPanel * panel, int key)
{
    size_t i;

    if (is_abort_char (key))
    {
        stop_search (panel);
        return MSG_HANDLED;
    }

    if (panel->searching && ((key >= ' ' && key <= 255) || key == KEY_BACKSPACE))
    {
        do_search (panel, key);
        return MSG_HANDLED;
    }

    for (i = 0; panel_map[i].key != 0; i++)
        if (key == panel_map[i].key)
            return panel_execute_cmd (panel, panel_map[i].command);

    if (panels_options.torben_fj_mode && key == ALT ('h'))
    {
        goto_middle_file (panel);
        return MSG_HANDLED;
    }

    if (!command_prompt && ((key >= ' ' && key <= 255) || key == KEY_BACKSPACE))
    {
        start_search (panel);
        do_search (panel, key);
        return MSG_HANDLED;
    }

    return MSG_NOT_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
panel_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WPanel *panel = (WPanel *) w;
    WButtonBar *bb;

    switch (msg)
    {
    case MSG_INIT:
        /* subscribe to "history_load" event */
        mc_event_add (w->owner->event_group, MCEVENT_HISTORY_LOAD, panel_load_history, w, NULL);
        /* subscribe to "history_save" event */
        mc_event_add (w->owner->event_group, MCEVENT_HISTORY_SAVE, panel_save_history, w, NULL);
        return MSG_HANDLED;

    case MSG_DRAW:
        /* Repaint everything, including frame and separator */
        paint_frame (panel);    /* including show_dir */
        paint_dir (panel);
        mini_info_separator (panel);
        display_mini_info (panel);
        panel->dirty = 0;
        return MSG_HANDLED;

    case MSG_FOCUS:
        state_mark = -1;
        current_panel = panel;
        panel->active = 1;
        if (mc_chdir (panel->cwd_vpath) != 0)
        {
            char *cwd;

            cwd = vfs_path_to_str_flags (panel->cwd_vpath, 0, VPF_STRIP_PASSWORD);
            message (D_ERROR, MSG_ERROR, _("Cannot chdir to \"%s\"\n%s"),
                     cwd, unix_error_string (errno));
            g_free (cwd);
        }
        else
            subshell_chdir (panel->cwd_vpath);

        update_xterm_title_path ();
        select_item (panel);
        show_dir (panel);
        paint_dir (panel);
        panel->dirty = 0;

        bb = find_buttonbar (w->owner);
        midnight_set_buttonbar (bb);
        widget_redraw (WIDGET (bb));
        return MSG_HANDLED;

    case MSG_UNFOCUS:
        /* Janne: look at this for the multiple panel options */
        stop_search (panel);
        panel->active = 0;
        show_dir (panel);
        unselect_item (panel);
        return MSG_HANDLED;

    case MSG_KEY:
        return panel_key (panel, parm);

    case MSG_ACTION:
        return panel_execute_cmd (panel, parm);

    case MSG_DESTROY:
        /* unsubscribe from "history_load" event */
        mc_event_del (w->owner->event_group, MCEVENT_HISTORY_LOAD, panel_load_history, w);
        /* unsubscribe from "history_save" event */
        mc_event_del (w->owner->event_group, MCEVENT_HISTORY_SAVE, panel_save_history, w);
        panel_destroy (panel);
        free_my_statfs ();
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*                                     */
/* Panel mouse events support routines */
/*                                     */

static void
mouse_toggle_mark (WPanel * panel)
{
    do_mark_file (panel, MARK_DONT_MOVE);
    mouse_marking = selection (panel)->f.marked;
    mouse_mark_panel = current_panel;
}

/* --------------------------------------------------------------------------------------------- */

static void
mouse_set_mark (WPanel * panel)
{

    if (mouse_mark_panel == panel)
    {
        if (mouse_marking && !(selection (panel)->f.marked))
            do_mark_file (panel, MARK_DONT_MOVE);
        else if (!mouse_marking && (selection (panel)->f.marked))
            do_mark_file (panel, MARK_DONT_MOVE);
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mark_if_marking (WPanel * panel, Gpm_Event * event)
{
    if ((event->buttons & GPM_B_RIGHT) != 0)
    {
        if ((event->type & GPM_DOWN) != 0)
            mouse_toggle_mark (panel);
        else
            mouse_set_mark (panel);
        return TRUE;
    }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/** Determine which column was clicked, and sort the panel on
 * that column, or reverse sort on that column if already
 * sorted on that column.
 */

static void
mouse_sort_col (WPanel * panel, int x)
{
    int i;
    const char *lc_sort_name = NULL;
    panel_field_t *col_sort_format = NULL;
    format_e *format;
    gchar *title;

    for (i = 0, format = panel->format; format != NULL; format = format->next)
    {
        i += format->field_len;
        if (x < i + 1)
        {
            /* found column */
            lc_sort_name = format->title;
            break;
        }
    }

    if (lc_sort_name == NULL)
        return;

    for (i = 0; panel_fields[i].id != NULL; i++)
    {
        title = panel_get_title_without_hotkey (panel_fields[i].title_hotkey);
        if (!strcmp (lc_sort_name, title) && panel_fields[i].sort_routine)
        {
            col_sort_format = &panel_fields[i];
            g_free (title);
            break;
        }
        g_free (title);
    }

    if (col_sort_format == NULL)
        return;

    if (panel->sort_info.sort_field == col_sort_format)
    {
        /* reverse the sort if clicked column is already the sorted column */
        panel->sort_info.reverse = !panel->sort_info.reverse;
    }
    else
    {
        /* new sort is forced to be ascending */
        panel->sort_info.reverse = FALSE;
    }
    panel_set_sort_order (panel, col_sort_format);
}


/* --------------------------------------------------------------------------------------------- */
/**
 * Mouse callback of the panel minus repainting.
 */
static int
panel_event (Gpm_Event * event, void *data)
{
    WPanel *panel = (WPanel *) data;
    Widget *w = WIDGET (data);

    const int lines = llines (panel);
    const gboolean is_active = dlg_widget_active (panel);
    const gboolean mouse_down = (event->type & GPM_DOWN) != 0;
    Gpm_Event local;

    if (!mouse_global_in_widget (event, WIDGET (data)))
        return MOU_UNHANDLED;

    local = mouse_get_local (event, w);

    /* 1st line */
    if (local.y == 1)
    {
        /* "<" button */
        if (mouse_down && local.x == 2)
        {
            directory_history_prev (panel);
            goto finish;
        }

        /* ">" button */
        if (mouse_down && local.x == w->cols - 1)
        {
            directory_history_next (panel);
            goto finish;
        }

        /* "^" button */
        if (mouse_down && local.x >= w->cols - 4 && local.x <= w->cols - 2)
        {
            directory_history_list (panel);
            goto finish;
        }

        /* "." button show/hide hidden files */
        if (mouse_down && local.x == w->cols - 5)
        {
            send_message (midnight_dlg, NULL, MSG_ACTION, CK_ShowHidden, NULL);
            goto finish;
        }

        /* no other events on 1st line */
        return MOU_UNHANDLED;
    }

    /* sort on clicked column; don't handle wheel events */
    if (mouse_down && (local.buttons & (GPM_B_UP | GPM_B_DOWN)) == 0 && local.y == 2)
    {
        mouse_sort_col (panel, local.x);
        goto finish;
    }

    /* Mouse wheel events */
    if (mouse_down && (local.buttons & GPM_B_UP) != 0)
    {
        if (is_active)
        {
            if (panels_options.mouse_move_pages && (panel->top_file > 0))
                prev_page (panel);
            else                /* We are in first page */
                move_up (panel);
        }
        goto finish;
    }

    if (mouse_down && (local.buttons & GPM_B_DOWN) != 0)
    {
        if (is_active)
        {
            if (panels_options.mouse_move_pages && (panel->top_file + ITEMS (panel) < panel->count))
                next_page (panel);
            else                /* We are in last page */
                move_down (panel);
        }
        goto finish;
    }

    local.y -= 2;
    if ((local.type & (GPM_DOWN | GPM_DRAG)) != 0)
    {
        int my_index;

        if (!is_active)
            change_panel ();

        if (panel->top_file + local.y > panel->count)
            my_index = panel->count - 1;
        else
        {
            my_index = panel->top_file + local.y - 1;
            if (panel->split && (local.x > (w->cols - 2) / 2))
                my_index += llines (panel);

            if (my_index >= panel->count)
                my_index = panel->count - 1;
        }

        if (my_index != panel->selected)
        {
            unselect_item (panel);
            panel->selected = my_index;
            select_item (panel);
        }

        /* This one is new */
        mark_if_marking (panel, &local);
    }
    else if ((local.type & (GPM_UP | GPM_DOUBLE)) == (GPM_UP | GPM_DOUBLE) &&
             local.y > 0 && local.y <= lines)
        do_enter (panel);

  finish:
    if (panel->dirty)
        widget_redraw (w);

    return MOU_NORMAL;
}

/* --------------------------------------------------------------------------------------------- */

static void
reload_panelized (WPanel * panel)
{
    int i, j;
    dir_list *list = &panel->dir;

    if (panel != current_panel)
        (void) mc_chdir (panel->cwd_vpath);

    for (i = 0, j = 0; i < panel->count; i++)
    {
        vfs_path_t *vpath;

        if (list->list[i].f.marked)
        {
            /* Unmark the file in advance. In case the following mc_lstat
             * fails we are done, else we have to mark the file again
             * (Note: do_file_mark depends on a valid "list->list [i].buf").
             * IMO that's the best way to update the panel's summary status
             * -- Norbert
             */
            do_file_mark (panel, i, 0);
        }
        vpath = vfs_path_from_str (list->list[i].fname);
        if (mc_lstat (vpath, &list->list[i].st))
            g_free (list->list[i].fname);
        else
        {
            if (list->list[i].f.marked)
                do_file_mark (panel, i, 1);
            if (j != i)
                list->list[j] = list->list[i];
            j++;
        }
        vfs_path_free (vpath);
    }
    if (j == 0)
        panel->count = set_zero_dir (list) ? 1 : 0;
    else
        panel->count = j;

    if (panel != current_panel)
        (void) mc_chdir (current_panel->cwd_vpath);
}

/* --------------------------------------------------------------------------------------------- */

static void
update_one_panel_widget (WPanel * panel, panel_update_flags_t flags, const char *current_file)
{
    gboolean free_pointer;
    char *my_current_file = NULL;

    if ((flags & UP_RELOAD) != 0)
    {
        panel->is_panelized = FALSE;
        mc_setctl (panel->cwd_vpath, VFS_SETCTL_FLUSH, 0);
        memset (&(panel->dir_stat), 0, sizeof (panel->dir_stat));
    }

    /* If current_file == -1 (an invalid pointer) then preserve selection */
    free_pointer = current_file == UP_KEEPSEL;

    if (free_pointer)
    {
        my_current_file = g_strdup (panel->dir.list[panel->selected].fname);
        current_file = my_current_file;
    }

    if (panel->is_panelized)
        reload_panelized (panel);
    else
        panel_reload (panel);

    try_to_select (panel, current_file);
    panel->dirty = 1;

    if (free_pointer)
        g_free (my_current_file);
}

/* --------------------------------------------------------------------------------------------- */

static void
update_one_panel (int which, panel_update_flags_t flags, const char *current_file)
{
    if (get_display_type (which) == view_listing)
    {
        WPanel *panel;

        panel = (WPanel *) get_panel_widget (which);
        if (panel->is_panelized)
            flags &= ~UP_RELOAD;
        update_one_panel_widget (panel, flags, current_file);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
do_select (WPanel * panel, int i)
{
    if (i != panel->selected)
    {
        panel->dirty = 1;
        panel->selected = i;
        panel->top_file = panel->selected - (WIDGET (panel)->lines - 2) / 2;
        if (panel->top_file < 0)
            panel->top_file = 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
do_try_to_select (WPanel * panel, const char *name)
{
    int i;
    char *subdir;

    if (!name)
    {
        do_select (panel, 0);
        return;
    }

    /* We only want the last component of the directory,
     * and from this only the name without suffix.
     * Cut prefix if the panel is not panelized */

    if (panel->is_panelized)
        subdir = vfs_strip_suffix_from_filename (name);
    else
        subdir = vfs_strip_suffix_from_filename (x_basename (name));

    /* Search that subdir or filename without prefix (if not panelized panel), select it if found */
    for (i = 0; i < panel->count; i++)
    {
        if (strcmp (subdir, panel->dir.list[i].fname) == 0)
        {
            do_select (panel, i);
            g_free (subdir);
            return;
        }
    }

    /* Try to select a file near the file that is missing */
    if (panel->selected >= panel->count)
        do_select (panel, panel->count - 1);
    g_free (subdir);
}

/* --------------------------------------------------------------------------------------------- */

/* event callback */
static gboolean
event_update_panels (const gchar * event_group_name, const gchar * event_name,
                     gpointer init_data, gpointer data)
{
    (void) event_group_name;
    (void) event_name;
    (void) init_data;
    (void) data;

    update_panels (UP_RELOAD, UP_KEEPSEL);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* event callback */
static gboolean
panel_save_curent_file_to_clip_file (const gchar * event_group_name, const gchar * event_name,
                                     gpointer init_data, gpointer data)
{
    (void) event_group_name;
    (void) event_name;
    (void) init_data;
    (void) data;

    if (current_panel->marked == 0)
        mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_text_to_file",
                        (gpointer) selection (current_panel)->fname);
    else
    {
        int i;
        gboolean first = TRUE;
        char *flist = NULL;

        for (i = 0; i < current_panel->count; i++)
            if (current_panel->dir.list[i].f.marked != 0)
            {                   /* Skip the unmarked ones */
                if (first)
                {
                    flist = g_strdup (current_panel->dir.list[i].fname);
                    first = FALSE;
                }
                else
                {
                    /* Add empty lines after the file */
                    char *tmp;

                    tmp =
                        g_strconcat (flist, "\n", current_panel->dir.list[i].fname, (char *) NULL);
                    g_free (flist);
                    flist = tmp;
                }
            }

        mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_text_to_file", (gpointer) flist);
        g_free (flist);
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
try_to_select (WPanel * panel, const char *name)
{
    do_try_to_select (panel, name);
    select_item (panel);
}

/* --------------------------------------------------------------------------------------------- */

void
panel_clean_dir (WPanel * panel)
{
    int count = panel->count;

    panel->count = 0;
    panel->top_file = 0;
    panel->selected = 0;
    panel->marked = 0;
    panel->dirs_marked = 0;
    panel->total = 0;
    panel->searching = FALSE;
    panel->is_panelized = FALSE;
    panel->dirty = 1;
    panel->content_shift = -1;
    panel->max_shift = -1;

    clean_dir (&panel->dir, count);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Set Up panel's current dir object
 *
 * @param panel panel object
 * @param path_str string contain path
 */

void
panel_set_cwd (WPanel * panel, const char *path_str)
{
    vfs_path_free (panel->cwd_vpath);
    panel->cwd_vpath = vfs_path_from_str (path_str);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Set Up panel's last working dir object
 *
 * @param panel panel object
 * @param path_str string contain path
 */

void
panel_set_lwd (WPanel * panel, const char *path_str)
{
    vfs_path_free (panel->lwd_vpath);
    panel->lwd_vpath = vfs_path_from_str (path_str);
}

/* --------------------------------------------------------------------------------------------- */
/** Panel creation.
 * @param panel_name the name of the panel for setup retieving
 * @return new instance of WPanel
 */

WPanel *
panel_new (const char *panel_name)
{
    return panel_new_with_dir (panel_name, NULL);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Panel creation for specified directory.
 *
 * @param panel_name specifies the name of the panel for setup retieving
 * @param wpath the path of working panel directory. If path is NULL then panel will be created
 * for current directory
 *
 * @return new instance of WPanel
 */

WPanel *
panel_new_with_dir (const char *panel_name, const char *wpath)
{
    WPanel *panel;
    Widget *w;
    char *section;
    int i, err;
    char *curdir = NULL;

    panel = g_new0 (WPanel, 1);
    w = WIDGET (panel);
    /* No know sizes of the panel at startup */
    init_widget (w, 0, 0, 0, 0, panel_callback, panel_event);
    /* We do not want the cursor */
    widget_want_cursor (w, FALSE);

    if (wpath != NULL)
    {
        curdir = _vfs_get_cwd ();
        panel_set_cwd (panel, wpath);
    }
    else
    {
        vfs_setup_cwd ();
        panel->cwd_vpath = vfs_path_clone (vfs_get_raw_current_dir ());
    }

    panel_set_lwd (panel, ".");

    panel->hist_name = g_strconcat ("Dir Hist ", panel_name, (char *) NULL);
    /* directories history will be get later */

    panel->dir.list = g_new (file_entry, MIN_FILES);
    panel->dir.size = MIN_FILES;
    panel->active = 0;
    panel->filter = 0;
    panel->split = 0;
    panel->top_file = 0;
    panel->selected = 0;
    panel->marked = 0;
    panel->total = 0;
    panel->dirty = 1;
    panel->searching = FALSE;
    panel->dirs_marked = 0;
    panel->is_panelized = FALSE;
    panel->format = 0;
    panel->status_format = 0;
    panel->format_modified = 1;
    panel->content_shift = -1;
    panel->max_shift = -1;

    panel->panel_name = g_strdup (panel_name);
    panel->user_format = g_strdup (DEFAULT_USER_FORMAT);

#ifdef HAVE_CHARSET
    panel->codepage = SELECT_CHARSET_NO_TRANSLATE;
#endif

    for (i = 0; i < LIST_TYPES; i++)
        panel->user_status_format[i] = g_strdup (DEFAULT_USER_FORMAT);

    panel->search_buffer[0] = '\0';
    panel->prev_search_buffer[0] = '\0';
    panel->frame_size = frame_half;

    section = g_strconcat ("Temporal:", panel->panel_name, (char *) NULL);
    if (!mc_config_has_group (mc_main_config, section))
    {
        g_free (section);
        section = g_strdup (panel->panel_name);
    }
    panel_load_setup (panel, section);
    g_free (section);

    /* Load format strings */
    err = set_panel_formats (panel);
    if (err != 0)
        set_panel_formats (panel);

#ifdef HAVE_CHARSET
    {
        const vfs_path_element_t *path_element;

        path_element = vfs_path_get_by_index (panel->cwd_vpath, -1);
        if (path_element->encoding != NULL)
            panel->codepage = get_codepage_index (path_element->encoding);
    }
#endif

    if (mc_chdir (panel->cwd_vpath) != 0)
    {
#ifdef HAVE_CHARSET
        panel->codepage = SELECT_CHARSET_NO_TRANSLATE;
#endif
        vfs_setup_cwd ();
        vfs_path_free (panel->cwd_vpath);
        panel->cwd_vpath = vfs_path_clone (vfs_get_raw_current_dir ());
    }

    /* Load the default format */
    panel->count =
        do_load_dir (panel->cwd_vpath, &panel->dir, panel->sort_info.sort_field->sort_routine,
                     panel->sort_info.reverse, panel->sort_info.case_sensitive,
                     panel->sort_info.exec_first, panel->filter);

    /* Restore old right path */
    if (curdir != NULL)
    {
        vfs_path_t *vpath;

        vpath = vfs_path_from_str (curdir);
        err = mc_chdir (vpath);
        vfs_path_free (vpath);
    }
    g_free (curdir);

    return panel;
}

/* --------------------------------------------------------------------------------------------- */

void
panel_reload (WPanel * panel)
{
    struct stat current_stat;
    char *tmp_path;
    gboolean ok;

    tmp_path = vfs_path_to_str (panel->cwd_vpath);
    ok = (panels_options.fast_reload && stat (tmp_path, &current_stat) == 0
          && current_stat.st_ctime == panel->dir_stat.st_ctime
          && current_stat.st_mtime == panel->dir_stat.st_mtime);
    g_free (tmp_path);

    if (ok)
        return;

    do
    {
        char *last_slash;

        if (mc_chdir (panel->cwd_vpath) != -1)
            break;

        tmp_path = vfs_path_to_str (panel->cwd_vpath);
        if (tmp_path[0] == PATH_SEP && tmp_path[1] == '\0')
        {
            panel_clean_dir (panel);
            panel->count = set_zero_dir (&panel->dir) ? 1 : 0;
            g_free (tmp_path);
            return;
        }
        last_slash = strrchr (tmp_path, PATH_SEP);
        vfs_path_free (panel->cwd_vpath);
        if (last_slash == NULL || last_slash == tmp_path)
            panel->cwd_vpath = vfs_path_from_str (PATH_SEP_STR);
        else
        {
            *last_slash = '\0';
            panel->cwd_vpath = vfs_path_from_str (tmp_path);
        }
        g_free (tmp_path);
        memset (&(panel->dir_stat), 0, sizeof (panel->dir_stat));
        show_dir (panel);
    }
    while (TRUE);

    panel->count =
        do_reload_dir (panel->cwd_vpath, &panel->dir, panel->sort_info.sort_field->sort_routine,
                       panel->count, panel->sort_info.reverse, panel->sort_info.case_sensitive,
                       panel->sort_info.exec_first, panel->filter);

    panel->dirty = 1;
    if (panel->selected >= panel->count)
        do_select (panel, panel->count - 1);

    recalculate_panel_summary (panel);
}

/* --------------------------------------------------------------------------------------------- */
/* Switches the panel to the mode specified in the format           */
/* Seting up both format and status string. Return: 0 - on success; */
/* 1 - format error; 2 - status error; 3 - errors in both formats.  */

int
set_panel_formats (WPanel * p)
{
    format_e *form;
    char *err = NULL;
    int retcode = 0;

    form = use_display_format (p, panel_format (p), &err, 0);

    if (err != NULL)
    {
        g_free (err);
        retcode = 1;
    }
    else
    {
        delete_format (p->format);
        p->format = form;
    }

    if (panels_options.show_mini_info)
    {
        form = use_display_format (p, mini_status_format (p), &err, 1);

        if (err != NULL)
        {
            g_free (err);
            retcode += 2;
        }
        else
        {
            delete_format (p->status_format);
            p->status_format = form;
        }
    }

    panel_format_modified (p);
    panel_update_cols (WIDGET (p), p->frame_size);

    if (retcode)
        message (D_ERROR, _("Warning"),
                 _("User supplied format looks invalid, reverting to default."));
    if (retcode & 0x01)
    {
        g_free (p->user_format);
        p->user_format = g_strdup (DEFAULT_USER_FORMAT);
    }
    if (retcode & 0x02)
    {
        g_free (p->user_status_format[p->list_type]);
        p->user_status_format[p->list_type] = g_strdup (DEFAULT_USER_FORMAT);
    }

    return retcode;
}

/* --------------------------------------------------------------------------------------------- */

void
panel_update_cols (Widget * widget, panel_display_t frame_size)
{
    int cols, origin;

    /* don't touch panel if it is not in dialog yet */
    /* if panel is not in dialog it is not in widgets list
       and cannot be compared with get_panel_widget() result */
    if (widget->owner == NULL)
        return;

    if (panels_layout.horizontal_split)
    {
        widget->cols = COLS;
        return;
    }

    if (frame_size == frame_full)
    {
        cols = COLS;
        origin = 0;
    }
    else if (widget == get_panel_widget (0))
    {
        cols = panels_layout.left_panel_size;
        origin = 0;
    }
    else
    {
        cols = COLS - panels_layout.left_panel_size;
        origin = panels_layout.left_panel_size;
    }

    widget->cols = cols;
    widget->x = origin;
}

/* --------------------------------------------------------------------------------------------- */

/* Select current item and readjust the panel */
void
select_item (WPanel * panel)
{
    /* Although currently all over the code we set the selection and
       top file to decent values before calling select_item, I could
       forget it someday, so it's better to do the actual fitting here */

    if (panel->selected < 0)
        panel->selected = 0;

    if (panel->selected > panel->count - 1)
        panel->selected = panel->count - 1;

    adjust_top_file (panel);

    panel->dirty = 1;

    execute_hooks (select_file_hook);
}

/* --------------------------------------------------------------------------------------------- */
/** Clears all files in the panel, used only when one file was marked */
void
unmark_files (WPanel * panel)
{
    int i;

    if (!panel->marked)
        return;
    for (i = 0; i < panel->count; i++)
        file_mark (panel, i, 0);

    panel->dirs_marked = 0;
    panel->marked = 0;
    panel->total = 0;
}

/* --------------------------------------------------------------------------------------------- */
/** Recalculate the panels summary information, used e.g. when marked
   files might have been removed by an external command */

void
recalculate_panel_summary (WPanel * panel)
{
    int i;

    panel->marked = 0;
    panel->dirs_marked = 0;
    panel->total = 0;

    for (i = 0; i < panel->count; i++)
        if (panel->dir.list[i].f.marked)
        {
            /* do_file_mark will return immediately if newmark == oldmark.
               So we have to first unmark it to get panel's summary information
               updated. (Norbert) */
            panel->dir.list[i].f.marked = 0;
            do_file_mark (panel, i, 1);
        }
}

/* --------------------------------------------------------------------------------------------- */
/** This routine marks a file or a directory */

void
do_file_mark (WPanel * panel, int idx, int mark)
{
    if (panel->dir.list[idx].f.marked == mark)
        return;

    /* Only '..' can't be marked, '.' isn't visible */
    if (strcmp (panel->dir.list[idx].fname, "..") == 0)
        return;

    file_mark (panel, idx, mark);
    if (panel->dir.list[idx].f.marked)
    {
        panel->marked++;
        if (S_ISDIR (panel->dir.list[idx].st.st_mode))
        {
            if (panel->dir.list[idx].f.dir_size_computed)
                panel->total += (uintmax_t) panel->dir.list[idx].st.st_size;
            panel->dirs_marked++;
        }
        else
            panel->total += (uintmax_t) panel->dir.list[idx].st.st_size;
        set_colors (panel);
    }
    else
    {
        if (S_ISDIR (panel->dir.list[idx].st.st_mode))
        {
            if (panel->dir.list[idx].f.dir_size_computed)
                panel->total -= (uintmax_t) panel->dir.list[idx].st.st_size;
            panel->dirs_marked--;
        }
        else
            panel->total -= (uintmax_t) panel->dir.list[idx].st.st_size;
        panel->marked--;
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Changes the current directory of the panel.
 * Record change in the directory history.
 */
gboolean
do_panel_cd (struct WPanel *panel, const vfs_path_t * new_dir_vpath, enum cd_enum cd_type)
{
    gboolean r;

    r = _do_panel_cd (panel, new_dir_vpath, cd_type);
    if (r)
        directory_history_add (panel, panel->cwd_vpath);
    return r;
}

/* --------------------------------------------------------------------------------------------- */

void
file_mark (WPanel * panel, int lc_index, int val)
{
    if (panel->dir.list[lc_index].f.marked != val)
    {
        panel->dir.list[lc_index].f.marked = val;
        panel->dirty = 1;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
panel_re_sort (WPanel * panel)
{
    char *filename;
    int i;

    if (panel == NULL)
        return;

    filename = g_strdup (selection (panel)->fname);
    unselect_item (panel);
    do_sort (&panel->dir, panel->sort_info.sort_field->sort_routine, panel->count - 1,
             panel->sort_info.reverse, panel->sort_info.case_sensitive,
             panel->sort_info.exec_first);
    panel->selected = -1;
    for (i = panel->count; i; i--)
    {
        if (!strcmp (panel->dir.list[i - 1].fname, filename))
        {
            panel->selected = i - 1;
            break;
        }
    }
    g_free (filename);
    panel->top_file = panel->selected - ITEMS (panel) / 2;
    select_item (panel);
    panel->dirty = 1;
}

/* --------------------------------------------------------------------------------------------- */

void
panel_set_sort_order (WPanel * panel, const panel_field_t * sort_order)
{
    if (sort_order == NULL)
        return;

    panel->sort_info.sort_field = sort_order;

    /* The directory is already sorted, we have to load the unsorted stuff */
    if (sort_order->sort_routine == (sortfn *) unsorted)
    {
        char *current_file;

        current_file = g_strdup (panel->dir.list[panel->selected].fname);
        panel_reload (panel);
        try_to_select (panel, current_file);
        g_free (current_file);
    }
    panel_re_sort (panel);
}

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_CHARSET

/**
 * Change panel encoding.
 * @param panel WPanel object
 */

void
panel_change_encoding (WPanel * panel)
{
    const char *encoding = NULL;
    char *errmsg;
    int r;

    r = select_charset (-1, -1, panel->codepage, FALSE);

    if (r == SELECT_CHARSET_CANCEL)
        return;                 /* Cancel */

    panel->codepage = r;

    if (panel->codepage == SELECT_CHARSET_NO_TRANSLATE)
    {
        /* No translation */
        vfs_path_t *cd_path_vpath;

        g_free (init_translation_table (mc_global.display_codepage, mc_global.display_codepage));
        cd_path_vpath = remove_encoding_from_path (panel->cwd_vpath);
        do_panel_cd (panel, cd_path_vpath, cd_parse_command);
        show_dir (panel);
        vfs_path_free (cd_path_vpath);
        return;
    }

    errmsg = init_translation_table (panel->codepage, mc_global.display_codepage);
    if (errmsg != NULL)
    {
        message (D_ERROR, MSG_ERROR, "%s", errmsg);
        g_free (errmsg);
        return;
    }

    encoding = get_codepage_id (panel->codepage);
    if (encoding != NULL)
    {
        char *cd_path;
        vfs_change_encoding (panel->cwd_vpath, encoding);

        cd_path = vfs_path_to_str (panel->cwd_vpath);
        if (!do_panel_cd (panel, panel->cwd_vpath, cd_parse_command))
            message (D_ERROR, MSG_ERROR, _("Cannot chdir to \"%s\""), cd_path);
        g_free (cd_path);
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Remove encode info from last path element.
 *
 */
vfs_path_t *
remove_encoding_from_path (const vfs_path_t * vpath)
{
    vfs_path_t *ret_vpath;
    GString *tmp_conv;
    int indx;

    ret_vpath = vfs_path_new ();

    tmp_conv = g_string_new ("");

    for (indx = 0; indx < vfs_path_elements_count (vpath); indx++)
    {
        GIConv converter;
        vfs_path_element_t *path_element;

        path_element = vfs_path_element_clone (vfs_path_get_by_index (vpath, indx));
        vfs_path_add_element (ret_vpath, path_element);

        if (path_element->encoding == NULL)
            continue;

        converter = str_crt_conv_to (path_element->encoding);
        if (converter == INVALID_CONV)
            continue;

        g_free (path_element->encoding);
        path_element->encoding = NULL;

        str_vfs_convert_from (converter, path_element->path, tmp_conv);

        g_free (path_element->path);
        path_element->path = g_strndup (tmp_conv->str, tmp_conv->len);

        g_string_set_size (tmp_conv, 0);

        str_close_conv (converter);
        str_close_conv (path_element->dir.converter);
        path_element->dir.converter = INVALID_CONV;
    }
    g_string_free (tmp_conv, TRUE);
    return ret_vpath;
}
#endif /* HAVE_CHARSET */

/* --------------------------------------------------------------------------------------------- */

/**
 * This routine reloads the directory in both panels. It tries to
 * select current_file in current_panel and other_file in other_panel.
 * If current_file == -1 then it automatically sets current_file and
 * other_file to the currently selected files in the panels.
 *
 * if force_update has the UP_ONLY_CURRENT bit toggled on, then it
 * will not reload the other panel.
 *
 * @param flags for reload panel
 * @param current_file name of the current file
 */

void
update_panels (panel_update_flags_t flags, const char *current_file)
{
    gboolean reload_other = (flags & UP_ONLY_CURRENT) == 0;
    WPanel *panel;

    update_one_panel (get_current_index (), flags, current_file);
    if (reload_other)
        update_one_panel (get_other_index (), flags, UP_KEEPSEL);

    if (get_current_type () == view_listing)
        panel = (WPanel *) get_panel_widget (get_current_index ());
    else
        panel = (WPanel *) get_panel_widget (get_other_index ());

    if (!panel->is_panelized)
        (void) mc_chdir (panel->cwd_vpath);
}

/* --------------------------------------------------------------------------------------------- */

gsize
panel_get_num_of_sortable_fields (void)
{
    gsize ret = 0, lc_index;

    for (lc_index = 0; panel_fields[lc_index].id != NULL; lc_index++)
        if (panel_fields[lc_index].is_user_choice)
            ret++;
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

const char **
panel_get_sortable_fields (gsize * array_size)
{
    char **ret;
    gsize lc_index, i;

    lc_index = panel_get_num_of_sortable_fields ();

    ret = g_try_new0 (char *, lc_index + 1);
    if (ret == NULL)
        return NULL;

    if (array_size != NULL)
        *array_size = lc_index;

    lc_index = 0;

    for (i = 0; panel_fields[i].id != NULL; i++)
        if (panel_fields[i].is_user_choice)
            ret[lc_index++] = g_strdup (_(panel_fields[i].title_hotkey));
    return (const char **) ret;
}

/* --------------------------------------------------------------------------------------------- */

const panel_field_t *
panel_get_field_by_id (const char *name)
{
    gsize lc_index;
    for (lc_index = 0; panel_fields[lc_index].id != NULL; lc_index++)
        if (panel_fields[lc_index].id != NULL && strcmp (name, panel_fields[lc_index].id) == 0)
            return &panel_fields[lc_index];
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

const panel_field_t *
panel_get_field_by_title_hotkey (const char *name)
{
    gsize lc_index;
    for (lc_index = 0; panel_fields[lc_index].id != NULL; lc_index++)
        if (panel_fields[lc_index].title_hotkey != NULL &&
            strcmp (name, _(panel_fields[lc_index].title_hotkey)) == 0)
            return &panel_fields[lc_index];
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

const panel_field_t *
panel_get_field_by_title (const char *name)
{
    gsize lc_index;
    gchar *title = NULL;

    for (lc_index = 0; panel_fields[lc_index].id != NULL; lc_index++)
    {
        title = panel_get_title_without_hotkey (panel_fields[lc_index].title_hotkey);
        if (panel_fields[lc_index].title_hotkey != NULL && strcmp (name, title) == 0)
        {
            g_free (title);
            return &panel_fields[lc_index];
        }
    }
    g_free (title);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

gsize
panel_get_num_of_user_possible_fields (void)
{
    gsize ret = 0, lc_index;

    for (lc_index = 0; panel_fields[lc_index].id != NULL; lc_index++)
        if (panel_fields[lc_index].use_in_user_format)
            ret++;
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

const char **
panel_get_user_possible_fields (gsize * array_size)
{
    char **ret;
    gsize lc_index, i;

    lc_index = panel_get_num_of_user_possible_fields ();

    ret = g_try_new0 (char *, lc_index + 1);
    if (ret == NULL)
        return NULL;

    if (array_size != NULL)
        *array_size = lc_index;

    lc_index = 0;

    for (i = 0; panel_fields[i].id != NULL; i++)
        if (panel_fields[i].use_in_user_format)
            ret[lc_index++] = g_strdup (_(panel_fields[i].title_hotkey));
    return (const char **) ret;
}

/* --------------------------------------------------------------------------------------------- */

void
panel_init (void)
{
    panel_sort_up_sign = mc_skin_get ("widget-common", "sort-sign-up", "'");
    panel_sort_down_sign = mc_skin_get ("widget-common", "sort-sign-down", ",");

    panel_hiddenfiles_sign_show = mc_skin_get ("widget-panel", "hiddenfiles-sign-show", ".");
    panel_hiddenfiles_sign_hide = mc_skin_get ("widget-panel", "hiddenfiles-sign-hide", ".");
    panel_history_prev_item_sign = mc_skin_get ("widget-panel", "history-prev-item-sign", "<");
    panel_history_next_item_sign = mc_skin_get ("widget-panel", "history-next-item-sign", ">");
    panel_history_show_list_sign = mc_skin_get ("widget-panel", "history-show-list-sign", "^");
    panel_filename_scroll_left_char =
        mc_skin_get ("widget-panel", "filename-scroll-left-char", "{");
    panel_filename_scroll_right_char =
        mc_skin_get ("widget-panel", "filename-scroll-right-char", "}");

    mc_event_add (MCEVENT_GROUP_FILEMANAGER, "update_panels", event_update_panels, NULL, NULL);
    mc_event_add (MCEVENT_GROUP_FILEMANAGER, "panel_save_curent_file_to_clip_file",
                  panel_save_curent_file_to_clip_file, NULL, NULL);

}

/* --------------------------------------------------------------------------------------------- */

void
panel_deinit (void)
{
    g_free (panel_sort_up_sign);
    g_free (panel_sort_down_sign);

    g_free (panel_hiddenfiles_sign_show);
    g_free (panel_hiddenfiles_sign_hide);
    g_free (panel_history_prev_item_sign);
    g_free (panel_history_next_item_sign);
    g_free (panel_history_show_list_sign);
    g_free (panel_filename_scroll_left_char);
    g_free (panel_filename_scroll_right_char);

}

/* --------------------------------------------------------------------------------------------- */

gboolean
do_cd (const vfs_path_t * new_dir_vpath, enum cd_enum exact)
{
    gboolean res;
    const vfs_path_t *_new_dir_vpath = new_dir_vpath;

    if (current_panel->is_panelized)
    {
        size_t new_vpath_len;

        new_vpath_len = vfs_path_len (new_dir_vpath);
        if (vfs_path_equal_len (new_dir_vpath, panelized_panel.root_vpath, new_vpath_len))
            _new_dir_vpath = panelized_panel.root_vpath;
    }

    res = do_panel_cd (current_panel, _new_dir_vpath, exact);

#ifdef HAVE_CHARSET
    if (res)
    {
        const vfs_path_element_t *path_element;

        path_element = vfs_path_get_by_index (current_panel->cwd_vpath, -1);
        if (path_element->encoding != NULL)
            current_panel->codepage = get_codepage_index (path_element->encoding);
        else
            current_panel->codepage = SELECT_CHARSET_NO_TRANSLATE;
    }
#endif /* HAVE_CHARSET */

    return res;
}

/* --------------------------------------------------------------------------------------------- */
