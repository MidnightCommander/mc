/*
 * (C) 1998 the Free Software Foundation
 *
 * Written by Miguel de Icaza
 */
#include <config.h>
#include "util.h"
#include <gnome.h>
#include <gdk/gdkkeysyms.h>
#include "gconf.h"
#include "myslang.h"
#include "dlg.h"
#undef HAVE_LIBGPM
#include "mouse.h"
#include "key.h"
#include "widget.h"
#include "wtools.h"
#include "dialog.h"
#include "color.h"

Dlg_head *last_query_dlg;
static int sel_pos;

void query_set_sel (int new_sel)
{
	sel_pos = new_sel;
}

static void
pack_button (WButton *button, GtkBox *box)
{
	if (!button)
		return;
	gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (button->widget.wdata), 0, 0, 0);
}

int query_dialog (char *header, char *text, int flags, int count, ...)
{
	va_list ap;
	Dlg_head  *h;
	WLabel    *label;
	GtkDialog *dialog;
	GList     *list;
	int i, result = -1;
	
	if (header == MSG_ERROR)
		header = _("Error");
	
	h = create_dlg (0, 0, 0, 0, dialog_colors, default_dlg_callback, "[QueryBox]", "query",
			DLG_NO_TED | DLG_NO_TOPLEVEL);
	dialog = GTK_DIALOG (gtk_dialog_new ());
	x_dlg_set_window (h, dialog);
	gtk_window_set_policy (GTK_WINDOW (dialog), 0, 0, 0);

	x_set_dialog_title (h, header);
	
	label = label_new (0, 0, text, NULL);
	add_widget (h, label);
	
	if (flags & D_ERROR)
		fprintf (stderr, "Messagebox: this should be displayed like an error\n");
	
	va_start (ap, count);
	list = g_list_alloc ();
	for (i = 0; i < count; i++){
		WButton *button;
		char *cur_name = va_arg (ap, char *);
		
		button = button_new (0, 0, B_USER+i, NORMAL_BUTTON, cur_name, 0, 0, NULL);

		add_widget (h, button);
		list = g_list_append (list, button);

		if (i == sel_pos)
			h->initfocus = h->current;
	}
	va_end (ap);
	
	/* Init the widgets */
	init_dlg (h);
	if (!(flags & D_INSERT))
		gtk_grab_add (GTK_WIDGET (dialog));

	/* Now do the GTK stuff */
	gtk_container_add (GTK_CONTAINER (dialog->vbox), GTK_WIDGET (label->widget.wdata));
	gtk_container_border_width (GTK_CONTAINER (dialog->vbox), 5);

	g_list_foreach (list, (GFunc) pack_button, dialog->action_area);
	g_list_free (list);
	
	gtk_widget_show (GTK_WIDGET (dialog));

	if (flags & D_INSERT){
		last_query_dlg = h;
		return 0;
	}
	
	frontend_run_dlg (h);
	dlg_run_done (h);
	last_query_dlg = h;
	gtk_grab_remove (GTK_WIDGET (dialog));

	switch (h->ret_value){
	case B_CANCEL:
		break;
	default:
		result = h->ret_value - B_USER;
	}
	destroy_dlg (h);
	
	sel_pos = 0;
	return result;
}

/* To show nice messages to the users */
Dlg_head *message (int error, char *header, char *text, ...)
{
	va_list  args;
	char     buffer [4096];
	Dlg_head *d;

	/* Setup the display information */
	strcpy (buffer, "\n");
	va_start (args, text);
	vsprintf (&buffer [1], text, args);
	strcat (buffer, "\n");
	va_end (args);
	
	query_dialog (header, buffer, error, 1, _("&Ok"));
	d = last_query_dlg;

	if (error & D_INSERT){
		x_flush_events ();
		return d;
	}
	return d;
}

int
translate_gdk_keysym_to_curses (GdkEventKey *event)
{
	int keyval = event->keyval;
	
	switch (keyval){
	case GDK_BackSpace:
		return KEY_BACKSPACE;
	case GDK_Tab:
		if (event->state & GDK_SHIFT_MASK)
			return '\t'; /* return KEY_BTAB */
		return '\t';
	case GDK_KP_Enter:
	case GDK_Return:
		return '\n';
	case GDK_Escape:
		return 27;
	case GDK_Delete:
	case GDK_KP_Delete:
		return KEY_DC;
	case GDK_KP_Home:
	case GDK_Home:
		return KEY_HOME;
	case GDK_KP_End:
	case GDK_End:
		return KEY_END;
	case GDK_KP_Left:
	case GDK_Left:
		return KEY_LEFT;
	case GDK_KP_Right:
	case GDK_Right:
		return KEY_RIGHT;
	case GDK_KP_Up:
	case GDK_Up:
		return KEY_UP;
	case GDK_KP_Down:
	case GDK_Down:
		return KEY_DOWN;
	case GDK_KP_Page_Up:
	case GDK_Page_Up:
		return KEY_PPAGE;
	case GDK_KP_Page_Down:
	case GDK_Page_Down:
		return KEY_NPAGE;
	case GDK_KP_Insert:
	case GDK_Insert:
		return KEY_IC;
	case GDK_Menu:
		return KEY_F(9);
	case GDK_F1:
		return KEY_F(1);
	case GDK_F2:
		return KEY_F(2);
	case GDK_F3:
		return KEY_F(3);
	case GDK_F4:
		return KEY_F(4);
	case GDK_F5:
		return KEY_F(5);
	case GDK_F6:
		return KEY_F(6);
	case GDK_F7:
		return KEY_F(7);
	case GDK_F8:
		return KEY_F(8);
	case GDK_F9:
		return KEY_F(9);
	case GDK_F10:
		return KEY_F(10);
	case GDK_F11:
		return KEY_F(11);
	case GDK_F12:
		return KEY_F(12);
	case GDK_F13:
		return KEY_F(13);
	case GDK_F14:
		return KEY_F(14);
	case GDK_F15:
		return KEY_F(15);
	case GDK_F16:
		return KEY_F(16);
	case GDK_F17:
		return KEY_F(17);
	case GDK_F18:
		return KEY_F(18);
	case GDK_F19:
		return KEY_F(19);
	case GDK_F20:
		return KEY_F(20);

		/* The keys we ignore */
	case GDK_Control_L:
	case GDK_Control_R:
	case GDK_Meta_L:
	case GDK_Meta_R:
	case GDK_Alt_L:
	case GDK_Alt_R:
	case GDK_Shift_L:
	case GDK_Shift_R:
		return -1;
				 
	default:
		if ((event->keyval >= 0x20) && (event->keyval <= 0xff)){
			int key = event->keyval;

			if (event->state & GDK_CONTROL_MASK){
				return key - 'a' + 1;
			}

			if (event->state & GDK_MOD1_MASK){
				return ALT (key);
			}

			return key;
		}
	}

	return -1;
}



