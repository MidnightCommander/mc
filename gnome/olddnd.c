#ifdef OLD_DND
/*
 * Handler for text/plain and url:ALL drag types
 *
 * Sets the drag information to the filenames selected on the panel
 */
static void
panel_transfer_file_names (GtkWidget *widget, GdkEventDragRequest *event, WPanel *panel, int *len, char **data)
{
	*data = panel_build_selected_file_list (panel, len);
}

/*
 * Handler for file:ALL type (target application only understands local pathnames)
 *
 * Makes local copies of the files and transfers the filenames.
 */
static void
panel_make_local_copies_and_transfer (GtkWidget *widget, GdkEventDragRequest *event, WPanel *panel,
				      int *len, char **data)
{
	char *filename, *localname;
	int i;
	
	if (panel->marked){
		char **local_names_array, *p;
		int j, total_len;

		/* First assemble all of the filenames */
		local_names_array = malloc (sizeof (char *) * panel->marked);
		total_len = j = 0;
		for (i = 0; i < panel->count; i++){
			char *filename;

			if (!panel->dir.list [i].f.marked)
				continue;
			
			filename = concat_dir_and_file (panel->cwd, panel->dir.list [i].fname);
			localname = mc_getlocalcopy (filename);
			total_len += strlen (localname) + 1;
			local_names_array [j++] = localname;
			free (filename);
		}
		*len = total_len;
		*data = p = malloc (total_len);
		for (i = 0; i < j; i++){
			strcpy (p, local_names_array [i]);
			g_free (local_names_array [i]);
			p += strlen (p) + 1;
		}
	} else {
		filename = concat_dir_and_file (panel->cwd, panel->dir.list [panel->selected].fname);
		localname = mc_getlocalcopy (filename);
		free (filename);
		*data = localname;
		*len = strlen (localname + 1);
	}
}

static void
panel_drag_request (GtkWidget *widget, GdkEventDragRequest *event, WPanel *panel, int *len, char **data)
{
	*len = 0;
	*data = 0;
	
	if ((strcmp (event->data_type, "text/plain") == 0) ||
	    (strcmp (event->data_type, "url:ALL") == 0)){
		panel_transfer_file_names (widget, event, panel, len, data);
	} else if (strcmp (event->data_type, "file:ALL") == 0){
		if (vfs_file_is_local (panel->cwd))
			panel_transfer_file_names (widget, event, panel, len, data);
		else
			panel_make_local_copies_and_transfer (widget, event, panel, len, data);
	}
}

/*
 * Listing mode: drag request handler
 */
static void
panel_clist_drag_request (GtkWidget *widget, GdkEventDragRequest *event, WPanel *panel)
{
	GdkWindowPrivate *clist_window = (GdkWindowPrivate *) (GTK_WIDGET (widget)->window);
	GdkWindowPrivate *clist_areaw  = (GdkWindowPrivate *) (GTK_CLIST (widget)->clist_window);
	char *data;
	int len;

	panel_drag_request (widget, event, panel, &len, &data);
	
	/* Now transfer the DnD information */
	if (len && data){
		if (clist_window->dnd_drag_accepted)
			gdk_window_dnd_data_set ((GdkWindow *)clist_window, (GdkEvent *) event, data, len);
		else
			gdk_window_dnd_data_set ((GdkWindow *)clist_areaw, (GdkEvent *) event, data, len);
		free (data);
	}
}

/*
 * Invoked when a drop has happened on the panel
 */
static void
panel_clist_drop_data_available (GtkWidget *widget, GdkEventDropDataAvailable *data, WPanel *panel)
{
	gint winx, winy;
	gint dropx, dropy;
	gint row;
	char *drop_dir;
	
	gdk_window_get_origin (GTK_CLIST (widget)->clist_window, &winx, &winy);
	dropx = data->coords.x - winx;
	dropy = data->coords.y - winy;

	if (dropx < 0 || dropy < 0)
		return;

	if (gtk_clist_get_selection_info (GTK_CLIST (widget), dropx, dropy, &row, NULL) == 0)
		drop_dir = panel->cwd;
	else {
		g_assert (row < panel->count);

	}
#if 0
	drop_on_directory (data, drop_dir, 0);
#endif

	if (drop_dir != panel->cwd)
		free (drop_dir);

	update_one_panel_widget (panel, 0, UP_KEEPSEL);
	panel_update_contents (panel);
}

static void
panel_drag_begin (GtkWidget *widget, GdkEvent *event, WPanel *panel)
{
	GdkPoint hotspot = { 15, 15 };

	if (panel->marked > 1){
		if (drag_multiple && drag_multiple_ok){
			gdk_dnd_set_drag_shape (drag_multiple->window, &hotspot,
						drag_multiple_ok->window, &hotspot);
			gtk_widget_show (drag_multiple);
			gtk_widget_show (drag_multiple_ok);
		}
			
	} else {
		if (drag_directory && drag_directory_ok)
			gdk_dnd_set_drag_shape (drag_directory->window, &hotspot,
						drag_directory_ok->window, &hotspot);	
			gtk_widget_show (drag_directory_ok);
			gtk_widget_show (drag_directory);
	}

}

static void
panel_icon_list_drag_begin (GtkWidget *widget, GdkEvent *event, WPanel *panel)
{
	GnomeIconList *icons = GNOME_ICON_LIST (panel->icons);
	
	icons->last_clicked = NULL;
	panel_drag_begin (widget, event, panel);
}

static void
panel_artificial_drag_start (GtkCList *window, GdkEventMotion *event)
{
	artificial_drag_start (window->clist_window, event->x, event->y);
}
#endif /* OLD_DND */

#if OLD_DND
static void
panel_icon_list_artificial_drag_start (GtkObject *obj, GdkEventMotion *event)
{
	artificial_drag_start (GTK_WIDGET (obj)->window, event->x, event->y);
}

/*
 * Icon view drag request handler
 */
static void
panel_icon_list_drag_request (GtkWidget *widget, GdkEventDragRequest *event, WPanel *panel)
{
	char *data;
	int len;

	panel_drag_request (widget, event, panel, &len, &data);

	if (len && data){
		gdk_window_dnd_data_set (widget->window, (GdkEvent *) event, data, len);
		free (data);
	}
}

static void
panel_icon_list_drop_data_available (GtkWidget *widget, GdkEventDropDataAvailable *data, WPanel *panel)
{
	GnomeIconList *ilist = GNOME_ICON_LIST (widget);
	gint winx, winy;
	gint dropx, dropy;
	gint item;
	char *drop_dir;
	
	gdk_window_get_origin (widget->window, &winx, &winy);
	dropx = data->coords.x - winx;
	dropy = data->coords.y - winy;

	if (dropx < 0 || dropy < 0)
		return;

	item = gnome_icon_list_get_icon_at (ilist, dropx, dropy);
	if (item == -1)
		drop_dir = panel->cwd;
	else {
		g_assert (item < panel->count);

		if (S_ISDIR (panel->dir.list [item].buf.st_mode))
			drop_dir = concat_dir_and_file (panel->cwd, panel->dir.list [item].fname);
		else 
			drop_dir = panel->cwd;
	}
#if 0
	drop_on_directory (data, drop_dir, 0);
#endif

	if (drop_dir != panel->cwd)
		free (drop_dir);

	update_one_panel_widget (panel, 0, UP_KEEPSEL);
	panel_update_contents (panel);
}
#endif
