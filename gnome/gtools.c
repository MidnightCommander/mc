#include "util.h"
#include <gnome.h>
#include <gdk/gdkkeysyms.h>
#include "myslang.h"
#include "dlg.h"
#undef HAVE_LIBGPM
#include "mouse.h"
#include "key.h"
#include "widget.h"
#include "wtools.h"
#include "dialog.h"

static int sel_pos;

void query_set_sel (int new_sel)
{
	sel_pos = new_sel;
}

int query_dialog (char *header, char *text, int flags, int count, ...)
{
	va_list ap;
	GtkDialog *dialog;
	GtkWidget *label, *focus_widget;
	int i;
	
	va_start (ap, count);
	dialog = GTK_DIALOG (gtk_dialog_new ());
	label = gtk_label_new (text);
	gtk_widget_show (label);

	if (flags & D_ERROR)
		fprintf (stderr, "Messagebox: this should be displayed like an error\n");
	
	gtk_container_add (GTK_CONTAINER (dialog->vbox), label);
	gtk_container_border_width (GTK_CONTAINER (dialog->vbox), 5);
	
	for (i = 0; i < count; i++){
		GtkWidget *button;
		char *cur_name = va_arg (ap, char *);
		
		button = gtk_button_new_with_label (cur_name);
		gtk_widget_show (button);

		gtk_box_pack_start_defaults (GTK_BOX (GTK_DIALOG (dialog)->action_area), button);

		if (i == sel_pos){
			focus_widget = button;
		}
	}
	if (focus_widget)
		gtk_widget_grab_focus (focus_widget);
	
	gtk_widget_show (GTK_WIDGET (dialog));

	gtk_grab_add (GTK_WIDGET (dialog));
	gtk_main ();
	gtk_grab_remove (GTK_WIDGET (dialog));
	
	sel_pos = 0;
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
	
	query_dialog (header, buffer, error, 1, "&Ok");
}

int
translate_gdk_keysym_to_curses (GdkEventKey *event)
{
	int keyval = event->keyval;
	
	switch (keyval){
	case GDK_BackSpace:
		return KEY_BACKSPACE;
	case GDK_Tab:
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
	
}



