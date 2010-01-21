/* Hypertext file browser.
   Copyright (C) 1994, 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007 Free Software Foundation, Inc.
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/


/** \file help.c
 *  \brief Source: hypertext file browser
 *
 *  Implements the hypertext file viewer.
 *  The hypertext file is a file that may have one or more nodes.  Each
 *  node ends with a ^D character and starts with a bracket, then the
 *  name of the node and then a closing bracket. Right after the closing
 *  bracket a newline is placed. This newline is not to be displayed by
 *  the help viewer and must be skipped - its sole purpose is to faciliate
 *  the work of the people managing the help file template (xnc.hlp) .
 *
 *  Links in the hypertext file are specified like this: the text that
 *  will be highlighted should have a leading ^A, then it comes the
 *  text, then a ^B indicating that highlighting is done, then the name
 *  of the node you want to link to and then a ^C.
 *
 *  The file must contain a ^D at the beginning and at the end of the
 *  file or the program will not be able to detect the end of file.
 *
 *  Lazyness/widgeting attack: This file does use the dialog manager
 *  and uses mainly the dialog to achieve the help work.  there is only
 *  one specialized widget and it's only used to forward the mouse messages
 *  to the appropiate routine.
 */


#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/skin/skin.h"
#include "lib/tty/mouse.h"
#include "lib/tty/key.h"
#include "lib/strutil.h"

#include "dialog.h"		/* For Dlg_head */
#include "widget.h"		/* For Widget */
#include "wtools.h"		/* For common_dialog_repaint() */
#include "cmddef.h"
#include "keybind.h"
#include "help.h"
#include "main.h"

const global_keymap_t *help_map;

#define MAXLINKNAME 80
#define HISTORY_SIZE 20
#define HELP_WINDOW_WIDTH (HELP_TEXT_WIDTH + 4)

#define STRING_LINK_START	"\01"
#define STRING_LINK_POINTER	"\02"
#define STRING_LINK_END		"\03"
#define STRING_NODE_END		"\04"


static char *fdata = NULL;		/* Pointer to the loaded data file */
static int help_lines;			/* Lines in help viewer */
static int  history_ptr;		/* For the history queue */
static const char *main_node;		/* The main node */
static const char *last_shown = NULL;	/* Last byte shown in a screen */
static gboolean end_of_node = FALSE;	/* Flag: the last character of the node shown? */
static const char *currentpoint;
static const char *selected_item;

/* The widget variables */
static Dlg_head *whelp;

static struct {
    const char *page;		/* Pointer to the selected page */
    const char *link;		/* Pointer to the selected link */
} history [HISTORY_SIZE];

/* Link areas for the mouse */
typedef struct Link_Area {
    int x1, y1, x2, y2;
    const char *link_name;
    struct Link_Area *next;
} Link_Area;

static Link_Area *link_area = NULL;
static gboolean inside_link_area = FALSE;

static cb_ret_t help_callback (Dlg_head *h, Widget *sender,
				dlg_msg_t msg, int parm, void *data);

/* returns the position where text was found in the start buffer */
/* or 0 if not found */
static const char *
search_string (const char *start, const char *text)
{
    const char *result = NULL;
    char *local_text = g_strdup (text);
    char *d = local_text;
    const char *e = start;

    /* fmt sometimes replaces a space with a newline in the help file */
    /* Replace the newlines in the link name with spaces to correct the situation */
    while (*d != '\0') {
	if (*d == '\n')
	    *d = ' ';
	str_next_char (&d);
    }

    /* Do search */
    for (d = local_text; *e; e++){
	if (*d == *e)
	    d++;
	else
	    d = local_text;
	if (*d == '\0') {
	    result = e + 1;
	    break;
	}
    }

    g_free (local_text);
    return result;
}

/* Searches text in the buffer pointed by start.  Search ends */
/* if the CHAR_NODE_END is found in the text.  Returns 0 on failure */
static const char *
search_string_node (const char *start, const char *text)
{
    const char *d = text;
    const char *e = start;

    if (start != NULL)
	for (; *e && *e != CHAR_NODE_END; e++) {
	    if (*d == *e)
		d++;
	    else
		d = text;
	    if (*d == '\0')
		return e + 1;
	}

    return NULL;
}

/* Searches the_char in the buffer pointer by start and searches */
/* it can search forward (direction = 1) or backward (direction = -1) */
static const char *
search_char_node (const char *start, char the_char, int direction)
{
    const char *e;

    for (e = start; (*e != '\0') && (*e != CHAR_NODE_END); e += direction)
	if (*e == the_char)
	    return e;

    return NULL;
}

/* Returns the new current pointer when moved lines lines */
static const char *
move_forward2 (const char *c, int lines)
{
    const char *p;
    int line;

    currentpoint = c;
    for (line = 0, p = currentpoint; (*p != '\0') && (*p != CHAR_NODE_END);
		str_cnext_char (&p)) {
	if (line == lines)
	    return currentpoint = p;

	if (*p == '\n')
	    line++;
    }
    return currentpoint = c;
}

static const char *
move_backward2 (const char *c, int lines)
{
    const char *p;
    int line;

    currentpoint = c;
    for (line = 0, p = currentpoint; (*p != '\0') && ((int) (p - fdata) >= 0);
		str_cprev_char (&p)) {
	if (*p == CHAR_NODE_END) {
	    /* We reached the beginning of the node */
	    /* Skip the node headers */
	    while (*p != ']')
		str_cnext_char (&p);
	    return currentpoint = p + 2; /* Skip the newline following the start of the node */
	}

	if (*(p - 1) == '\n')
	    line++;
	if (line == lines)
	    return currentpoint = p;
    }
    return currentpoint = c;
}

static void
move_forward (int i)
{
    if (!end_of_node)
	currentpoint = move_forward2 (currentpoint, i);
}

static void
move_backward (int i)
{
    currentpoint = move_backward2 (currentpoint, ++i);
}

static void
move_to_top (void)
{
    while (((int) (currentpoint > fdata) > 0) && (*currentpoint != CHAR_NODE_END))
	currentpoint--;

    while (*currentpoint != ']')
	currentpoint++;
    currentpoint = currentpoint + 2; /* Skip the newline following the start of the node */
    selected_item = NULL;
}

static void
move_to_bottom (void)
{
    while ((*currentpoint != '\0') && (*currentpoint != CHAR_NODE_END))
	currentpoint++;
    currentpoint--;
    move_backward (help_lines - 1);
}

static const char *
help_follow_link (const char *start, const char *lc_selected_item)
{
    char link_name [MAXLINKNAME];
    const char *p;
    int i = 0;

    if (lc_selected_item == NULL)
	return start;

    for (p = lc_selected_item; *p && *p != CHAR_NODE_END && *p != CHAR_LINK_POINTER; p++)
	;
    if (*p == CHAR_LINK_POINTER){
	link_name [0] = '[';
	for (i = 1; *p != CHAR_LINK_END && *p && *p != CHAR_NODE_END && i < MAXLINKNAME-3; )
	    link_name [i++] = *++p;
	link_name [i - 1] = ']';
	link_name [i] = '\0';
	p = search_string (fdata, link_name);
	if (p != NULL) {
	    p += 1; /* Skip the newline following the start of the node */
	    return p;
	}
    }

    /* Create a replacement page with the error message */
    return _(" Help file format error\n");
}

static const char *
select_next_link (const char *current_link)
{
    const char *p;

    if (current_link == NULL)
	return NULL;

    p = search_string_node (current_link, STRING_LINK_END);
    if (p == NULL)
	return NULL;
    p = search_string_node (p, STRING_LINK_START);
    if (p == NULL)
	return NULL;
    return p - 1;
}

static const char *
select_prev_link (const char *current_link)
{
    return current_link == NULL
	    ? NULL
	    : search_char_node (current_link - 1, CHAR_LINK_START, -1);
}

static void
start_link_area (int x, int y, const char *link_name)
{
    Link_Area *new;

    if (inside_link_area)
	message (D_NORMAL, _("Warning"), _(" Internal bug: Double start of link area "));

    /* Allocate memory for a new link area */
    new = g_new (Link_Area, 1);
    new->next = link_area;
    link_area = new;

    /* Save the beginning coordinates of the link area */
    link_area->x1 = x;
    link_area->y1 = y;

    /* Save the name of the destination anchor */
    link_area->link_name = link_name;

    inside_link_area = TRUE;
}

static void
end_link_area (int x, int y)
{
    if (inside_link_area) {
	/* Save the end coordinates of the link area */
	link_area->x2 = x;
	link_area->y2 = y;
	inside_link_area = FALSE;
    }
}

static void
clear_link_areas (void)
{
    Link_Area *current;

    while (link_area != NULL) {
	current = link_area;
	link_area = current->next;
	g_free (current);
    }

    inside_link_area = FALSE;
}

static void
help_show (Dlg_head *h, const char *paint_start)
{
    const char *p, *n;
    int col, line, c, w;
    gboolean painting = TRUE;
    gboolean acs;			/* Flag: Alternate character set active? */
    gboolean repeat_paint;
    int active_col, active_line;	/* Active link position */
    static char buff[MB_LEN_MAX + 1];

    tty_setcolor (HELP_NORMAL_COLOR);
    do {
	line = col = active_col = active_line = 0;
	repeat_paint = FALSE;
	acs = FALSE;

	clear_link_areas ();
	if ((int) (selected_item - paint_start) < 0)
	    selected_item = NULL;
	
        p = paint_start;
        n = paint_start;
        while (n[0] != '\0' && n[0] != CHAR_NODE_END && line < help_lines) {
            p = n;
            n = str_cget_next_char (p);
            memcpy (buff, p, n - p);
            buff[n - p] = '\0';

	    c = (unsigned char) buff[0];
	    switch (c){
	    case CHAR_LINK_START:
		if (selected_item == NULL)
		    selected_item = p;
		if (p == selected_item){
		    tty_setcolor (HELP_SLINK_COLOR);

		    /* Store the coordinates of the link */
		    active_col = col + 2;
		    active_line = line + 2;
		}
		else
		    tty_setcolor (HELP_LINK_COLOR);
		start_link_area (col, line, p);
		break;
	    case CHAR_LINK_POINTER:
		painting = FALSE;
		end_link_area (col - 1, line);
		break;
	    case CHAR_LINK_END:
		painting = TRUE;
		tty_setcolor (HELP_NORMAL_COLOR);
		break;
	    case CHAR_ALTERNATE:
		acs = TRUE;
		break;
	    case CHAR_NORMAL:
		acs = FALSE;
		break;
	    case CHAR_VERSION:
		dlg_move (h, line+2, col+2);
		tty_print_string (VERSION);
		col += str_term_width1 (VERSION);
		break;
	    case CHAR_FONT_BOLD:
		tty_setcolor (HELP_BOLD_COLOR);
		break;
	    case CHAR_FONT_ITALIC:
		tty_setcolor (HELP_ITALIC_COLOR);
		break;
	    case CHAR_FONT_NORMAL:
		tty_setcolor (HELP_NORMAL_COLOR);
		break;
	    case '\n':
		line++;
		col = 0;
		break;
	    case '\t':
		col = (col / 8 + 1) * 8;
		break;
	    default:
		if (!painting)
		    continue;
                w = str_term_width1 (buff);
		if (col + w > HELP_WINDOW_WIDTH)
		    continue;
		
		dlg_move (h, line + 2, col + 2);
		if (!acs)
		    tty_print_string (buff);
		else if (c == ' ' || c == '.')
		    tty_print_char (c);
		else
#ifndef HAVE_SLANG
		    tty_print_char (acs_map [c]);
#else
		    SLsmg_draw_object (h->y + line + 2, h->x + col + 2, c);
#endif
		col += w;
		break;
	    }
	}

	last_shown = p;
	end_of_node = line < help_lines;
	tty_setcolor (HELP_NORMAL_COLOR);
	if ((int) (selected_item - last_shown) >= 0) {
	    if (link_area == NULL)
		selected_item = NULL;
	    else {
		selected_item = link_area->link_name;
		repeat_paint = TRUE;
	    }
	}
    } while (repeat_paint);

    /* Position the cursor over a nice link */
    if (active_col)
	dlg_move (h, active_line, active_col);
}

static int
help_event (Gpm_Event *event, void *vp)
{
    Widget *w = vp;
    Link_Area *current_area;

    if ((event->type & GPM_UP) == 0)
	return 0;

    /* The event is relative to the dialog window, adjust it: */
    event->x -= 2;
    event->y -= 2;

    if (event->buttons & GPM_B_RIGHT){
	currentpoint = history [history_ptr].page;
	selected_item = history [history_ptr].link;
	history_ptr--;
	if (history_ptr < 0)
	    history_ptr = HISTORY_SIZE-1;
	
	help_callback (w->parent, NULL, DLG_DRAW, 0, NULL);
	return 0;
    }

    /* Test whether the mouse click is inside one of the link areas */
    current_area = link_area;
    while (current_area != NULL)
    {
	/* Test one line link area */
	if (event->y == current_area->y1 && event->x >= current_area->x1 &&
	    event->y == current_area->y2 && event->x <= current_area->x2)
	    break;
	/* Test two line link area */
	if (current_area->y1 + 1 == current_area->y2){
	    /* The first line */
	    if (event->y == current_area->y1 && event->x >= current_area->x1)
		break;
	    /* The second line */
	    if (event->y == current_area->y2 && event->x <= current_area->x2)
		break;
	}
	/* Mouse will not work with link areas of more than two lines */

	current_area = current_area->next;
    }

    /* Test whether a link area was found */
    if (current_area != NULL) {
	/* The click was inside a link area -> follow the link */
	history_ptr = (history_ptr+1) % HISTORY_SIZE;
	history [history_ptr].page = currentpoint;
	history [history_ptr].link = current_area->link_name;
	currentpoint = help_follow_link (currentpoint, current_area->link_name);
	selected_item = NULL;
    } else if (event->y < 0)
	move_backward (help_lines - 1);
    else if (event->y >= help_lines)
	move_forward (help_lines - 1);
    else if (event->y < help_lines/2)
	move_backward (1);
    else
	move_forward (1);

    /* Show the new node */
    help_callback (w->parent, NULL, DLG_DRAW, 0, NULL);

    return 0;
}

/* show help */
static void
help_help (Dlg_head *h)
{
    const char *p;

    history_ptr = (history_ptr + 1) % HISTORY_SIZE;
    history [history_ptr].page = currentpoint;
    history [history_ptr].link = selected_item;

    p = search_string (fdata, "[How to use help]");
    if (p != NULL) {
	currentpoint = p + 1; /* Skip the newline following the start of the node */
	selected_item = NULL;
	help_callback (h, NULL, DLG_DRAW, 0, NULL);
    }
}

static void
help_index (Dlg_head *h)
{
    const char *new_item;

    new_item = search_string (fdata, "[Contents]");

    if (new_item == NULL)
	message (D_ERROR, MSG_ERROR, _(" Cannot find node %s in help file "),
		 "[Contents]");
    else {
	history_ptr = (history_ptr + 1) % HISTORY_SIZE;
	history[history_ptr].page = currentpoint;
	history[history_ptr].link = selected_item;

	currentpoint = new_item + 1; /* Skip the newline following the start of the node */
	selected_item = NULL;
	help_callback (h, NULL, DLG_DRAW, 0, NULL);
    }
}

static void
help_back (Dlg_head *h)
{
    currentpoint = history [history_ptr].page;
    selected_item = history [history_ptr].link;
    history_ptr--;
    if (history_ptr < 0)
	history_ptr = HISTORY_SIZE - 1;

    help_callback (h, NULL, DLG_DRAW, 0, NULL);	/* FIXME: unneeded? */
}

static void
help_cmk_move_backward(void *vp, int lines)
{
    (void) &vp;
    move_backward (lines);
}

static void
help_cmk_move_forward(void *vp, int lines)
{
    (void) &vp;
    move_forward (lines);
}

static void
help_cmk_moveto_top(void *vp, int lines)
{
    (void) &vp;
    (void) &lines;
    move_to_top ();
}

static void
help_cmk_moveto_bottom(void *vp, int lines)
{
    (void) &vp;
    (void) &lines;
    move_to_bottom ();
}

static void
help_next_link (gboolean move_down)
 {
    const char *new_item;

    new_item = select_next_link (selected_item);
    if (new_item != NULL) {
	selected_item = new_item;
	if ((int) (selected_item - last_shown) >= 0) {
	    if (move_down)
		move_forward (1);
	    else
		selected_item = NULL;
	}
    } else if (move_down)
	move_forward (1);
    else
	selected_item = NULL;
}

static void
help_prev_link (gboolean move_up)
{
    const char *new_item;

    new_item = select_prev_link (selected_item);
    selected_item = new_item;
    if ((selected_item == NULL) || (selected_item < currentpoint)) {
	if (move_up)
	    move_backward (1);
	else if (link_area != NULL)
	    selected_item = link_area->link_name;
	else
	    selected_item = NULL;
    }
}

static void
help_next_node (void)
{
    const char *new_item;

    new_item = currentpoint;
    while ((*new_item != '\0') && (*new_item != CHAR_NODE_END))
	new_item++;

    if (*++new_item == '[')
	while (*++new_item != '\0')
	    if ((*new_item == ']') && (*++new_item != '\0')
		    && (*++new_item != '\0')) {
		currentpoint = new_item;
		selected_item = NULL;
		break;
	    }
}

static void
help_prev_node (void)
{
    const char *new_item;

    new_item = currentpoint;
    while (((int) (new_item - fdata) > 1) && (*new_item != CHAR_NODE_END))
	new_item--;
    new_item--;
    while (((int) (new_item - fdata) > 0) && (*new_item != CHAR_NODE_END))
	new_item--;
    while (*new_item != ']')
	new_item++;
    currentpoint = new_item + 2;
    selected_item = NULL;
}

static void
help_select_link (void)
{
    /* follow link */
    if (selected_item == NULL) {
#ifdef WE_WANT_TO_GO_BACKWARD_ON_KEY_RIGHT
	/* Is there any reason why the right key would take us
	 * backward if there are no links selected?, I agree
	 * with Torben than doing nothing in this case is better
	 */
	/* If there are no links, go backward in history */
	history_ptr--;
	if (history_ptr < 0)
	    history_ptr = HISTORY_SIZE-1;

	currentpoint = history [history_ptr].page;
	selected_item = history [history_ptr].link;
#endif
    } else {
	history_ptr = (history_ptr + 1) % HISTORY_SIZE;
	history [history_ptr].page = currentpoint;
	history [history_ptr].link = selected_item;
	currentpoint = help_follow_link (currentpoint, selected_item);
    }

    selected_item = NULL;
}

static cb_ret_t
help_execute_cmd (unsigned long command)
{
    cb_ret_t ret = MSG_HANDLED;

    switch (command) {
    case CK_HelpHelp:
	help_help (whelp);
	break;
    case CK_HelpIndex:
	help_index (whelp);
	break;
    case CK_HelpBack:
	help_back (whelp);
	break;
    case CK_HelpMoveUp:
	help_prev_link (TRUE);
	break;
    case CK_HelpMoveDown:
	help_next_link (TRUE);
	break;
    case CK_HelpSelectLink:
	help_select_link ();
	break;
    case CK_HelpNextLink:
	help_next_link (FALSE);
	break;
    case CK_HelpPrevLink:
	help_prev_link (FALSE);
	break;
    case CK_HelpNextNode:
	help_next_node ();
	break;
    case CK_HelpPrevNode:
	help_prev_node ();
	break;
    case CK_HelpQuit:
	dlg_stop (whelp);
	break;
    default:
	ret = MSG_NOT_HANDLED;
    }

    return ret;
}

static cb_ret_t
help_handle_key (Dlg_head *h, int c)
{
    if (c != KEY_UP && c != KEY_DOWN &&
	check_movement_keys (c, help_lines, NULL,
			     help_cmk_move_backward,
			     help_cmk_move_forward,
			     help_cmk_moveto_top,
			     help_cmk_moveto_bottom) == MSG_HANDLED) {
	/* Nothing */;
    } else {
	unsigned long command;

	command = lookup_keymap_command (help_map, c);
	if ((command == CK_Ignore_Key)
	    || (help_execute_cmd (command) == MSG_NOT_HANDLED))
		return MSG_NOT_HANDLED;
    }

    help_callback (h, NULL, DLG_DRAW, 0, NULL);
    return MSG_HANDLED;
}

static cb_ret_t
help_callback (Dlg_head *h, Widget *sender,
		dlg_msg_t msg, int parm, void *data)
{
    WButtonBar *bb;

    switch (msg) {
    case DLG_RESIZE:
	help_lines = min (LINES - 4, max (2 * LINES / 3, 18));
	dlg_set_size (h, help_lines + 4, HELP_WINDOW_WIDTH + 4);
	bb = find_buttonbar (h);
	widget_set_size (&bb->widget, LINES - 1, 0, 1, COLS);
	return MSG_HANDLED;

    case DLG_DRAW:
	common_dialog_repaint (h);
	help_show (h, currentpoint);
	return MSG_HANDLED;

    case DLG_KEY:
	return help_handle_key (h, parm);

    case DLG_ACTION:
	/* command from buttonbar */
	return help_execute_cmd (parm);

    default:
	return default_dlg_callback (h, sender, msg, parm, data);
    }
}

static void
interactive_display_finish (void)
{
    clear_link_areas ();
}

/* translate help file into terminal encoding */
static void
translate_file (char *filedata)
{
    GIConv conv;
    GString *translated_data;

    translated_data = g_string_new ("");

    conv = str_crt_conv_from ("UTF-8");

    if (conv == INVALID_CONV)
	g_string_free (translated_data, TRUE);
    else {
	g_free (fdata);

	if (str_convert (conv, filedata, translated_data) != ESTR_FAILURE) {
	    fdata = translated_data->str;
	    g_string_free (translated_data, FALSE);
	} else {
	    fdata = NULL;
	    g_string_free (translated_data, TRUE);
	}
	str_close_conv (conv);
    }
}

static cb_ret_t
md_callback (Widget *w, widget_msg_t msg, int parm)
{
    switch (msg) {
    case WIDGET_RESIZED:
	w->lines = help_lines;
	return MSG_HANDLED;

    default:
	return default_proc (msg, parm);
    }
}

static Widget *
mousedispatch_new (int y, int x, int yl, int xl)
{
    Widget *w = g_new (Widget, 1);
    init_widget (w, y, x, yl, xl, md_callback, help_event);
    return w;
}

void
interactive_display (const char *filename, const char *node)
{
    WButtonBar *help_bar;
    Widget *md;
    char *hlpfile = NULL;
    char *filedata;

    if (filename != NULL)
	filedata = load_file (filename);
    else
	filedata = load_mc_home_file (mc_home, mc_home_alt, "mc.hlp", &hlpfile);

    if (filedata == NULL)
	message (D_ERROR, MSG_ERROR, _(" Cannot open file %s \n %s "), filename ? filename : hlpfile,
		 unix_error_string (errno));

    g_free (hlpfile);

    if (filedata == NULL)
	return;

    translate_file (filedata);

    g_free (filedata);

    if (fdata == NULL)
	return;

    if ((node == NULL) || (*node == '\0'))
	node = "[main]";

    main_node = search_string (fdata, node);

    if (main_node == NULL) {
	message (D_ERROR, MSG_ERROR, _(" Cannot find node %s in help file "),
		 node);

	/* Fallback to [main], return if it also cannot be found */
	main_node = search_string (fdata, "[main]");
	if (main_node == NULL) {
	    interactive_display_finish ();
	    return;
	}
    }

    help_lines = min (LINES - 4, max (2 * LINES / 3, 18));

    whelp =
	create_dlg (0, 0, help_lines + 4, HELP_WINDOW_WIDTH + 4,
		    dialog_colors, help_callback, "[Help]", _("Help"),
		    DLG_TRYUP | DLG_CENTER | DLG_WANT_TAB);

    selected_item = search_string_node (main_node, STRING_LINK_START) - 1;
    currentpoint = main_node + 1; /* Skip the newline following the start of the node */

    for (history_ptr = HISTORY_SIZE; history_ptr;) {
	history_ptr--;
	history[history_ptr].page = currentpoint;
	history[history_ptr].link = selected_item;
    }

    help_bar = buttonbar_new (TRUE);
    help_bar->widget.y -= whelp->y;
    help_bar->widget.x -= whelp->x;

    md = mousedispatch_new (1, 1, help_lines, HELP_WINDOW_WIDTH - 2);

    add_widget (whelp, md);
    add_widget (whelp, help_bar);

    buttonbar_set_label (help_bar, 1, Q_("ButtonBar|Help"), help_map, NULL);
    buttonbar_set_label (help_bar, 2, Q_("ButtonBar|Index"), help_map, NULL);
    buttonbar_set_label (help_bar, 3, Q_("ButtonBar|Prev"), help_map, NULL);
    buttonbar_set_label (help_bar, 4, "", help_map, NULL);
    buttonbar_set_label (help_bar, 5, "", help_map, NULL);
    buttonbar_set_label (help_bar, 6, "", help_map, NULL);
    buttonbar_set_label (help_bar, 7, "", help_map, NULL);
    buttonbar_set_label (help_bar, 8, "", help_map, NULL);
    buttonbar_set_label (help_bar, 9, "", help_map, NULL);
    buttonbar_set_label (help_bar, 10, Q_("ButtonBar|Quit"), help_map, NULL);

    run_dlg (whelp);
    interactive_display_finish ();
    destroy_dlg (whelp);
}
