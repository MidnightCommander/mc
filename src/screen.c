/* Panel managing.
   Copyright (C) 1994, 1995 Miguel de Icaza.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "global.h"
#include "tty.h"
#include "dir.h"
#include "panel.h"
#include "color.h"
#include "tree.h"
#include "win.h"
#include "ext.h"		/* regexp_command */
#include "mouse.h"		/* For Gpm_Event */
#include "layout.h"		/* Most layout variables are here */
#include "wtools.h"		/* for message (...) */
#include "cmd.h"
#include "key.h"		/* XCTRL and ALT macros  */
#include "setup.h"		/* For loading/saving panel options */
#include "user.h"
#include "profile.h"
#include "execute.h"
#include "widget.h"
#include "menu.h"		/* menubar_visible */
#define WANT_WIDGETS
#include "main.h"		/* the_menubar */
#include "unixcompat.h"

#define ELEMENTS(arr) ( sizeof(arr) / sizeof((arr)[0]) )

#define J_LEFT 		1
#define J_RIGHT		2
#define J_CENTER	3

#define IS_FIT(x)	((x) & 0x0004)
#define MAKE_FIT(x)	((x) | 0x0004)
#define HIDE_FIT(x)	((x) & 0x0003)

#define J_LEFT_FIT	5
#define J_RIGHT_FIT	6
#define J_CENTER_FIT	7

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
    int    just_mode;
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

static cb_ret_t panel_callback (WPanel *p, widget_msg_t msg, int parm);
static int panel_event (Gpm_Event *event, WPanel *panel);
static void paint_frame (WPanel *panel);
static const char *panel_format (WPanel *panel);
static const char *mini_status_format (WPanel *panel);

/* This macro extracts the number of available lines in a panel */
#define llines(p) (p->widget.lines-3 - (show_mini_info ? 2 : 0))

static void
set_colors (WPanel *panel)
{
    standend ();
    attrset (NORMAL_COLOR);
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
                attrset (MARKED_SELECTED_COLOR);
            else
                attrset (MARKED_COLOR);
        } else
            attrset (color);

	addch (dest[i]);
    }
}

/* String representations of various file attributes */
/* name */
static const char *
string_file_name (file_entry *fe, int len)
{
    static char buffer [BUF_SMALL];
    size_t i;

    for (i = 0; i < sizeof(buffer) - 1; i++) {
	char c;

	c = fe->fname[i];

	if (!c)
	    break;

	if (!is_printable(c))
	    c = '?';

	buffer[i] = c;
    }

    buffer[i] = 0;
    return buffer;
}

static inline int ilog10(dev_t n)
{
    int digits = 0;
    do {
	digits++, n /= 10;
    } while (n != 0);
    return digits;
}

static void format_device_number (char *buf, size_t bufsize, dev_t dev)
{
    dev_t major_dev = major(dev);
    dev_t minor_dev = minor(dev);
    int major_digits = ilog10(major_dev);
    int minor_digits = ilog10(minor_dev);

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
    if (!strcmp (fe->fname, "..")) {
       return "";
    }
    return file_date (fe->st.st_mtime);
}

/* atime */
static const char *
string_file_atime (file_entry *fe, int len)
{
    if (!strcmp (fe->fname, "..")) {
       return "";
    }
    return file_date (fe->st.st_atime);
}

/* ctime */
static const char *
string_file_ctime (file_entry *fe, int len)
{
    if (!strcmp (fe->fname, "..")) {
       return "";
    }
    return file_date (fe->st.st_ctime);
}

/* perm */
static const char *
string_file_permission (file_entry *fe, int len)
{
    return string_perm (fe->st.st_mode);
}

/* mode */
static const char *
string_file_perm_octal (file_entry *fe, int len)
{
    static char buffer [10];

    g_snprintf (buffer, sizeof (buffer), "0%06lo", (unsigned long) fe->st.st_mode);
    return buffer;
}

/* nlink */
static const char *
string_file_nlinks (file_entry *fe, int len)
{
    static char buffer[BUF_TINY];

    g_snprintf (buffer, sizeof (buffer), "%16d", (int) fe->st.st_nlink);
    return buffer;
}

/* inode */
static const char *
string_inode (file_entry *fe, int len)
{
    static char buffer [10];

    g_snprintf (buffer, sizeof (buffer), "%lu",
		(unsigned long) fe->st.st_ino);
    return buffer;
}

/* nuid */
static const char *
string_file_nuid (file_entry *fe, int len)
{
    static char buffer [10];

    g_snprintf (buffer, sizeof (buffer), "%lu",
		(unsigned long) fe->st.st_uid);
    return buffer;
}

/* ngid */
static const char *
string_file_ngid (file_entry *fe, int len)
{
    static char buffer [10];

    g_snprintf (buffer, sizeof (buffer), "%lu",
		(unsigned long) fe->st.st_gid);
    return buffer;
}

/* owner */
static const char *
string_file_owner (file_entry *fe, int len)
{
    return get_owner (fe->st.st_uid);
}

/* group */
static const char *
string_file_group (file_entry *fe, int len)
{
    return get_group (fe->st.st_gid);
}

/* mark */
static const char *
string_marked (file_entry *fe, int len)
{
    return fe->f.marked ? "*" : " ";
}

/* space */
static const char *
string_space (file_entry *fe, int len)
{
    return " ";
}

/* dot */
static const char *
string_dot (file_entry *fe, int len)
{
    return ".";
}

#define GT 1

static struct {
    const char *id;
    int  min_size;
    int  expands;
    int  default_just;
    const char *title;
    int  use_in_gui;
    const char *(*string_fn)(file_entry *, int);
    sortfn *sort_routine;
} formats [] = {
{ "name",  12, 1, J_LEFT_FIT,	N_("Name"),	1, string_file_name,	   (sortfn *) sort_name },
{ "size",  7,  0, J_RIGHT,	N_("Size"),	1, string_file_size,	   (sortfn *) sort_size },
{ "bsize", 7,  0, J_RIGHT,	N_("Size"),	1, string_file_size_brief, (sortfn *) sort_size },
{ "type",  GT, 0, J_LEFT,	"",		2, string_file_type,	   (sortfn *) sort_type },
{ "mtime", 12, 0, J_RIGHT,	N_("MTime"),	1, string_file_mtime,	   (sortfn *) sort_time },
{ "atime", 12, 0, J_RIGHT,	N_("ATime"),	1, string_file_atime,	   (sortfn *) sort_atime },
{ "ctime", 12, 0, J_RIGHT,	N_("CTime"),	1, string_file_ctime,	   (sortfn *) sort_ctime },
{ "perm",  10, 0, J_LEFT,	N_("Permission"),1,string_file_permission, NULL },
{ "mode",  6,  0, J_RIGHT,	N_("Perm"),	1, string_file_perm_octal, NULL },
{ "nlink", 2,  0, J_RIGHT,	N_("Nl"),	1, string_file_nlinks,	   (sortfn *) sort_links },
{ "inode", 5,  0, J_RIGHT,	N_("Inode"),	1, string_inode,	   (sortfn *) sort_inode },
{ "nuid",  5,  0, J_RIGHT,	N_("UID"),	1, string_file_nuid,	   (sortfn *) sort_nuid },
{ "ngid",  5,  0, J_RIGHT,	N_("GID"),	1, string_file_ngid,	   (sortfn *) sort_ngid },
{ "owner", 8,  0, J_LEFT_FIT,	N_("Owner"),	1, string_file_owner,	   (sortfn *) sort_owner },
{ "group", 8,  0, J_LEFT_FIT,	N_("Group"),	1, string_file_group,	   (sortfn *) sort_group },
{ "mark",  1,  0, J_RIGHT,	" ",		1, string_marked,	   NULL },
{ "|",     1,  0, J_RIGHT,	" ",		0, NULL,		   NULL },
{ "space", 1,  0, J_RIGHT,	" ",		0, string_space,	   NULL },
{ "dot",   1,  0, J_RIGHT,	" ",		0, string_dot,		   NULL },
};

static char *
to_buffer (char *dest, int just_mode, int len, const char *txt)
{
    int txtlen = strlen (txt);
    int still, over;

    /* Fill buffer with spaces */
    memset (dest, ' ', len);

    still = (over=(txtlen > len)) ? (txtlen - len) : (len - txtlen);

    switch (HIDE_FIT(just_mode)){
        case J_LEFT:
	    still = 0;
	    break;
	case J_CENTER:
	    still /= 2;
	    break;
	case J_RIGHT:
	default:
	    break;
    }

    if (over){
	if (IS_FIT(just_mode))
	    strcpy (dest, name_trunc(txt, len));
	else
	    strncpy (dest, txt+still, len);
    } else
	strncpy (dest+still, txt, txtlen);

    dest[len] = '\0';

    return (dest + len);
}

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
    char     *old_pos;
    char     *cdest = dest;
    format_e *format, *home;
    file_entry *fe;

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
	    int len;

	    if (empty_line)
		txt = " ";
	    else
		txt = (*format->string_fn)(fe, format->field_len);

	    old_pos = cdest;

	    len = format->field_len;
	    if (len + length > width)
		len = width - length;
	    if (len + (cdest - dest) > limit)
		len = limit - (cdest - dest);
	    if (len <= 0)
		break;
	    cdest = to_buffer (cdest, format->just_mode, len, txt);
	    length += len;

            attrset (color);

            if (permission_mode && !strcmp(format->id, "perm"))
                add_permission_string (old_pos, format->field_len, fe, attr, color, 0);
            else if (permission_mode && !strcmp(format->id, "mode"))
                add_permission_string (old_pos, format->field_len, fe, attr, color, 1);
            else
		addstr (old_pos);

	} else {
            if (attr == SELECTED || attr == MARKED_SELECTED)
                attrset (SELECTED_COLOR);
            else
                attrset (NORMAL_COLOR);
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
	    attrset (NORMAL_COLOR);
	    one_vline ();
	}
    }
}

static void
display_mini_info (WPanel *panel)
{
    if (!show_mini_info)
	return;

    widget_move (&panel->widget, llines (panel)+3, 1);

    if (panel->searching){
	attrset (INPUT_COLOR);
	printw ("/%-*s", panel->widget.cols-3, panel->search_buffer);
	attrset (NORMAL_COLOR);
	return;
    }

    /* Status displays total marked size */
    if (panel->marked){
	char buffer [BUF_SMALL];
	const char *p = "  %-*s";
	int  cols = panel->widget.cols-2;

	attrset (MARKED_COLOR);
	printw  ("%*s", cols, " ");
	widget_move (&panel->widget, llines (panel)+3, 1);
	/* FIXME: use ngettext() here when gettext-0.10.35 becomes history */
	g_snprintf (buffer, sizeof (buffer), (panel->marked == 1) ?
		 _("%s bytes in %d file") : _("%s bytes in %d files"),
		 size_trunc_sep (panel->total), panel->marked);
	if ((int) strlen (buffer) > cols-2){
	    buffer [cols] = 0;
	    p += 2;
	} else
	    cols -= 2;
	printw ((char *) p, cols, buffer);
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
	    printw ("-> %-*s", panel->widget.cols - 5,
		     name_trunc (link_target, panel->widget.cols - 5));
	} else
	    printw ("%-*s", panel->widget.cols - 2, _("<readlink failed>"));
	return;
    }

    /* Default behavior */
    repaint_file (panel, panel->selected, 0, STATUS, 1);
    return;
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
mini_info_separator (WPanel *panel)
{
    if (!show_mini_info)
	return;

    standend ();
    widget_move (&panel->widget, llines (panel) + 2, 1);
#ifdef HAVE_SLANG
    attrset (NORMAL_COLOR);
    hline (ACS_HLINE, panel->widget.cols - 2);
#else
    hline ((slow_terminal ? '-' : ACS_HLINE) | NORMAL_COLOR,
	   panel->widget.cols - 2);
#endif				/* !HAVE_SLANG */
}

static void
show_dir (WPanel *panel)
{
    char *tmp;

    set_colors (panel);
    draw_double_box (panel->widget.parent,
		     panel->widget.y, panel->widget.x,
		     panel->widget.lines, panel->widget.cols);

#ifdef HAVE_SLANG
    if (show_mini_info) {
	SLsmg_draw_object (panel->widget.y + llines (panel) + 2,
			   panel->widget.x, SLSMG_LTEE_CHAR);
	SLsmg_draw_object (panel->widget.y + llines (panel) + 2,
			   panel->widget.x + panel->widget.cols - 1,
			   SLSMG_RTEE_CHAR);
    }
#endif				/* HAVE_SLANG */

    if (panel->active)
	attrset (REVERSE_COLOR);

    widget_move (&panel->widget, 0, 3);

    tmp = g_malloc (panel->widget.cols + 1);
    tmp[panel->widget.cols] = '\0';

    trim (strip_home_and_password (panel->cwd), tmp,
	 min (max (panel->widget.cols - 7, 0), panel->widget.cols) );
    addstr (tmp);
    g_free (tmp);
    widget_move (&panel->widget, 0, 1);
    addstr ("<");
    widget_move (&panel->widget, 0, panel->widget.cols - 2);
    addstr (">");
    widget_move (&panel->widget, 0, panel->widget.cols - 3);
    addstr ("v");

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

/*
 * Repaint everything that can change on the panel - title, entries and
 * mini status.  The rest of the frame and the mini status separator are
 * not repainted.
 */
static void
panel_update_contents (WPanel *panel)
{
    show_dir (panel);
    paint_dir (panel);
    display_mini_info (panel);
    panel->dirty = 0;
}

/* Repaint everything, including frame and separator */
static void
paint_panel (WPanel *panel)
{
    paint_frame (panel);
    panel_update_contents (panel);
    mini_info_separator (panel);
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
	panel_update_contents (current_panel);

    if ((get_other_type () == view_listing) && other_panel->dirty)
	panel_update_contents (other_panel);
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
    display_mini_info (panel);
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
    init_widget (&panel->widget, 0, 0, 0, 0, (callback_fn)
		 panel_callback, (mouse_h) panel_event);

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
    if (!profile_has_section (section, profile_name)) {
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
		     panel->reverse, panel->case_sensitive, panel->filter);
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
		       panel->filter);

    panel->dirty = 1;
    if (panel->selected >= panel->count)
	do_select (panel, panel->count - 1);

    recalculate_panel_summary (panel);
}

static void
paint_frame (WPanel *panel)
{
    int  header_len;
    int  spaces, extra;
    int  side, width;

    const char *txt;
    if (!panel->split)
	adjust_top_file (panel);

    widget_erase (&panel->widget);
    show_dir (panel);

    widget_move (&panel->widget, 1, 1);

    for (side = 0; side <= panel->split; side++){
	format_e *format;

	if (side){
	    attrset (NORMAL_COLOR);
	    one_vline ();
	    width = panel->widget.cols - panel->widget.cols/2 - 1;
	} else if (panel->split)
	    width = panel->widget.cols/2 - 3;
	else
	    width = panel->widget.cols - 2;

	for (format = panel->format; format; format = format->next){
            if (format->string_fn){
                txt = format->title;

		header_len = strlen (txt);
		if (header_len > format->field_len)
		    header_len = format->field_len;

                attrset (MARKED_COLOR);
                spaces = (format->field_len - header_len) / 2;
                extra  = (format->field_len - header_len) % 2;
		printw ("%*s%.*s%*s", spaces, "",
			 header_len, txt, spaces+extra, "");
		width -= 2 * spaces + extra + header_len;
	    } else {
		attrset (NORMAL_COLOR);
		one_vline ();
		width --;
		continue;
	    }
	}

	if (width > 0)
	    printw ("%*s", width, "");
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
    int  justify = 0;                  	/* Which mode. */
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
    int  total_cols;
    const char *expand_list [MAX_EXPAND];    /* Expand at most 4 fields. */
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

    /* Clean expand list */
    for (i = 0; i < MAX_EXPAND; i++)
        expand_list [i] = '\0';


    /* Look for the expandable fields and set field_len based on the requested field len */
    for (darr = home; darr && expand_top < MAX_EXPAND; darr = darr->next){
	darr->field_len = darr->requested_field_len;
	if (darr->expand)
	    expand_list [expand_top++] = darr->id;
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
      message( 1, _("Warning" ), _( "User supplied format looks invalid, reverting to default." ) );
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
    if (panel->split) {
	move_selection (panel, -llines (panel));
	return MSG_HANDLED;
    } else
	return maybe_cd (c_code, 0);
}

static int
move_right (WPanel *panel, int c_code)
{
    if (panel->split) {
	move_selection (panel, llines (panel));
	return MSG_HANDLED;
    } else
	return maybe_cd (c_code, 1);
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
    int i;
    int wrapped = 0;
    int found;

    l = strlen (panel->search_buffer);
    if (c_code == KEY_BACKSPACE) {
	if (l)
	    panel->search_buffer[--l] = '\0';
    } else {
	if (c_code && l < sizeof (panel->search_buffer)) {
	    panel->search_buffer[l] = c_code;
	    panel->search_buffer[l + 1] = 0;
	    l++;
	}
    }

    found = 0;
    for (i = panel->selected; !wrapped || i != panel->selected; i++) {
	if (i >= panel->count) {
	    i = 0;
	    if (wrapped)
		break;
	    wrapped = 1;
	}
	if (panel->
	    case_sensitive
	    ? (strncmp (panel->dir.list[i].fname, panel->search_buffer, l)
	       == 0) : (g_strncasecmp (panel->dir.list[i].fname,
				       panel->search_buffer, l) == 0)) {
	    unselect_item (panel);
	    panel->selected = i;
	    select_item (panel);
	    found = 1;
	    break;
	}
    }
    if (!found)
	panel->search_buffer[--l] = 0;

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
	panel->search_buffer [0] = 0;
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
	    message (1, MSG_ERROR, _("Cannot change directory"));
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
	     _(" Do you really want to execute? "), 0, 2, _("&Yes"),
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

/*
 * Make the current directory of the current panel also the current
 * directory of the other panel.  Put the other panel to the listing
 * mode if needed.  If the current panel is panelized, the other panel
 * doesn't become panelized.
 */
static void
chdir_other_panel (WPanel *panel)
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
static void cmd_view_simple(WPanel *wp) { view_simple_cmd(); }
static void cmd_edit_new(WPanel *wp) { edit_cmd_new(); }
static void cmd_copy_local(WPanel *wp) { copy_cmd_local(); }
static void cmd_rename_local(WPanel *wp) { ren_cmd_local(); }
static void cmd_delete_local(WPanel *wp) { delete_cmd_local(); }
static void cmd_select(WPanel *wp) { select_cmd(); }
static void cmd_unselect(WPanel *wp) { unselect_cmd(); }
static void cmd_reverse_selection(WPanel *wp) { reverse_selection_cmd(); }

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
    { XCTRL('t'), mark_file },
    { ALT('o'),   chdir_other_panel },
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
panel_callback (WPanel *panel, widget_msg_t msg, int parm)
{
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
	    message (1, MSG_ERROR, _(" Cannot chdir to \"%s\" \n %s "),
		     cwd, unix_error_string (errno));
	    g_free(cwd);
	} else
	    subshell_chdir (panel->cwd);

	update_xterm_title_path ();
	select_item (panel);
	show_dir (panel);
	paint_dir (panel);

	define_label (h, 1, _("Help"), help_cmd);
	define_label (h, 2, _("Menu"), user_file_menu_cmd);
	define_label (h, 3, _("View"), view_cmd);
	define_label (h, 4, _("Edit"), edit_cmd);
	define_label (h, 5, _("Copy"), copy_cmd);
	define_label (h, 6, _("RenMov"), ren_cmd);
	define_label (h, 7, _("Mkdir"), mkdir_cmd);
	define_label (h, 8, _("Delete"), delete_cmd);
	redraw_labels (h);
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
panel_event (Gpm_Event *event, WPanel *panel)
{
    int ret;
    int redir = 0;

    ret = do_panel_event (event, panel, &redir);
    if (!redir)
	panel_update_contents (panel);

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
    do_sort (&panel->dir, panel->sort_type, panel->count-1, panel->reverse, panel->case_sensitive);
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
