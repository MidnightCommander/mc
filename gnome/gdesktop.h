/* Desktop management for the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#ifndef GDESKTOP_H
#define GDESKTOP_H


/* Snap granularity for desktop icons -- maybe these should be calculated in
 * terms of the font size?
 */
#define DESKTOP_SNAP_X 80
#define DESKTOP_SNAP_Y 80


/* Configuration options for the desktop */

extern int desktop_use_shaped_icons;	/* Whether to use shaped icons or not (for slow X servers) */
extern int desktop_auto_placement;	/* Whether to auto-place icons or not (user placement) */
extern int desktop_snap_icons;		/* Whether to snap icons to the grid or not */

extern int tree_panel_visible;	        

/* Initializes the desktop -- init DnD, load the default desktop icons, etc. */
void desktop_init (void);

/* Shuts the desktop down by destroying the desktop icons. */
void desktop_destroy (void);


/* This structure defines the information carried by a desktop icon */
typedef struct {
	GtkWidget *dicon;		/* The desktop icon widget */
	int x, y;			/* Position in the desktop */
	int slot;			/* Index of the slot the icon is in, or -1 for none */
	char *filename;			/* The file this icon refers to
                                         * (relative to the desktop_directory)
					 */
	int selected : 1;		/* Is the icon selected? */
	int tmp_selected : 1;		/* Temp storage for original selection while rubberbanding */
	int finishing_selection : 1;	/* Flag set while we are releasing
					 * button after selecting in the text
					 */
} DesktopIconInfo;

void desktop_icon_info_destroy   (DesktopIconInfo *dii);
void desktop_icon_info_open      (DesktopIconInfo *dii);
void desktop_icon_info_delete    (DesktopIconInfo *dii);

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


#endif
