#ifndef _GDESKTOP_H
#define _GDESKTOP_H

#define MC_LIB_DESKTOP "mc.desktop"


/* Types of desktop icons:
 *
 * o Application: Double click: start up application;
 *                Dropping:     start up program with arguments.
 *
 * o Directory:   Double click: opens the directory in a panel.
 *                Double click: copies/moves files.
 *
 * o File:        Opens the application according to regex_command
 */ 
		  
typedef enum {
	application,
	directory,
	file
} icon_t;

/* A structure that describes each icon on the desktop */
typedef struct {
	GnomeDesktopEntry *dentry;
	GtkWidget         *widget;
	icon_t            type;
	int               x, y;
	int               grid_x, grid_y;
	char              *title;
	char              *pathname;
} desktop_icon_t;

/* size of the snap to grid size */
#define SNAP_X 80
#define SNAP_Y 80

/* gtrans.c */

extern int want_transparent_icons;
extern int want_transparent_text;

GtkWidget *create_transparent_text_window (char *file, char *text, int extra_events);
GtkWidget *make_transparent_window (char *file);

/* gdesktop.c */
void drop_on_directory (GdkEventDropDataAvailable *event, char *dest, int force_manually);
void gnome_arrange_icons (void);
void artificial_drag_start (GdkWindow *source_window, int x, int y);
extern int icons_snap_to_grid;

#endif
