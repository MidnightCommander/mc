/* Desktop management for the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#ifndef GDESKTOP_H
#define GDESKTOP_H

#if 1


/* Snap granularity for desktop icons -- maybe these should be calculated in terms of the font size? */
#define DESKTOP_SNAP_X 80
#define DESKTOP_SNAP_Y 80


/* Configuration options for the desktop */

extern int desktop_use_shaped_icons;		/* Whether to use shaped icons or not (for slow X servers) */
extern int desktop_auto_placement;		/* Whether to auto-place icons or not (user placement) */
extern int desktop_snap_icons;			/* Whether to snap icons to the grid or not */

extern int want_transparent_icons;
extern int want_transparent_text;

/* Initializes the desktop -- init DnD, load the default desktop icons, etc. */
void desktop_init (void);

/* Shuts the desktop down by destroying the desktop icons. */
void desktop_destroy (void);


/* This structure defines the information carried by a desktop icon */
typedef struct desktop_icon_info {
	GtkWidget *dicon;		/* The desktop icon widget */
	int x, y;			/* Position in the desktop */
	int slot;			/* Index of the slot the icon is in, or -1 for none */
	char *filename;			/* The file this icon refers to (relative to the desktop_directory) */
	int selected : 1;		/* Is the icon selected? */
	int tmp_selected : 1;		/* Temp storage for original selection while rubberbanding */
	int finishing_selection : 1;	/* Flag set while we are releasing
					 * button after selecting in the text
					 */
} desktop_icon_info;

void desktop_icon_destroy   (struct desktop_icon_info *dii);
void desktop_icon_open      (struct desktop_icon_info *dii);
void desktop_icon_delete    (struct desktop_icon_info *dii);

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
	char              *pathname;
} desktop_icon_t;

#else

#define MC_LIB_DESKTOP "mc.desktop"

/* Drag and drop types recognized by us */
enum {
	TARGET_URI_LIST,
	TARGET_TEXT_PLAIN,
};


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
	char              *pathname;
} desktop_icon_t;

/* size of the snap to grid size */
#define SNAP_X 80
#define SNAP_Y 80

/* gtrans.c */

extern int want_transparent_icons;
extern int want_transparent_text;

GtkWidget *make_transparent_window (char *file);

/* gdesktop.c */
void drop_on_directory (GtkSelectionData *sel_data, GdkDragContext *context,
			GdkDragAction action, char *dest, int force_manually);
#if 0
void drop_on_directory (GdkEventDropDataAvailable *event, char *dest, int force_manually);
void artificial_drag_start (GdkWindow *source_window, int x, int y);
#endif

void gnome_arrange_icons (void);
void start_desktop (void);
void stop_desktop (void);

/* These get invoked by the context sensitive popup menu in gscreen.c */
void desktop_icon_properties (GtkWidget *widget, desktop_icon_t *di);
void desktop_icon_execute    (GtkWidget *widget, desktop_icon_t *di); 
void desktop_icon_delete     (GtkWidget *widget, desktop_icon_t *di);

/* Pops up the context sensitive menu for a WPanel or a desktop_icon_t */
void file_popup (GdkEventButton *event, void *WPanel_pointer, void *desktop_icon_t_pointer, int row, char *filename);

#endif

#endif
