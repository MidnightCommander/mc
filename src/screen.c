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

#include "global.h"

#include "../src/tty/tty.h"
#include "../src/tty/color.h"
#include "../src/tty/mouse.h"		/* For Gpm_Event */
#include "../src/tty/key.h"		/* XCTRL and ALT macros  */
#include "../src/tty/win.h"

#include "dir.h"
#include "panel.h"
#include "tree.h"
#include "ext.h"		/* regexp_command */
#include "layout.h"		/* Most layout variables are here */
#include "wtools.h"		/* for message (...) */
#include "cmd.h"
#include "setup.h"		/* For loading/saving panel options */
#include "user.h"
#include "../src/mcconfig/mcconfig.h"
#include "execute.h"
#include "widget.h"
#include "menu.h"		/* menubar_visible */
#include "main-widgets.h"
#include "main.h"		/* the_menubar */
#include "unixcompat.h"
#include "mountlist.h"		/* my_statfs */
#include "selcodepage.h"	/* select_charset () */
#include "charsets.h"		/* get_codepage_id () */
#include "strutil.h"

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
    const char   *title;
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

static cb_ret_t panel_callback (Widget *, widget_msg_t msg, int parm);
static int panel_event (Gpm_Event *event, void *);
static void paint_frame (WPanel *panel);
static const char *panel_format (WPanel *panel);
static const char *mini_status_format (WPanel *panel);

/* This macro extracts the number of available lines in a panel */
#define llines(p) (p->widget.lines-3 - (show_mini_info ? 2 : 0))

static void
set_colors (WPanel *panel)
{
    (void) panel;
    standend ();
    tty_setcolor (NORMAL_COLOR);
}

/* Delete format string, it is a linked list */
static void
delete_format (format_e *format)
{
    format_e *next;

    while (format){
        next = format->next;
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
        } else
            tty_setcolor (color);

	addch (dest[i]);
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

static inline unsigned int ilog10(dev_t n)
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
	size_trunc_len (buffer, len, fe->st.st_size, 0);
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
    if (!strcmp (fe->fname, "..")) {
       return "";
    }
    return file_date (fe->st.st_mtime);
}

/* atime */
static const char *
string_file_atime (file_entry *fe, int len)
{
    (void) len;
    if (!strcmp (fe->fname, "..")) {
       return "";
    }
    return file_date (fe->st.st_atime);
}

/* ctime */
static const char *
string_file_ctime (file_entry *fe, int len)
{
    (void) len;
    if (!strcmp (fe->fname, "..")) {
       return "";
    }
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

static struct {
    const char *id;
    int  min_size;
    int  expands;
    align_crt_t default_just;
    const char *title;
    int  use_in_gui;
    const char *(*string_fn)(file_entry *, int);
    sortfn *sort_routine; /* This field is currently unused. */
} formats [] = {
{ "name",  12, 1, J_LEFT_FIT,	N_("Name"),	1, string_file_name,	   (sortfn *) sort_name },
{ "size",  7,  0, J_RIGHT,	N_("Size"),	1, string_file_size,	   (sortfn *) sort_size },
{ "bsize", 7,  0, J_RIGHT,	N_("Size"),	1, string_file_size_brief, (sortfn *) sort_size },
{ "type",  GT, 0, J_LEFT,	"",		2, string_file_type,	   NULL },
{ "mtime", 12, 0, J_RIGHT,	N_("MTime"),	1, string_file_mtime,	   (sortfn *) sort_time },
{ "atime", 12, 0, J_RIGHT,	N_("ATime"),	1, string_file_atime,	   (sortfn *) sort_atime },
{ "ctime", 12, 0, J_RIGHT,	N_("CTime"),	1, string_file_ctime,	   (sortfn *) sort_ctime },
{ "perm",  10, 0, J_LEFT,	N_("Permission"),1,string_file_permission, NULL },
{ "mode",  6,  0, J_RIGHT,	N_("Perm"),	1, string_file_perm_octal, NULL },
{ "nlink", 2,  0, J_RIGHT,	N_("Nl"),	1, string_file_nlinks,	   NULL },
{ "inode", 5,  0, J_RIGHT,	N_("Inode"),	1, string_inode,	   (sortfn *) sort_inode },
{ "nuid",  5,  0, J_RIGHT,	N_("UID"),	1, string_file_nuid,	   NULL },
{ "ngid",  5,  0, J_RIGHT,	N_("GID"),	1, string_file_ngid,	   NULL },
{ "owner", 8,  0, J_LEFT_FIT,	N_("Owner"),	1, string_file_owner,	   NULL },
{ "group", 8,  0, J_LEFT_FIT,	N_("Group"),	1, string_file_group,	   NULL },
{ "mark",  1,  0, J_RIGHT,	" ",		1, string_marked,	   NULL },
{ "|",     1,  0, J_RIGHT,	" ",		0, NULL,		   NULL },
{ "space", 1,  0, J_RIGHT,	" ",		0, string_space,	   NULL },
{ "dot",   1,  0, J_RIGHT,	" ",		0, string_dot,		   NULL },
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

    /* if filetype_mode == true  */
    if (S_ISDIR (fe->st.st_mode))
	return (DIRECTORY_COLOR);
    else if (S_ISLNK (fe->st.st_mode)) {
	if (fe->f.link_to_dir)
	    return (DIRECTORY_COLOR);
	else if (fe->f.stale_link)
	    return (STALE_LINK_COLOR);
	else
	    return (LINK_COLOR);
    } else if (S_ISSOCK (fe->st.st_mode))
	return (SPECIAL_COLOR);
    else if (S_ISCHR (fe->st.st_mode))
	return (DEVICE_COLOR);
    else if (S_ISBLK (fe->st.st_mode))
	return (DEVICE_COLOR);
    else if (S_ISNAM (fe->st.st_mode))
	return (DEVICE_COLOR);
    else if (S_ISFIFO (fe->st.st_mode))
	return (SPECIAL_COLOR);
    else if (S_ISDOOR (fe->st.st_mode))
	return (SPECIAL_COLOR);
    else if (!S_ISREG (fe->st.st_mode))
	return (STALE_LINK_COLOR);	/* non-regular file of unknown kind */
    else if (is_exe (fe->st.st_mode))
	return (EXECUTABLE_COLOR);
    else if (fe->fname && (!strcmp (fe->fname, "core")
			   || !strcmp (extension (fe->fname), "core")))
	return (CORE_COLOR);

    return (NORMAL_COLOR);
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

            tty_setcolor (color);

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
	    one_vline ();
	    length++;
	}
    }

    if (length < width){
	int still = width - length;
	while (still--)
	    addch (' ');
    }
}

static void
repaint_file (WPanel *panel, int file_index, int mv, int attr, int isstatus)
{
    int    second_column = 0;
    int	   width, offset;
    char   buffer [BUF_MEDIUM];

    offset = 0;
    if (!isstatus && panel->split){

	second_column = (file_index - panel->top_file) / llines (panel);
	width = (panel->widget.cols - 2)/2 - 1;

	if (second_column){
	    offset = 1 + width;
	    width = (panel->widget.cols-2) - (panel->widget.cols-2)/2 - 1;
	}
    } else
        width = (panel->widget.cols - 2);

    /* Nothing to paint */
    if (width <= 0)
	return;

    if (mv){
	if (!isstatus && panel->split){
	    widget_move (&panel->widget,
			 (file_index - panel->top_file) %
			 llines (panel) + 2,
			 (offset + 1));
	} else
	    widget_move (&panel->widget, file_index - panel->top_file + 2, 1);
    }

    format_file (buffer, sizeof(buffer), panel, file_index, width, attr, isstatus);

    if (!isstatus && panel->split){
	if (second_column)
	    addch (' ');
	else {
	    tty_setcolor (NORMAL_COLOR);
	    one_vline ();
	}
    }
}

static void
display_mini_info (WPanel *panel)
{
    widget_move (&panel->widget, llines (panel)+3, 1);

    if (panel->searching){
	tty_setcolor (INPUT_COLOR);
        tty_print_string ("/");
        tty_print_string (str_fit_to_term (panel->search_buffer, 
                panel->widget.cols - 3, J_LEFT));
	tty_setcolor (NORMAL_COLOR);
	return;
    }

    /* Status resolves links and show them */
    set_colors (panel);

    if (S_ISLNK (panel->dir.list [panel->selected].st.st_mode)){
	char *link, link_target [MC_MAXPATHLEN];
	int  len;

	link = concat_dir_and_file (panel->cwd, panel->dir.list [panel->selected].fname);
	len = mc_readlink (link, link_target, MC_MAXPATHLEN - 1);
	g_free (link);
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
	 * while loading directory (do_load_dir() and do_reload_dir(),
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
    standend ();
}

static void
display_total_marked_size (WPanel *panel, int y, int x, gboolean size_only)
{
    char buffer[BUF_SMALL], b_bytes[BUF_SMALL], *buf;
    int cols;
    size_t blen;

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

    blen = strlen (buf);

    /* don't forget spaces around buffer content */
    if ((int) blen > cols - 2) {
	buf[cols - 2] = '\0';
	blen = (size_t) (cols - 2);
    }

    if (x < 0)
	/* center in panel */
	x = (panel->widget.cols - (int) blen) / 2 - 1;

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
    const int y = llines (panel) + 2;

    standend ();
    widget_move (&panel->widget, y, 1);
    tty_setcolor (NORMAL_COLOR);
#ifdef HAVE_SLANG
    hline (ACS_HLINE, panel->widget.cols - 2);
#else
    hline ((slow_terminal ? '-' : ACS_HLINE) | NORMAL_COLOR,
	   panel->widget.cols - 2);
#endif				/* !HAVE_SLANG */

    /* Status displays total marked size.
     * Centered in panel, full format. */
    display_total_marked_size (panel, y, -1, FALSE);
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
    int len;

    set_colors (panel);
    draw_double_box (panel->widget.parent,
		     panel->widget.y, panel->widget.x,
		     panel->widget.lines, panel->widget.cols);

    if (show_mini_info) {
	widget_move (&panel->widget, llines (panel) + 2, 0);
	tty_print_alt_char (ACS_LTEE);
	widget_move (&panel->widget, llines (panel) + 2,
			panel->widget.cols - 1);
	tty_print_alt_char (ACS_RTEE);
    }

    if (panel->active)
	tty_setcolor (REVERSE_COLOR);

    widget_move (&panel->widget, 0, 3);

    addch (' ');
    len = min (max (panel->widget.cols - 9, 0), panel->widget.cols); 
    tty_print_string (str_term_trim (strip_home_and_password (panel->cwd), len));
    addch (' ');

    widget_move (&panel->widget, 0, 1);
    tty_print_string ("<");
    widget_move (&panel->widget, 0, panel->widget.cols - 2);
    tty_print_string (">");
    widget_move (&panel->widget, 0, panel->widget.cols - 3);
    tty_print_string ("v");

    if (!show_mini_info) {
	if (panel->marked == 0) {
	    /* Show size of curret file in the bottom of panel */
	    if (S_ISREG (panel->dir.list [panel->selected].st.st_mode)) {
		char buffer[BUF_SMALL];

		g_snprintf (buffer, sizeof (buffer), " %s ",
			    size_trunc_sep (panel->dir.list [panel->selected].st.st_size));
		tty_setcolor (NORMAL_COLOR);
		widget_move (&panel->widget, panel->widget.lines - 1, 2);
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
	standend ();
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

    if (show_mini_info) {
	mini_info_separator (panel);
	display_mini_info (panel);
    }

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
            result = g_strconcat (path, "/#enc:", encoding, NULL);
        } else {
            *semi = 0;
            result = g_strconcat (path, "/#enc:", encoding, NULL);
            *semi = '#';
        }
    } else {
        result = g_strconcat (path, "/#enc:", encoding, NULL);
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

static inline void
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

static char *
panel_save_name (WPanel *panel)
{
    extern int saving_setup;

    /* If the program is shuting down */
    if ((midnight_shutdown && auto_save_setup) || saving_setup)
	return  g_strdup (panel->panel_name);
    else
	return  g_strconcat ("Temporal:", panel->panel_name, (char *) NULL);
}

static void
panel_destroy (WPanel *p)
{
    int i;

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
    WPanel *panel;
    char *section;
    int i, err;

    panel = g_new0 (WPanel, 1);

    /* No know sizes of the panel at startup */
    init_widget (&panel->widget, 0, 0, 0, 0, panel_callback, panel_event);

    /* We do not want the cursor */
    widget_want_cursor (panel->widget, 0);

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
    if (err) {
	set_panel_formats (panel);
    }

    /* Load the default format */
    panel->count =
	do_load_dir (panel->cwd, &panel->dir, panel->sort_type,
		     panel->reverse, panel->case_sensitive,
		     panel->exec_first, panel->filter);
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
	    panel->count = set_zero_dir (&panel->dir);
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
	do_reload_dir (panel->cwd, &panel->dir, panel->sort_type,
		       panel->count, panel->reverse, panel->case_sensitive,
		       panel->exec_first, panel->filter);

    panel->dirty = 1;
    if (panel->selected >= panel->count)
	do_select (panel, panel->count - 1);

    recalculate_panel_summary (panel);
}

static void
paint_frame (WPanel *panel)
{
    int  header_len;
    int  side, width;

    char *txt = NULL;
    if (!panel->split)
	adjust_top_file (panel);

    widget_erase (&panel->widget);
    show_dir (panel);

    widget_move (&panel->widget, 1, 1);

    for (side = 0; side <= panel->split; side++){
	format_e *format;

	if (side){
	    tty_setcolor (NORMAL_COLOR);
	    one_vline ();
	    width = panel->widget.cols - panel->widget.cols/2 - 1;
	} else if (panel->split)
	    width = panel->widget.cols/2 - 3;
	else
	    width = panel->widget.cols - 2;

	for (format = panel->format; format; format = format->next){
            if (format->string_fn){
                if (panel->filter && !strcmp (format->id, "name")) {
                    txt = g_strdup_printf ("%s [%s]", format->title, panel->filter);
                } else {
                    txt = g_strdup (format->title);
                }

		header_len = strlen (txt);
		if (header_len > format->field_len)
		    header_len = format->field_len;

                tty_setcolor (MARKED_COLOR);
                tty_print_string (str_fit_to_term (txt, format->field_len, J_CENTER_LEFT));
                g_free(txt);
                width -= format->field_len;
	    } else {
		tty_setcolor (NORMAL_COLOR);
		one_vline ();
		width --;
		continue;
	    }
	}

	if (width > 0)
	    tty_printf ("%*s", width, "");
    }
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
    int  items = 0;			/* Number of items in the format */
    size_t  i;

    static size_t i18n_timelength = 0; /* flag: check ?Time length at startup */

    *error = 0;

    if (i18n_timelength == 0) {
	i18n_timelength = i18n_checktimelength ();	/* Musn't be 0 */

	for (i = 0; i < ELEMENTS(formats); i++)
	    if (strcmp ("time", formats [i].id+1) == 0)
		formats [i].min_size = i18n_timelength;
    }

    /*
     * This makes sure that the panel and mini status full/half mode
     * setting is equal
     */
    format = parse_panel_size (panel, format, isstatus);

    while (*format){           /* format can be an empty string */
	int found = 0;

        darr = g_new (format_e, 1);

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

	for (i = 0; i < ELEMENTS(formats); i++){
	    int klen = strlen (formats [i].id);

	    if (strncmp (format, formats [i].id, klen) != 0)
		continue;

	    format += klen;

	    if (formats [i].use_in_gui)
	    	items++;

            darr->requested_field_len = formats [i].min_size;
            darr->string_fn           = formats [i].string_fn;
	    if (formats [i].title [0])
		    darr->title = _(formats [i].title);
	    else
		    darr->title = "";
            darr->id                  = formats [i].id;
	    darr->expand              = formats [i].expands;
	    darr->just_mode 	      = formats [i].default_just;

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
    char *err;
    int retcode = 0;

    form = use_display_format (p, panel_format (p), &err, 0);

    if (err){
        g_free (err);
        retcode = 1;
    }
    else {
        if (p->format)
    	    delete_format (p->format);

	p->format = form;
    }

    if (show_mini_info){

	form = use_display_format (p, mini_status_format (p), &err, 1);

	if (err){
	    g_free (err);
	    retcode += 2;
	}
	else {
	    if (p->status_format)
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
	panel_scroll_pages){
	/* Scroll window half screen */
	panel->top_file += ITEMS (panel)/2;
	if (panel->top_file > panel->count - ITEMS (panel))
		panel->top_file = panel->count - ITEMS (panel);
	paint_dir (panel);
	select_item (panel);
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
    if (panel->selected < panel->top_file && panel_scroll_pages){
	/* Scroll window half screen */
	panel->top_file -= ITEMS (panel)/2;
	if (panel->top_file < 0) panel->top_file = 0;
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
move_left (WPanel *panel, int c_code)
{
    (void) c_code;
    if (panel->split) {
	move_selection (panel, -llines (panel));
	return MSG_HANDLED;
    } else
	return maybe_cd (1); /* cd .. */
}

static int
move_right (WPanel *panel, int c_code)
{
    (void) c_code;
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
    size_t l, max, buf_max;
    int i, sel;
    int wrapped = 0;
    char *act;

    l = strlen (panel->search_buffer);
    if (c_code == KEY_BACKSPACE) {
	if (l != 0) {
            act = panel->search_buffer + l;
            str_prev_noncomb_char (&act, panel->search_buffer);
            act[0] = '\0';
        }
        panel->search_chpoint = 0;
    } else {
        if (c_code && (gsize) panel->search_chpoint < sizeof (panel->search_char)) {
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

    buf_max = panel->case_sensitive ? 
            str_prefix (panel->search_buffer, panel->search_buffer) :
            str_caseprefix (panel->search_buffer, panel->search_buffer);
    max = 0;
    sel = panel->selected;
    for (i = panel->selected; !wrapped || i != panel->selected; i++) {
	if (i >= panel->count) {
	    i = 0;
	    if (wrapped)
		break;
	    wrapped = 1;
	}
        l = panel->case_sensitive ? 
            str_prefix (panel->dir.list[i].fname, panel->search_buffer) :
            str_caseprefix (panel->dir.list[i].fname, panel->search_buffer);
        if (l > max) {
            max = l;
            sel = i;
            if (max == buf_max) break;
	}
    }
    
	    unselect_item (panel);
    panel->selected = sel;
	    select_item (panel);
    
    act = panel->search_buffer + strlen (panel->search_buffer);
    while (max < buf_max) {
        str_prev_char_safe (&act);
        act[0] = '\0';
        buf_max = panel->case_sensitive ? 
            str_prefix (panel->search_buffer, panel->search_buffer) :
            str_caseprefix (panel->search_buffer, panel->search_buffer);
    }

    paint_panel (panel);
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
	tty_refresh ();
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
#ifdef USE_VFS
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

typedef void (*panel_key_callback) (WPanel *);
typedef struct {
    int key_code;
    panel_key_callback fn;
} panel_key_map;

static void cmd_do_enter(WPanel *wp) { (void) do_enter(wp); }
static void cmd_view_simple(WPanel *wp) { (void) wp; view_simple_cmd(); }
static void cmd_edit_new(WPanel *wp) { (void) wp; edit_cmd_new(); }
static void cmd_copy_local(WPanel *wp) { (void) wp;copy_cmd_local(); }
static void cmd_rename_local(WPanel *wp) { (void) wp;ren_cmd_local(); }
static void cmd_delete_local(WPanel *wp) { (void) wp;delete_cmd_local(); }
static void cmd_select(WPanel *wp) { (void) wp;select_cmd(); }
static void cmd_unselect(WPanel *wp) { (void) wp;unselect_cmd(); }
static void cmd_reverse_selection(WPanel *wp) { (void) wp;reverse_selection_cmd(); }

static const panel_key_map panel_keymap [] = {
    { KEY_DOWN,   move_down },
    { KEY_UP, 	move_up },

    /* The action button :-) */
    { '\n',       cmd_do_enter },
    { KEY_ENTER,  cmd_do_enter },

    { KEY_IC,     mark_file },
    { KEY_HOME,	  move_home },
    { KEY_A1,     move_home },
    { ALT ('<'),  move_home },
    { KEY_C1,     move_end },
    { KEY_END,    move_end },
    { ALT ('>'),  move_end },
    { KEY_NPAGE,  next_page },
    { KEY_PPAGE,  prev_page },
    { KEY_NPAGE | KEY_M_CTRL, ctrl_next_page },
    { KEY_PPAGE | KEY_M_CTRL, ctrl_prev_page },

    /* To quickly move in the panel */
    { ALT('g'),   goto_top_file },
    { ALT('r'),   goto_middle_file }, /* M-r like emacs */
    { ALT('j'),   goto_bottom_file },

    /* Emacs-like bindings */
    { XCTRL('v'), next_page },		/* C-v like emacs */
    { ALT('v'),   prev_page },		/* M-v like emacs */
    { XCTRL('p'), move_up },		/* C-p like emacs */
    { XCTRL('n'), move_down },		/* C-n like emacs */
    { XCTRL('s'), start_search },	/* C-s like emacs */
    { ALT('s'),   start_search },	/* M-s not like emacs */
/*    { XCTRL('t'), mark_file },*/
    { XCTRL('t'), set_panel_encoding },
    { ALT('o'),   chdir_other_panel },
    { ALT('i'),   sync_other_panel },
    { ALT('l'),   chdir_to_readlink },
    { ALT('H'),   directory_history_list },
    { KEY_F(13),  cmd_view_simple },
    { KEY_F(14),  cmd_edit_new },
    { KEY_F(15),  cmd_copy_local },
    { KEY_F(16),  cmd_rename_local },
    { KEY_F(18),  cmd_delete_local },
    { ALT('y'),   directory_history_prev },
    { ALT('u'),   directory_history_next },
    { ALT('+'),	  cmd_select },
    { KEY_KP_ADD, cmd_select },
    { ALT('\\'),  cmd_unselect },
    { ALT('-'),	  cmd_unselect },
    { KEY_KP_SUBTRACT, cmd_unselect },
    { ALT('*'),	  cmd_reverse_selection },
    { KEY_KP_MULTIPLY, cmd_reverse_selection },
    { 0, 0 }
};

static inline cb_ret_t
panel_key (WPanel *panel, int key)
{
    int i;

    for (i = 0; panel_keymap[i].key_code; i++) {
	if (key == panel_keymap[i].key_code) {
	    int old_searching = panel->searching;

	    if (panel_keymap[i].fn != start_search)
		panel->searching = 0;

	    (*panel_keymap[i].fn) (panel);

	    if (panel->searching != old_searching)
		display_mini_info (panel);
	    return MSG_HANDLED;
	}
    }
    if (torben_fj_mode && key == ALT ('h')) {
	goto_middle_file (panel);
	return MSG_HANDLED;
    }

    /* We do not want to take a key press if nothing can be done with it */
    /* The command line widget may do something more useful */
    if (key == KEY_LEFT)
	return move_left (panel, key);

    if (key == KEY_RIGHT)
	return move_right (panel, key);

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
    Dlg_head *h = panel->widget.parent;

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

	buttonbar_set_label (h, 1, _("Help"), help_cmd);
	buttonbar_set_label (h, 2, _("Menu"), user_file_menu_cmd);
	buttonbar_set_label (h, 3, _("View"), view_cmd);
	buttonbar_set_label (h, 4, _("Edit"), edit_cmd);
	buttonbar_set_label (h, 5, _("Copy"), copy_cmd);
	buttonbar_set_label (h, 6, _("RenMov"), ren_cmd);
	buttonbar_set_label (h, 7, _("Mkdir"), mkdir_cmd);
	buttonbar_set_label (h, 8, _("Delete"), delete_cmd);
	buttonbar_redraw (h);
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
file_mark (WPanel *panel, int index, int val)
{
    if (panel->dir.list[index].f.marked != val) {
	panel->dir.list[index].f.marked = val;
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

static inline int
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

/*
 * Mouse callback of the panel minus repainting.
 * If the event is redirected to the menu, *redir is set to 1.
 */
static int
do_panel_event (Gpm_Event *event, WPanel *panel, int *redir)
{
    const int lines = llines (panel);

    int my_index;

    /* Mouse wheel events */
    if ((event->buttons & GPM_B_UP) && (event->type & GPM_DOWN)) {
	prev_page (panel);
	return MOU_NORMAL;
    }
    if ((event->buttons & GPM_B_DOWN) && (event->type & GPM_DOWN)) {
	next_page (panel);
	return MOU_NORMAL;
    }

    /* "<" button */
    if (event->type & GPM_DOWN && event->x == 2 && event->y == 1) {
	directory_history_prev (panel);
	return MOU_NORMAL;
    }

    /* ">" button */
    if (event->type & GPM_DOWN && event->x == panel->widget.cols - 1
	&& event->y == 1) {
	directory_history_next (panel);
	return MOU_NORMAL;
    }

    /* "v" button */
    if (event->type & GPM_DOWN && event->x == panel->widget.cols - 2
	&& event->y == 1) {
	directory_history_list (panel);
	return MOU_NORMAL;
    }

    /* rest of the upper frame, the menu is invisible - call menu */
    if (event->type & GPM_DOWN && event->y == 1 && !menubar_visible) {
	*redir = 1;
	event->x += panel->widget.x;
	return (*(the_menubar->widget.mouse)) (event, the_menubar);
    }

    event->y -= 2;
    if ((event->type & (GPM_DOWN | GPM_DRAG))) {

	if (!dlg_widget_active (panel))
	    change_panel ();

	if (event->y <= 0) {
	    mark_if_marking (panel, event);
	    if (mouse_move_pages)
		prev_page (panel);
	    else
		move_up (panel);
	    return MOU_REPEAT;
	}

	if (!((panel->top_file + event->y <= panel->count)
	      && event->y <= lines)) {
	    mark_if_marking (panel, event);
	    if (mouse_move_pages)
		next_page (panel);
	    else
		move_down (panel);
	    return MOU_REPEAT;
	}
	my_index = panel->top_file + event->y - 1;
	if (panel->split) {
	    if (event->x > ((panel->widget.cols - 2) / 2))
		my_index += llines (panel);
	}

	if (my_index >= panel->count)
	    my_index = panel->count - 1;

	if (my_index != panel->selected) {
	    unselect_item (panel);
	    panel->selected = my_index;
	    select_item (panel);
	}

	/* This one is new */
	mark_if_marking (panel, event);

    } else if ((event->type & (GPM_UP | GPM_DOUBLE)) ==
	       (GPM_UP | GPM_DOUBLE)) {
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
    int redir = 0;

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
    do_sort (&panel->dir, panel->sort_type, panel->count-1, panel->reverse,
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
panel_set_sort_order (WPanel *panel, sortfn *sort_order)
{
    if (sort_order == 0)
	return;

    panel->sort_type = sort_order;

    /* The directory is already sorted, we have to load the unsorted stuff */
    if (sort_order == (sortfn *) unsorted){
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
    int width = (panel->widget.x)? panel->widget.cols : panel->widget.cols * (-1);

    r = select_charset (width, 0, source_codepage, FALSE);

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
