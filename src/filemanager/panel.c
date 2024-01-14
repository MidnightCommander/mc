/*
   Panel managing.

   Copyright (C) 1994-2024
   Free Software Foundation, Inc.

   Written by:
   Miguel de Icaza, 1995
   Timur Bakeyev, 1997, 1999
   Slava Zanko <slavazanko@gmail.com>, 2013
   Andrew Borodin <aborodin@vmail.ru>, 2013-2023

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/key.h"        /* XCTRL and ALT macros  */
#include "lib/skin.h"
#include "lib/strescape.h"
#include "lib/mcconfig.h"
#include "lib/vfs/vfs.h"
#include "lib/unixcompat.h"
#include "lib/search.h"
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
#include "src/keymap.h"         /* global_keymap_t */
#include "src/history.h"
#ifdef ENABLE_SUBSHELL
#include "src/subshell/subshell.h"      /* do_subshell_chdir() */
#endif

#include "src/usermenu.h"

#include "dir.h"
#include "boxes.h"
#include "tree.h"
#include "ext.h"                /* regexp_command */
#include "layout.h"             /* Most layout variables are here */
#include "cmd.h"
#include "command.h"            /* cmdline */
#include "filemanager.h"
#include "mountlist.h"          /* my_statfs */
#include "cd.h"                 /* cd_error_message() */

#include "panel.h"

/*** global variables ****************************************************************************/

/* The hook list for the select file function */
hook_t *select_file_hook = NULL;

mc_fhl_t *mc_filehighlight = NULL;

/*** file scope macro definitions ****************************************************************/

typedef enum
{
    FATTR_NORMAL = 0,
    FATTR_CURRENT,
    FATTR_MARKED,
    FATTR_MARKED_CURRENT,
    FATTR_STATUS
} file_attr_t;

/* select/unselect dialog results */
#define SELECT_RESET ((mc_search_t *)(-1))
#define SELECT_ERROR ((mc_search_t *)(-2))

/* mouse position relative to file list */
#define MOUSE_UPPER_FILE_LIST (-1)
#define MOUSE_BELOW_FILE_LIST (-2)
#define MOUSE_AFTER_LAST_FILE (-3)

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
 * the user specified format and creates a linked list of format_item_t structures.
 */
typedef struct format_item_t
{
    int requested_field_len;
    int field_len;
    align_crt_t just_mode;
    gboolean expand;
    const char *(*string_fn) (file_entry_t *, int len);
    char *title;
    const char *id;
} format_item_t;

/* File name scroll states */
typedef enum
{
    FILENAME_NOSCROLL = 1,
    FILENAME_SCROLL_LEFT = 2,
    FILENAME_SCROLL_RIGHT = 4
} filename_scroll_flag_t;

/*** forward declarations (file scope functions) *************************************************/

static const char *string_file_name (file_entry_t * fe, int len);
static const char *string_file_size (file_entry_t * fe, int len);
static const char *string_file_size_brief (file_entry_t * fe, int len);
static const char *string_file_type (file_entry_t * fe, int len);
static const char *string_file_mtime (file_entry_t * fe, int len);
static const char *string_file_atime (file_entry_t * fe, int len);
static const char *string_file_ctime (file_entry_t * fe, int len);
static const char *string_file_permission (file_entry_t * fe, int len);
static const char *string_file_perm_octal (file_entry_t * fe, int len);
static const char *string_file_nlinks (file_entry_t * fe, int len);
static const char *string_inode (file_entry_t * fe, int len);
static const char *string_file_nuid (file_entry_t * fe, int len);
static const char *string_file_ngid (file_entry_t * fe, int len);
static const char *string_file_owner (file_entry_t * fe, int len);
static const char *string_file_group (file_entry_t * fe, int len);
static const char *string_marked (file_entry_t * fe, int len);
static const char *string_space (file_entry_t * fe, int len);
static const char *string_dot (file_entry_t * fe, int len);

/*** file scope variables ************************************************************************/

/* *INDENT-OFF* */
static panel_field_t panel_fields[] = {
    {
     "unsorted", 12, TRUE, J_LEFT_FIT,
     /* TRANSLATORS: one single character to represent 'unsorted' sort mode  */
     /* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
     N_("sort|u"),
     N_("&Unsorted"), TRUE, FALSE,
     string_file_name,
     (GCompareFunc) unsorted
    }
    ,
    {
     "name", 12, TRUE, J_LEFT_FIT,
     /* TRANSLATORS: one single character to represent 'name' sort mode  */
     /* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
     N_("sort|n"),
     N_("&Name"), TRUE, TRUE,
     string_file_name,
     (GCompareFunc) sort_name
    }
    ,
    {
     "version", 12, TRUE, J_LEFT_FIT,
     /* TRANSLATORS: one single character to represent 'version' sort mode  */
     /* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
     N_("sort|v"),
     N_("&Version"), TRUE, FALSE,
     string_file_name,
     (GCompareFunc) sort_vers
    }
    ,
    {
     "extension", 12, TRUE, J_LEFT_FIT,
     /* TRANSLATORS: one single character to represent 'extension' sort mode  */
     /* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
     N_("sort|e"),
     N_("E&xtension"), TRUE, FALSE,
     string_file_name,          /* TODO: string_file_ext */
     (GCompareFunc) sort_ext
    }
    ,
    {
     "size", 7, FALSE, J_RIGHT,
     /* TRANSLATORS: one single character to represent 'size' sort mode  */
     /* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
     N_("sort|s"),
     N_("&Size"), TRUE, TRUE,
     string_file_size,
     (GCompareFunc) sort_size
    }
    ,
    {
     "bsize", 7, FALSE, J_RIGHT,
     "",
     N_("Block Size"), FALSE, FALSE,
     string_file_size_brief,
     (GCompareFunc) sort_size
    }
    ,
    {
     "type", 1, FALSE, J_LEFT,
     "",
     "", FALSE, TRUE,
     string_file_type,
     NULL
    }
    ,
    {
     "mtime", 12, FALSE, J_RIGHT,
     /* TRANSLATORS: one single character to represent 'Modify time' sort mode  */
     /* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
     N_("sort|m"),
     N_("&Modify time"), TRUE, TRUE,
     string_file_mtime,
     (GCompareFunc) sort_time
    }
    ,
    {
     "atime", 12, FALSE, J_RIGHT,
     /* TRANSLATORS: one single character to represent 'Access time' sort mode  */
     /* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
     N_("sort|a"),
     N_("&Access time"), TRUE, TRUE,
     string_file_atime,
     (GCompareFunc) sort_atime
    }
    ,
    {
     "ctime", 12, FALSE, J_RIGHT,
     /* TRANSLATORS: one single character to represent 'Change time' sort mode  */
     /* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
     N_("sort|h"),
     N_("C&hange time"), TRUE, TRUE,
     string_file_ctime,
     (GCompareFunc) sort_ctime
    }
    ,
    {
     "perm", 10, FALSE, J_LEFT,
     "",
     N_("Permission"), FALSE, TRUE,
     string_file_permission,
     NULL
    }
    ,
    {
     "mode", 6, FALSE, J_RIGHT,
     "",
     N_("Perm"), FALSE, TRUE,
     string_file_perm_octal,
     NULL
    }
    ,
    {
     "nlink", 2, FALSE, J_RIGHT,
     "",
     N_("Nl"), FALSE, TRUE,
     string_file_nlinks, NULL
    }
    ,
    {
     "inode", 5, FALSE, J_RIGHT,
     /* TRANSLATORS: one single character to represent 'inode' sort mode  */
     /* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
     N_("sort|i"),
     N_("&Inode"), TRUE, TRUE,
     string_inode,
     (GCompareFunc) sort_inode
    }
    ,
    {
     "nuid", 5, FALSE, J_RIGHT,
     "",
     N_("UID"), FALSE, FALSE,
     string_file_nuid,
     NULL
    }
    ,
    {
     "ngid", 5, FALSE, J_RIGHT,
     "",
     N_("GID"), FALSE, FALSE,
     string_file_ngid,
     NULL
    }
    ,
    {
     "owner", 8, FALSE, J_LEFT_FIT,
     "",
     N_("Owner"), FALSE, TRUE,
     string_file_owner,
     NULL
    }
    ,
    {
     "group", 8, FALSE, J_LEFT_FIT,
     "",
     N_("Group"), FALSE, TRUE,
     string_file_group,
     NULL
    }
    ,
    {
     "mark", 1, FALSE, J_RIGHT,
     "",
     " ", FALSE, TRUE,
     string_marked,
     NULL
    }
    ,
    {
     "|", 1, FALSE, J_RIGHT,
     "",
     " ", FALSE, TRUE,
     NULL,
     NULL
    }
    ,
    {
     "space", 1, FALSE, J_RIGHT,
     "",
     " ", FALSE, TRUE,
     string_space,
     NULL
    }
    ,
    {
     "dot", 1, FALSE, J_RIGHT,
     "",
     " ", FALSE, FALSE,
     string_dot,
     NULL
    }
    ,
    {
     NULL, 0, FALSE, J_RIGHT, NULL, NULL, FALSE, FALSE, NULL, NULL
    }
};
/* *INDENT-ON* */

static char *panel_sort_up_char = NULL;
static char *panel_sort_down_char = NULL;

static char *panel_hiddenfiles_show_char = NULL;
static char *panel_hiddenfiles_hide_char = NULL;
static char *panel_history_prev_item_char = NULL;
static char *panel_history_next_item_char = NULL;
static char *panel_history_show_list_char = NULL;
static char *panel_filename_scroll_left_char = NULL;
static char *panel_filename_scroll_right_char = NULL;

/* Panel that selection started */
static WPanel *mouse_mark_panel = NULL;

static gboolean mouse_marking = FALSE;
static int state_mark = 0;

static GString *string_file_name_buffer;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static panelized_descr_t *
panelized_descr_new (void)
{
    panelized_descr_t *p;

    p = g_new0 (panelized_descr_t, 1);
    p->list.len = -1;

    return p;
}

/* --------------------------------------------------------------------------------------------- */

static void
panelized_descr_free (panelized_descr_t * p)
{
    if (p != NULL)
    {
        dir_list_free_list (&p->list);
        vfs_path_free (p->root_vpath, TRUE);
        g_free (p);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
set_colors (const WPanel * panel)
{
    (void) panel;

    tty_set_normal_attrs ();
    tty_setcolor (NORMAL_COLOR);
}

/* --------------------------------------------------------------------------------------------- */
/** Delete format_item_t object */

static void
format_item_free (format_item_t * format)
{
    g_free (format->title);
    g_free (format);
}

/* --------------------------------------------------------------------------------------------- */
/** Extract the number of available lines in a panel */

static int
panel_lines (const WPanel * p)
{
    /* 3 lines are: top frame, column header, button frame */
    return (CONST_WIDGET (p)->rect.lines - 3 - (panels_options.show_mini_info ? 2 : 0));
}

/* --------------------------------------------------------------------------------------------- */
/** This code relies on the default justification!!! */

static void
add_permission_string (const char *dest, int width, file_entry_t * fe, file_attr_t attr, int color,
                       gboolean is_octal)
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
            if (attr == FATTR_CURRENT || attr == FATTR_MARKED_CURRENT)
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
string_file_name (file_entry_t * fe, int len)
{
    (void) len;

    mc_g_string_copy (string_file_name_buffer, fe->fname);

    return string_file_name_buffer->str;
}

/* --------------------------------------------------------------------------------------------- */

static unsigned int
ilog10 (dev_t n)
{
    unsigned int digits = 0;

    do
    {
        digits++;
        n /= 10;
    }
    while (n != 0);

    return digits;
}

/* --------------------------------------------------------------------------------------------- */

static void
format_device_number (char *buf, size_t bufsize, dev_t dev)
{
    dev_t major_dev, minor_dev;
    unsigned int major_digits, minor_digits;

    major_dev = major (dev);
    major_digits = ilog10 (major_dev);

    minor_dev = minor (dev);
    minor_digits = ilog10 (minor_dev);

    g_assert (bufsize >= 1);

    if (major_digits + 1 + minor_digits + 1 <= bufsize)
        g_snprintf (buf, bufsize, "%lu,%lu", (unsigned long) major_dev, (unsigned long) minor_dev);
    else
        g_strlcpy (buf, _("[dev]"), bufsize);
}

/* --------------------------------------------------------------------------------------------- */
/** size */

static const char *
string_file_size (file_entry_t * fe, int len)
{
    static char buffer[BUF_TINY];

    /* Don't ever show size of ".." since we don't calculate it */
    if (DIR_IS_DOTDOT (fe->fname->str))
        return _("UP--DIR");

#ifdef HAVE_STRUCT_STAT_ST_RDEV
    if (S_ISBLK (fe->st.st_mode) || S_ISCHR (fe->st.st_mode))
        format_device_number (buffer, len + 1, fe->st.st_rdev);
    else
#endif
        size_trunc_len (buffer, (unsigned int) len, fe->st.st_size, 0, panels_options.kilobyte_si);

    return buffer;
}

/* --------------------------------------------------------------------------------------------- */
/** bsize */

static const char *
string_file_size_brief (file_entry_t * fe, int len)
{
    if (S_ISLNK (fe->st.st_mode) && !link_isdir (fe))
        return _("SYMLINK");

    if ((S_ISDIR (fe->st.st_mode) || link_isdir (fe)) && !DIR_IS_DOTDOT (fe->fname->str))
        return _("SUB-DIR");

    return string_file_size (fe, len);
}

/* --------------------------------------------------------------------------------------------- */
/** This functions return a string representation of a file entry type */

static const char *
string_file_type (file_entry_t * fe, int len)
{
    static char buffer[2];

    (void) len;

    if (S_ISDIR (fe->st.st_mode))
        buffer[0] = PATH_SEP;
    else if (S_ISLNK (fe->st.st_mode))
    {
        if (link_isdir (fe))
            buffer[0] = '~';
        else if (fe->f.stale_link != 0)
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
string_file_mtime (file_entry_t * fe, int len)
{
    (void) len;

    return file_date (fe->st.st_mtime);
}

/* --------------------------------------------------------------------------------------------- */
/** atime */

static const char *
string_file_atime (file_entry_t * fe, int len)
{
    (void) len;

    return file_date (fe->st.st_atime);
}

/* --------------------------------------------------------------------------------------------- */
/** ctime */

static const char *
string_file_ctime (file_entry_t * fe, int len)
{
    (void) len;

    return file_date (fe->st.st_ctime);
}

/* --------------------------------------------------------------------------------------------- */
/** perm */

static const char *
string_file_permission (file_entry_t * fe, int len)
{
    (void) len;

    return string_perm (fe->st.st_mode);
}

/* --------------------------------------------------------------------------------------------- */
/** mode */

static const char *
string_file_perm_octal (file_entry_t * fe, int len)
{
    static char buffer[10];

    (void) len;

    g_snprintf (buffer, sizeof (buffer), "0%06lo", (unsigned long) fe->st.st_mode);
    return buffer;
}

/* --------------------------------------------------------------------------------------------- */
/** nlink */

static const char *
string_file_nlinks (file_entry_t * fe, int len)
{
    static char buffer[BUF_TINY];

    (void) len;

    g_snprintf (buffer, sizeof (buffer), "%16d", (int) fe->st.st_nlink);
    return buffer;
}

/* --------------------------------------------------------------------------------------------- */
/** inode */

static const char *
string_inode (file_entry_t * fe, int len)
{
    static char buffer[10];

    (void) len;

    g_snprintf (buffer, sizeof (buffer), "%lu", (unsigned long) fe->st.st_ino);
    return buffer;
}

/* --------------------------------------------------------------------------------------------- */
/** nuid */

static const char *
string_file_nuid (file_entry_t * fe, int len)
{
    static char buffer[10];

    (void) len;

    g_snprintf (buffer, sizeof (buffer), "%lu", (unsigned long) fe->st.st_uid);
    return buffer;
}

/* --------------------------------------------------------------------------------------------- */
/** ngid */

static const char *
string_file_ngid (file_entry_t * fe, int len)
{
    static char buffer[10];

    (void) len;

    g_snprintf (buffer, sizeof (buffer), "%lu", (unsigned long) fe->st.st_gid);
    return buffer;
}

/* --------------------------------------------------------------------------------------------- */
/** owner */

static const char *
string_file_owner (file_entry_t * fe, int len)
{
    (void) len;

    return get_owner (fe->st.st_uid);
}

/* --------------------------------------------------------------------------------------------- */
/** group */

static const char *
string_file_group (file_entry_t * fe, int len)
{
    (void) len;

    return get_group (fe->st.st_gid);
}

/* --------------------------------------------------------------------------------------------- */
/** mark */

static const char *
string_marked (file_entry_t * fe, int len)
{
    (void) len;

    return fe->f.marked != 0 ? "*" : " ";
}

/* --------------------------------------------------------------------------------------------- */
/** space */

static const char *
string_space (file_entry_t * fe, int len)
{
    (void) fe;
    (void) len;

    return " ";
}

/* --------------------------------------------------------------------------------------------- */
/** dot */

static const char *
string_dot (file_entry_t * fe, int len)
{
    (void) fe;
    (void) len;

    return ".";
}

/* --------------------------------------------------------------------------------------------- */

static int
file_compute_color (file_attr_t attr, file_entry_t * fe)
{
    switch (attr)
    {
    case FATTR_CURRENT:
        return (SELECTED_COLOR);
    case FATTR_MARKED:
        return (MARKED_COLOR);
    case FATTR_MARKED_CURRENT:
        return (MARKED_SELECTED_COLOR);
    case FATTR_STATUS:
        return (NORMAL_COLOR);
    case FATTR_NORMAL:
    default:
        if (!panels_options.filetype_mode)
            return (NORMAL_COLOR);
    }

    return mc_fhl_get_color (mc_filehighlight, fe);
}

/* --------------------------------------------------------------------------------------------- */
/** Returns the number of items in the given panel */

static int
panel_items (const WPanel * p)
{
    return panel_lines (p) * p->list_cols;
}

/* --------------------------------------------------------------------------------------------- */
/** Formats the file number file_index of panel in the buffer dest */

static filename_scroll_flag_t
format_file (WPanel * panel, int file_index, int width, file_attr_t attr, gboolean isstatus,
             int *field_length)
{
    int color = NORMAL_COLOR;
    int length = 0;
    GSList *format, *home;
    file_entry_t *fe = NULL;
    filename_scroll_flag_t res = FILENAME_NOSCROLL;

    *field_length = 0;

    if (file_index < panel->dir.len)
    {
        fe = &panel->dir.list[file_index];
        color = file_compute_color (attr, fe);
    }

    home = isstatus ? panel->status_format : panel->format;

    for (format = home; format != NULL && length != width; format = g_slist_next (format))
    {
        format_item_t *fi = (format_item_t *) format->data;

        if (fi->string_fn != NULL)
        {
            const char *txt = " ";
            int len, perm = 0;
            const char *prepared_text;
            int name_offset = 0;

            if (fe != NULL)
                txt = fi->string_fn (fe, fi->field_len);

            len = fi->field_len;
            if (len + length > width)
                len = width - length;
            if (len <= 0)
                break;

            if (!isstatus && panel->content_shift > -1 && strcmp (fi->id, "name") == 0)
            {
                int str_len;
                int i;

                *field_length = len + 1;

                str_len = str_length (txt);
                i = MAX (0, str_len - len);
                panel->max_shift = MAX (panel->max_shift, i);
                i = MIN (panel->content_shift, i);

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

            if (panels_options.permission_mode)
            {
                if (strcmp (fi->id, "perm") == 0)
                    perm = 1;
                else if (strcmp (fi->id, "mode") == 0)
                    perm = 2;
            }

            if (color >= 0)
                tty_setcolor (color);
            else
                tty_lowlevel_setcolor (-color);

            if (!isstatus && panel->content_shift > -1)
                prepared_text = str_fit_to_term (txt + name_offset, len, HIDE_FIT (fi->just_mode));
            else
                prepared_text = str_fit_to_term (txt, len, fi->just_mode);

            if (perm != 0 && fe != NULL)
                add_permission_string (prepared_text, fi->field_len, fe, attr, color, perm != 1);
            else
                tty_print_string (prepared_text);

            length += len;
        }
        else
        {
            if (attr == FATTR_CURRENT || attr == FATTR_MARKED_CURRENT)
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
repaint_file (WPanel * panel, int file_index, file_attr_t attr)
{
    Widget *w = WIDGET (panel);

    int nth_column = 0;
    int width;
    int offset = 0;
    filename_scroll_flag_t ret_frm;
    int ypos = 0;
    gboolean panel_is_split;
    int fln = 0;

    panel_is_split = panel->list_cols > 1;
    width = w->rect.cols - 2;

    if (panel_is_split)
    {
        nth_column = (file_index - panel->top) / panel_lines (panel);
        width /= panel->list_cols;

        offset = width * nth_column;

        if (nth_column + 1 >= panel->list_cols)
            width = w->rect.cols - offset - 2;
    }

    /* Nothing to paint */
    if (width <= 0)
        return;

    ypos = file_index - panel->top;

    if (panel_is_split)
        ypos %= panel_lines (panel);

    ypos += 2;                  /* top frame and header */
    widget_gotoyx (w, ypos, offset + 1);

    ret_frm = format_file (panel, file_index, width, attr, FALSE, &fln);

    if (panel_is_split && nth_column + 1 < panel->list_cols)
    {
        tty_setcolor (NORMAL_COLOR);
        tty_print_one_vline (TRUE);
    }

    if (ret_frm != FILENAME_NOSCROLL)
    {
        if (!panel_is_split && fln > 0)
        {
            if (panel->list_format != list_long)
                width = fln;
            else
            {
                offset = width - fln + 1;
                width = fln - 1;
            }
        }

        widget_gotoyx (w, ypos, offset);
        tty_setcolor (NORMAL_COLOR);
        tty_print_string (panel_filename_scroll_left_char);

        if ((ret_frm & FILENAME_SCROLL_RIGHT) != 0)
        {
            offset += width;
            if (nth_column + 1 >= panel->list_cols)
                offset++;

            widget_gotoyx (w, ypos, offset);
            tty_setcolor (NORMAL_COLOR);
            tty_print_string (panel_filename_scroll_right_char);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
repaint_status (WPanel * panel)
{
    int width;

    width = WIDGET (panel)->rect.cols - 2;
    if (width > 0)
    {
        int fln = 0;

        (void) format_file (panel, panel->current, width, FATTR_STATUS, TRUE, &fln);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
display_mini_info (WPanel * panel)
{
    Widget *w = WIDGET (panel);
    const file_entry_t *fe;

    if (!panels_options.show_mini_info || panel->current < 0)
        return;

    widget_gotoyx (w, panel_lines (panel) + 3, 1);

    if (panel->quick_search.active)
    {
        tty_setcolor (INPUT_COLOR);
        tty_print_char ('/');
        tty_print_string (str_fit_to_term
                          (panel->quick_search.buffer->str, w->rect.cols - 3, J_LEFT));
        return;
    }

    /* Status resolves links and show them */
    set_colors (panel);

    fe = panel_current_entry (panel);

    if (S_ISLNK (fe->st.st_mode))
    {
        char link_target[MC_MAXPATHLEN];
        vfs_path_t *lc_link_vpath;
        int len;

        lc_link_vpath = vfs_path_append_new (panel->cwd_vpath, fe->fname->str, (char *) NULL);
        len = mc_readlink (lc_link_vpath, link_target, MC_MAXPATHLEN - 1);
        vfs_path_free (lc_link_vpath, TRUE);
        if (len > 0)
        {
            link_target[len] = 0;
            tty_print_string ("-> ");
            tty_print_string (str_fit_to_term (link_target, w->rect.cols - 5, J_LEFT_FIT));
        }
        else
            tty_print_string (str_fit_to_term (_("<readlink failed>"), w->rect.cols - 2, J_LEFT));
    }
    else if (DIR_IS_DOTDOT (fe->fname->str))
    {
        /* FIXME:
         * while loading directory (dir_list_load() and dir_list_reload()),
         * the actual stat info about ".." directory isn't got;
         * so just don't display incorrect info about ".." directory */
        tty_print_string (str_fit_to_term (_("UP--DIR"), w->rect.cols - 2, J_LEFT));
    }
    else
        /* Default behavior */
        repaint_status (panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
paint_dir (WPanel * panel)
{
    int i;
    int items;                  /* Number of items */

    items = panel_items (panel);
    /* reset max len of filename because we have the new max length for the new file list */
    panel->max_shift = -1;

    for (i = 0; i < items; i++)
    {
        file_attr_t attr = FATTR_NORMAL;        /* Color value of the line */
        int n;
        gboolean marked;

        n = i + panel->top;
        marked = (panel->dir.list[n].f.marked != 0);

        if (n < panel->dir.len)
        {
            if (panel->current == n && panel->active)
                attr = marked ? FATTR_MARKED_CURRENT : FATTR_CURRENT;
            else if (marked)
                attr = FATTR_MARKED;
        }

        repaint_file (panel, n, attr);
    }

    tty_set_normal_attrs ();
}

/* --------------------------------------------------------------------------------------------- */

static void
display_total_marked_size (const WPanel * panel, int y, int x, gboolean size_only)
{
    const Widget *w = CONST_WIDGET (panel);

    char buffer[BUF_SMALL], b_bytes[BUF_SMALL];
    const char *buf;
    int cols;

    if (panel->marked <= 0)
        return;

    buf = size_only ? b_bytes : buffer;
    cols = w->rect.cols - 2;

    g_strlcpy (b_bytes, size_trunc_sep (panel->total, panels_options.kilobyte_si),
               sizeof (b_bytes));

    if (!size_only)
        g_snprintf (buffer, sizeof (buffer),
                    ngettext ("%s in %d file", "%s in %d files", panel->marked),
                    b_bytes, panel->marked);

    /* don't forget spaces around buffer content */
    buf = str_trunc (buf, cols - 4);

    if (x < 0)
        /* center in panel */
        x = (w->rect.cols - str_term_width1 (buf)) / 2 - 1;

    /*
     * y == panel_lines (panel) + 2  for mini_info_separator
     * y == w->lines - 1             for panel bottom frame
     */
    widget_gotoyx (w, y, x);
    tty_setcolor (MARKED_COLOR);
    tty_printf (" %s ", buf);
}

/* --------------------------------------------------------------------------------------------- */

static void
mini_info_separator (const WPanel * panel)
{
    if (panels_options.show_mini_info)
    {
        const Widget *w = CONST_WIDGET (panel);
        int y;

        y = panel_lines (panel) + 2;

        tty_setcolor (NORMAL_COLOR);
        tty_draw_hline (w->rect.y + y, w->rect.x + 1, ACS_HLINE, w->rect.cols - 2);
        /* Status displays total marked size.
         * Centered in panel, full format. */
        display_total_marked_size (panel, y, -1, FALSE);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
show_free_space (const WPanel * panel)
{
    /* Used to figure out how many free space we have */
    static struct my_statfs myfs_stats;
    /* Old current working directory for displaying free space */
    static char *old_cwd = NULL;

    /* Don't try to stat non-local fs */
    if (!vfs_file_is_local (panel->cwd_vpath) || !free_space)
        return;

    if (old_cwd == NULL || strcmp (old_cwd, vfs_path_as_str (panel->cwd_vpath)) != 0)
    {
        char rpath[PATH_MAX];

        init_my_statfs ();
        g_free (old_cwd);
        old_cwd = g_strdup (vfs_path_as_str (panel->cwd_vpath));

        if (mc_realpath (old_cwd, rpath) == NULL)
            return;

        my_statfs (&myfs_stats, rpath);
    }

    if (myfs_stats.avail != 0 || myfs_stats.total != 0)
    {
        const Widget *w = CONST_WIDGET (panel);
        char buffer1[6], buffer2[6], tmp[BUF_SMALL];

        size_trunc_len (buffer1, sizeof (buffer1) - 1, myfs_stats.avail, 1,
                        panels_options.kilobyte_si);
        size_trunc_len (buffer2, sizeof (buffer2) - 1, myfs_stats.total, 1,
                        panels_options.kilobyte_si);
        g_snprintf (tmp, sizeof (tmp), " %s / %s (%d%%) ", buffer1, buffer2,
                    myfs_stats.total == 0 ? 0 :
                    (int) (100 * (long double) myfs_stats.avail / myfs_stats.total));
        widget_gotoyx (w, w->rect.lines - 1, w->rect.cols - 2 - (int) strlen (tmp));
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
panel_correct_path_to_show (const WPanel * panel)
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
            last_vpath = vfs_path_from_str_flags (archive_name + 1, VPF_NO_CANON);
        else
        {
            last_vpath = vfs_path_from_str_flags (prev_path_element->path, VPF_NO_CANON);
            last_vpath->relative = TRUE;
        }
    }
    else
        last_vpath = vfs_path_new (TRUE);

    vfs_path_add_element (last_vpath, path_element);
    return_path =
        vfs_path_to_str_flags (last_vpath, 0,
                               VPF_STRIP_HOME | VPF_STRIP_PASSWORD | VPF_HIDE_CHARSET);
    vfs_path_free (last_vpath, TRUE);

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
panel_get_encoding_info_str (const WPanel * panel)
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
show_dir (const WPanel * panel)
{
    const Widget *w = CONST_WIDGET (panel);
    gchar *tmp;

    set_colors (panel);
    tty_draw_box (w->rect.y, w->rect.x, w->rect.lines, w->rect.cols, FALSE);

    if (panels_options.show_mini_info)
    {
        int y;

        y = panel_lines (panel) + 2;

        widget_gotoyx (w, y, 0);
        tty_print_alt_char (ACS_LTEE, FALSE);
        widget_gotoyx (w, y, w->rect.cols - 1);
        tty_print_alt_char (ACS_RTEE, FALSE);
    }

    widget_gotoyx (w, 0, 1);
    tty_print_string (panel_history_prev_item_char);

    tmp = panels_options.show_dot_files ? panel_hiddenfiles_show_char : panel_hiddenfiles_hide_char;
    tmp = g_strdup_printf ("%s[%s]%s", tmp, panel_history_show_list_char,
                           panel_history_next_item_char);

    widget_gotoyx (w, 0, w->rect.cols - 6);
    tty_print_string (tmp);

    g_free (tmp);

    widget_gotoyx (w, 0, 3);

    if (panel->is_panelized)
        tty_printf (" %s ", _("Panelize"));
#ifdef HAVE_CHARSET
    else
    {
        tmp = panel_get_encoding_info_str (panel);
        if (tmp != NULL)
        {
            tty_printf ("%s", tmp);
            widget_gotoyx (w, 0, 3 + strlen (tmp));
            g_free (tmp);
        }
    }
#endif

    if (panel->active)
        tty_setcolor (REVERSE_COLOR);

    tmp = panel_correct_path_to_show (panel);
    tty_printf (" %s ", str_term_trim (tmp, MIN (MAX (w->rect.cols - 12, 0), w->rect.cols)));
    g_free (tmp);

    if (!panels_options.show_mini_info)
    {
        if (panel->marked == 0)
        {
            const file_entry_t *fe;

            fe = panel_current_entry (panel);

            /* Show size of curret file in the bottom of panel */
            if (S_ISREG (fe->st.st_mode))
            {
                char buffer[BUF_SMALL];

                g_snprintf (buffer, sizeof (buffer), " %s ",
                            size_trunc_sep (fe->st.st_size, panels_options.kilobyte_si));
                tty_setcolor (NORMAL_COLOR);
                widget_gotoyx (w, w->rect.lines - 1, 4);
                tty_print_string (buffer);
            }
        }
        else
        {
            /* Show total size of marked files
             * In the bottom of panel, display size only. */
            display_total_marked_size (panel, w->rect.lines - 1, 2, TRUE);
        }
    }

    show_free_space (panel);

    if (panel->active)
        tty_set_normal_attrs ();
}

/* --------------------------------------------------------------------------------------------- */

static void
adjust_top_file (WPanel * panel)
{
    int items;

    /* Update panel->current to avoid out of range in panel->dir.list[panel->current]
     * when panel is redrawing when directory is reloading, for example in path:
     * dir_list_reload() -> mc_refresh() -> dialog_change_screen_size() ->
     * midnight_callback (MSG_RESIZE) -> setup_panels() -> panel_callback(MSG_DRAW) ->
     * display_mini_info()
     */
    panel->current = CLAMP (panel->current, 0, panel->dir.len - 1);

    items = panel_items (panel);

    if (panel->dir.len <= items)
    {
        /* If all files fit, show them all. */
        panel->top = 0;
    }
    else
    {
        int i;

        /* top_file has to be in the range [current-items+1, current] so that
           the current file is visible.
           top_file should be in the range [0, count-items] so that there's
           no empty space wasted.
           Within these ranges, adjust it by as little as possible. */

        if (panel->top < 0)
            panel->top = 0;

        i = panel->current - items + 1;
        if (panel->top < i)
            panel->top = i;

        i = panel->dir.len - items;
        if (panel->top > i)
            panel->top = i;

        if (panel->top > panel->current)
            panel->top = panel->current;
    }
}

/* --------------------------------------------------------------------------------------------- */
/** add "#enc:encoding" to end of path */
/* if path ends width a previous #enc:, only encoding is changed no additional
 * #enc: is appended
 * return new string
 */

static char *
panel_save_name (WPanel * panel)
{
    /* If the program is shutting down */
    if ((mc_global.midnight_shutdown && auto_save_setup) || saving_setup)
        return g_strdup (panel->name);

    return g_strconcat ("Temporal:", panel->name, (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
directory_history_add (WPanel * panel, const vfs_path_t * vpath)
{
    char *tmp;

    tmp = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_PASSWORD);
    panel->dir_history.list = list_append_unique (panel->dir_history.list, tmp);
    panel->dir_history.current = panel->dir_history.list;
}

/* --------------------------------------------------------------------------------------------- */

/* "history_load" event handler */
static gboolean
panel_load_history (const gchar * event_group_name, const gchar * event_name,
                    gpointer init_data, gpointer data)
{
    WPanel *p = PANEL (init_data);
    ev_history_load_save_t *ev = (ev_history_load_save_t *) data;

    (void) event_group_name;
    (void) event_name;

    if (ev->receiver == NULL || ev->receiver == WIDGET (p))
    {
        if (ev->cfg != NULL)
            p->dir_history.list = mc_config_history_load (ev->cfg, p->dir_history.name);
        else
            p->dir_history.list = mc_config_history_get (p->dir_history.name);

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
    WPanel *p = PANEL (init_data);

    (void) event_group_name;
    (void) event_name;

    if (p->dir_history.list != NULL)
    {
        ev_history_load_save_t *ev = (ev_history_load_save_t *) data;

        mc_config_history_save (ev->cfg, p->dir_history.name, p->dir_history.list);
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
    if (p->dir_history.list != NULL)
    {
        /* directory history is already saved before this moment */
        p->dir_history.list = g_list_first (p->dir_history.list);
        g_list_free_full (p->dir_history.list, g_free);
    }
    g_free (p->dir_history.name);

    file_filter_clear (&p->filter);

    g_slist_free_full (p->format, (GDestroyNotify) format_item_free);
    g_slist_free_full (p->status_format, (GDestroyNotify) format_item_free);

    g_free (p->user_format);
    for (i = 0; i < LIST_FORMATS; i++)
        g_free (p->user_status_format[i]);

    g_free (p->name);

    panelized_descr_free (p->panelized_descr);

    g_string_free (p->quick_search.buffer, TRUE);
    g_string_free (p->quick_search.prev_buffer, TRUE);

    vfs_path_free (p->lwd_vpath, TRUE);
    vfs_path_free (p->cwd_vpath, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_paint_sort_info (const WPanel * panel)
{
    if (*panel->sort_field->hotkey != '\0')
    {
        const char *sort_sign =
            panel->sort_info.reverse ? panel_sort_up_char : panel_sort_down_char;
        char *str;

        str = g_strdup_printf ("%s%s", sort_sign, Q_ (panel->sort_field->hotkey));
        widget_gotoyx (panel, 1, 1);
        tty_print_string (str);
        g_free (str);
    }
}

/* --------------------------------------------------------------------------------------------- */

static const char *
panel_get_title_without_hotkey (const char *title)
{
    static char translated_title[BUF_TINY];

    if (title == NULL || title[0] == '\0')
        translated_title[0] = '\0';
    else
    {
        char *hkey;

        g_snprintf (translated_title, sizeof (translated_title), "%s", _(title));

        hkey = strchr (translated_title, '&');
        if (hkey != NULL && hkey[1] != '\0')
            memmove (hkey, hkey + 1, strlen (hkey));
    }

    return translated_title;
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_print_header (const WPanel * panel)
{
    const Widget *w = CONST_WIDGET (panel);

    int y, x;
    int i;
    GString *format_txt;

    widget_gotoyx (w, 1, 1);
    tty_getyx (&y, &x);
    tty_setcolor (NORMAL_COLOR);
    tty_draw_hline (y, x, ' ', w->rect.cols - 2);

    format_txt = g_string_new ("");

    for (i = 0; i < panel->list_cols; i++)
    {
        GSList *format;

        for (format = panel->format; format != NULL; format = g_slist_next (format))
        {
            format_item_t *fi = (format_item_t *) format->data;

            if (fi->string_fn != NULL)
            {
                g_string_set_size (format_txt, 0);

                if (panel->list_format == list_long && strcmp (fi->id, panel->sort_field->id) == 0)
                    g_string_append (format_txt,
                                     panel->sort_info.reverse
                                     ? panel_sort_up_char : panel_sort_down_char);

                g_string_append (format_txt, fi->title);

                if (panel->filter.handler != NULL && strcmp (fi->id, "name") == 0)
                {
                    g_string_append (format_txt, " [");
                    g_string_append (format_txt, panel->filter.value);
                    g_string_append (format_txt, "]");
                }

                tty_setcolor (HEADER_COLOR);
                tty_print_string (str_fit_to_term (format_txt->str, fi->field_len, J_CENTER_LEFT));
            }
            else
            {
                tty_setcolor (NORMAL_COLOR);
                tty_print_one_vline (TRUE);
            }
        }

        if (i < panel->list_cols - 1)
        {
            tty_setcolor (NORMAL_COLOR);
            tty_print_one_vline (TRUE);
        }
    }

    g_string_free (format_txt, TRUE);

    if (panel->list_format != list_long)
        panel_paint_sort_info (panel);
}

/* --------------------------------------------------------------------------------------------- */

static const char *
parse_panel_size (WPanel * panel, const char *format, gboolean isstatus)
{
    panel_display_t frame = frame_half;

    format = skip_separators (format);

    if (strncmp (format, "full", 4) == 0)
    {
        frame = frame_full;
        format += 4;
    }
    else if (strncmp (format, "half", 4) == 0)
    {
        frame = frame_half;
        format += 4;
    }

    if (!isstatus)
    {
        panel->frame_size = frame;
        panel->list_cols = 1;
    }

    /* Now, the optional column specifier */
    format = skip_separators (format);

    if (g_ascii_isdigit (*format))
    {
        if (!isstatus)
        {
            panel->list_cols = g_ascii_digit_value (*format);
            if (panel->list_cols < 1)
                panel->list_cols = 1;
        }

        format++;
    }

    if (!isstatus)
        panel_update_cols (WIDGET (panel), panel->frame_size);

    return skip_separators (format);
}

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
/* Format is:

   all              := panel_format? format
   panel_format     := [full|half] [1-9]
   format           := one_format_item_t
                     | format , one_format_item_t

   one_format_item_t := just format.id [opt_size]
   just              := [<=>]
   opt_size          := : size [opt_expand]
   size              := [0-9]+
   opt_expand        := +

 */
/* *INDENT-ON* */

static GSList *
parse_display_format (WPanel * panel, const char *format, char **error, gboolean isstatus,
                      int *res_total_cols)
{
    GSList *home = NULL;        /* The formats we return */
    int total_cols = 0;         /* Used columns by the format */
    size_t i;

    static size_t i18n_timelength = 0;  /* flag: check ?Time length at startup */

    *error = NULL;

    if (i18n_timelength == 0)
    {
        i18n_timelength = i18n_checktimelength ();      /* Mustn't be 0 */

        for (i = 0; panel_fields[i].id != NULL; i++)
            if (strcmp ("time", panel_fields[i].id + 1) == 0)
                panel_fields[i].min_size = i18n_timelength;
    }

    /*
     * This makes sure that the panel and mini status full/half mode
     * setting is equal
     */
    format = parse_panel_size (panel, format, isstatus);

    while (*format != '\0')
    {                           /* format can be an empty string */
        format_item_t *darr;
        align_crt_t justify;    /* Which mode. */
        gboolean set_justify = TRUE;    /* flag: set justification mode? */
        gboolean found = FALSE;
        size_t klen = 0;

        darr = g_new0 (format_item_t, 1);
        home = g_slist_append (home, darr);

        format = skip_separators (format);

        switch (*format)
        {
        case '<':
            justify = J_LEFT;
            format = skip_separators (format + 1);
            break;
        case '=':
            justify = J_CENTER;
            format = skip_separators (format + 1);
            break;
        case '>':
            justify = J_RIGHT;
            format = skip_separators (format + 1);
            break;
        default:
            justify = J_LEFT;
            set_justify = FALSE;
            break;
        }

        for (i = 0; !found && panel_fields[i].id != NULL; i++)
        {
            klen = strlen (panel_fields[i].id);
            found = strncmp (format, panel_fields[i].id, klen) == 0;
        }

        if (found)
        {
            i--;
            format += klen;

            darr->requested_field_len = panel_fields[i].min_size;
            darr->string_fn = panel_fields[i].string_fn;
            darr->title = g_strdup (panel_get_title_without_hotkey (panel_fields[i].title_hotkey));
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

            format = skip_separators (format);

            /* If we have a size specifier */
            if (*format == ':')
            {
                int req_length;

                /* If the size was specified, we don't want
                 * auto-expansion by default
                 */
                darr->expand = FALSE;
                format++;
                req_length = atoi (format);
                darr->requested_field_len = req_length;

                format = skip_numbers (format);

                /* Now, if they insist on expansion */
                if (*format == '+')
                {
                    darr->expand = TRUE;
                    format++;
                }
            }
        }
        else
        {
            size_t pos;
            char *tmp_format;

            pos = strlen (format);
            if (pos > 8)
                pos = 8;

            tmp_format = g_strndup (format, pos);
            g_slist_free_full (home, (GDestroyNotify) format_item_free);
            *error =
                g_strconcat (_("Unknown tag on display format:"), " ", tmp_format, (char *) NULL);
            g_free (tmp_format);

            return NULL;
        }

        total_cols += darr->requested_field_len;
    }

    *res_total_cols = total_cols;
    return home;
}

/* --------------------------------------------------------------------------------------------- */

static GSList *
use_display_format (WPanel * panel, const char *format, char **error, gboolean isstatus)
{
#define MAX_EXPAND 4
    int expand_top = 0;         /* Max used element in expand */
    int usable_columns;         /* Usable columns in the panel */
    int total_cols = 0;
    GSList *darr, *home;

    if (format == NULL)
        format = DEFAULT_USER_FORMAT;

    home = parse_display_format (panel, format, error, isstatus, &total_cols);

    if (*error != NULL)
        return NULL;

    panel->dirty = TRUE;

    usable_columns = WIDGET (panel)->rect.cols - 2;
    /* Status needn't to be split */
    if (!isstatus)
    {
        usable_columns /= panel->list_cols;
        if (panel->list_cols > 1)
            usable_columns--;
    }

    /* Look for the expandable fields and set field_len based on the requested field len */
    for (darr = home; darr != NULL && expand_top < MAX_EXPAND; darr = g_slist_next (darr))
    {
        format_item_t *fi = (format_item_t *) darr->data;

        fi->field_len = fi->requested_field_len;
        if (fi->expand)
            expand_top++;
    }

    /* If we used more columns than the available columns, adjust that */
    if (total_cols > usable_columns)
    {
        int dif;
        int pdif = 0;

        dif = total_cols - usable_columns;

        while (dif != 0 && pdif != dif)
        {
            pdif = dif;

            for (darr = home; darr != NULL; darr = g_slist_next (darr))
            {
                format_item_t *fi = (format_item_t *) darr->data;

                if (dif != 0 && fi->field_len != 1)
                {
                    fi->field_len--;
                    dif--;
                }
            }
        }

        total_cols = usable_columns;    /* give up, the rest should be truncated */
    }

    /* Expand the available space */
    if (usable_columns > total_cols && expand_top != 0)
    {
        int i;
        int spaces;

        spaces = (usable_columns - total_cols) / expand_top;

        for (i = 0, darr = home; darr != NULL && i < expand_top; darr = g_slist_next (darr))
        {
            format_item_t *fi = (format_item_t *) darr->data;

            if (fi->expand)
            {
                fi->field_len += spaces;
                if (i == 0)
                    fi->field_len += (usable_columns - total_cols) % expand_top;
                i++;
            }
        }
    }

    return home;
}

/* --------------------------------------------------------------------------------------------- */
/** Given the panel->view_type returns the format string to be parsed */

static const char *
panel_format (WPanel * panel)
{
    switch (panel->list_format)
    {
    case list_long:
        return "full perm space nlink space owner space group space size space mtime space name";

    case list_brief:
        {
            static char format[BUF_TINY];
            int brief_cols = panel->brief_cols;

            if (brief_cols < 1)
                brief_cols = 2;

            if (brief_cols > 9)
                brief_cols = 9;

            g_snprintf (format, sizeof (format), "half %d type name", brief_cols);
            return format;
        }

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
        return panel->user_status_format[panel->list_format];

    switch (panel->list_format)
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

static void
cd_up_dir (WPanel * panel)
{
    vfs_path_t *up_dir;

    up_dir = vfs_path_from_str ("..");
    panel_cd (panel, up_dir, cd_exact);
    vfs_path_free (up_dir, TRUE);
}

/* --------------------------------------------------------------------------------------------- */
/** Used to emulate Lynx's entering leaving a directory with the arrow keys */

static cb_ret_t
maybe_cd (WPanel * panel, gboolean move_up_dir)
{
    if (panels_options.navigate_with_arrows && input_is_empty (cmdline))
    {
        const file_entry_t *fe;

        if (move_up_dir)
        {
            cd_up_dir (panel);
            return MSG_HANDLED;
        }

        fe = panel_current_entry (panel);

        if (S_ISDIR (fe->st.st_mode) || link_isdir (fe))
        {
            vfs_path_t *vpath;

            vpath = vfs_path_from_str (fe->fname->str);
            panel_cd (panel, vpath, cd_exact);
            vfs_path_free (vpath, TRUE);
            return MSG_HANDLED;
        }
    }

    return MSG_NOT_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

/* if command line is empty then do 'cd ..' */
static cb_ret_t
force_maybe_cd (WPanel * panel)
{
    if (input_is_empty (cmdline))
    {
        cd_up_dir (panel);
        return MSG_HANDLED;
    }

    return MSG_NOT_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static inline void
unselect_item (WPanel * panel)
{
    repaint_file (panel, panel->current,
                  panel_current_entry (panel)->f.marked != 0 ? FATTR_MARKED : FATTR_NORMAL);
}

/* --------------------------------------------------------------------------------------------- */
/** Select/unselect all the files like a current file by extension */

static void
panel_select_ext_cmd (WPanel * panel)
{
    const file_entry_t *fe;
    GString *filename;
    gboolean do_select;
    char *reg_exp, *cur_file_ext;
    mc_search_t *search;
    int i;

    fe = panel_current_entry (panel);

    filename = fe->fname;
    if (filename == NULL)
        return;

    do_select = (fe->f.marked == 0);

    cur_file_ext = strutils_regex_escape (extension (filename->str));
    if (cur_file_ext[0] != '\0')
        reg_exp = g_strconcat ("^.*\\.", cur_file_ext, "$", (char *) NULL);
    else
        reg_exp = g_strdup ("^[^\\.]+$");

    g_free (cur_file_ext);

    search = mc_search_new (reg_exp, NULL);
    search->search_type = MC_SEARCH_T_REGEX;
    search->is_case_sensitive = FALSE;

    for (i = 0; i < panel->dir.len; i++)
    {
        fe = &panel->dir.list[i];

        if (DIR_IS_DOTDOT (fe->fname->str) || S_ISDIR (fe->st.st_mode))
            continue;

        if (!mc_search_run (search, fe->fname->str, 0, fe->fname->len, NULL))
            continue;

        do_file_mark (panel, i, do_select ? 1 : 0);
    }

    mc_search_free (search);
    g_free (reg_exp);
}

/* --------------------------------------------------------------------------------------------- */

static int
panel_current_at_half (const WPanel * panel)
{
    int lines, top;

    lines = panel_lines (panel);

    /* define top file of column */
    top = panel->top;
    if (panel->list_cols > 1)
        top += lines * ((panel->current - top) / lines);

    return (panel->current - top - lines / 2);
}

/* --------------------------------------------------------------------------------------------- */

static void
move_down (WPanel * panel)
{
    int items;

    if (panel->current + 1 == panel->dir.len)
        return;

    unselect_item (panel);
    panel->current++;

    items = panel_items (panel);

    if (panels_options.scroll_pages && panel->current - panel->top == items)
    {
        /* Scroll window half screen */
        panel->top += items / 2;
        if (panel->top > panel->dir.len - items)
            panel->top = panel->dir.len - items;
        paint_dir (panel);
    }
    else if (panels_options.scroll_center && panel_current_at_half (panel) > 0)
    {
        /* Scroll window when cursor is halfway down */
        panel->top++;
        if (panel->top > panel->dir.len - items)
            panel->top = panel->dir.len - items;
    }
    select_item (panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
move_up (WPanel * panel)
{
    if (panel->current == 0)
        return;

    unselect_item (panel);
    panel->current--;

    if (panels_options.scroll_pages && panel->current < panel->top)
    {
        /* Scroll window half screen */
        panel->top -= panel_items (panel) / 2;
        if (panel->top < 0)
            panel->top = 0;
        paint_dir (panel);
    }
    else if (panels_options.scroll_center && panel_current_at_half (panel) < 0)
    {
        /* Scroll window when cursor is halfway up */
        panel->top--;
        if (panel->top < 0)
            panel->top = 0;
    }
    select_item (panel);
}

/* --------------------------------------------------------------------------------------------- */
/** Changes the current by lines (may be negative) */

static void
panel_move_current (WPanel * panel, int lines)
{
    int new_pos;
    gboolean adjust = FALSE;

    new_pos = panel->current + lines;
    if (new_pos >= panel->dir.len)
        new_pos = panel->dir.len - 1;

    if (new_pos < 0)
        new_pos = 0;

    unselect_item (panel);
    panel->current = new_pos;

    if (panel->current - panel->top >= panel_items (panel))
    {
        panel->top += lines;
        adjust = TRUE;
    }

    if (panel->current - panel->top < 0)
    {
        panel->top += lines;
        adjust = TRUE;
    }

    if (adjust)
    {
        if (panel->top > panel->current)
            panel->top = panel->current;
        if (panel->top < 0)
            panel->top = 0;
        paint_dir (panel);
    }
    select_item (panel);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
move_left (WPanel * panel)
{
    if (panel->list_cols > 1)
    {
        panel_move_current (panel, -panel_lines (panel));
        return MSG_HANDLED;
    }

    return maybe_cd (panel, TRUE);      /* cd .. */
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
move_right (WPanel * panel)
{
    if (panel->list_cols > 1)
    {
        panel_move_current (panel, panel_lines (panel));
        return MSG_HANDLED;
    }

    return maybe_cd (panel, FALSE);     /* cd (current) */
}

/* --------------------------------------------------------------------------------------------- */

static void
prev_page (WPanel * panel)
{
    int items;

    if (panel->current == 0 && panel->top == 0)
        return;

    unselect_item (panel);
    items = panel_items (panel);
    if (panel->top < items)
        items = panel->top;
    if (items == 0)
        panel->current = 0;
    else
        panel->current -= items;
    panel->top -= items;

    select_item (panel);
    paint_dir (panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
goto_parent_dir (WPanel * panel)
{
    if (!panel->is_panelized)
        cd_up_dir (panel);
    else
    {
        GString *fname;
        const char *bname;
        vfs_path_t *dname_vpath;

        fname = panel_current_entry (panel)->fname;

        if (g_path_is_absolute (fname->str))
            fname = mc_g_string_dup (fname);
        else
        {
            char *fname2;

            fname2 =
                mc_build_filename (vfs_path_as_str (panel->panelized_descr->root_vpath), fname->str,
                                   (char *) NULL);

            fname = g_string_new_take (fname2);
        }

        bname = x_basename (fname->str);

        if (bname == fname->str)
            dname_vpath = vfs_path_from_str (".");
        else
        {
            g_string_truncate (fname, bname - fname->str);
            dname_vpath = vfs_path_from_str (fname->str);
        }

        panel_cd (panel, dname_vpath, cd_exact);
        panel_set_current_by_name (panel, bname);

        vfs_path_free (dname_vpath, TRUE);
        g_string_free (fname, TRUE);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
next_page (WPanel * panel)
{
    int items;

    if (panel->current == panel->dir.len - 1)
        return;

    unselect_item (panel);
    items = panel_items (panel);
    if (panel->top > panel->dir.len - 2 * items)
        items = panel->dir.len - items - panel->top;
    if (panel->top + items < 0)
        items = -panel->top;
    if (items == 0)
        panel->current = panel->dir.len - 1;
    else
        panel->current += items;
    panel->top += items;

    select_item (panel);
    paint_dir (panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
goto_child_dir (WPanel * panel)
{
    const file_entry_t *fe;

    fe = panel_current_entry (panel);

    if (S_ISDIR (fe->st.st_mode) || link_isdir (fe))
    {
        vfs_path_t *vpath;

        vpath = vfs_path_from_str (fe->fname->str);
        panel_cd (panel, vpath, cd_exact);
        vfs_path_free (vpath, TRUE);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
goto_top_file (WPanel * panel)
{
    unselect_item (panel);
    panel->current = panel->top;
    select_item (panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
goto_middle_file (WPanel * panel)
{
    unselect_item (panel);
    panel->current = panel->top + panel_items (panel) / 2;
    select_item (panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
goto_bottom_file (WPanel * panel)
{
    unselect_item (panel);
    panel->current = panel->top + panel_items (panel) - 1;
    select_item (panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
move_home (WPanel * panel)
{
    if (panel->current == 0)
        return;

    unselect_item (panel);

    if (panels_options.torben_fj_mode)
    {
        int middle_pos;

        middle_pos = panel->top + panel_items (panel) / 2;

        if (panel->current > middle_pos)
        {
            goto_middle_file (panel);
            return;
        }
        if (panel->current != panel->top)
        {
            goto_top_file (panel);
            return;
        }
    }

    panel->top = 0;
    panel->current = 0;

    paint_dir (panel);
    select_item (panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
move_end (WPanel * panel)
{
    if (panel->current == panel->dir.len - 1)
        return;

    unselect_item (panel);

    if (panels_options.torben_fj_mode)
    {
        int items, middle_pos;

        items = panel_items (panel);
        middle_pos = panel->top + items / 2;

        if (panel->current < middle_pos)
        {
            goto_middle_file (panel);
            return;
        }
        if (panel->current != panel->top + items - 1)
        {
            goto_bottom_file (panel);
            return;
        }
    }

    panel->current = panel->dir.len - 1;
    paint_dir (panel);
    select_item (panel);
}

/* --------------------------------------------------------------------------------------------- */

static void
do_mark_file (WPanel * panel, mark_act_t do_move)
{
    do_file_mark (panel, panel->current, panel_current_entry (panel)->f.marked ? 0 : 1);

    if ((panels_options.mark_moves_down && do_move == MARK_DOWN) || do_move == MARK_FORCE_DOWN)
        move_down (panel);
    else if (do_move == MARK_FORCE_UP)
        move_up (panel);
}

/* --------------------------------------------------------------------------------------------- */

static inline void
mark_file (WPanel * panel)
{
    do_mark_file (panel, MARK_DOWN);
}

/* --------------------------------------------------------------------------------------------- */

static inline void
mark_file_up (WPanel * panel)
{
    do_mark_file (panel, MARK_FORCE_UP);
}

/* --------------------------------------------------------------------------------------------- */

static inline void
mark_file_down (WPanel * panel)
{
    do_mark_file (panel, MARK_FORCE_DOWN);
}

/* --------------------------------------------------------------------------------------------- */

static void
mark_file_right (WPanel * panel)
{
    int lines;

    if (state_mark < 0)
        state_mark = panel_current_entry (panel)->f.marked ? 0 : 1;

    lines = panel_lines (panel);
    lines = MIN (lines, panel->dir.len - panel->current - 1);
    for (; lines != 0; lines--)
    {
        do_file_mark (panel, panel->current, state_mark);
        move_down (panel);
    }
    do_file_mark (panel, panel->current, state_mark);
}

/* --------------------------------------------------------------------------------------------- */

static void
mark_file_left (WPanel * panel)
{
    int lines;

    if (state_mark < 0)
        state_mark = panel_current_entry (panel)->f.marked ? 0 : 1;

    lines = panel_lines (panel);
    lines = MIN (lines, panel->current + 1);
    for (; lines != 0; lines--)
    {
        do_file_mark (panel, panel->current, state_mark);
        move_up (panel);
    }
    do_file_mark (panel, panel->current, state_mark);
}

/* --------------------------------------------------------------------------------------------- */

static mc_search_t *
panel_select_unselect_files_dialog (select_flags_t * flags, const char *title,
                                    const char *history_name, const char *help_section, char **str)
{
    gboolean files_only = (*flags & SELECT_FILES_ONLY) != 0;
    gboolean case_sens = (*flags & SELECT_MATCH_CASE) != 0;
    gboolean shell_patterns = (*flags & SELECT_SHELL_PATTERNS) != 0;

    char *reg_exp;
    mc_search_t *search;

    quick_widget_t quick_widgets[] = {
        /* *INDENT-OFF* */
        QUICK_INPUT (INPUT_LAST_TEXT, history_name, &reg_exp, NULL,
                     FALSE, FALSE, INPUT_COMPLETE_FILENAMES),
        QUICK_START_COLUMNS,
            QUICK_CHECKBOX (N_("&Files only"), &files_only, NULL),
            QUICK_CHECKBOX (N_("&Using shell patterns"), &shell_patterns, NULL),
        QUICK_NEXT_COLUMN,
            QUICK_CHECKBOX (N_("&Case sensitive"), &case_sens, NULL),
        QUICK_STOP_COLUMNS,
        QUICK_END
        /* *INDENT-ON* */
    };

    WRect r = { -1, -1, 0, 50 };

    quick_dialog_t qdlg = {
        r, title, help_section,
        quick_widgets, NULL, NULL
    };

    if (quick_dialog (&qdlg) == B_CANCEL)
        return NULL;

    if (*reg_exp == '\0')
    {
        g_free (reg_exp);
        if (str != NULL)
            *str = NULL;
        return SELECT_RESET;
    }

    search = mc_search_new (reg_exp, NULL);
    search->search_type = shell_patterns ? MC_SEARCH_T_GLOB : MC_SEARCH_T_REGEX;
    search->is_entire_line = TRUE;
    search->is_case_sensitive = case_sens;

    if (str != NULL)
        *str = reg_exp;
    else
        g_free (reg_exp);

    if (!mc_search_prepare (search))
    {
        message (D_ERROR, MSG_ERROR, _("Malformed regular expression"));
        mc_search_free (search);
        return SELECT_ERROR;
    }

    /* result flags */
    *flags = 0;
    if (case_sens)
        *flags |= SELECT_MATCH_CASE;
    if (files_only)
        *flags |= SELECT_FILES_ONLY;
    if (shell_patterns)
        *flags |= SELECT_SHELL_PATTERNS;

    return search;
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_select_unselect_files (WPanel * panel, const char *title, const char *history_name,
                             const char *help_section, gboolean do_select)
{
    mc_search_t *search;
    gboolean files_only;
    int i;

    search = panel_select_unselect_files_dialog (&panels_options.select_flags, title, history_name,
                                                 help_section, NULL);
    if (search == NULL || search == SELECT_RESET || search == SELECT_ERROR)
        return;

    files_only = (panels_options.select_flags & SELECT_FILES_ONLY) != 0;

    for (i = 0; i < panel->dir.len; i++)
    {
        if (DIR_IS_DOTDOT (panel->dir.list[i].fname->str))
            continue;
        if (S_ISDIR (panel->dir.list[i].st.st_mode) && files_only)
            continue;

        if (mc_search_run
            (search, panel->dir.list[i].fname->str, 0, panel->dir.list[i].fname->len, NULL))
            do_file_mark (panel, i, do_select ? 1 : 0);
    }

    mc_search_free (search);
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_select_files (WPanel * panel)
{
    panel_select_unselect_files (panel, _("Select"), MC_HISTORY_FM_PANEL_SELECT,
                                 "[Select/Unselect Files]", TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_unselect_files (WPanel * panel)
{
    panel_select_unselect_files (panel, _("Unselect"), MC_HISTORY_FM_PANEL_UNSELECT,
                                 "[Select/Unselect Files]", FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_select_invert_files (WPanel * panel)
{
    int i;

    for (i = 0; i < panel->dir.len; i++)
    {
        file_entry_t *file = &panel->dir.list[i];

        if (!panels_options.reverse_files_only || !S_ISDIR (file->st.st_mode))
            do_file_mark (panel, i, file->f.marked ? 0 : 1);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_do_set_filter (WPanel * panel)
{
    /* *INDENT-OFF* */
    file_filter_t ff = { .value = NULL, .handler = NULL, .flags = panel->filter.flags };
    /* *INDENT-ON* */

    ff.handler =
        panel_select_unselect_files_dialog (&ff.flags, _("Filter"), MC_HISTORY_FM_PANEL_FILTER,
                                            "[Filter...]", &ff.value);

    if (ff.handler == NULL || ff.handler == SELECT_ERROR)
        return;

    if (ff.handler == SELECT_RESET)
        ff.handler = NULL;

    panel_set_filter (panel, &ff);
}

/* --------------------------------------------------------------------------------------------- */
/** Incremental search of a file name in the panel.
  * @param panel instance of WPanel structure
  * @param c_code key code
  */

static void
do_search (WPanel * panel, int c_code)
{
    int curr;
    int i;
    gboolean wrapped = FALSE;
    char *act;
    mc_search_t *search;
    char *reg_exp, *esc_str;
    gboolean is_found = FALSE;

    if (c_code == KEY_BACKSPACE)
    {
        if (panel->quick_search.buffer->len != 0)
        {
            act = panel->quick_search.buffer->str + panel->quick_search.buffer->len;
            str_prev_noncomb_char (&act, panel->quick_search.buffer->str);
            g_string_set_size (panel->quick_search.buffer, act - panel->quick_search.buffer->str);
        }
        panel->quick_search.chpoint = 0;
    }
    else
    {
        if (c_code != 0 && (gsize) panel->quick_search.chpoint < sizeof (panel->quick_search.ch))
        {
            panel->quick_search.ch[panel->quick_search.chpoint] = c_code;
            panel->quick_search.chpoint++;
        }

        if (panel->quick_search.chpoint > 0)
        {
            switch (str_is_valid_char (panel->quick_search.ch, panel->quick_search.chpoint))
            {
            case -2:
                return;
            case -1:
                panel->quick_search.chpoint = 0;
                return;
            default:
                g_string_append_len (panel->quick_search.buffer, panel->quick_search.ch,
                                     panel->quick_search.chpoint);
                panel->quick_search.chpoint = 0;
            }
        }
    }

    reg_exp = g_strdup_printf ("%s*", panel->quick_search.buffer->str);
    esc_str = strutils_escape (reg_exp, -1, ",|\\{}[]", TRUE);
    search = mc_search_new (esc_str, NULL);
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

    curr = panel->current;

    for (i = panel->current; !wrapped || i != panel->current; i++)
    {
        if (i >= panel->dir.len)
        {
            i = 0;
            if (wrapped)
                break;
            wrapped = TRUE;
        }
        if (mc_search_run
            (search, panel->dir.list[i].fname->str, 0, panel->dir.list[i].fname->len, NULL))
        {
            curr = i;
            is_found = TRUE;
            break;
        }
    }
    if (is_found)
    {
        unselect_item (panel);
        panel->current = curr;
        select_item (panel);
        widget_draw (WIDGET (panel));
    }
    else if (c_code != KEY_BACKSPACE)
    {
        act = panel->quick_search.buffer->str + panel->quick_search.buffer->len;
        str_prev_noncomb_char (&act, panel->quick_search.buffer->str);
        g_string_set_size (panel->quick_search.buffer, act - panel->quick_search.buffer->str);
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
    if (panel->quick_search.active)
    {
        if (panel->current == panel->dir.len - 1)
            panel->current = 0;
        else
            move_down (panel);

        /* in case if there was no search string we need to recall
           previous string, with which we ended previous searching */
        if (panel->quick_search.buffer->len == 0)
            mc_g_string_copy (panel->quick_search.buffer, panel->quick_search.prev_buffer);

        do_search (panel, 0);
    }
    else
    {
        panel->quick_search.active = TRUE;
        g_string_set_size (panel->quick_search.buffer, 0);
        panel->quick_search.ch[0] = '\0';
        panel->quick_search.chpoint = 0;
        display_mini_info (panel);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
stop_search (WPanel * panel)
{
    if (!panel->quick_search.active)
        return;

    panel->quick_search.active = FALSE;

    /* if user overrdied search string, we need to store it
       to the quick_search.prev_buffer */
    if (panel->quick_search.buffer->len != 0)
        mc_g_string_copy (panel->quick_search.prev_buffer, panel->quick_search.buffer);

    display_mini_info (panel);
}

/* --------------------------------------------------------------------------------------------- */
/** Return TRUE if the Enter key has been processed, FALSE otherwise */

static gboolean
do_enter_on_file_entry (WPanel * panel, file_entry_t * fe)
{
    const char *fname = fe->fname->str;
    char *fname_quoted;
    vfs_path_t *full_name_vpath;
    gboolean ok;

    /*
     * Directory or link to directory - change directory.
     * Try the same for the entries on which mc_lstat() has failed.
     */
    if (S_ISDIR (fe->st.st_mode) || link_isdir (fe) || (fe->st.st_mode == 0))
    {
        vfs_path_t *fname_vpath;

        fname_vpath = vfs_path_from_str (fname);
        if (!panel_cd (panel, fname_vpath, cd_exact))
            cd_error_message (fname);
        vfs_path_free (fname_vpath, TRUE);
        return TRUE;
    }

    full_name_vpath = vfs_path_append_new (panel->cwd_vpath, fname, (char *) NULL);

    /* Try associated command */
    ok = regex_command (full_name_vpath, "Open") != 0;
    vfs_path_free (full_name_vpath, TRUE);
    if (ok)
        return TRUE;

    /* Check if the file is executable */
    full_name_vpath = vfs_path_append_new (panel->cwd_vpath, fname, (char *) NULL);
    ok = (is_exe (fe->st.st_mode) && if_link_is_exe (full_name_vpath, fe));
    vfs_path_free (full_name_vpath, TRUE);
    if (!ok)
        return FALSE;

    if (confirm_execute
        && query_dialog (_("The Midnight Commander"), _("Do you really want to execute?"), D_NORMAL,
                         2, _("&Yes"), _("&No")) != 0)
        return TRUE;

    if (!vfs_current_is_local ())
    {
        int ret;
        vfs_path_t *tmp_vpath;

        tmp_vpath = vfs_path_append_new (vfs_get_raw_current_dir (), fname, (char *) NULL);
        ret = mc_setctl (tmp_vpath, VFS_SETCTL_RUN, NULL);
        vfs_path_free (tmp_vpath, TRUE);
        /* We took action only if the dialog was shown or the execution was successful */
        return confirm_execute || (ret == 0);
    }

    fname_quoted = name_quote (fname, FALSE);
    if (fname_quoted != NULL)
    {
        char *cmd;

        cmd = g_strconcat ("." PATH_SEP_STR, fname_quoted, (char *) NULL);
        g_free (fname_quoted);

        shell_execute (cmd, 0);
        g_free (cmd);
    }

#ifdef HAVE_CHARSET
    mc_global.source_codepage = default_source_codepage;
#endif

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static inline gboolean
do_enter (WPanel * panel)
{
    return do_enter_on_file_entry (panel, panel_current_entry (panel));
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_cycle_listing_format (WPanel * panel)
{
    panel->list_format = (panel->list_format + 1) % LIST_FORMATS;

    if (set_panel_formats (panel) == 0)
        do_refresh ();
}

/* --------------------------------------------------------------------------------------------- */

static void
chdir_other_panel (WPanel * panel)
{
    const file_entry_t *entry;
    vfs_path_t *new_dir_vpath;
    char *curr_entry = NULL;
    WPanel *p;

    entry = panel_current_entry (panel);

    if (get_other_type () != view_listing)
        create_panel (get_other_index (), view_listing);

    if (S_ISDIR (entry->st.st_mode) || link_isdir (entry))
        new_dir_vpath = vfs_path_append_new (panel->cwd_vpath, entry->fname->str, (char *) NULL);
    else
    {
        new_dir_vpath = vfs_path_append_new (panel->cwd_vpath, "..", (char *) NULL);
        curr_entry = strrchr (vfs_path_get_last_path_str (panel->cwd_vpath), PATH_SEP);
    }

    p = change_panel ();
    panel_cd (p, new_dir_vpath, cd_exact);
    vfs_path_free (new_dir_vpath, TRUE);

    if (curr_entry != NULL)
        panel_set_current_by_name (p, curr_entry);
    (void) change_panel ();

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
        create_panel (get_other_index (), view_listing);

    panel_do_cd (other_panel, panel->cwd_vpath, cd_exact);

    /* try to set current filename on the other panel */
    if (!panel->is_panelized)
        panel_set_current_by_name (other_panel, panel_current_entry (panel)->fname->str);
}

/* --------------------------------------------------------------------------------------------- */

static void
chdir_to_readlink (WPanel * panel)
{
    const file_entry_t *fe;
    vfs_path_t *new_dir_vpath;
    char buffer[MC_MAXPATHLEN];
    int i;
    struct stat st;
    vfs_path_t *panel_fname_vpath;
    gboolean ok;
    WPanel *cpanel;

    if (get_other_type () != view_listing)
        return;

    fe = panel_current_entry (panel);

    if (!S_ISLNK (fe->st.st_mode))
        return;

    i = readlink (fe->fname->str, buffer, MC_MAXPATHLEN - 1);
    if (i < 0)
        return;

    panel_fname_vpath = vfs_path_from_str (fe->fname->str);
    ok = (mc_stat (panel_fname_vpath, &st) >= 0);
    vfs_path_free (panel_fname_vpath, TRUE);
    if (!ok)
        return;

    buffer[i] = '\0';
    if (!S_ISDIR (st.st_mode))
    {
        char *p;

        p = strrchr (buffer, PATH_SEP);
        if (p != NULL && p[1] == '\0')
        {
            *p = '\0';
            p = strrchr (buffer, PATH_SEP);
        }
        if (p == NULL)
            return;

        p[1] = '\0';
    }
    if (IS_PATH_SEP (*buffer))
        new_dir_vpath = vfs_path_from_str (buffer);
    else
        new_dir_vpath = vfs_path_append_new (panel->cwd_vpath, buffer, (char *) NULL);

    cpanel = change_panel ();
    panel_cd (cpanel, new_dir_vpath, cd_exact);
    vfs_path_free (new_dir_vpath, TRUE);
    (void) change_panel ();

    move_down (panel);
}

/* --------------------------------------------------------------------------------------------- */
/**
   function return 0 if not found and REAL_INDEX+1 if found
 */

static gsize
panel_get_format_field_index_by_name (const WPanel * panel, const char *name)
{
    GSList *format;
    gsize lc_index;

    for (lc_index = 1, format = panel->format;
         format != NULL && strcmp (((format_item_t *) format->data)->title, name) != 0;
         format = g_slist_next (format), lc_index++)
        ;

    if (format == NULL)
        lc_index = 0;

    return lc_index;
}

/* --------------------------------------------------------------------------------------------- */

static const panel_field_t *
panel_get_sortable_field_by_format (const WPanel * panel, gsize lc_index)
{
    const panel_field_t *pfield;
    const format_item_t *format;

    format = (const format_item_t *) g_slist_nth_data (panel->format, lc_index);
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
    const char *title;
    const panel_field_t *pfield = NULL;

    title = panel_get_title_without_hotkey (panel->sort_field->title_hotkey);
    lc_index = panel_get_format_field_index_by_name (panel, title);

    if (lc_index > 1)
    {
        /* search for prev sortable column in panel format */
        for (i = lc_index - 1;
             i != 0 && (pfield = panel_get_sortable_field_by_format (panel, i - 1)) == NULL; i--)
            ;
    }

    if (pfield == NULL)
    {
        /* Sortable field not found. Try to search in each array */
        for (i = g_slist_length (panel->format);
             i != 0 && (pfield = panel_get_sortable_field_by_format (panel, i - 1)) == NULL; i--)
            ;
    }

    if (pfield != NULL)
    {
        panel->sort_field = pfield;
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
    const char *title;

    format_field_count = g_slist_length (panel->format);
    title = panel_get_title_without_hotkey (panel->sort_field->title_hotkey);
    lc_index = panel_get_format_field_index_by_name (panel, title);

    if (lc_index != 0 && lc_index != format_field_count)
    {
        /* search for prev sortable column in panel format */
        for (i = lc_index;
             i != format_field_count
             && (pfield = panel_get_sortable_field_by_format (panel, i)) == NULL; i++)
            ;
    }

    if (pfield == NULL)
    {
        /* Sortable field not found. Try to search in each array */
        for (i = 0;
             i != format_field_count
             && (pfield = panel_get_sortable_field_by_format (panel, i)) == NULL; i++)
            ;
    }

    if (pfield != NULL)
    {
        panel->sort_field = pfield;
        panel_set_sort_order (panel, pfield);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_select_sort_order (WPanel * panel)
{
    const panel_field_t *sort_order;

    sort_order = sort_box (&panel->sort_info, panel->sort_field);
    if (sort_order != NULL)
    {
        panel->sort_field = sort_order;
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
    if (strcmp (panel->sort_field->id, name) == 0)
        panel->sort_info.reverse = !panel->sort_info.reverse;
    else
    {
        const panel_field_t *sort_order;

        sort_order = panel_get_field_by_id (name);
        if (sort_order == NULL)
            return;

        panel->sort_field = sort_order;
    }

    panel_set_sort_order (panel, panel->sort_field);
}

/* --------------------------------------------------------------------------------------------- */
/**
 *  If we moved to the parent directory move the 'current' pointer to
 *  the old directory name; If we leave VFS dir, remove FS specificator.
 *
 *  You do _NOT_ want to add any vfs aware code here. <pavel@ucw.cz>
 */

static const char *
get_parent_dir_name (const vfs_path_t * cwd_vpath, const vfs_path_t * lwd_vpath)
{
    size_t llen, clen;
    const char *p, *lwd;

    llen = vfs_path_len (lwd_vpath);
    clen = vfs_path_len (cwd_vpath);

    if (llen <= clen)
        return NULL;

    lwd = vfs_path_as_str (lwd_vpath);

    p = g_strrstr (lwd, VFS_PATH_URL_DELIMITER);

    if (p == NULL)
    {
        const char *cwd;

        cwd = vfs_path_as_str (cwd_vpath);

        p = strrchr (lwd, PATH_SEP);

        if (p != NULL && strncmp (cwd, lwd, (size_t) (p - lwd)) == 0
            && (clen == (size_t) (p - lwd) || (p == lwd && IS_PATH_SEP (cwd[0]) && cwd[1] == '\0')))
            return (p + 1);

        return NULL;
    }

    /* skip VFS prefix */
    while (--p > lwd && !IS_PATH_SEP (*p))
        ;
    /* get last component */
    while (--p > lwd && !IS_PATH_SEP (*p))
        ;

    /* return last component */
    return (p != lwd || IS_PATH_SEP (*p)) ? p + 1 : p;
}

/* --------------------------------------------------------------------------------------------- */
/** Wrapper for do_subshell_chdir, check for availability of subshell */

static void
subshell_chdir (const vfs_path_t * vpath)
{
#ifdef ENABLE_SUBSHELL
    if (mc_global.tty.use_subshell && vfs_current_is_local ())
        do_subshell_chdir (vpath, FALSE);
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
panel_do_cd_int (WPanel * panel, const vfs_path_t * new_dir_vpath, enum cd_enum cd_type)
{
    vfs_path_t *olddir_vpath;

    /* Convert *new_path to a suitable pathname, handle ~user */
    if (cd_type == cd_parse_command)
    {
        const vfs_path_element_t *element;

        element = vfs_path_get_by_index (new_dir_vpath, 0);
        if (strcmp (element->path, "-") == 0)
            new_dir_vpath = panel->lwd_vpath;
    }

    if (mc_chdir (new_dir_vpath) == -1)
        return FALSE;

    /* Success: save previous directory, shutdown status of previous dir */
    olddir_vpath = vfs_path_clone (panel->cwd_vpath);
    panel_set_lwd (panel, panel->cwd_vpath);
    input_complete_free (cmdline);

    vfs_path_free (panel->cwd_vpath, TRUE);
    vfs_setup_cwd ();
    panel->cwd_vpath = vfs_path_clone (vfs_get_raw_current_dir ());

    vfs_release_path (olddir_vpath);

    subshell_chdir (panel->cwd_vpath);

    /* Reload current panel */
    panel_clean_dir (panel);

    if (!dir_list_load (&panel->dir, panel->cwd_vpath, panel->sort_field->sort_routine,
                        &panel->sort_info, &panel->filter))
        message (D_ERROR, MSG_ERROR, _("Cannot read directory contents"));

    panel_set_current_by_name (panel, get_parent_dir_name (panel->cwd_vpath, olddir_vpath));

    load_hint (FALSE);
    panel->dirty = TRUE;
    update_xterm_title_path ();

    vfs_path_free (olddir_vpath, TRUE);

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
        next = g_list_next (panel->dir_history.current);
        if (next != NULL)
        {
            vfs_path_t *data_vpath;

            data_vpath = vfs_path_from_str ((char *) next->data);
            ok = panel_do_cd_int (panel, data_vpath, cd_exact);
            vfs_path_free (data_vpath, TRUE);
            panel->dir_history.current = next;
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
        prev = g_list_previous (panel->dir_history.current);
        if (prev != NULL)
        {
            vfs_path_t *data_vpath;

            data_vpath = vfs_path_from_str ((char *) prev->data);
            ok = panel_do_cd_int (panel, data_vpath, cd_exact);
            vfs_path_free (data_vpath, TRUE);
            panel->dir_history.current = prev;
        }
        /* skip directories that present in history but absent in file system */
    }
    while (!ok);
}

/* --------------------------------------------------------------------------------------------- */

static void
directory_history_list (WPanel * panel)
{
    history_descriptor_t hd;
    gboolean ok = FALSE;
    size_t pos;

    pos = g_list_position (panel->dir_history.current, panel->dir_history.list);

    history_descriptor_init (&hd, WIDGET (panel)->rect.y, WIDGET (panel)->rect.x,
                             panel->dir_history.list, (int) pos);
    history_show (&hd);

    panel->dir_history.list = hd.list;
    if (hd.text != NULL)
    {
        vfs_path_t *s_vpath;

        s_vpath = vfs_path_from_str (hd.text);
        ok = panel_do_cd_int (panel, s_vpath, cd_exact);
        if (ok)
            directory_history_add (panel, panel->cwd_vpath);
        else
            cd_error_message (hd.text);
        vfs_path_free (s_vpath, TRUE);
        g_free (hd.text);
    }

    if (!ok)
    {
        /* Since history is fully modified in history_show(), panel->dir_history actually
         * points to the invalid place. Try restore current position here. */

        size_t i;

        panel->dir_history.current = panel->dir_history.list;

        for (i = 0; i <= pos; i++)
        {
            GList *prev;

            prev = g_list_previous (panel->dir_history.current);
            if (prev == NULL)
                break;

            panel->dir_history.current = prev;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
panel_execute_cmd (WPanel * panel, long command)
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
    default:
        break;
    }

    switch (command)
    {
    case CK_CycleListingFormat:
        panel_cycle_listing_format (panel);
        break;
    case CK_PanelOtherCd:
        chdir_other_panel (panel);
        break;
    case CK_PanelOtherCdLink:
        chdir_to_readlink (panel);
        break;
    case CK_CopySingle:
        copy_cmd_local (panel);
        break;
    case CK_DeleteSingle:
        delete_cmd_local (panel);
        break;
    case CK_Enter:
        do_enter (panel);
        break;
    case CK_ViewRaw:
        view_raw_cmd (panel);
        break;
    case CK_EditNew:
        edit_cmd_new ();
        break;
    case CK_MoveSingle:
        rename_cmd_local (panel);
        break;
    case CK_SelectInvert:
        panel_select_invert_files (panel);
        break;
    case CK_Select:
        panel_select_files (panel);
        break;
    case CK_SelectExt:
        panel_select_ext_cmd (panel);
        break;
    case CK_Unselect:
        panel_unselect_files (panel);
        break;
    case CK_Filter:
        panel_do_set_filter (panel);
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
        res = force_maybe_cd (panel);
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
        panel_set_sort_order (panel, panel->sort_field);
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
    default:
        res = MSG_NOT_HANDLED;
        break;
    }

    return res;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
panel_key (WPanel * panel, int key)
{
    long command;

    if (is_abort_char (key))
    {
        stop_search (panel);
        return MSG_HANDLED;
    }

    if (panel->quick_search.active && ((key >= ' ' && key <= 255) || key == KEY_BACKSPACE))
    {
        do_search (panel, key);
        return MSG_HANDLED;
    }

    command = widget_lookup_key (WIDGET (panel), key);
    if (command != CK_IgnoreKey)
        return panel_execute_cmd (panel, command);

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
    WPanel *panel = PANEL (w);
    WDialog *h = DIALOG (w->owner);
    WButtonBar *bb;

    switch (msg)
    {
    case MSG_INIT:
        /* subscribe to "history_load" event */
        mc_event_add (h->event_group, MCEVENT_HISTORY_LOAD, panel_load_history, w, NULL);
        /* subscribe to "history_save" event */
        mc_event_add (h->event_group, MCEVENT_HISTORY_SAVE, panel_save_history, w, NULL);
        return MSG_HANDLED;

    case MSG_DRAW:
        /* Repaint everything, including frame and separator */
        widget_erase (w);
        show_dir (panel);
        panel_print_header (panel);
        adjust_top_file (panel);
        paint_dir (panel);
        mini_info_separator (panel);
        display_mini_info (panel);
        panel->dirty = FALSE;
        return MSG_HANDLED;

    case MSG_FOCUS:
        state_mark = -1;
        current_panel = panel;
        panel->active = TRUE;

        if (mc_chdir (panel->cwd_vpath) != 0)
        {
            char *cwd;

            cwd = vfs_path_to_str_flags (panel->cwd_vpath, 0, VPF_STRIP_PASSWORD);
            cd_error_message (cwd);
            g_free (cwd);
        }
        else
            subshell_chdir (panel->cwd_vpath);

        update_xterm_title_path ();
        select_item (panel);

        bb = buttonbar_find (h);
        midnight_set_buttonbar (bb);
        widget_draw (WIDGET (bb));
        return MSG_HANDLED;

    case MSG_UNFOCUS:
        /* Janne: look at this for the multiple panel options */
        stop_search (panel);
        panel->active = FALSE;
        unselect_item (panel);
        return MSG_HANDLED;

    case MSG_KEY:
        return panel_key (panel, parm);

    case MSG_ACTION:
        return panel_execute_cmd (panel, parm);

    case MSG_DESTROY:
        vfs_stamp_path (panel->cwd_vpath);
        /* unsubscribe from "history_load" event */
        mc_event_del (h->event_group, MCEVENT_HISTORY_LOAD, panel_load_history, w);
        /* unsubscribe from "history_save" event */
        mc_event_del (h->event_group, MCEVENT_HISTORY_SAVE, panel_save_history, w);
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
    mouse_marking = (panel_current_entry (panel)->f.marked != 0);
    mouse_mark_panel = current_panel;
}

/* --------------------------------------------------------------------------------------------- */

static void
mouse_set_mark (WPanel * panel)
{
    if (mouse_mark_panel == panel)
    {
        const file_entry_t *fe;

        fe = panel_current_entry (panel);

        if (mouse_marking && fe->f.marked == 0)
            do_mark_file (panel, MARK_DONT_MOVE);
        else if (!mouse_marking && fe->f.marked != 0)
            do_mark_file (panel, MARK_DONT_MOVE);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
mark_if_marking (WPanel * panel, const mouse_event_t * event, int previous_current)
{
    if ((event->buttons & GPM_B_RIGHT) == 0)
        return;

    if (event->msg == MSG_MOUSE_DOWN)
        mouse_toggle_mark (panel);
    else
    {
        int pcurr, curr1, curr2;

        pcurr = panel->current;
        curr1 = MIN (previous_current, panel->current);
        curr2 = MAX (previous_current, panel->current);

        for (; curr1 <= curr2; curr1++)
        {
            panel->current = curr1;
            mouse_set_mark (panel);
        }

        panel->current = pcurr;
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Determine which column was clicked, and sort the panel on
 * that column, or reverse sort on that column if already
 * sorted on that column.
 */

static void
mouse_sort_col (WPanel * panel, int x)
{
    int i = 0;
    GSList *format;
    const char *lc_sort_name = NULL;
    panel_field_t *col_sort_format = NULL;

    for (format = panel->format; format != NULL; format = g_slist_next (format))
    {
        format_item_t *fi = (format_item_t *) format->data;

        i += fi->field_len;
        if (x < i + 1)
        {
            /* found column */
            lc_sort_name = fi->title;
            break;
        }
    }

    if (lc_sort_name == NULL)
        return;

    for (i = 0; panel_fields[i].id != NULL; i++)
    {
        const char *title;

        title = panel_get_title_without_hotkey (panel_fields[i].title_hotkey);
        if (panel_fields[i].sort_routine != NULL && strcmp (title, lc_sort_name) == 0)
        {
            col_sort_format = &panel_fields[i];
            break;
        }
    }

    if (col_sort_format != NULL)
    {
        if (panel->sort_field == col_sort_format)
            /* reverse the sort if clicked column is already the sorted column */
            panel->sort_info.reverse = !panel->sort_info.reverse;
        else
            /* new sort is forced to be ascending */
            panel->sort_info.reverse = FALSE;

        panel_set_sort_order (panel, col_sort_format);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
panel_mouse_is_on_item (const WPanel * panel, int y, int x)
{
    int lines, col_width, col;

    if (y < 0)
        return MOUSE_UPPER_FILE_LIST;

    lines = panel_lines (panel);
    if (y >= lines)
        return MOUSE_BELOW_FILE_LIST;

    col_width = (CONST_WIDGET (panel)->rect.cols - 2) / panel->list_cols;
    /* column where mouse is */
    col = x / col_width;

    y += panel->top + lines * col;

    /* are we below or in the next column of last file? */
    if (y > panel->dir.len)
        return MOUSE_AFTER_LAST_FILE;

    /* we are on item of the file file; return an index to select a file */
    return y;
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_mouse_callback (Widget * w, mouse_msg_t msg, mouse_event_t * event)
{
    WPanel *panel = PANEL (w);
    gboolean is_active;

    is_active = widget_is_active (w);

    switch (msg)
    {
    case MSG_MOUSE_DOWN:
        if (event->y == 0)
        {
            /* top frame */
            if (event->x == 1)
                /* "<" button */
                directory_history_prev (panel);
            else if (event->x == w->rect.cols - 2)
                /* ">" button */
                directory_history_next (panel);
            else if (event->x >= w->rect.cols - 5 && event->x <= w->rect.cols - 3)
                /* "^" button */
                directory_history_list (panel);
            else if (event->x == w->rect.cols - 6)
                /* "." button show/hide hidden files */
                send_message (filemanager, NULL, MSG_ACTION, CK_ShowHidden, NULL);
            else
            {
                /* no other events on 1st line, return MOU_UNHANDLED */
                event->result.abort = TRUE;
                /* avoid extra panel redraw */
                panel->dirty = FALSE;
            }
            break;
        }

        if (event->y == 1)
        {
            /* sort on clicked column */
            mouse_sort_col (panel, event->x + 1);
            break;
        }

        if (!is_active)
            (void) change_panel ();
        MC_FALLTHROUGH;

    case MSG_MOUSE_DRAG:
        {
            int my_index;
            int previous_current;

            my_index = panel_mouse_is_on_item (panel, event->y - 2, event->x);
            previous_current = panel->current;

            switch (my_index)
            {
            case MOUSE_UPPER_FILE_LIST:
                move_up (panel);
                mark_if_marking (panel, event, previous_current);
                break;

            case MOUSE_BELOW_FILE_LIST:
                move_down (panel);
                mark_if_marking (panel, event, previous_current);
                break;

            case MOUSE_AFTER_LAST_FILE:
                break;          /* do nothing */

            default:
                if (my_index != panel->current)
                {
                    unselect_item (panel);
                    panel->current = my_index;
                    select_item (panel);
                }

                mark_if_marking (panel, event, previous_current);
                break;
            }
        }
        break;

    case MSG_MOUSE_UP:
        break;

    case MSG_MOUSE_CLICK:
        if ((event->count & GPM_DOUBLE) != 0 && (event->buttons & GPM_B_LEFT) != 0 &&
            panel_mouse_is_on_item (panel, event->y - 2, event->x) >= 0)
            do_enter (panel);
        break;

    case MSG_MOUSE_MOVE:
        break;

    case MSG_MOUSE_SCROLL_UP:
        if (is_active)
        {
            if (panels_options.mouse_move_pages && panel->top > 0)
                prev_page (panel);
            else                /* We are in first page */
                move_up (panel);
        }
        break;

    case MSG_MOUSE_SCROLL_DOWN:
        if (is_active)
        {
            if (panels_options.mouse_move_pages
                && panel->top + panel_items (panel) < panel->dir.len)
                next_page (panel);
            else                /* We are in last page */
                move_down (panel);
        }
        break;

    default:
        break;
    }

    if (panel->dirty)
        widget_draw (w);
}

/* --------------------------------------------------------------------------------------------- */

static void
reload_panelized (WPanel * panel)
{
    int i, j;
    dir_list *list = &panel->dir;

    /* refresh current VFS directory required for vfs_path_from_str() */
    (void) mc_chdir (panel->cwd_vpath);

    for (i = 0, j = 0; i < list->len; i++)
    {
        vfs_path_t *vpath;

        vpath = vfs_path_from_str (list->list[i].fname->str);
        if (mc_lstat (vpath, &list->list[i].st) != 0)
            g_string_free (list->list[i].fname, TRUE);
        else
        {
            if (j != i)
                list->list[j] = list->list[i];
            j++;
        }
        vfs_path_free (vpath, TRUE);
    }
    if (j == 0)
        dir_list_init (list);
    else
        list->len = j;

    recalculate_panel_summary (panel);

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
        mc_setctl (panel->cwd_vpath, VFS_SETCTL_FLUSH, NULL);
        memset (&(panel->dir_stat), 0, sizeof (panel->dir_stat));
    }

    /* If current_file == -1 (an invalid pointer) then preserve current */
    free_pointer = current_file == UP_KEEPSEL;

    if (free_pointer)
    {
        const GString *fname;

        fname = panel_current_entry (panel)->fname;
        my_current_file = g_strndup (fname->str, fname->len);
        current_file = my_current_file;
    }

    if (panel->is_panelized)
        reload_panelized (panel);
    else
        panel_reload (panel);

    panel_set_current_by_name (panel, current_file);
    panel->dirty = TRUE;

    if (free_pointer)
        g_free (my_current_file);
}

/* --------------------------------------------------------------------------------------------- */

static void
update_one_panel (int which, panel_update_flags_t flags, const char *current_file)
{
    if (get_panel_type (which) == view_listing)
    {
        WPanel *panel;

        panel = PANEL (get_panel_widget (which));
        if (panel->is_panelized)
            flags &= ~UP_RELOAD;
        update_one_panel_widget (panel, flags, current_file);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_set_current (WPanel * panel, int i)
{
    if (i != panel->current)
    {
        panel->dirty = TRUE;
        panel->current = i;
        panel->top = panel->current - (WIDGET (panel)->rect.lines - 2) / 2;
        if (panel->top < 0)
            panel->top = 0;
    }
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
panel_save_current_file_to_clip_file (const gchar * event_group_name, const gchar * event_name,
                                      gpointer init_data, gpointer data)
{
    (void) event_group_name;
    (void) event_name;
    (void) init_data;
    (void) data;

    if (current_panel->marked == 0)
        mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_text_to_file",
                        (gpointer) panel_current_entry (current_panel)->fname->str);
    else
    {
        int i;
        gboolean first = TRUE;
        char *flist = NULL;

        for (i = 0; i < current_panel->dir.len; i++)
        {
            const file_entry_t *fe = &current_panel->dir.list[i];

            if (fe->f.marked != 0)
            {                   /* Skip the unmarked ones */
                if (first)
                {
                    flist = g_strndup (fe->fname->str, fe->fname->len);
                    first = FALSE;
                }
                else
                {
                    /* Add empty lines after the file */
                    char *tmp;

                    tmp = g_strconcat (flist, "\n", fe->fname->str, (char *) NULL);
                    g_free (flist);
                    flist = tmp;
                }
            }
        }

        mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_text_to_file", (gpointer) flist);
        g_free (flist);
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static vfs_path_t *
panel_recursive_cd_to_parent (const vfs_path_t * vpath)
{
    vfs_path_t *cwd_vpath;

    cwd_vpath = vfs_path_clone (vpath);

    while (mc_chdir (cwd_vpath) < 0)
    {
        const char *panel_cwd_path;
        vfs_path_t *tmp_vpath;

        /* check if path contains only '/' */
        panel_cwd_path = vfs_path_as_str (cwd_vpath);
        if (panel_cwd_path != NULL && IS_PATH_SEP (panel_cwd_path[0]) && panel_cwd_path[1] == '\0')
        {
            vfs_path_free (cwd_vpath, TRUE);
            return NULL;
        }

        tmp_vpath = vfs_path_vtokens_get (cwd_vpath, 0, -1);
        vfs_path_free (cwd_vpath, TRUE);
        cwd_vpath =
            vfs_path_build_filename (PATH_SEP_STR, vfs_path_as_str (tmp_vpath), (char *) NULL);
        vfs_path_free (tmp_vpath, TRUE);
    }

    return cwd_vpath;
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_dir_list_callback (dir_list_cb_state_t state, void *data)
{
    static int count = 0;

    (void) data;

    switch (state)
    {
    case DIR_OPEN:
        count = 0;
        break;

    case DIR_READ:
        count++;
        if ((count & 15) == 0)
            rotate_dash (TRUE);
        break;

    case DIR_CLOSE:
        rotate_dash (FALSE);
        break;

    default:
        g_assert_not_reached ();
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
panel_set_current_by_name (WPanel * panel, const char *name)
{
    int i;
    char *subdir;

    if (name == NULL)
    {
        panel_set_current (panel, 0);
        return;
    }

    /* We only want the last component of the directory,
     * and from this only the name without suffix.
     * Cut prefix if the panel is not panelized */
    if (panel->is_panelized)
        subdir = vfs_strip_suffix_from_filename (name);
    else
        subdir = vfs_strip_suffix_from_filename (x_basename (name));

    /* Search that subdir or filename without prefix (if not panelized panel),
       make it current if found */
    for (i = 0; i < panel->dir.len; i++)
        if (strcmp (subdir, panel->dir.list[i].fname->str) == 0)
        {
            panel_set_current (panel, i);
            g_free (subdir);
            return;
        }

    /* Make current near the filee that is missing */
    if (panel->current >= panel->dir.len)
        panel_set_current (panel, panel->dir.len - 1);
    g_free (subdir);

    select_item (panel);
}

/* --------------------------------------------------------------------------------------------- */

void
panel_clean_dir (WPanel * panel)
{
    panel->top = 0;
    panel->current = 0;
    panel->marked = 0;
    panel->dirs_marked = 0;
    panel->total = 0;
    panel->quick_search.active = FALSE;
    panel->is_panelized = FALSE;
    panel->dirty = TRUE;
    panel->content_shift = -1;
    panel->max_shift = -1;

    dir_list_free_list (&panel->dir);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Set Up panel's current dir object
 *
 * @param panel panel object
 * @param path_str string contain path
 */

void
panel_set_cwd (WPanel * panel, const vfs_path_t * vpath)
{
    if (vpath != panel->cwd_vpath)      /* check if new vpath is not the panel->cwd_vpath object */
    {
        vfs_path_free (panel->cwd_vpath, TRUE);
        panel->cwd_vpath = vfs_path_clone (vpath);
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Set Up panel's last working dir object
 *
 * @param panel panel object
 * @param path_str string contain path
 */

void
panel_set_lwd (WPanel * panel, const vfs_path_t * vpath)
{
    if (vpath != panel->lwd_vpath)      /* check if new vpath is not the panel->lwd_vpath object */
    {
        vfs_path_free (panel->lwd_vpath, TRUE);
        panel->lwd_vpath = vfs_path_clone (vpath);
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Creatie an empty panel with specified size.
 *
 * @param panel_name name of panel for setup receiving
 *
 * @return new instance of WPanel
 */

WPanel *
panel_sized_empty_new (const char *panel_name, int y, int x, int lines, int cols)
{
    WRect r = { y, x, lines, cols };
    WPanel *panel;
    Widget *w;
    char *section;
    int i, err;

    panel = g_new0 (WPanel, 1);
    w = WIDGET (panel);
    widget_init (w, &r, panel_callback, panel_mouse_callback);
    w->options |= WOP_SELECTABLE | WOP_TOP_SELECT;
    w->keymap = panel_map;

    panel->dir.size = DIR_LIST_MIN_SIZE;
    panel->dir.list = g_new (file_entry_t, panel->dir.size);
    panel->dir.len = 0;
    panel->dir.callback = panel_dir_list_callback;

    panel->list_cols = 1;
    panel->brief_cols = 2;
    panel->dirty = TRUE;
    panel->content_shift = -1;
    panel->max_shift = -1;

    panel->list_format = list_full;
    panel->user_format = g_strdup (DEFAULT_USER_FORMAT);

    panel->filter.flags = FILE_FILTER_DEFAULT_FLAGS;

    for (i = 0; i < LIST_FORMATS; i++)
        panel->user_status_format[i] = g_strdup (DEFAULT_USER_FORMAT);

#ifdef HAVE_CHARSET
    panel->codepage = SELECT_CHARSET_NO_TRANSLATE;
#endif

    panel->frame_size = frame_half;

    panel->quick_search.buffer = g_string_sized_new (MC_MAXFILENAMELEN);
    panel->quick_search.prev_buffer = g_string_sized_new (MC_MAXFILENAMELEN);

    panel->name = g_strdup (panel_name);
    panel->dir_history.name = g_strconcat ("Dir Hist ", panel->name, (char *) NULL);
    /* directories history will be get later */

    section = g_strconcat ("Temporal:", panel->name, (char *) NULL);
    if (!mc_config_has_group (mc_global.main_config, section))
    {
        g_free (section);
        section = g_strdup (panel->name);
    }
    panel_load_setup (panel, section);
    g_free (section);

    if (panel->filter.value != NULL)
    {
        gboolean case_sens = (panel->filter.flags & SELECT_MATCH_CASE) != 0;
        gboolean shell_patterns = (panel->filter.flags & SELECT_SHELL_PATTERNS) != 0;

        panel->filter.handler = mc_search_new (panel->filter.value, NULL);
        panel->filter.handler->search_type = shell_patterns ? MC_SEARCH_T_GLOB : MC_SEARCH_T_REGEX;
        panel->filter.handler->is_entire_line = TRUE;
        panel->filter.handler->is_case_sensitive = case_sens;

        /* FIXME: silent check -- do not display an error message */
        if (!mc_search_prepare (panel->filter.handler))
            file_filter_clear (&panel->filter);
    }

    /* Load format strings */
    err = set_panel_formats (panel);
    if (err != 0)
        set_panel_formats (panel);

    return panel;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Panel creation for specified size and directory.
 *
 * @param panel_name name of panel for setup retrieving
 * @param y y coordinate of top-left corner
 * @param x x coordinate of top-left corner
 * @param lines vertical size
 * @param cols horizontal size
 * @param vpath working panel directory. If NULL then current directory is used
 *
 * @return new instance of WPanel
 */

WPanel *
panel_sized_with_dir_new (const char *panel_name, int y, int x, int lines, int cols,
                          const vfs_path_t * vpath)
{
    WPanel *panel;
    char *curdir = NULL;
#ifdef HAVE_CHARSET
    const vfs_path_element_t *path_element;
#endif

    panel = panel_sized_empty_new (panel_name, y, x, lines, cols);

    if (vpath != NULL)
    {
        curdir = vfs_get_cwd ();
        panel_set_cwd (panel, vpath);
    }
    else
    {
        vfs_setup_cwd ();
        panel->cwd_vpath = vfs_path_clone (vfs_get_raw_current_dir ());
    }

    panel_set_lwd (panel, vfs_get_raw_current_dir ());

#ifdef HAVE_CHARSET
    path_element = vfs_path_get_by_index (panel->cwd_vpath, -1);
    if (path_element->encoding != NULL)
        panel->codepage = get_codepage_index (path_element->encoding);
#endif

    if (mc_chdir (panel->cwd_vpath) != 0)
    {
#ifdef HAVE_CHARSET
        panel->codepage = SELECT_CHARSET_NO_TRANSLATE;
#endif
        vfs_setup_cwd ();
        vfs_path_free (panel->cwd_vpath, TRUE);
        panel->cwd_vpath = vfs_path_clone (vfs_get_raw_current_dir ());
    }

    /* Load the default format */
    if (!dir_list_load (&panel->dir, panel->cwd_vpath, panel->sort_field->sort_routine,
                        &panel->sort_info, &panel->filter))
        message (D_ERROR, MSG_ERROR, _("Cannot read directory contents"));

    /* Restore old right path */
    if (curdir != NULL)
    {
        vfs_path_t *tmp_vpath;
        int err;

        tmp_vpath = vfs_path_from_str (curdir);
        mc_chdir (tmp_vpath);
        vfs_path_free (tmp_vpath, TRUE);
        (void) err;
    }
    g_free (curdir);

    return panel;
}

/* --------------------------------------------------------------------------------------------- */

void
panel_reload (WPanel * panel)
{
    struct stat current_stat;
    vfs_path_t *cwd_vpath;

    if (panels_options.fast_reload && stat (vfs_path_as_str (panel->cwd_vpath), &current_stat) == 0
        && current_stat.st_ctime == panel->dir_stat.st_ctime
        && current_stat.st_mtime == panel->dir_stat.st_mtime)
        return;

    cwd_vpath = panel_recursive_cd_to_parent (panel->cwd_vpath);
    vfs_path_free (panel->cwd_vpath, TRUE);

    if (cwd_vpath == NULL)
    {
        panel->cwd_vpath = vfs_path_from_str (PATH_SEP_STR);
        panel_clean_dir (panel);
        dir_list_init (&panel->dir);
        return;
    }

    panel->cwd_vpath = cwd_vpath;
    memset (&(panel->dir_stat), 0, sizeof (panel->dir_stat));
    show_dir (panel);

    if (!dir_list_reload (&panel->dir, panel->cwd_vpath, panel->sort_field->sort_routine,
                          &panel->sort_info, &panel->filter))
        message (D_ERROR, MSG_ERROR, _("Cannot read directory contents"));

    panel->dirty = TRUE;
    if (panel->current >= panel->dir.len)
        panel_set_current (panel, panel->dir.len - 1);

    recalculate_panel_summary (panel);
}

/* --------------------------------------------------------------------------------------------- */
/* Switches the panel to the mode specified in the format           */
/* Setting up both format and status string. Return: 0 - on success; */
/* 1 - format error; 2 - status error; 3 - errors in both formats.  */

int
set_panel_formats (WPanel * p)
{
    GSList *form;
    char *err = NULL;
    int retcode = 0;

    form = use_display_format (p, panel_format (p), &err, FALSE);

    if (err != NULL)
    {
        g_free (err);
        retcode = 1;
    }
    else
    {
        g_slist_free_full (p->format, (GDestroyNotify) format_item_free);
        p->format = form;
    }

    if (panels_options.show_mini_info)
    {
        form = use_display_format (p, mini_status_format (p), &err, TRUE);

        if (err != NULL)
        {
            g_free (err);
            retcode += 2;
        }
        else
        {
            g_slist_free_full (p->status_format, (GDestroyNotify) format_item_free);
            p->status_format = form;
        }
    }

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
        g_free (p->user_status_format[p->list_format]);
        p->user_status_format[p->list_format] = g_strdup (DEFAULT_USER_FORMAT);
    }

    return retcode;
}

/* --------------------------------------------------------------------------------------------- */

void
panel_set_filter (WPanel * panel, const file_filter_t * filter)
{
    MC_PTR_FREE (panel->filter.value);
    mc_search_free (panel->filter.handler);
    panel->filter.handler = NULL;

    /* NULL to clear filter */
    if (filter != NULL)
        panel->filter = *filter;

    reread_cmd ();
}

/* --------------------------------------------------------------------------------------------- */

/* Select current item and readjust the panel */
void
select_item (WPanel * panel)
{
    adjust_top_file (panel);

    panel->dirty = TRUE;

    execute_hooks (select_file_hook);
}

/* --------------------------------------------------------------------------------------------- */
/** Clears all files in the panel, used only when one file was marked */
void
unmark_files (WPanel * panel)
{
    if (panel->marked != 0)
    {
        int i;

        for (i = 0; i < panel->dir.len; i++)
            file_mark (panel, i, 0);

        panel->dirs_marked = 0;
        panel->marked = 0;
        panel->total = 0;
    }
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

    for (i = 0; i < panel->dir.len; i++)
        if (panel->dir.list[i].f.marked != 0)
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
    if (DIR_IS_DOTDOT (panel->dir.list[idx].fname->str))
        return;

    file_mark (panel, idx, mark);
    if (panel->dir.list[idx].f.marked != 0)
    {
        panel->marked++;

        if (S_ISDIR (panel->dir.list[idx].st.st_mode))
        {
            if (panel->dir.list[idx].f.dir_size_computed != 0)
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
            if (panel->dir.list[idx].f.dir_size_computed != 0)
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
panel_do_cd (WPanel * panel, const vfs_path_t * new_dir_vpath, enum cd_enum cd_type)
{
    gboolean r;

    r = panel_do_cd_int (panel, new_dir_vpath, cd_type);
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
        panel->dirty = TRUE;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
panel_re_sort (WPanel * panel)
{
    char *filename;
    const file_entry_t *fe;
    int i;

    if (panel == NULL)
        return;

    fe = panel_current_entry (panel);
    filename = g_strndup (fe->fname->str, fe->fname->len);
    unselect_item (panel);
    dir_list_sort (&panel->dir, panel->sort_field->sort_routine, &panel->sort_info);
    panel->current = -1;

    for (i = panel->dir.len; i != 0; i--)
        if (strcmp (panel->dir.list[i - 1].fname->str, filename) == 0)
        {
            panel->current = i - 1;
            break;
        }

    g_free (filename);
    panel->top = panel->current - panel_items (panel) / 2;
    select_item (panel);
    panel->dirty = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
panel_set_sort_order (WPanel * panel, const panel_field_t * sort_order)
{
    if (sort_order == NULL)
        return;

    panel->sort_field = sort_order;

    /* The directory is already sorted, we have to load the unsorted stuff */
    if (sort_order->sort_routine == (GCompareFunc) unsorted)
    {
        char *current_file;
        const GString *fname;

        fname = panel_current_entry (panel)->fname;
        current_file = g_strndup (fname->str, fname->len);
        panel_reload (panel);
        panel_set_current_by_name (panel, current_file);
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
        panel_do_cd (panel, cd_path_vpath, cd_parse_command);
        show_dir (panel);
        vfs_path_free (cd_path_vpath, TRUE);
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
        vfs_path_change_encoding (panel->cwd_vpath, encoding);

        if (!panel_do_cd (panel, panel->cwd_vpath, cd_parse_command))
            cd_error_message (vfs_path_as_str (panel->cwd_vpath));
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

    ret_vpath = vfs_path_new (FALSE);

    tmp_conv = g_string_new ("");

    for (indx = 0; indx < vfs_path_elements_count (vpath); indx++)
    {
        GIConv converter;
        vfs_path_element_t *path_element;

        path_element = vfs_path_element_clone (vfs_path_get_by_index (vpath, indx));

        if (path_element->encoding == NULL)
        {
            vfs_path_add_element (ret_vpath, path_element);
            continue;
        }

        converter = str_crt_conv_to (path_element->encoding);
        if (converter == INVALID_CONV)
        {
            vfs_path_add_element (ret_vpath, path_element);
            continue;
        }

        MC_PTR_FREE (path_element->encoding);

        str_vfs_convert_from (converter, path_element->path, tmp_conv);

        g_free (path_element->path);
        path_element->path = g_strndup (tmp_conv->str, tmp_conv->len);

        g_string_set_size (tmp_conv, 0);

        str_close_conv (converter);
        str_close_conv (path_element->dir.converter);
        path_element->dir.converter = INVALID_CONV;
        vfs_path_add_element (ret_vpath, path_element);
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
 * other_file to the current files in the panels.
 *
 * If flags has the UP_ONLY_CURRENT bit toggled on, then it
 * will not reload the other panel.
 *
 * @param flags for reload panel
 * @param current_file name of the current file
 */

void
update_panels (panel_update_flags_t flags, const char *current_file)
{
    WPanel *panel;

    /* first, update other panel... */
    if ((flags & UP_ONLY_CURRENT) == 0)
        update_one_panel (get_other_index (), flags, UP_KEEPSEL);
    /* ...then current one */
    update_one_panel (get_current_index (), flags, current_file);

    if (get_current_type () == view_listing)
        panel = PANEL (get_panel_widget (get_current_index ()));
    else
        panel = PANEL (get_panel_widget (get_other_index ()));

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

char **
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

    return ret;
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

    for (lc_index = 0; panel_fields[lc_index].id != NULL; lc_index++)
    {
        const char *title;

        title = panel_get_title_without_hotkey (panel_fields[lc_index].title_hotkey);
        if (strcmp (title, name) == 0)
            return &panel_fields[lc_index];
    }

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

char **
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

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

void
panel_panelize_cd (void)
{
    WPanel *panel;
    int i;
    dir_list *list;
    panelized_descr_t *pdescr;
    dir_list *plist;
    gboolean panelized_same;

    if (!SELECTED_IS_PANEL)
        create_panel (MENU_PANEL_IDX, view_listing);

    panel = PANEL (get_panel_widget (MENU_PANEL_IDX));

    dir_list_clean (&panel->dir);

    if (panel->panelized_descr == NULL)
        panel->panelized_descr = panelized_descr_new ();

    pdescr = panel->panelized_descr;
    plist = &pdescr->list;

    if (pdescr->root_vpath == NULL)
        panel_panelize_change_root (panel, panel->cwd_vpath);

    if (plist->len < 1)
        dir_list_init (plist);
    else if (plist->len > panel->dir.size)
        dir_list_grow (&panel->dir, plist->len - panel->dir.size);

    list = &panel->dir;
    list->len = plist->len;

    panelized_same = vfs_path_equal (pdescr->root_vpath, panel->cwd_vpath);

    for (i = 0; i < plist->len; i++)
    {
        if (panelized_same || DIR_IS_DOTDOT (plist->list[i].fname->str))
            list->list[i].fname = mc_g_string_dup (plist->list[i].fname);
        else
        {
            vfs_path_t *tmp_vpath;

            tmp_vpath =
                vfs_path_append_new (pdescr->root_vpath, plist->list[i].fname->str, (char *) NULL);
            list->list[i].fname = g_string_new_take (vfs_path_free (tmp_vpath, FALSE));
        }
        list->list[i].f.link_to_dir = plist->list[i].f.link_to_dir;
        list->list[i].f.stale_link = plist->list[i].f.stale_link;
        list->list[i].f.dir_size_computed = plist->list[i].f.dir_size_computed;
        list->list[i].f.marked = plist->list[i].f.marked;
        list->list[i].st = plist->list[i].st;
        list->list[i].name_sort_key = plist->list[i].name_sort_key;
        list->list[i].extension_sort_key = plist->list[i].extension_sort_key;
    }

    panel->is_panelized = TRUE;
    panel_panelize_absolutize_if_needed (panel);

    panel_set_current_by_name (panel, NULL);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Change root directory of panelized content.
 * @param panel file panel
 * @param new_root new path
 */
void
panel_panelize_change_root (WPanel * panel, const vfs_path_t * new_root)
{
    if (panel->panelized_descr == NULL)
        panel->panelized_descr = panelized_descr_new ();
    else
        vfs_path_free (panel->panelized_descr->root_vpath, TRUE);

    panel->panelized_descr->root_vpath = vfs_path_clone (new_root);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Conditionally switches a panel's directory to "/" (root).
 *
 * If a panelized panel's listing contain absolute paths, this function
 * sets the panel's directory to "/". Otherwise it does nothing.
 *
 * Rationale:
 *
 * This makes tokenized strings like "%d/%p" work. This also makes other
 * places work where such naive concatenation is done in code (e.g., when
 * pressing ctrl+shift+enter, for CK_PutCurrentFullSelected).
 *
 * When to call:
 *
 * You should always call this function after you populate the listing
 * of a panelized panel.
 */
void
panel_panelize_absolutize_if_needed (WPanel * panel)
{
    const dir_list *const list = &panel->dir;

    /* Note: We don't support mixing of absolute and relative paths, which is
     * why it's ok for us to check only the 1st entry. */
    if (list->len > 1 && g_path_is_absolute (list->list[1].fname->str))
    {
        vfs_path_t *root;

        root = vfs_path_from_str (PATH_SEP_STR);
        panel_set_cwd (panel, root);
        if (panel == current_panel)
            mc_chdir (root);
        vfs_path_free (root, TRUE);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
panel_panelize_save (WPanel * panel)
{
    int i;
    dir_list *list = &panel->dir;
    dir_list *plist;

    panel_panelize_change_root (panel, panel->cwd_vpath);

    plist = &panel->panelized_descr->list;

    if (plist->len > 0)
        dir_list_clean (plist);
    if (panel->dir.len == 0)
        return;

    if (panel->dir.len > plist->size)
        dir_list_grow (plist, panel->dir.len - plist->size);
    plist->len = panel->dir.len;

    for (i = 0; i < panel->dir.len; i++)
    {
        plist->list[i].fname = mc_g_string_dup (list->list[i].fname);
        plist->list[i].f.link_to_dir = list->list[i].f.link_to_dir;
        plist->list[i].f.stale_link = list->list[i].f.stale_link;
        plist->list[i].f.dir_size_computed = list->list[i].f.dir_size_computed;
        plist->list[i].f.marked = list->list[i].f.marked;
        plist->list[i].st = list->list[i].st;
        plist->list[i].name_sort_key = list->list[i].name_sort_key;
        plist->list[i].extension_sort_key = list->list[i].extension_sort_key;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
panel_init (void)
{
    panel_sort_up_char = mc_skin_get ("widget-panel", "sort-up-char", "'");
    panel_sort_down_char = mc_skin_get ("widget-panel", "sort-down-char", ".");
    panel_hiddenfiles_show_char = mc_skin_get ("widget-panel", "hiddenfiles-show-char", ".");
    panel_hiddenfiles_hide_char = mc_skin_get ("widget-panel", "hiddenfiles-hide-char", ".");
    panel_history_prev_item_char = mc_skin_get ("widget-panel", "history-prev-item-char", "<");
    panel_history_next_item_char = mc_skin_get ("widget-panel", "history-next-item-char", ">");
    panel_history_show_list_char = mc_skin_get ("widget-panel", "history-show-list-char", "^");
    panel_filename_scroll_left_char =
        mc_skin_get ("widget-panel", "filename-scroll-left-char", "{");
    panel_filename_scroll_right_char =
        mc_skin_get ("widget-panel", "filename-scroll-right-char", "}");

    string_file_name_buffer = g_string_sized_new (MC_MAXFILENAMELEN);

    mc_event_add (MCEVENT_GROUP_FILEMANAGER, "update_panels", event_update_panels, NULL, NULL);
    mc_event_add (MCEVENT_GROUP_FILEMANAGER, "panel_save_current_file_to_clip_file",
                  panel_save_current_file_to_clip_file, NULL, NULL);
}

/* --------------------------------------------------------------------------------------------- */

void
panel_deinit (void)
{
    g_free (panel_sort_up_char);
    g_free (panel_sort_down_char);
    g_free (panel_hiddenfiles_show_char);
    g_free (panel_hiddenfiles_hide_char);
    g_free (panel_history_prev_item_char);
    g_free (panel_history_next_item_char);
    g_free (panel_history_show_list_char);
    g_free (panel_filename_scroll_left_char);
    g_free (panel_filename_scroll_right_char);
    g_string_free (string_file_name_buffer, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
panel_cd (WPanel * panel, const vfs_path_t * new_dir_vpath, enum cd_enum exact)
{
    gboolean res;
    const vfs_path_t *_new_dir_vpath = new_dir_vpath;

    if (panel->is_panelized)
    {
        size_t new_vpath_len;

        new_vpath_len = vfs_path_len (new_dir_vpath);
        if (vfs_path_equal_len (new_dir_vpath, panel->panelized_descr->root_vpath, new_vpath_len))
            _new_dir_vpath = panel->panelized_descr->root_vpath;
    }

    res = panel_do_cd (panel, _new_dir_vpath, exact);

#ifdef HAVE_CHARSET
    if (res)
    {
        const vfs_path_element_t *path_element;

        path_element = vfs_path_get_by_index (panel->cwd_vpath, -1);
        if (path_element->encoding != NULL)
            panel->codepage = get_codepage_index (path_element->encoding);
        else
            panel->codepage = SELECT_CHARSET_NO_TRANSLATE;
    }
#endif /* HAVE_CHARSET */

    return res;
}

/* --------------------------------------------------------------------------------------------- */
