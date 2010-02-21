/* Panel managing.
   Copyright (C) 1994, 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2009 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   Written by: 1995 Miguel de Icaza
         1997, 1999 Timur Bakeyev

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/** \file screen.c
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
#include "lib/skin.h"
#include "lib/strescape.h"
#include "lib/tty/mouse.h"		/* For Gpm_Event */
#include "lib/tty/key.h"		/* XCTRL and ALT macros  */
#include "lib/filehighlight.h"
#include "lib/mcconfig.h"
#include "lib/vfs/mc-vfs/vfs.h"
#include "lib/unixcompat.h"

#include "dir.h"
#include "panel.h"
#include "boxes.h"
#include "tree.h"
#include "ext.h"		/* regexp_command */
#include "layout.h"		/* Most layout variables are here */
#include "wtools.h"		/* for message (...) */
#include "cmd.h"
#include "command.h"		/* cmdline */
#include "setup.h"		/* For loading/saving panel options */
#include "user.h"
#include "execute.h"
#include "widget.h"
#include "menu.h"		/* menubar_visible */
#include "main-widgets.h"
#include "main.h"
#include "mountlist.h"		/* my_statfs */
#include "selcodepage.h"	/* select_charset () */
#include "charsets.h"		/* get_codepage_id () */
#include "cmddef.h"		/* CK_ cmd name const */
#include "keybind.h"		/* global_keymap_t */

#define ELEMENTS(arr) ( sizeof(arr) / sizeof((arr)[0]) )

#define NORMAL		0
#define SELECTED	1
#define MARKED		2
#define MARKED_SELECTED	3
#define STATUS		5

/*
 * This describes a format item.  The parse_display_format routine parses
 * the user specified format and creates a linked list of format_e structures.
 */
typedef struct format_e {
    struct format_e *next;
    int    requested_field_len;
    int    field_len;
    align_crt_t just_mode;
    int    expand;
    const char *(*string_fn)(file_entry *, int len);
    char   *title;
    const char   *id;
} format_e;

/* If true, show the mini-info on the panel */
int show_mini_info = 1;

/* If true, then use stat() on the cwd to determine directory changes */
int fast_reload = 0;

/* If true, use some usability hacks by Torben */
int torben_fj_mode = 0;

/* If true, up/down keys scroll the pane listing by pages */
int panel_scroll_pages = 1;

/* If 1, we use permission hilighting */
int permission_mode = 0;

/* If 1 - then add per file type hilighting */
int filetype_mode = 1;

/* The hook list for the select file function */
Hook *select_file_hook = 0;

const global_keymap_t *panel_map;

static cb_ret_t panel_callback (Widget *, widget_msg_t msg, int parm);
static int panel_event (Gpm_Event *event, void *);
static void paint_frame (WPanel *panel);
static const char *panel_format (WPanel *panel);
static const char *mini_status_format (WPanel *panel);

static char *panel_sort_up_sign = NULL;
static char *panel_sort_down_sign = NULL;

static char *panel_hiddenfiles_sign_show = NULL;
static char *panel_hiddenfiles_sign_hide = NULL;
static char *panel_history_prev_item_sign = NULL;
static char *panel_history_next_item_sign = NULL;
static char *panel_history_show_list_sign = NULL;

/* This macro extracts the number of available lines in a panel */
#define llines(p) (p->widget.lines-3 - (show_mini_info ? 2 : 0))

static void
set_colors (WPanel *panel)
{
    (void) panel;
    tty_set_normal_attrs ();
    tty_setcolor (NORMAL_COLOR);
}

/* Delete format string, it is a linked list */
static void
delete_format (format_e *format)
{
    while (format != NULL) {
        format_e *next = format->next;
        g_free (format->title);
        g_free (format);
        format = next;
     }
}

/* This code relies on the default justification!!! */
static void
add_permission_string (char *dest, int width, file_entry *fe, int attr, int color, int is_octal)
{
    int i, r, l;

    l = get_user_permissions (&fe->st);

    if (is_octal){
	/* Place of the access bit in octal mode */
        l = width + l - 3;
	r = l + 1;
    } else {
	/* The same to the triplet in string mode */
        l = l * 3 + 1;
	r = l + 3;
    }

    for(i = 0; i < width; i++){
	if (i >= l && i < r){
            if (attr == SELECTED || attr == MARKED_SELECTED)
                tty_setcolor (MARKED_SELECTED_COLOR);
            else
                tty_setcolor (MARKED_COLOR);
        } else if (color >= 0)
            tty_setcolor (color);
	else
            tty_lowlevel_setcolor (-color);

	tty_print_char (dest[i]);
    }
}

/* String representations of various file attributes */
/* name */
static const char *
string_file_name (file_entry *fe, int len)
{
    static char buffer [MC_MAXPATHLEN * MB_LEN_MAX + 1];

    (void) len;
    g_strlcpy (buffer, fe->fname, sizeof(buffer));
    return buffer;
}

static unsigned int ilog10(dev_t n)
{
    unsigned int digits = 0;
    do {
	digits++, n /= 10;
    } while (n != 0);
    return digits;
}

static void format_device_number (char *buf, size_t bufsize, dev_t dev)
{
    dev_t major_dev = major(dev);
    dev_t minor_dev = minor(dev);
    unsigned int major_digits = ilog10(major_dev);
    unsigned int minor_digits = ilog10(minor_dev);

    g_assert(bufsize >= 1);
    if (major_digits + 1 + minor_digits + 1 <= bufsize) {
        g_snprintf(buf, bufsize, "%lu,%lu", (unsigned long) major_dev,
                   (unsigned long) minor_dev);
    } else {
        g_strlcpy(buf, _("[dev]"), bufsize);
    }
}

/* size */
static const char *
string_file_size (file_entry *fe, int len)
{
    static char buffer [BUF_TINY];

    /* Don't ever show size of ".." since we don't calculate it */
    if (!strcmp (fe->fname, "..")) {
	return _("UP--DIR");
    }

#ifdef HAVE_STRUCT_STAT_ST_RDEV
    if (S_ISBLK (fe->st.st_mode) || S_ISCHR (fe->st.st_mode))
        format_device_number (buffer, len + 1, fe->st.st_rdev);
    else
#endif
    {
	size_trunc_len (buffer, (unsigned int) len, fe->st.st_size, 0);
    }
    return buffer;
}

/* bsize */
static const char *
string_file_size_brief (file_entry *fe, int len)
{
    if (S_ISLNK (fe->st.st_mode) && !fe->f.link_to_dir) {
	return _("SYMLINK");
    }

    if ((S_ISDIR (fe->st.st_mode) || fe->f.link_to_dir) && strcmp (fe->fname, "..")) {
	return _("SUB-DIR");
    }

    return string_file_size (fe, len);
}

/* This functions return a string representation of a file entry */
/* type */
static const char *
string_file_type (file_entry *fe, int len)
{
    static char buffer[2];

    (void) len;
    if (S_ISDIR (fe->st.st_mode))
	buffer[0] = PATH_SEP;
    else if (S_ISLNK (fe->st.st_mode)) {
	if (fe->f.link_to_dir)
	    buffer[0] = '~';
	else if (fe->f.stale_link)
	    buffer[0] = '!';
	else
	    buffer[0] = '@';
    } else if (S_ISCHR (fe->st.st_mode))
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
	buffer[0] = '?';	/* non-regular of unknown kind */
    else if (is_exe (fe->st.st_mode))
	buffer[0] = '*';
    else
	buffer[0] = ' ';
    buffer[1] = '\0';
    return buffer;
}

/* mtime */
static const char *
string_file_mtime (file_entry *fe, int len)
{
    (void) len;
    return file_date (fe->st.st_mtime);
}

/* atime */
static const char *
string_file_atime (file_entry *fe, int len)
{
    (void) len;
    return file_date (fe->st.st_atime);
}

/* ctime */
static const char *
string_file_ctime (file_entry *fe, int len)
{
    (void) len;
    return file_date (fe->st.st_ctime);
}

/* perm */
static const char *
string_file_permission (file_entry *fe, int len)
{
    (void) len;
    return string_perm (fe->st.st_mode);
}

/* mode */
static const char *
string_file_perm_octal (file_entry *fe, int len)
{
    static char buffer [10];

    (void) len;
    g_snprintf (buffer, sizeof (buffer), "0%06lo", (unsigned long) fe->st.st_mode);
    return buffer;
}

/* nlink */
static const char *
string_file_nlinks (file_entry *fe, int len)
{
    static char buffer[BUF_TINY];

    (void) len;
    g_snprintf (buffer, sizeof (buffer), "%16d", (int) fe->st.st_nlink);
    return buffer;
}

/* inode */
static const char *
string_inode (file_entry *fe, int len)
{
    static char buffer [10];

    (void) len;
    g_snprintf (buffer, sizeof (buffer), "%lu",
		(unsigned long) fe->st.st_ino);
    return buffer;
}

/* nuid */
static const char *
string_file_nuid (file_entry *fe, int len)
{
    static char buffer [10];

    (void) len;
    g_snprintf (buffer, sizeof (buffer), "%lu",
		(unsigned long) fe->st.st_uid);
    return buffer;
}

/* ngid */
static const char *
string_file_ngid (file_entry *fe, int len)
{
    static char buffer [10];

    (void) len;
    g_snprintf (buffer, sizeof (buffer), "%lu",
		(unsigned long) fe->st.st_gid);
    return buffer;
}

/* owner */
static const char *
string_file_owner (file_entry *fe, int len)
{
    (void) len;
    return get_owner (fe->st.st_uid);
}

/* group */
static const char *
string_file_group (file_entry *fe, int len)
{
    (void) len;
    return get_group (fe->st.st_gid);
}

/* mark */
static const char *
string_marked (file_entry *fe, int len)
{
    (void) len;
    return fe->f.marked ? "*" : " ";
}

/* space */
static const char *
string_space (file_entry *fe, int len)
{
    (void) fe;
    (void) len;
    return " ";
}

/* dot */
static const char *
string_dot (file_entry *fe, int len)
{
    (void) fe;
    (void) len;
    return ".";
}

#define GT 1

panel_field_t panel_fields [] = {
    {
	"unsorted", 12, 1, J_LEFT_FIT,
	/* TRANSLATORS: one single character to represent 'unsorted' sort mode  */
	/* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
	N_("sort|u"),
	N_("&Unsorted"), TRUE, FALSE,
	string_file_name,
	(sortfn *) unsorted
    },
    {
	"name", 12, 1, J_LEFT_FIT,
	/* TRANSLATORS: one single character to represent 'name' sort mode  */
	/* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
	N_("sort|n"),
	N_("&Name"), TRUE, TRUE,
	string_file_name,
	(sortfn *) sort_name
    },
    {
	"extension", 12, 1, J_LEFT_FIT,
	/* TRANSLATORS: one single character to represent 'extension' sort mode  */
	/* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
	N_("sort|e"),
	N_("&Extension"), TRUE, FALSE,
	string_file_name, /* TODO: string_file_ext*/
	(sortfn *) sort_ext
    },
    {
	"size", 7,  0, J_RIGHT,
	/* TRANSLATORS: one single character to represent 'size' sort mode  */
	/* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
	N_("sort|s"),
	N_("&Size"), TRUE,TRUE,
	string_file_size,
	(sortfn *) sort_size
    },
    {
	"bsize", 7,  0, J_RIGHT,
	"",
	N_("Block Size"), FALSE, FALSE,
	string_file_size_brief,
	(sortfn *) sort_size
    },
    {
	"type", GT, 0, J_LEFT,
	"",
	"", FALSE, TRUE,
	string_file_type,
	NULL
    },
    {
	"mtime", 12, 0, J_RIGHT,
	/* TRANSLATORS: one single character to represent 'Modify time' sort mode  */
	/* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
	N_("sort|m"),
	N_("&Modify time"), TRUE,TRUE,
	string_file_mtime,
	(sortfn *) sort_time
    },
    {
	"atime", 12, 0, J_RIGHT,
	/* TRANSLATORS: one single character to represent 'Access time' sort mode  */
	/* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
	N_("sort|a"),
	N_("&Access time"), TRUE,TRUE,
	string_file_atime,
	(sortfn *) sort_atime
    },
    {
	"ctime", 12, 0, J_RIGHT,
	/* TRANSLATORS: one single character to represent 'Change time' sort mode  */
	/* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
	N_("sort|h"),
	N_("C&Hange time"), TRUE,TRUE,
	string_file_ctime,
	(sortfn *) sort_ctime
    },
    {
	"perm", 10, 0, J_LEFT,
	"",
	N_("Permission"), FALSE,TRUE,
	string_file_permission,
	NULL
    },
    {
	"mode", 6,  0, J_RIGHT,
	"",
	N_("Perm"), FALSE,TRUE,
	string_file_perm_octal,
	NULL
    },
    {
	"nlink", 2,  0, J_RIGHT,
	"",
	N_("Nl"), FALSE,TRUE,
	string_file_nlinks, NULL
    },
    {
	"inode", 5,  0, J_RIGHT,
	/* TRANSLATORS: one single character to represent 'inode' sort mode  */
	/* TRANSLATORS: no need to translate 'sort', it's just a context prefix  */
	N_("sort|i"),
	N_("&Inode"), TRUE,TRUE,
	string_inode,
	(sortfn *) sort_inode
    },
    {
	"nuid", 5,  0, J_RIGHT,
	"",
	N_("UID"), FALSE,FALSE,
	string_file_nuid,
	NULL
    },
    {
	"ngid", 5,  0, J_RIGHT,
	"",
	N_("GID"), FALSE,FALSE,
	string_file_ngid,
	NULL
    },
    {
	"owner", 8, 0, J_LEFT_FIT,
	"",
	N_("Owner"), FALSE,TRUE,
	string_file_owner,
	NULL
    },
    {
	"group", 8,0, J_LEFT_FIT,
	"",
	N_("Group"), FALSE,TRUE,
	string_file_group,
	NULL
    },
    {
	"mark", 1, 0, J_RIGHT,
	"",
	" ", FALSE,TRUE,
	string_marked,
	NULL
    },
    {
	"|", 1, 0, J_RIGHT,
	"",
	" ", FALSE,TRUE,
	NULL,
	NULL
    },
    {
	"space", 1, 0, J_RIGHT,
	"",
	" ", FALSE,TRUE,
	string_space,
	NULL
    },
    {
	"dot", 1, 0, J_RIGHT,
	"",
	" ", FALSE,FALSE,
	string_dot,
	NULL
    },
    {
	NULL, 0, 0, J_RIGHT, NULL, NULL, FALSE, FALSE, NULL, NULL
    },
};

static int
file_compute_color (int attr, file_entry *fe)
{
    switch (attr) {
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
	if (!filetype_mode)
	    return (NORMAL_COLOR);
    }

    return mc_fhl_get_color (mc_filehighlight, fe);
}

/* Formats the file number file_index of panel in the buffer dest */
static void
format_file (char *dest, int limit, WPanel *panel, int file_index, int width, int attr, int isstatus)
{
    int      color, length, empty_line;
    const char *txt;
    format_e *format, *home;
    file_entry *fe;

    (void) dest;
    (void) limit;
    length     = 0;
    empty_line = (file_index >= panel->count);
    home       = (isstatus) ? panel->status_format : panel->format;
    fe         = &panel->dir.list [file_index];

    if (!empty_line)
	color = file_compute_color (attr, fe);
    else
	color = NORMAL_COLOR;

    for (format = home; format; format = format->next){
	if (length == width)
	    break;

	if (format->string_fn){
            int len, perm;
            char *preperad_text;

	    if (empty_line)
		txt = " ";
	    else
		txt = (*format->string_fn)(fe, format->field_len);

	    len = format->field_len;
	    if (len + length > width)
		len = width - length;
	    if (len <= 0)
		break;

            perm = 0;
            if (permission_mode) {
                if (!strcmp(format->id, "perm"))
                    perm = 1;
                else if (!strcmp(format->id, "mode"))
                    perm = 2;
            }

	    if (color >= 0)
		tty_setcolor (color);
	    else
		tty_lowlevel_setcolor (-color);

            preperad_text = (char*) str_fit_to_term(txt, len, format->just_mode);
            if (perm)
                add_permission_string (preperad_text, format->field_len, fe,
                                       attr, color, perm - 1);
            else
                tty_print_string (preperad_text);

            length+= len;
	} else {
            if (attr == SELECTED || attr == MARKED_SELECTED)
                tty_setcolor (SELECTED_COLOR);
            else
                tty_setcolor (NORMAL_COLOR);
	    tty_print_one_vline ();
	    length++;
	}
    }

    if (length < width)
	tty_draw_hline (-1, -1, ' ', width - length);
}

static void
repaint_file (WPanel *panel, int file_index, int mv, int attr, int isstatus)
{
    int second_column = 0;
    int width;
    int offset = 0;
    char buffer [BUF_MEDIUM];

    gboolean panel_is_split = !isstatus && panel->split;

    width = panel->widget.cols - 2;

    if (panel_is_split) {
	second_column = (file_index - panel->top_file) / llines (panel);
	width = width/2 - 1;

	if (second_column != 0) {
	    offset = 1 + width;
	    /*width = (panel->widget.cols-2) - (panel->widget.cols-2)/2 - 1;*/
	    width = panel->widget.cols - offset - 2;
	}
    }

    /* Nothing to paint */
    if (width <= 0)
	return;

    if (mv){
	if (panel_is_split)
	    widget_move (&panel->widget,
			 (file_index - panel->top_file) % llines (panel) + 2,
			 offset + 1);
	else
	    widget_move (&panel->widget, file_index - panel->top_file + 2, 1);
    }

    format_file (buffer, sizeof(buffer), panel, file_index, width, attr, isstatus);

    if (panel_is_split) {
	if (second_column)
	    tty_print_char (' ');
	else {
	    tty_setcolor (NORMAL_COLOR);
	    tty_print_one_vline ();
	}
    }
}

static void
display_mini_info (WPanel *panel)
{
    if (!show_mini_info)
	return;

    widget_move (&panel->widget, llines (panel) + 3, 1);

    if (panel->searching) {
	tty_setcolor (INPUT_COLOR);
	tty_print_char ('/');
	tty_print_string (str_fit_to_term (panel->search_buffer,
				panel->widget.cols - 3, J_LEFT));
	return;
    }

    /* Status resolves links and show them */
    set_colors (panel);

    if (S_ISLNK (panel->dir.list [panel->selected].st.st_mode)){
	char *lc_link, link_target [MC_MAXPATHLEN];
	int  len;

	lc_link = concat_dir_and_file (panel->cwd, panel->dir.list [panel->selected].fname);
	len = mc_readlink (lc_link, link_target, MC_MAXPATHLEN - 1);
	g_free (lc_link);
	if (len > 0){
	    link_target[len] = 0;
            tty_print_string ("-> ");
            tty_print_string (str_fit_to_term (link_target, panel->widget.cols - 5, 
                    J_LEFT_FIT));
	} else
            tty_print_string (str_fit_to_term (_("<readlink failed>"),
                    panel->widget.cols - 2, J_LEFT));
    } else if (strcmp (panel->dir.list [panel->selected].fname, "..") == 0) {
	/* FIXME:
	 * while loading directory (do_load_dir() and do_reload_dir()),
	 * the actual stat info about ".." directory isn't got;
	 * so just don't display incorrect info about ".." directory */
	tty_print_string (str_fit_to_term (_("UP--DIR"), panel->widget.cols - 2, J_LEFT));
    } else
	/* Default behavior */
	repaint_file (panel, panel->selected, 0, STATUS, 1);
}

static void
paint_dir (WPanel *panel)
{
    int i;
    int color;			/* Color value of the line */
    int items;			/* Number of items */

    items = llines (panel) * (panel->split ? 2 : 1);

    for (i = 0; i < items; i++){
	if (i+panel->top_file >= panel->count)
	    color = 0;
	else {
	    color = 2 * (panel->dir.list [i+panel->top_file].f.marked);
	    color += (panel->selected==i+panel->top_file && panel->active);
	}
	repaint_file (panel, i+panel->top_file, 1, color, 0);
    }
    tty_set_normal_attrs ();
}

static void
display_total_marked_size (WPanel *panel, int y, int x, gboolean size_only)
{
    char buffer[BUF_SMALL], b_bytes[BUF_SMALL], *buf;
    int cols;

    if (panel->marked <= 0)
	return;

    buf = size_only ? b_bytes : buffer;
    cols = panel->widget.cols - 2;

    /*
     * This is a trick to use two ngettext() calls in one sentence.
     * First make "N bytes", then insert it into "X in M files".
     */
    g_snprintf (b_bytes, sizeof (b_bytes),
		ngettext("%s byte", "%s bytes", (unsigned long) panel->total),
		size_trunc_sep (panel->total));
    if (!size_only)
        g_snprintf (buffer, sizeof (buffer),
		    ngettext("%s in %d file", "%s in %d files", panel->marked),
		    b_bytes, panel->marked);

    /* don't forget spaces around buffer content */
    buf = (char *) str_trunc (buf, cols - 4);

    if (x < 0)
	/* center in panel */
	x = (panel->widget.cols - str_term_width1 (buf)) / 2 - 1;

    /*
     * y == llines (panel) + 2      for mini_info_separator
     * y == panel->widget.lines - 1 for panel bottom frame
     */
    widget_move (&panel->widget, y, x);
    tty_setcolor (MARKED_COLOR);
    tty_printf (" %s ", buf);
}

static void
mini_info_separator (WPanel *panel)
{
    if (show_mini_info) {
	const int y = llines (panel) + 2;

	tty_setcolor (NORMAL_COLOR);
	tty_draw_hline (panel->widget.y + y, panel->widget.x + 1,
			    ACS_HLINE, panel->widget.cols - 2);
	/* Status displays total marked size.
	 * Centered in panel, full format. */
	display_total_marked_size (panel, y, -1, FALSE);
    }
}

static void
show_free_space (WPanel *panel)
{
    /* Used to figure out how many free space we have */
    static struct my_statfs myfs_stats;
    /* Old current working directory for displaying free space */
    static char *old_cwd = NULL;

    /* Don't try to stat non-local fs */
    if (!vfs_file_is_local (panel->cwd) || !free_space)
	return;

    if (old_cwd == NULL || strcmp (old_cwd, panel->cwd) != 0) {
	char rpath[PATH_MAX];

	init_my_statfs ();
	g_free (old_cwd);
	old_cwd = g_strdup (panel->cwd);

	if (mc_realpath (panel->cwd, rpath) == NULL)
	    return;

	my_statfs (&myfs_stats, rpath);
    }

    if (myfs_stats.avail > 0 || myfs_stats.total > 0) {
	char buffer1[6], buffer2[6], tmp[BUF_SMALL];
	size_trunc_len (buffer1, sizeof(buffer1) - 1, myfs_stats.avail, 1);
	size_trunc_len (buffer2, sizeof(buffer2) - 1, myfs_stats.total, 1);
	g_snprintf (tmp, sizeof(tmp), " %s/%s (%d%%) ", buffer1, buffer2,
				myfs_stats.total > 0 ?
				(int)(100 * (double)myfs_stats.avail / myfs_stats.total) : 0);
	widget_move (&panel->widget, panel->widget.lines - 1,
				     panel->widget.cols - 2 - (int) strlen (tmp));
	tty_setcolor (NORMAL_COLOR);
	tty_print_string (tmp);
    }
}

static void
show_dir (WPanel *panel)
{
    gchar *tmp;
    set_colors (panel);
    draw_box (panel->widget.parent,
		 panel->widget.y, panel->widget.x,
		 panel->widget.lines, panel->widget.cols);

    if (show_mini_info) {
	widget_move (&panel->widget, llines (panel) + 2, 0);
	tty_print_alt_char (ACS_LTEE);
	widget_move (&panel->widget, llines (panel) + 2,
			panel->widget.cols - 1);
	tty_print_alt_char (ACS_RTEE);
    }

    widget_move (&panel->widget, 0, 1);
    tty_print_string (panel_history_prev_item_sign);

    tmp = (show_dot_files) ? panel_hiddenfiles_sign_show : panel_hiddenfiles_sign_hide;
    tmp = g_strdup_printf("%s[%s]%s",tmp,panel_history_show_list_sign,panel_history_next_item_sign);

    widget_move (&panel->widget, 0, panel->widget.cols - 6);
    tty_print_string (tmp);

    g_free(tmp);

    if (panel->active)
	tty_setcolor (REVERSE_COLOR);

    widget_move (&panel->widget, 0, 3);

    tty_printf (" %s ",
		str_term_trim (strip_home_and_password (panel->cwd),
				min (max (panel->widget.cols - 12, 0),
					panel->widget.cols)));

    if (!show_mini_info) {
	if (panel->marked == 0) {
	    /* Show size of curret file in the bottom of panel */
	    if (S_ISREG (panel->dir.list [panel->selected].st.st_mode)) {
		char buffer[BUF_SMALL];

		g_snprintf (buffer, sizeof (buffer), " %s ",
			    size_trunc_sep (panel->dir.list [panel->selected].st.st_size));
		tty_setcolor (NORMAL_COLOR);
		widget_move (&panel->widget, panel->widget.lines - 1, 4);
		tty_print_string (buffer);
	    }
	} else {
	    /* Show total size of marked files
	     * In the bottom of panel, display size only. */
	    display_total_marked_size (panel, panel->widget.lines - 1, 2, TRUE);
	}
    }

    show_free_space (panel);

    if (panel->active)
	tty_set_normal_attrs ();
}

/* To be used only by long_frame and full_frame to adjust top_file */
static void
adjust_top_file (WPanel *panel)
{
    int old_top = panel->top_file;

    if (panel->selected - old_top > llines (panel))
	panel->top_file = panel->selected;
    if (old_top - panel->count > llines (panel))
	panel->top_file = panel->count - llines (panel);
}

/* Repaint everything, including frame and separator */
static void
paint_panel (WPanel *panel)
{
    paint_frame (panel); /* including show_dir */
    paint_dir (panel);
    mini_info_separator (panel);
    display_mini_info (panel);
    panel->dirty = 0;
}

/* add "#enc:encodning" to end of path */
/* if path end width a previous #enc:, only encoding is changed no additional 
 * #enc: is appended 
 * retun new string
 */
static char
*add_encoding_to_path (const char *path, const char *encoding)
{
    char *result;
    char *semi;
    char *slash;

    semi = g_strrstr (path, "#enc:");

    if (semi != NULL) {
        slash = strchr (semi, PATH_SEP);
        if (slash != NULL) {
            result = g_strconcat (path, "/#enc:", encoding, (char *) NULL);
        } else {
            *semi = 0;
            result = g_strconcat (path, "/#enc:", encoding, (char *) NULL);
            *semi = '#';
        }
    } else {
        result = g_strconcat (path, "/#enc:", encoding, (char *) NULL);
    }

    return result;
}

char *
remove_encoding_from_path (const char *path)
{
    GString *ret;
    GString *tmp_path, *tmp_conv;
    char *tmp, *tmp2;
    const char *enc;
    GIConv converter;

    ret = g_string_new("");
    tmp_conv = g_string_new("");

    tmp_path = g_string_new(path);

    while ((tmp = g_strrstr (tmp_path->str, "/#enc:")) != NULL){
	enc = vfs_get_encoding ((const char *) tmp);
	converter = enc ? str_crt_conv_to (enc): str_cnv_to_term;
        if (converter == INVALID_CONV) converter = str_cnv_to_term;

	tmp2=tmp+1;
	while (*tmp2 && *tmp2 != '/')
	    tmp2++;

	if (*tmp2){
	    str_vfs_convert_from (converter, tmp2, tmp_conv);
	    g_string_prepend(ret, tmp_conv->str);
	    g_string_set_size(tmp_conv,0);
	}
	g_string_set_size(tmp_path,tmp - tmp_path->str);
	str_close_conv (converter);
    }
    g_string_prepend(ret, tmp_path->str);
    g_string_free(tmp_path,TRUE);
    g_string_free(tmp_conv,TRUE);

    tmp = ret->str;
    g_string_free(ret, FALSE);
    return tmp;
}

/*
 * Repaint the contents of the panels without frames.  To schedule panel
 * for repainting, set panel->dirty to 1.  There are many reasons why
 * the panels need to be repainted, and this is a costly operation, so
 * it's done once per event.
 */
void
update_dirty_panels (void)
{
    if (current_panel->dirty)
	paint_panel (current_panel);

    if ((get_other_type () == view_listing) && other_panel->dirty)
	paint_panel (other_panel);
}

static void
do_select (WPanel *panel, int i)
{
    if (i != panel->selected) {
	panel->dirty = 1;
	panel->selected = i;
	panel->top_file = panel->selected - (panel->widget.lines - 2) / 2;
	if (panel->top_file < 0)
	    panel->top_file = 0;
    }
}

static void
do_try_to_select (WPanel *panel, const char *name)
{
    int i;
    char *subdir;

    if (!name) {
        do_select(panel, 0);
	return;
    }

    /* We only want the last component of the directory,
     * and from this only the name without suffix. */
    subdir = vfs_strip_suffix_from_filename (x_basename(name));

    /* Search that subdirectory, if found select it */
    for (i = 0; i < panel->count; i++){
	if (strcmp (subdir, panel->dir.list [i].fname) == 0) {
	    do_select (panel, i);
            g_free (subdir);
	    return;
        }
    }

    /* Try to select a file near the file that is missing */
    if (panel->selected >= panel->count)
        do_select (panel, panel->count-1);
    g_free (subdir);
}

void
try_to_select (WPanel *panel, const char *name)
{
    do_try_to_select (panel, name);
    select_item (panel);
}

void
panel_update_cols (Widget *widget, int frame_size)
{
    int cols, origin;

    if (horizontal_split){
	widget->cols = COLS;
	return;
    }

    if (frame_size == frame_full){
	cols = COLS;
	origin = 0;
    } else {
	if (widget == get_panel_widget (0)){
	    cols   = first_panel_size;
	    origin = 0;
	} else {
	    cols   = COLS-first_panel_size;
	    origin = first_panel_size;
	}
    }

    widget->cols = cols;
    widget->x = origin;
}

extern int saving_setup;
static char *
panel_save_name (WPanel *panel)
{
    /* If the program is shuting down */
    if ((midnight_shutdown && auto_save_setup) || saving_setup)
	return  g_strdup (panel->panel_name);
    else
	return  g_strconcat ("Temporal:", panel->panel_name, (char *) NULL);
}

void
panel_clean_dir (WPanel *panel)
{
    int count = panel->count;

    panel->count = 0;
    panel->top_file = 0;
    panel->selected = 0;
    panel->marked = 0;
    panel->dirs_marked = 0;
    panel->total = 0;
    panel->searching = 0;
    panel->is_panelized = 0;
    panel->dirty = 1;

    clean_dir (&panel->dir, count);
}

static void
panel_destroy (WPanel *p)
{
    size_t i;

    char *name = panel_save_name (p);

    panel_save_setup (p, name);
    panel_clean_dir (p);

    /* save and clean history */
    if (p->dir_history) {
	history_put (p->hist_name, p->dir_history);

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
    g_free (name);
}

static void
panel_format_modified (WPanel *panel)
{
    panel->format_modified = 1;
}

/* Panel creation */
/* The parameter specifies the name of the panel for setup retieving */
WPanel *
panel_new (const char *panel_name)
{
    return panel_new_with_dir(panel_name, NULL);
}

/* Panel creation for specified directory */
/* The parameter specifies the name of the panel for setup retieving */
/* and the path of working panel directory. If path is NULL then */
/* panel will be created for current directory */
WPanel *
panel_new_with_dir (const char *panel_name, const char *wpath)
{
    WPanel *panel;
    char *section;
    int i, err;
    char curdir[MC_MAXPATHLEN];

    panel = g_new0 (WPanel, 1);

    /* No know sizes of the panel at startup */
    init_widget (&panel->widget, 0, 0, 0, 0, panel_callback, panel_event);

    /* We do not want the cursor */
    widget_want_cursor (panel->widget, 0);

    if (wpath) {
	g_strlcpy(panel->cwd, wpath, sizeof (panel->cwd));
	mc_get_current_wd (curdir, sizeof (curdir) - 2);
    } else
	mc_get_current_wd (panel->cwd, sizeof (panel->cwd) - 2);

    strcpy (panel->lwd, ".");

    panel->hist_name = g_strconcat ("Dir Hist ", panel_name, (char *) NULL);
    panel->dir_history = history_get (panel->hist_name);
    directory_history_add (panel, panel->cwd);

    panel->dir.list = g_new (file_entry, MIN_FILES);
    panel->dir.size = MIN_FILES;
    panel->active = 0;
    panel->filter = 0;
    panel->split = 0;
    panel->top_file = 0;
    panel->selected = 0;
    panel->marked = 0;
    panel->total = 0;
    panel->reverse = 0;
    panel->dirty = 1;
    panel->searching = 0;
    panel->dirs_marked = 0;
    panel->is_panelized = 0;
    panel->format = 0;
    panel->status_format = 0;
    panel->format_modified = 1;

    panel->panel_name = g_strdup (panel_name);
    panel->user_format = g_strdup (DEFAULT_USER_FORMAT);

    for (i = 0; i < LIST_TYPES; i++)
	panel->user_status_format[i] = g_strdup (DEFAULT_USER_FORMAT);

    panel->search_buffer[0] = 0;
    panel->frame_size = frame_half;
    section = g_strconcat ("Temporal:", panel->panel_name, (char *) NULL);
    if (!mc_config_has_group (mc_main_config, section)) {
	g_free (section);
	section = g_strdup (panel->panel_name);
    }
    panel_load_setup (panel, section);
    g_free (section);

    /* Load format strings */
    err = set_panel_formats (panel);
    if (err != 0)
	set_panel_formats (panel);

    /* Because do_load_dir lists files in current directory */
    if (wpath)
	mc_chdir(wpath);

    /* Load the default format */
    panel->count =
	do_load_dir (panel->cwd, &panel->dir, panel->current_sort_field->sort_routine,
		     panel->reverse, panel->case_sensitive,
		     panel->exec_first, panel->filter);

    /* Restore old right path */
    if (wpath)
	mc_chdir(curdir);

    return panel;
}

void
panel_reload (WPanel *panel)
{
    struct stat current_stat;

    if (fast_reload && !stat (panel->cwd, &current_stat)
	&& current_stat.st_ctime == panel->dir_stat.st_ctime
	&& current_stat.st_mtime == panel->dir_stat.st_mtime)
	return;

    while (mc_chdir (panel->cwd) == -1) {
	char *last_slash;

	if (panel->cwd[0] == PATH_SEP && panel->cwd[1] == 0) {
	    panel_clean_dir (panel);
	    panel->count = set_zero_dir (&panel->dir) ? 1 : 0;
	    return;
	}
	last_slash = strrchr (panel->cwd, PATH_SEP);
	if (!last_slash || last_slash == panel->cwd)
	    strcpy (panel->cwd, PATH_SEP_STR);
	else
	    *last_slash = 0;
	memset (&(panel->dir_stat), 0, sizeof (panel->dir_stat));
	show_dir (panel);
    }

    panel->count =
	do_reload_dir (panel->cwd, &panel->dir, panel->current_sort_field->sort_routine,
		       panel->count, panel->reverse, panel->case_sensitive,
		       panel->exec_first, panel->filter);

    panel->dirty = 1;
    if (panel->selected >= panel->count)
	do_select (panel, panel->count - 1);

    recalculate_panel_summary (panel);
}

static void
panel_paint_sort_info(WPanel *panel)
{
    const char *sort_sign = (panel->reverse) ? panel_sort_down_sign : panel_sort_up_sign;
    char *str;

    if (*panel->current_sort_field->hotkey == '\0')
	return;

    str = g_strdup_printf("%s%s",sort_sign, Q_(panel->current_sort_field->hotkey));

    widget_move (&panel->widget, 1, 1);
    tty_print_string (str);
    g_free(str);
}

static gchar*
panel_get_title_without_hotkey(const char *title)
{
    char *translated_title;
    char *hkey;

    if (title == NULL)
	return NULL;
    if (title[0] == '\0')
	return g_strdup("");

    translated_title = g_strdup(_(title));

    hkey = strchr(translated_title, '&');
    if ((hkey != NULL) && (hkey[1] != '\0'))
        memmove((void *) hkey, (void *) hkey+1,strlen(hkey));

    return translated_title;
}

static void
paint_frame (WPanel *panel)
{
    int side, width;
    GString *format_txt;

    if (!panel->split)
	adjust_top_file (panel);

    widget_erase (&panel->widget);
    show_dir (panel);

    widget_move (&panel->widget, 1, 1);

    for (side = 0; side <= panel->split; side++){
	format_e *format;

	if (side){
	    tty_setcolor (NORMAL_COLOR);
	    tty_print_one_vline ();
	    width = panel->widget.cols - panel->widget.cols/2 - 1;
	} else if (panel->split)
	    width = panel->widget.cols/2 - 3;
	else
	    width = panel->widget.cols - 2;

	format_txt = g_string_new("");
	for (format = panel->format; format; format = format->next){
            if (format->string_fn){
                g_string_set_size(format_txt, 0);

                if (panel->list_type == list_long && strcmp (format->id, panel->current_sort_field->id) == 0)
                    g_string_append (format_txt, (panel->reverse) ? panel_sort_down_sign : panel_sort_up_sign);

                g_string_append (format_txt, format->title);
                if (strcmp (format->id, "name") == 0 && panel->filter && *panel->filter) {
                        g_string_append (format_txt, " [");
                        g_string_append (format_txt, panel->filter);
                        g_string_append (format_txt, "]");
                }

                tty_setcolor (MARKED_COLOR);
                tty_print_string (str_fit_to_term (format_txt->str, format->field_len,
                                                    J_CENTER_LEFT));
                width -= format->field_len;
	    } else {
		tty_setcolor (NORMAL_COLOR);
		tty_print_one_vline ();
		width--;
	    }
	}
	g_string_free(format_txt, TRUE);

	if (width > 0)
	    tty_draw_hline (-1, -1, ' ', width);
    }

    if (panel->list_type != list_long)
	panel_paint_sort_info(panel);
}

static const char *
parse_panel_size (WPanel *panel, const char *format, int isstatus)
{
    int frame = frame_half;
    format = skip_separators (format);

    if (!strncmp (format, "full", 4)){
	frame = frame_full;
	format += 4;
    } else if (!strncmp (format, "half", 4)){
	frame = frame_half;
	format += 4;
    }

    if (!isstatus){
        panel->frame_size = frame;
        panel->split = 0;
    }

    /* Now, the optional column specifier */
    format = skip_separators (format);

    if (*format == '1' || *format == '2'){
	if (!isstatus)
	    panel->split = *format == '2';
	format++;
    }

    if (!isstatus)
        panel_update_cols (&(panel->widget), panel->frame_size);

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

static format_e *
parse_display_format (WPanel *panel, const char *format, char **error, int isstatus, int *res_total_cols)
{
    format_e *darr, *old = 0, *home = 0; /* The formats we return */
    int  total_cols = 0;		/* Used columns by the format */
    int  set_justify;                  	/* flag: set justification mode? */
    align_crt_t justify = J_LEFT;	/* Which mode. */
    size_t  i;

    static size_t i18n_timelength = 0; /* flag: check ?Time length at startup */

    *error = 0;

    if (i18n_timelength == 0) {
	i18n_timelength = i18n_checktimelength ();	/* Musn't be 0 */

	for (i = 0; panel_fields[i].id != NULL; i++)
	    if (strcmp ("time", panel_fields[i].id + 1) == 0)
		panel_fields [i].min_size = i18n_timelength;
    }

    /*
     * This makes sure that the panel and mini status full/half mode
     * setting is equal
     */
    format = parse_panel_size (panel, format, isstatus);

    while (*format){           /* format can be an empty string */
	int found = 0;

        darr = g_new0 (format_e, 1);

        /* I'm so ugly, don't look at me :-) */
        if (!home)
            home = old = darr;

        old->next = darr;
        darr->next = 0;
        old = darr;

	format = skip_separators (format);

	if (strchr ("<=>", *format)){
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
	    format = skip_separators (format+1);
	} else
	    set_justify = 0;

	for (i = 0; panel_fields[i].id != NULL; i++) {
	    size_t klen = strlen (panel_fields [i].id);

	    if (strncmp (format, panel_fields [i].id, klen) != 0)
		continue;

	    format += klen;

            darr->requested_field_len = panel_fields [i].min_size;
            darr->string_fn           = panel_fields [i].string_fn;
            darr->title = panel_get_title_without_hotkey(panel_fields [i].title_hotkey);

            darr->id                  = panel_fields [i].id;
	    darr->expand              = panel_fields [i].expands;
	    darr->just_mode 	      = panel_fields [i].default_just;

	    if (set_justify) {
		if (IS_FIT(darr->just_mode))
		    darr->just_mode = MAKE_FIT(justify);
		else
		    darr->just_mode = justify;
	    }
	    found = 1;

	    format = skip_separators (format);

	    /* If we have a size specifier */
	    if (*format == ':'){
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
		if (*format == '+'){
		    darr->expand = 1;
		    format++;
		}

	    }

	    break;
	}
	if (!found){
	    char *tmp_format = g_strdup (format);

	    int pos = min (8, strlen (format));
	    delete_format (home);
	    tmp_format [pos] = 0;
	    *error = g_strconcat (_("Unknown tag on display format: "), tmp_format, (char *) NULL);
	    g_free (tmp_format);
	    return 0;
	}
	total_cols += darr->requested_field_len;
    }

    *res_total_cols = total_cols;
    return home;
}

static format_e *
use_display_format (WPanel *panel, const char *format, char **error, int isstatus)
{
#define MAX_EXPAND 4
    int  expand_top = 0;               /* Max used element in expand */
    int  usable_columns;               /* Usable columns in the panel */
    int  total_cols = 0;
    int  i;
    format_e *darr, *home;

    if (!format)
	format = DEFAULT_USER_FORMAT;

    home = parse_display_format (panel, format, error, isstatus, &total_cols);

    if (*error)
	return 0;

    panel->dirty = 1;

    /* Status needn't to be split */
    usable_columns = ((panel->widget.cols-2)/((isstatus)
					      ? 1
					      : (panel->split+1))) - (!isstatus && panel->split);

    /* Look for the expandable fields and set field_len based on the requested field len */
    for (darr = home; darr && expand_top < MAX_EXPAND; darr = darr->next){
	darr->field_len = darr->requested_field_len;
	if (darr->expand)
	    expand_top++;
    }

    /* If we used more columns than the available columns, adjust that */
    if (total_cols > usable_columns){
	int pdif, dif = total_cols - usable_columns;

        while (dif){
	    pdif = dif;
	    for (darr = home; darr; darr = darr->next){
		if (dif && darr->field_len - 1){
		    darr->field_len--;
		    dif--;
		}
	    }

	    /* avoid endless loop if num fields > 40 */
	    if (pdif == dif)
		break;
	}
	total_cols  = usable_columns; /* give up, the rest should be truncated */
    }

    /* Expand the available space */
    if ((usable_columns > total_cols) && expand_top){
	int spaces = (usable_columns - total_cols) / expand_top;
	int extra  = (usable_columns - total_cols) % expand_top;

	for (i = 0, darr = home; darr && (i < expand_top); darr = darr->next)
		if (darr->expand){
			darr->field_len += (spaces + ((i == 0) ? extra : 0));
			i++;
		}
    }
    return home;
}

/* Switches the panel to the mode specified in the format 	    */
/* Seting up both format and status string. Return: 0 - on success; */
/* 1 - format error; 2 - status error; 3 - errors in both formats.  */
int
set_panel_formats (WPanel *p)
{
    format_e *form;
    char *err = NULL;
    int retcode = 0;

    form = use_display_format (p, panel_format (p), &err, 0);

    if (err != NULL) {
        g_free (err);
        retcode = 1;
    } else {
        delete_format (p->format);
        p->format = form;
    }

    if (show_mini_info) {
	form = use_display_format (p, mini_status_format (p), &err, 1);

	if (err != NULL) {
	    g_free (err);
	    retcode += 2;
	} else {
	    delete_format (p->status_format);
	    p->status_format = form;
	}
    }

    panel_format_modified (p);
    panel_update_cols (&(p->widget), p->frame_size);

    if (retcode)
        message (D_ERROR, _("Warning" ), _( "User supplied format looks invalid, reverting to default." ) );
    if (retcode & 0x01){
        g_free (p->user_format);
        p->user_format = g_strdup (DEFAULT_USER_FORMAT);
    }
    if (retcode & 0x02){
        g_free (p->user_status_format [p->list_type]);
        p->user_status_format [p->list_type] = g_strdup (DEFAULT_USER_FORMAT);
    }

    return retcode;
}

/* Given the panel->view_type returns the format string to be parsed */
static const char *
panel_format (WPanel *panel)
{
    switch (panel->list_type){

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

static const char *
mini_status_format (WPanel *panel)
{
    if (panel->user_mini_status)
       return panel->user_status_format [panel->list_type];

    switch (panel->list_type){

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

/* Used to emulate Lynx's entering leaving a directory with the arrow keys */
static cb_ret_t
maybe_cd (int move_up_dir)
{
    if (navigate_with_arrows) {
	if (!cmdline->buffer[0]) {
	    if (move_up_dir) {
		do_cd ("..", cd_exact);
		return MSG_HANDLED;
	    }
	    if (S_ISDIR (selection (current_panel)->st.st_mode)
		|| link_isdir (selection (current_panel))) {
		do_cd (selection (current_panel)->fname, cd_exact);
		return MSG_HANDLED;
	    }
	}
    }
    return MSG_NOT_HANDLED;
}

/* Returns the number of items in the given panel */
static int
ITEMS (WPanel *p)
{
    if (p->split)
	return llines (p) * 2;
    else
	return llines (p);
}

/* Select current item and readjust the panel */
void
select_item (WPanel *panel)
{
    int items = ITEMS (panel);

    /* Although currently all over the code we set the selection and
       top file to decent values before calling select_item, I could
       forget it someday, so it's better to do the actual fitting here */

    if (panel->top_file < 0)
	panel->top_file = 0;

    if (panel->selected < 0)
	panel->selected = 0;

    if (panel->selected > panel->count - 1)
	panel->selected = panel->count - 1;

    if (panel->top_file > panel->count - 1)
	panel->top_file = panel->count - 1;

    if ((panel->count - panel->top_file) < items) {
	panel->top_file = panel->count - items;
	if (panel->top_file < 0)
	    panel->top_file = 0;
    }

    if (panel->selected < panel->top_file)
	panel->top_file = panel->selected;

    if ((panel->selected - panel->top_file) >= items)
	panel->top_file = panel->selected - items + 1;

    panel->dirty = 1;

    execute_hooks (select_file_hook);
}

/* Clears all files in the panel, used only when one file was marked */
void
unmark_files (WPanel *panel)
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

static void
unselect_item (WPanel *panel)
{
    repaint_file (panel, panel->selected, 1, 2*selection (panel)->f.marked, 0);
}

static void
move_down (WPanel *panel)
{
    if (panel->selected+1 == panel->count)
	return;

    unselect_item (panel);
    panel->selected++;
    if (panel->selected - panel->top_file == ITEMS (panel) &&
	panel_scroll_pages) {
	/* Scroll window half screen */
	panel->top_file += ITEMS (panel)/2;
	if (panel->top_file > panel->count - ITEMS (panel))
	    panel->top_file = panel->count - ITEMS (panel);
	paint_dir (panel);
    }
    select_item (panel);
}

static void
move_up (WPanel *panel)
{
    if (panel->selected == 0)
	return;

    unselect_item (panel);
    panel->selected--;
    if (panel->selected < panel->top_file && panel_scroll_pages) {
	/* Scroll window half screen */
	panel->top_file -= ITEMS (panel)/2;
	if (panel->top_file < 0)
	    panel->top_file = 0;
	paint_dir (panel);
    }
    select_item (panel);
}

/* Changes the selection by lines (may be negative) */
static void
move_selection (WPanel *panel, int lines)
{
    int new_pos;
    int adjust = 0;

    new_pos = panel->selected + lines;
    if (new_pos >= panel->count)
	new_pos = panel->count-1;

    if (new_pos < 0)
	new_pos = 0;

    unselect_item (panel);
    panel->selected = new_pos;

    if (panel->selected - panel->top_file >= ITEMS (panel)){
	panel->top_file += lines;
	adjust = 1;
    }

    if (panel->selected - panel->top_file < 0){
	panel->top_file += lines;
	adjust = 1;
    }

    if (adjust){
	if (panel->top_file > panel->selected)
	    panel->top_file = panel->selected;
	if (panel->top_file < 0)
	    panel->top_file = 0;
	paint_dir (panel);
    }
    select_item (panel);
}

static cb_ret_t
move_left (WPanel *panel)
{
    if (panel->split) {
	move_selection (panel, -llines (panel));
	return MSG_HANDLED;
    } else
	return maybe_cd (1); /* cd .. */
}

static int
move_right (WPanel *panel)
{
    if (panel->split) {
	move_selection (panel, llines (panel));
	return MSG_HANDLED;
    } else
	return maybe_cd (0); /* cd (selection) */
}

static void
prev_page (WPanel *panel)
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

    /* This keeps the selection in a reasonable place */
    if (panel->selected < 0)
	panel->selected = 0;
    if (panel->top_file < 0)
	panel->top_file = 0;
    select_item (panel);
    paint_dir (panel);
}

static void
ctrl_prev_page (WPanel *panel)
{
    (void) panel;
    do_cd ("..", cd_exact);
}

static void
next_page (WPanel *panel)
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

    /* This keeps the selection in it's relative position */
    if (panel->selected >= panel->count)
	panel->selected = panel->count - 1;
    if (panel->top_file >= panel->count)
	panel->top_file = panel->count - 1;
    select_item (panel);
    paint_dir (panel);
}

static void
ctrl_next_page (WPanel *panel)
{
    if ((S_ISDIR (selection (panel)->st.st_mode)
	 || link_isdir (selection (panel)))) {
	do_cd (selection (panel)->fname, cd_exact);
    }
}

static void
goto_top_file (WPanel *panel)
{
    unselect_item (panel);
    panel->selected = panel->top_file;
    select_item (panel);
}

static void
goto_middle_file (WPanel *panel)
{
    unselect_item (panel);
    panel->selected = panel->top_file + (ITEMS (panel)/2);
    if (panel->selected >= panel->count)
	panel->selected = panel->count - 1;
    select_item (panel);
}

static void
goto_bottom_file (WPanel *panel)
{
    unselect_item (panel);
    panel->selected = panel->top_file + ITEMS (panel)-1;
    if (panel->selected >= panel->count)
	panel->selected = panel->count - 1;
    select_item (panel);
}

static void
move_home (WPanel *panel)
{
    if (panel->selected == 0)
	return;
    unselect_item (panel);

    if (torben_fj_mode){
	int middle_pos = panel->top_file + (ITEMS (panel)/2);

	if (panel->selected > middle_pos){
	    goto_middle_file (panel);
	    return;
	}
	if (panel->selected != panel->top_file){
	    goto_top_file (panel);
	    return;
	}
    }

    panel->top_file = 0;
    panel->selected = 0;

    paint_dir (panel);
    select_item (panel);
}

static void
move_end (WPanel *panel)
{
    if (panel->selected == panel->count-1)
	return;
    unselect_item (panel);
    if (torben_fj_mode){
	int middle_pos = panel->top_file + (ITEMS (panel)/2);

	if (panel->selected < middle_pos){
	    goto_middle_file (panel);
	    return;
	}
	if (panel->selected != (panel->top_file + ITEMS(panel)-1)){
	    goto_bottom_file (panel);
	    return;
	}
    }

    panel->selected = panel->count-1;
    paint_dir (panel);
    select_item (panel);
}

/* Recalculate the panels summary information, used e.g. when marked
   files might have been removed by an external command */
void
recalculate_panel_summary (WPanel *panel)
{
    int i;

    panel->marked = 0;
    panel->dirs_marked = 0;
    panel->total  = 0;

    for (i = 0; i < panel->count; i++)
	if (panel->dir.list [i].f.marked){
	    /* do_file_mark will return immediately if newmark == oldmark.
	       So we have to first unmark it to get panel's summary information
	       updated. (Norbert) */
	    panel->dir.list [i].f.marked = 0;
	    do_file_mark (panel, i, 1);
	}
}

/* This routine marks a file or a directory */
void
do_file_mark (WPanel *panel, int idx, int mark)
{
    if (panel->dir.list[idx].f.marked == mark)
	return;

    /* Only '..' can't be marked, '.' isn't visible */
    if (!strcmp (panel->dir.list[idx].fname, ".."))
	return;

    file_mark (panel, idx, mark);
    if (panel->dir.list[idx].f.marked) {
	panel->marked++;
	if (S_ISDIR (panel->dir.list[idx].st.st_mode)) {
	    if (panel->dir.list[idx].f.dir_size_computed)
		panel->total += panel->dir.list[idx].st.st_size;
	    panel->dirs_marked++;
	} else
	    panel->total += panel->dir.list[idx].st.st_size;
	set_colors (panel);
    } else {
	if (S_ISDIR (panel->dir.list[idx].st.st_mode)) {
	    if (panel->dir.list[idx].f.dir_size_computed)
		panel->total -= panel->dir.list[idx].st.st_size;
	    panel->dirs_marked--;
	} else
	    panel->total -= panel->dir.list[idx].st.st_size;
	panel->marked--;
    }
}

static void
do_mark_file (WPanel *panel, int do_move)
{
    do_file_mark (panel, panel->selected,
		  selection (panel)->f.marked ? 0 : 1);
    if (mark_moves_down && do_move)
	move_down (panel);
}

static void
mark_file (WPanel *panel)
{
    do_mark_file (panel, 1);
}

/* Incremental search of a file name in the panel */
static void
do_search (WPanel *panel, int c_code)
{
    size_t l;
    int i, sel;
    gboolean wrapped = FALSE;
    char *act;
    mc_search_t *search;
    char *reg_exp, *esc_str;
    gboolean is_found = FALSE;

    l = strlen (panel->search_buffer);
    if (c_code == KEY_BACKSPACE) {
        if (l != 0) {
            act = panel->search_buffer + l;
            str_prev_noncomb_char (&act, panel->search_buffer);
            act[0] = '\0';
        }
        panel->search_chpoint = 0;
    } else {
        if (c_code != 0 && (gsize) panel->search_chpoint < sizeof (panel->search_char)) {
            panel->search_char[panel->search_chpoint] = c_code;
            panel->search_chpoint++;
        }

        if (panel->search_chpoint > 0) {
            switch (str_is_valid_char (panel->search_char, panel->search_chpoint)) {
            case -2:
                return;
            case -1:
                panel->search_chpoint = 0;
                return;
            default:
                if (l + panel->search_chpoint < sizeof (panel->search_buffer)) {
                    memcpy (panel->search_buffer + l, panel->search_char,
                            panel->search_chpoint);
                    l+= panel->search_chpoint;
                    (panel->search_buffer + l)[0] = '\0';
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
    search->is_case_sentitive = 0;

    sel = panel->selected;
    for (i = panel->selected; !wrapped || i != panel->selected; i++) {
        if (i >= panel->count) {
            i = 0;
            if (wrapped)
                break;
            wrapped = TRUE;
        }
        if (mc_search_run (search, panel->dir.list[i].fname,
                           0, panel->dir.list[i].fnamelen, NULL)) {
            sel = i;
            is_found = TRUE;
            break;
        }
    }
    if (is_found) {
        unselect_item (panel);
        panel->selected = sel;
        select_item (panel);
        paint_panel (panel);
    } else if (c_code != KEY_BACKSPACE) {
        act = panel->search_buffer + l;
        str_prev_noncomb_char (&act, panel->search_buffer);
        act[0] = '\0';
    }
    mc_search_free (search);
    g_free (reg_exp);
    g_free (esc_str);
}

static void
start_search (WPanel *panel)
{
    if (panel->searching){
	if (panel->selected+1 == panel->count)
	    panel->selected = 0;
	else
	    move_down (panel);
	do_search (panel, 0);
    } else {
	panel->searching = 1;
	panel->search_buffer[0] = '\0';
        panel->search_char[0] = '\0';
        panel->search_chpoint = 0;
	display_mini_info (panel);
	mc_refresh ();
    }
}

/* Return 1 if the Enter key has been processed, 0 otherwise */
static int
do_enter_on_file_entry (file_entry *fe)
{
    char *full_name;

    /*
     * Directory or link to directory - change directory.
     * Try the same for the entries on which mc_lstat() has failed.
     */
    if (S_ISDIR (fe->st.st_mode) || link_isdir (fe)
	|| (fe->st.st_mode == 0)) {
	if (!do_cd (fe->fname, cd_exact))
	    message (D_ERROR, MSG_ERROR, _("Cannot change directory"));
	return 1;
    }

    /* Try associated command */
    if (regex_command (fe->fname, "Open", 0) != 0)
	return 1;

    /* Check if the file is executable */
    full_name = concat_dir_and_file (current_panel->cwd, fe->fname);
    if (!is_exe (fe->st.st_mode) || !if_link_is_exe (full_name, fe)) {
	g_free (full_name);
	return 0;
    }
    g_free (full_name);

    if (confirm_execute) {
	if (query_dialog
	    (_(" The Midnight Commander "),
	     _(" Do you really want to execute? "), D_NORMAL, 2, _("&Yes"),
	     _("&No")) != 0)
	    return 1;
    }
#ifdef ENABLE_VFS
    if (!vfs_current_is_local ()) {
	char *tmp;
	int ret;

	tmp = concat_dir_and_file (vfs_get_current_dir (), fe->fname);
	ret = mc_setctl (tmp, VFS_SETCTL_RUN, NULL);
	g_free (tmp);
	/* We took action only if the dialog was shown or the execution
	 * was successful */
	return confirm_execute || (ret == 0);
    }
#endif

    {
	char *tmp = name_quote (fe->fname, 0);
	char *cmd = g_strconcat (".", PATH_SEP_STR, tmp, (char *) NULL);
	g_free (tmp);
	shell_execute (cmd, 0);
	g_free (cmd);
    }

#if HAVE_CHARSET
    source_codepage = default_source_codepage;
#endif

    return 1;
}

static int
do_enter (WPanel *panel)
{
    return do_enter_on_file_entry (selection (panel));
}

static void
chdir_other_panel (WPanel *panel)
{
    char *new_dir;
    char *sel_entry = NULL;
    
    if (get_other_type () != view_listing) {
	set_display_type (get_other_index (), view_listing);
    }

    if (!S_ISDIR (panel->dir.list [panel->selected].st.st_mode)) {
        new_dir = concat_dir_and_file (panel->cwd, "..");
	sel_entry = strrchr(panel->cwd, PATH_SEP);
    } else
        new_dir = concat_dir_and_file (panel->cwd, panel->dir.list [panel->selected].fname);

    change_panel ();
    do_cd (new_dir, cd_exact);
    if (sel_entry)
	try_to_select (current_panel, sel_entry);
    change_panel ();
 
    move_down (panel);

    g_free (new_dir);
}

/*
 * Make the current directory of the current panel also the current
 * directory of the other panel.  Put the other panel to the listing
 * mode if needed.  If the current panel is panelized, the other panel
 * doesn't become panelized.
 */
static void
sync_other_panel (WPanel *panel)
{
    if (get_other_type () != view_listing) {
	set_display_type (get_other_index (), view_listing);
    }

    do_panel_cd (other_panel, current_panel->cwd, cd_exact);

    /* try to select current filename on the other panel */
    if (!panel->is_panelized) {
	try_to_select (other_panel, selection (panel)->fname);
    }
}

static void
chdir_to_readlink (WPanel *panel)
{
    char *new_dir;

    if (get_other_type () != view_listing)
	return;

    if (S_ISLNK (panel->dir.list [panel->selected].st.st_mode)) {
	char buffer [MC_MAXPATHLEN], *p;
	int i;
	struct stat st;

	i = readlink (selection (panel)->fname, buffer, MC_MAXPATHLEN - 1);
	if (i < 0)
	    return;
	if (mc_stat (selection (panel)->fname, &st) < 0)
	    return;
	buffer [i] = 0;
	if (!S_ISDIR (st.st_mode)) {
	    p = strrchr (buffer, PATH_SEP);
	    if (p && !p[1]) {
		*p = 0;
		p = strrchr (buffer, PATH_SEP);
	    }
	    if (!p)
	        return;
	    p[1] = 0;
	}
	if (*buffer == PATH_SEP)
	    new_dir = g_strdup (buffer);
	else
	    new_dir = concat_dir_and_file (panel->cwd, buffer);

	change_panel ();
	do_cd (new_dir, cd_exact);
	change_panel ();

	move_down (panel);

	g_free (new_dir);
    }
}

static gsize
panel_get_format_field_count(WPanel *panel)
{
    format_e *format;
    gsize lc_index;
    for (
	lc_index=0, format = panel->format;
	format != NULL;
	format = format->next, lc_index++
    );
    return lc_index;
}

/*
function return 0 if not found and REAL_INDEX+1 if found
*/
static gsize
panel_get_format_field_index_by_name(WPanel *panel, const char *name)
{
    format_e *format;
    gsize lc_index;

    for (
	lc_index=1, format = panel->format;
	! ( format == NULL || strcmp(format->title, name) == 0 );
	format = format->next, lc_index++
    );
    if (format == NULL)
	lc_index = 0;

    return lc_index;
}

static format_e *
panel_get_format_field_by_index(WPanel *panel, gsize lc_index)
{
    format_e *format;
    for (
	format = panel->format;
	! ( format == NULL || lc_index == 0 );
	format = format->next, lc_index--
    );
    return format;
}

static const panel_field_t *
panel_get_sortable_field_by_format(WPanel *panel, gsize lc_index)
{
    const panel_field_t *pfield;
    format_e *format;

    format = panel_get_format_field_by_index(panel, lc_index);
    if (format == NULL)
	return NULL;
    pfield = panel_get_field_by_title(format->title);
    if (pfield == NULL)
	return NULL;
    if (pfield->sort_routine == NULL)
	return NULL;
    return pfield;
}

static void
panel_toggle_sort_order_prev(WPanel *panel)
{
    gsize lc_index, i;
    gchar *title;

    const panel_field_t *pfield = NULL;

    title = panel_get_title_without_hotkey(panel->current_sort_field->title_hotkey);
    lc_index = panel_get_format_field_index_by_name(panel, title);
    g_free(title);

    if (lc_index > 1){
	/* search for prev sortable column in panel format */
	for (
	    i = lc_index-1 ;
	    i != 0 && (pfield = panel_get_sortable_field_by_format(panel, i-1)) == NULL ;
	    i--
	);
    }

    if ( pfield == NULL) {
	/* Sortable field not found. Try to search in each array */
	for (
	    i = panel_get_format_field_count(panel) ;
	    i != 0  && (pfield = panel_get_sortable_field_by_format(panel, i-1)) == NULL ;
	    i--
	);
    }
    if ( pfield == NULL)
	return;
    panel->current_sort_field = pfield;
    panel_set_sort_order(panel, panel->current_sort_field);
}


static void
panel_toggle_sort_order_next(WPanel *panel)
{
    gsize lc_index, i;
    const panel_field_t *pfield = NULL;
    gsize format_field_count = panel_get_format_field_count(panel);
    gchar *title;

    title = panel_get_title_without_hotkey(panel->current_sort_field->title_hotkey);
    lc_index = panel_get_format_field_index_by_name(panel, title);
    g_free(title);

    if (lc_index != 0 && lc_index != format_field_count){
	/* search for prev sortable column in panel format */
	for (
	    i = lc_index;
	    i != format_field_count && (pfield = panel_get_sortable_field_by_format(panel, i)) == NULL ;
	    i++
	);
    }

    if ( pfield == NULL) {
	/* Sortable field not found. Try to search in each array */
	for (
	    i = 0 ;
	    i != format_field_count  && (pfield = panel_get_sortable_field_by_format(panel, i)) == NULL ;
	    i++
	);
    }
    if ( pfield == NULL)
	return;
    panel->current_sort_field = pfield;
    panel_set_sort_order(panel, panel->current_sort_field);
}

static void
panel_select_sort_order(WPanel *panel)
{
    const panel_field_t *sort_order;
    sort_order = sort_box (panel->current_sort_field, &panel->reverse,
			   &panel->case_sensitive,
			   &panel->exec_first);
    if (sort_order == NULL)
	return;
    panel->current_sort_field = sort_order;
    panel_set_sort_order (panel, panel->current_sort_field);

}

static void
panel_set_sort_type_by_id(WPanel *panel, const char *name)
{
    const panel_field_t *sort_order;

    if (strcmp(panel->current_sort_field->id, name) != 0) {
        sort_order = panel_get_field_by_id (name);
        if (sort_order == NULL)
	    return;
	panel->current_sort_field = sort_order;
    } else {
	panel->reverse = ! panel->reverse;
    }
    panel_set_sort_order (panel, panel->current_sort_field);
}

typedef void (*panel_key_callback) (WPanel *);

static void cmd_do_enter(WPanel *wp) { (void) do_enter(wp); }
static void cmd_view_simple(WPanel *wp) { (void) wp; view_simple_cmd(); }
static void cmd_edit_new(WPanel *wp) { (void) wp; edit_cmd_new(); }
static void cmd_copy_local(WPanel *wp) { (void) wp; copy_cmd_local(); }
static void cmd_rename_local(WPanel *wp) { (void) wp; rename_cmd_local(); }
static void cmd_delete_local(WPanel *wp) { (void) wp; delete_cmd_local(); }
static void cmd_select(WPanel *wp) { (void) wp; select_cmd(); }
static void cmd_unselect(WPanel *wp) { (void) wp; unselect_cmd(); }
static void cmd_reverse_selection(WPanel *wp) { (void) wp; reverse_selection_cmd(); }

static cb_ret_t
panel_execute_cmd (WPanel *panel, unsigned long command)
{
    int res = MSG_HANDLED;

    switch (command) {
    case CK_PanelChdirOtherPanel:
        chdir_other_panel (panel);
        break;
    case CK_PanelChdirToReadlink:
        chdir_to_readlink (panel);
        break;
    case CK_PanelCmdCopyLocal:
        cmd_copy_local (panel);
        break;
    case CK_PanelCmdDeleteLocal:
        cmd_delete_local (panel);
        break;
    case CK_PanelCmdDoEnter:
        cmd_do_enter (panel);
        break;
    case CK_PanelCmdViewSimple:
        cmd_view_simple (panel);
        break;
    case CK_PanelCmdEditNew:
        cmd_edit_new (panel);
        break;
    case CK_PanelCmdRenameLocal:
        cmd_rename_local (panel);
        break;
    case CK_PanelCmdReverseSelection:
        cmd_reverse_selection (panel);
        break;
    case CK_PanelCmdSelect:
        cmd_select (panel);
        break;
    case CK_PanelCmdUnselect:
        cmd_unselect (panel);
        break;
    case CK_PanelNextPage:
        next_page (panel);
        break;
    case CK_PanelPrevPage:
        prev_page (panel);
        break;
    case CK_PanelCtrlNextPage:
        ctrl_next_page (panel);
        break;
    case CK_PanelCtrlPrevPage:
        ctrl_prev_page (panel);
        break;
    case CK_PanelDirectoryHistoryList:
        directory_history_list (panel);
        break;
    case CK_PanelDirectoryHistoryNext:
        directory_history_next (panel);
        break;
    case CK_PanelDirectoryHistoryPrev:
        directory_history_prev (panel);
        break;
    case CK_PanelGotoBottomFile:
        goto_bottom_file (panel);
        break;
    case CK_PanelGotoMiddleFile:
        goto_middle_file (panel);
        break;
    case CK_PanelGotoTopFile:
        goto_top_file (panel);
        break;
    case CK_PanelMarkFile:
        mark_file (panel);
        break;
    case CK_PanelMoveUp:
        move_up (panel);
        break;
    case CK_PanelMoveDown:
        move_down (panel);
        break;
    case CK_PanelMoveLeft:
        res = move_left (panel);
        break;
    case CK_PanelMoveRight:
        res = move_right (panel);
        break;
    case CK_PanelMoveEnd:
        move_end (panel);
        break;
    case CK_PanelMoveHome:
        move_home (panel);
        break;
    case CK_PanelSetPanelEncoding:
        set_panel_encoding (panel);
        break;
    case CK_PanelStartSearch:
        start_search (panel);
        break;
    case CK_PanelSyncOtherPanel:
        sync_other_panel (panel);
        break;
    case CK_PanelSelectSortOrder:
        panel_select_sort_order(panel);
        break;
    case CK_PanelToggleSortOrderPrev:
        panel_toggle_sort_order_prev(panel);
        break;
    case CK_PanelToggleSortOrderNext:
        panel_toggle_sort_order_next(panel);
        break;
    case CK_PanelReverseSort:
        panel->reverse = ! panel->reverse;
        panel_set_sort_order (panel, panel->current_sort_field);
        break;
    case CK_PanelSortOrderByName:
        panel_set_sort_type_by_id(panel, "name");
        break;
    case CK_PanelSortOrderByExt:
        panel_set_sort_type_by_id(panel, "extension");
        break;
    case CK_PanelSortOrderBySize:
        panel_set_sort_type_by_id(panel, "size");
        break;
    case CK_PanelSortOrderByMTime:
        panel_set_sort_type_by_id(panel, "mtime");
        break;
    }
   return res;
}

static cb_ret_t
panel_key (WPanel *panel, int key)
{
    size_t i;
    unsigned long res, command;

    for (i = 0; panel_map[i].key != 0; i++) {
	if (key == panel_map[i].key) {
	    int old_searching = panel->searching;

	    if (panel_map[i].command != CK_PanelStartSearch)
		panel->searching = 0;

	    command = panel_map[i].command;
	    res = panel_execute_cmd (panel, command);

	    if (res == MSG_NOT_HANDLED)
		return res;

	    if (panel->searching != old_searching)
		display_mini_info (panel);
	    return MSG_HANDLED;
	}
    }

    if (torben_fj_mode && key == ALT ('h')) {
	goto_middle_file (panel);
	return MSG_HANDLED;
    }

    if (is_abort_char (key)) {
	panel->searching = 0;
	display_mini_info (panel);
	return MSG_HANDLED;
    }

    /* Do not eat characters not meant for the panel below ' ' (e.g. C-l). */
    if ((key >= ' ' && key <= 255) || key == KEY_BACKSPACE) {
	if (panel->searching) {
	    do_search (panel, key);
	    return MSG_HANDLED;
	}

	if (!command_prompt) {
	    start_search (panel);
	    do_search (panel, key);
	    return MSG_HANDLED;
	}
    }

    return MSG_NOT_HANDLED;
}

static cb_ret_t
panel_callback (Widget *w, widget_msg_t msg, int parm)
{
    WPanel *panel = (WPanel *) w;
    WButtonBar *bb;

    switch (msg) {
    case WIDGET_DRAW:
	paint_panel (panel);
	return MSG_HANDLED;

    case WIDGET_FOCUS:
	current_panel = panel;
	panel->active = 1;
	if (mc_chdir (panel->cwd) != 0) {
	    char *cwd = strip_password (g_strdup (panel->cwd), 1);
	    message (D_ERROR, MSG_ERROR, _(" Cannot chdir to \"%s\" \n %s "),
		     cwd, unix_error_string (errno));
	    g_free(cwd);
	} else
	    subshell_chdir (panel->cwd);

	update_xterm_title_path ();
	select_item (panel);
	show_dir (panel);
	paint_dir (panel);
	panel->dirty = 0;

	bb = find_buttonbar (panel->widget.parent);
	midnight_set_buttonbar (bb);
	buttonbar_redraw (bb);
	return MSG_HANDLED;

    case WIDGET_UNFOCUS:
	/* Janne: look at this for the multiple panel options */
	if (panel->searching){
	    panel->searching = 0;
	    display_mini_info (panel);
	}
	panel->active = 0;
	show_dir (panel);
	unselect_item (panel);
	return MSG_HANDLED;

    case WIDGET_KEY:
	return panel_key (panel, parm);

    case WIDGET_DESTROY:
	panel_destroy (panel);
	return MSG_HANDLED;

    default:
	return default_proc (msg, parm);
    }
}

void
file_mark (WPanel *panel, int lc_index, int val)
{
    if (panel->dir.list[lc_index].f.marked != val) {
	panel->dir.list[lc_index].f.marked = val;
	panel->dirty = 1;
    }
}

/*                                     */
/* Panel mouse events support routines */
/*                                     */
static int mouse_marking = 0;

static void
mouse_toggle_mark (WPanel *panel)
{
    do_mark_file (panel, 0);
    mouse_marking = selection (panel)->f.marked;
}

static void
mouse_set_mark (WPanel *panel)
{
    if (mouse_marking && !(selection (panel)->f.marked))
	do_mark_file (panel, 0);
    else if (!mouse_marking && (selection (panel)->f.marked))
	do_mark_file (panel, 0);
}

static int
mark_if_marking (WPanel *panel, Gpm_Event *event)
{
    if (event->buttons & GPM_B_RIGHT){
	if (event->type & GPM_DOWN)
	    mouse_toggle_mark (panel);
	else
	    mouse_set_mark (panel);
	return 1;
    }
    return 0;
}

/* Determine which column was clicked, and sort the panel on
 * that column, or reverse sort on that column if already
 * sorted on that column.
 */
static void
mouse_sort_col(Gpm_Event *event, WPanel *panel)
{
    int i;
    const char *lc_sort_name = NULL;
    panel_field_t *col_sort_format = NULL;
    format_e *format;
    gchar *title;

    for (i = 0, format = panel->format; format != NULL; format = format->next) {
	i += format->field_len;
	if (event->x < i + 1) {
	    /* found column */
	    lc_sort_name = format->title;
	    break;
	}
    }

    if (lc_sort_name == NULL)
	return;

    for(i = 0; panel_fields[i].id != NULL; i++) {
	title = panel_get_title_without_hotkey(panel_fields[i].title_hotkey);
	if (!strcmp (lc_sort_name, title) && panel_fields[i].sort_routine) {
	    col_sort_format = &panel_fields[i];
	    g_free(title);
	    break;
	}
	g_free(title);
    }

    if (!col_sort_format)
	return;

    if (panel->current_sort_field == col_sort_format) {
	/* reverse the sort if clicked column is already the sorted column */
	panel->reverse = !panel->reverse;
    } else {
        /* new sort is forced to be ascending */
	panel->reverse = 0;
    }
    panel_set_sort_order (panel, col_sort_format);
}


/*
 * Mouse callback of the panel minus repainting.
 * If the event is redirected to the menu, *redir is set to 1.
 */
static int
do_panel_event (Gpm_Event *event, WPanel *panel, int *redir)
{
    const int lines = llines (panel);
    const gboolean is_active = dlg_widget_active (panel);
    const gboolean mouse_down = (event->type & GPM_DOWN) != 0;

    /* "." button show/hide hidden files */
    if (event->type & GPM_DOWN && event->x == panel->widget.cols - 5 && event->y == 1) {
	toggle_show_hidden();
	repaint_screen ();
	return MOU_NORMAL;
    }

    /* "<" button */
    if (mouse_down && event->y == 1 && event->x == 2) {
	directory_history_prev (panel);
	return MOU_NORMAL;
    }

    /* ">" button */
    if (mouse_down && event->y == 1 && event->x == panel->widget.cols - 1) {
	directory_history_next (panel);
	return MOU_NORMAL;
    }

    /* "^" button */
    if (mouse_down && event->y == 1 && event->x >= panel->widget.cols - 4 && event->x <= panel->widget.cols - 2) {
	directory_history_list (panel);
	return MOU_NORMAL;
    }

    /* sort on clicked column */
    if (event->type & GPM_DOWN && event->y == 2) {
	mouse_sort_col(event,panel);
	return MOU_NORMAL;
    }

    /* rest of the upper frame, the menu is invisible - call menu */
    if (mouse_down && event->y == 1 && !menubar_visible) {
	*redir = 1;
	event->x += panel->widget.x;
	return the_menubar->widget.mouse (event, the_menubar);
    }

    /* Mouse wheel events */
    if (mouse_down && (event->buttons & GPM_B_UP)) {
	if (is_active) {
	    if (panel->top_file > 0)
		prev_page (panel);
	    else                    /* We are in first page */
		move_up (panel);
	}
	return MOU_NORMAL;
    }

    if (mouse_down && (event->buttons & GPM_B_DOWN)) {
	if (is_active) {
	    if (panel->top_file + ITEMS (panel) < panel->count)
		next_page (panel);
	    else                    /* We are in last page */
		move_down (panel);
	}
	return MOU_NORMAL;
    }

    event->y -= 2;
    if ((event->type & (GPM_DOWN | GPM_DRAG))) {
	int my_index;

	if (!is_active)
	    change_panel ();

	if (event->y <= 0) {
	    mark_if_marking (panel, event);
	    if (mouse_move_pages)
		prev_page (panel);
	    else
		move_up (panel);
	    return MOU_REPEAT;
	}

	if (!((panel->top_file + event->y <= panel->count) && event->y <= lines)) {
	    mark_if_marking (panel, event);
	    if (mouse_move_pages)
		next_page (panel);
	    else
		move_down (panel);
	    return MOU_REPEAT;
	}

	my_index = panel->top_file + event->y - 1;
	if (panel->split && (event->x > ((panel->widget.cols - 2) / 2)))
	    my_index += llines (panel);

	if (my_index >= panel->count)
	    my_index = panel->count - 1;

	if (my_index != panel->selected) {
	    unselect_item (panel);
	    panel->selected = my_index;
	    select_item (panel);
	}

	/* This one is new */
	mark_if_marking (panel, event);
    } else if ((event->type & (GPM_UP | GPM_DOUBLE)) == (GPM_UP | GPM_DOUBLE)) {
	if (event->y > 0 && event->y <= lines)
	    do_enter (panel);
    }
    return MOU_NORMAL;
}

/* Mouse callback of the panel */
static int
panel_event (Gpm_Event *event, void *data)
{
    WPanel *panel = data;
    int ret;
    int redir = MOU_NORMAL;

    ret = do_panel_event (event, panel, &redir);
    if (!redir)
	paint_panel (panel);

    return ret;
}

void
panel_re_sort (WPanel *panel)
{
    char *filename;
    int  i;

    if (panel == NULL)
	    return;

    filename = g_strdup (selection (panel)->fname);
    unselect_item (panel);
    do_sort (&panel->dir, panel->current_sort_field->sort_routine, panel->count-1, panel->reverse,
             panel->case_sensitive, panel->exec_first);
    panel->selected = -1;
    for (i = panel->count; i; i--){
	if (!strcmp (panel->dir.list [i-1].fname, filename)){
	    panel->selected = i-1;
	    break;
	}
    }
    g_free (filename);
    panel->top_file = panel->selected - ITEMS (panel)/2;
    if (panel->top_file < 0)
	panel->top_file = 0;
    select_item (panel);
    panel->dirty = 1;
}

void
panel_set_sort_order (WPanel *panel, const panel_field_t *sort_order)
{
    if (sort_order == 0)
	return;

    panel->current_sort_field = sort_order;

    /* The directory is already sorted, we have to load the unsorted stuff */
    if (sort_order->sort_routine == (sortfn *) unsorted){
	char *current_file;

	current_file = g_strdup (panel->dir.list [panel->selected].fname);
	panel_reload (panel);
	try_to_select (panel, current_file);
	g_free (current_file);
    }
    panel_re_sort (panel);
}

void
set_panel_encoding (WPanel *panel)
{
    const char *encoding = NULL;
    char *cd_path;
#ifdef HAVE_CHARSET
    const char *errmsg;
    int r;

    r = select_charset (-1, -1, default_source_codepage, FALSE);

    if (r == SELECT_CHARSET_CANCEL)
	return; /* Cancel */

    if (r == SELECT_CHARSET_NO_TRANSLATE) {
	/* No translation */
	errmsg = init_translation_table (display_codepage, display_codepage);
	cd_path = remove_encoding_from_path (panel->cwd);
	do_panel_cd (panel, cd_path, 0);
	g_free (cd_path);
	return;
    }

    source_codepage = r;

    errmsg = init_translation_table (source_codepage, display_codepage);
    if (errmsg) {
        message (D_ERROR, MSG_ERROR, "%s", errmsg);
        return;
    }

    encoding = get_codepage_id (source_codepage);
#endif
    if (encoding != NULL) {
        cd_path = add_encoding_to_path (panel->cwd, encoding);
        if (!do_panel_cd (panel, cd_path, 0))
            message (D_ERROR, MSG_ERROR, _(" Cannot chdir to %s "), cd_path);
        g_free (cd_path);
    }
}

static void
reload_panelized (WPanel *panel)
{
    int i, j;
    dir_list *list = &panel->dir;

    if (panel != current_panel)
	mc_chdir (panel->cwd);

    for (i = 0, j = 0; i < panel->count; i++) {
	if (list->list[i].f.marked) {
	    /* Unmark the file in advance. In case the following mc_lstat
	     * fails we are done, else we have to mark the file again
	     * (Note: do_file_mark depends on a valid "list->list [i].buf").
	     * IMO that's the best way to update the panel's summary status
	     * -- Norbert
	     */
	    do_file_mark (panel, i, 0);
	}
	if (mc_lstat (list->list[i].fname, &list->list[i].st)) {
	    g_free (list->list[i].fname);
	    continue;
	}
	if (list->list[i].f.marked)
	    do_file_mark (panel, i, 1);
	if (j != i)
	    list->list[j] = list->list[i];
	j++;
    }
    if (j == 0)
	panel->count = set_zero_dir (list) ? 1 : 0;
    else
	panel->count = j;

    if (panel != current_panel)
	mc_chdir (current_panel->cwd);
}

static void
update_one_panel_widget (WPanel *panel, int force_update,
			 const char *current_file)
{
    int free_pointer;
    char *my_current_file = NULL;

    if (force_update & UP_RELOAD) {
	panel->is_panelized = 0;
	mc_setctl (panel->cwd, VFS_SETCTL_FLUSH, 0);
	memset (&(panel->dir_stat), 0, sizeof (panel->dir_stat));
    }

    /* If current_file == -1 (an invalid pointer) then preserve selection */
    if (current_file == UP_KEEPSEL) {
	free_pointer = 1;
	my_current_file = g_strdup (panel->dir.list[panel->selected].fname);
	current_file = my_current_file;
    } else
	free_pointer = 0;

    if (panel->is_panelized)
	reload_panelized (panel);
    else
	panel_reload (panel);

    try_to_select (panel, current_file);
    panel->dirty = 1;

    if (free_pointer)
	g_free (my_current_file);
}

static void
update_one_panel (int which, int force_update, const char *current_file)
{
    if (get_display_type (which) == view_listing) {
	WPanel *panel;
	panel = (WPanel *) get_panel_widget (which);
	update_one_panel_widget (panel, force_update, current_file);
    }
}

/* This routine reloads the directory in both panels. It tries to
 * select current_file in current_panel and other_file in other_panel.
 * If current_file == -1 then it automatically sets current_file and
 * other_file to the currently selected files in the panels.
 *
 * if force_update has the UP_ONLY_CURRENT bit toggled on, then it
 * will not reload the other panel.
*/
void
update_panels (int force_update, const char *current_file)
{
    int reload_other = !(force_update & UP_ONLY_CURRENT);
    WPanel *panel;

    update_one_panel (get_current_index (), force_update, current_file);
    if (reload_other)
	update_one_panel (get_other_index (), force_update, UP_KEEPSEL);

    if (get_current_type () == view_listing)
	panel = (WPanel *) get_panel_widget (get_current_index ());
    else
	panel = (WPanel *) get_panel_widget (get_other_index ());

    mc_chdir (panel->cwd);
}

gsize
panel_get_num_of_sortable_fields(void)
{
    gsize ret = 0, lc_index;

    for(lc_index=0; panel_fields[lc_index].id != NULL; lc_index ++)
	if (panel_fields[lc_index].is_user_choice)
	    ret++;
    return ret;
}


const char **
panel_get_sortable_fields(gsize *array_size)
{
    char **ret;
    gsize lc_index, i;

    lc_index = panel_get_num_of_sortable_fields();

    ret = g_try_new0 (char *, lc_index + 1);
    if (ret == NULL)
        return NULL;

    if (array_size != NULL)
	*array_size = lc_index;

    lc_index=0;

    for(i=0; panel_fields[i].id != NULL; i ++)
	if (panel_fields[i].is_user_choice)
	    ret[lc_index++] = g_strdup(_(panel_fields[i].title_hotkey));
    return (const char**) ret;
}

const panel_field_t *
panel_get_field_by_id(const char *name)
{
    gsize lc_index;
    for(lc_index=0; panel_fields[lc_index].id != NULL; lc_index ++)
	if (
	    panel_fields[lc_index].id != NULL &&
	    strcmp(name, panel_fields[lc_index].id) == 0
	)
	    return &panel_fields[lc_index];
    return NULL;
}

const panel_field_t *
panel_get_field_by_title_hotkey(const char *name)
{
    gsize lc_index;
    for(lc_index=0; panel_fields[lc_index].id != NULL; lc_index ++)
	if (
	    panel_fields[lc_index].title_hotkey != NULL &&
	    strcmp(name, _(panel_fields[lc_index].title_hotkey)) == 0
	)
	    return &panel_fields[lc_index];
    return NULL;
}

const panel_field_t *
panel_get_field_by_title(const char *name)
{
    gsize lc_index;
    gchar *title = NULL;

    for(lc_index=0; panel_fields[lc_index].id != NULL; lc_index ++) {
	title = panel_get_title_without_hotkey(panel_fields[lc_index].title_hotkey);
	if (
	    panel_fields[lc_index].title_hotkey != NULL &&
	    strcmp(name, title) == 0
	) {
	    g_free(title);
	    return &panel_fields[lc_index];
	}
    }
    g_free(title);
    return NULL;
}

gsize
panel_get_num_of_user_possible_fields(void)
{
    gsize ret = 0, lc_index;

    for(lc_index=0; panel_fields[lc_index].id != NULL; lc_index ++)
	if (panel_fields[lc_index].use_in_user_format)
	    ret++;
    return ret;
}

const char **
panel_get_user_possible_fields(gsize *array_size)
{
    char **ret;
    gsize lc_index, i;

    lc_index = panel_get_num_of_user_possible_fields();

    ret = g_try_new0 (char *, lc_index + 1);
    if (ret == NULL)
        return NULL;

    if (array_size != NULL)
	*array_size = lc_index;

    lc_index=0;

    for(i=0; panel_fields[i].id != NULL; i ++)
	if (panel_fields[i].use_in_user_format)
	    ret[lc_index++] = g_strdup(_(panel_fields[i].title_hotkey));
    return (const char**) ret;
}

void
panel_init(void)
{
    panel_sort_up_sign   = mc_skin_get("widget-common","sort-sign-up","'");
    panel_sort_down_sign = mc_skin_get("widget-common","sort-sign-down",",");

    panel_hiddenfiles_sign_show  = mc_skin_get("widget-panel", "hiddenfiles-sign-show", ".");
    panel_hiddenfiles_sign_hide  = mc_skin_get("widget-panel", "hiddenfiles-sign-hide", ".");
    panel_history_prev_item_sign = mc_skin_get("widget-panel", "history-prev-item-sign", "<");
    panel_history_next_item_sign = mc_skin_get("widget-panel", "history-next-item-sign", ">");
    panel_history_show_list_sign = mc_skin_get("widget-panel", "history-show-list-sign", "^");

}

void
panel_deinit(void)
{
    g_free(panel_sort_up_sign);
    g_free(panel_sort_down_sign);

    g_free(panel_hiddenfiles_sign_show);
    g_free(panel_hiddenfiles_sign_hide);
    g_free(panel_history_prev_item_sign);
    g_free(panel_history_next_item_sign);
    g_free(panel_history_show_list_sign);

}
