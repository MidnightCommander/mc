/* Directory panel listing format editor -- for the Midnight Commander
   Copyright (C) 1994, 1995 The Free Software Foundation

   Written by: 1994 Radek Doulik
   	       1995 Janne Kukonlehto

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

#include <config.h>

#ifdef LISTMODE_EDITOR

#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "global.h"
#include "tty.h"
#include "win.h"
#include "color.h"
#include "dialog.h"
#include "widget.h"
#include "wtools.h"

/* Needed for the extern declarations of integer parameters */
#include "dir.h"
#include "panel.h"		/* Needed for the externs */
#include "file.h"
#include "main.h"
#include "listmode.h"

#define UX		5
#define UY		2

#define BX		5
#define BY		18

#define B_ADD		B_USER
#define B_REMOVE        (B_USER + 1)

static WListbox *l_listmode;

static WLabel *pname;

static char *listmode_section = "[Listing format edit]";

static char *s_genwidth[2] = { "Half width", "Full width" };
static WRadio *radio_genwidth;
static char *s_columns[2] = { "One column", "Two columns" };
static WRadio *radio_columns;
static char *s_justify[3] =
    { "Left justified", "Default justification", "Right justified" };
static WRadio *radio_justify;
static char *s_itemwidth[3] =
    { "Free width", "Fixed width", "Growable width" };
static WRadio *radio_itemwidth;

struct listmode_button {
    int ret_cmd, flags, y, x;
    char *text;
    bcback callback;
};

#define B_PLUS B_USER
#define B_MINUS (B_USER+1)

struct listmode_label {
    int y, x;
    char *text;
};

static char *
select_new_item (void)
{
    /* NOTE: The following array of possible items must match the
       formats array in screen.c. Better approach might be to make the
       formats array global */
    char *possible_items[] =
	{ "name", "size", "type", "mtime", "perm", "mode", "|", "nlink",
	"owner", "group", "atime", "ctime", "space", "mark",
	"inode", NULL
    };

    int i;
    Listbox *mylistbox;

    mylistbox =
	create_listbox_window (12, 20, " Add listing format item ",
			       listmode_section);
    for (i = 0; possible_items[i]; i++) {
	listbox_add_item (mylistbox->list, 0, 0, possible_items[i], NULL);
    }

    i = run_listbox (mylistbox);
    if (i >= 0)
	return possible_items[i];
    else
	return NULL;
}

static int
bplus_cback (int action)
{
    return 0;
}

static int
bminus_cback (int action)
{
    return 0;
}

static int
badd_cback (int action)
{
    char *s = select_new_item ();
    if (s) {
	listbox_add_item (l_listmode, 0, 0, s, NULL);
    }
    return 0;
}

static int
bremove_cback (int action)
{
    listbox_remove_current (l_listmode, 0);
    return 0;
}

static Dlg_head *
init_listmode (char *oldlistformat)
{
    int i;
    char *s;
    int format_width = 0;
    int format_columns = 0;
    Dlg_head *listmode_dlg;

    static struct listmode_label listmode_labels[] = {
	{UY + 13, UX + 22, "Item width:"}
    };

    static struct listmode_button listmode_but[] = {
	{B_CANCEL, NORMAL_BUTTON, BY, BX + 53, "&Cancel", NULL},
	{B_ADD, NORMAL_BUTTON, BY, BX + 22, "&Add item", badd_cback},
	{B_REMOVE, NORMAL_BUTTON, BY, BX + 10, "&Remove", bremove_cback},
	{B_ENTER, DEFPUSH_BUTTON, BY, BX, "&OK", NULL},
	{B_PLUS, NARROW_BUTTON, UY + 13, UX + 37, "&+", bplus_cback},
	{B_MINUS, NARROW_BUTTON, UY + 13, UX + 34, "&-", bminus_cback},
    };

    do_refresh ();

    listmode_dlg =
	create_dlg (0, 0, 22, 74, dialog_colors, NULL, listmode_section,
		    "Listing format edit", DLG_CENTER | DLG_REVERSE);

    add_widget (listmode_dlg,
		groupbox_new (UX, UY, 63, 4, "General options"));
    add_widget (listmode_dlg, groupbox_new (UX, UY + 4, 18, 11, "Items"));
    add_widget (listmode_dlg,
		groupbox_new (UX + 20, UY + 4, 43, 11, "Item options"));

    for (i = 0;
	 i < sizeof (listmode_but) / sizeof (struct listmode_button); i++)
	add_widget (listmode_dlg,
		    button_new (listmode_but[i].y, listmode_but[i].x,
				listmode_but[i].ret_cmd,
				listmode_but[i].flags,
				listmode_but[i].text,
				listmode_but[i].callback));

    /* We add the labels. */
    for (i = 0;
	 i < sizeof (listmode_labels) / sizeof (struct listmode_label);
	 i++) {
	pname =
	    label_new (listmode_labels[i].y, listmode_labels[i].x,
		       listmode_labels[i].text);
	add_widget (listmode_dlg, pname);
    }

    radio_itemwidth = radio_new (UY + 9, UX + 22, 3, s_itemwidth);
    add_widget (listmode_dlg, radio_itemwidth);
    radio_itemwidth = 0;
    radio_justify = radio_new (UY + 5, UX + 22, 3, s_justify);
    add_widget (listmode_dlg, radio_justify);
    radio_justify->sel = 1;

    /* get new listbox */
    l_listmode = listbox_new (UY + 5, UX + 1, 16, 9, NULL);

    if (strncmp (oldlistformat, "full ", 5) == 0) {
	format_width = 1;
	oldlistformat += 5;
    }
    if (strncmp (oldlistformat, "half ", 5) == 0) {
	oldlistformat += 5;
    }
    if (strncmp (oldlistformat, "2 ", 2) == 0) {
	format_columns = 1;
	oldlistformat += 2;
    }
    if (strncmp (oldlistformat, "1 ", 2) == 0) {
	oldlistformat += 2;
    }
    s = strtok (oldlistformat, ",");

    while (s) {
	listbox_add_item (l_listmode, 0, 0, s, NULL);
	s = strtok (NULL, ",");
    }

    /* add listbox to the dialogs */
    add_widget (listmode_dlg, l_listmode);

    radio_columns = radio_new (UY + 1, UX + 32, 2, s_columns);
    add_widget (listmode_dlg, radio_columns);
    radio_columns->sel = format_columns;
    radio_genwidth = radio_new (UY + 1, UX + 2, 2, s_genwidth);
    add_widget (listmode_dlg, radio_genwidth);
    radio_genwidth->sel = format_width;

    return listmode_dlg;
}

static void
listmode_done (Dlg_head *h)
{
    destroy_dlg (h);
    if (0)
	update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    repaint_screen ();
}

static char *
collect_new_format (void)
{
    char *newformat;
    int i;
    char *last;
    char *text, *extra;

    newformat = g_malloc (1024);
    if (radio_genwidth->sel)
	strcpy (newformat, "full ");
    else
	strcpy (newformat, "half ");
    if (radio_columns->sel)
	strcat (newformat, "2 ");
    last = NULL;
    for (i = 0;; i++) {
	listbox_select_by_number (l_listmode, i);
	listbox_get_current (l_listmode, &text, &extra);
	if (text == last)
	    break;
	if (last != NULL)
	    strcat (newformat, ",");
	strcat (newformat, text);
	last = text;
    }
    return newformat;
}

/* Return new format or NULL if the user cancelled the dialog */
char *
listmode_edit (char *oldlistformat)
{
    char *newformat = NULL;
    char *s;
    Dlg_head *listmode_dlg;

    s = g_strdup (oldlistformat);
    listmode_dlg = init_listmode (s);
    g_free (s);

    if (run_dlg (listmode_dlg) == B_ENTER) {
	newformat = collect_new_format ();
    }

    listmode_done (listmode_dlg);
    return newformat;
}

#endif				/* LISTMODE_EDITOR */
