/* Desktop management for the Midnight Commander
 *
 * Copyright (C) 1998-1999 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#ifndef GDESKTOP_H
#define GDESKTOP_H

#include "dir.h"


/* Snap granularity for desktop icons -- maybe these should be calculated in
 * terms of the font size?
 */
#define DESKTOP_SNAP_X 80
#define DESKTOP_SNAP_Y 80

/* Name of the user's desktop directory (i.e. ~/desktop) */
#define DESKTOP_DIR_NAME ".gnome-desktop"


/* Configuration options for the desktop */

extern int desktop_use_shaped_icons;	/* Whether to use shaped icons or not (for slow X servers) */
extern int desktop_use_shaped_text;     /* Shaped text for the icons on the desktop */
extern int desktop_auto_placement;	/* Whether to auto-place icons or not (user placement) */
extern int desktop_snap_icons;		/* Whether to snap icons to the grid or not */
extern int desktop_arr_r2l; /* Arrange from right to left */
extern int desktop_arr_b2t; /* Arrange from bottom to top */
extern int desktop_arr_rows; /* Arrange in rows instead of columns */
extern char *desktop_directory;

/* Menu items for arranging the desktop icons */
extern GnomeUIInfo desktop_arrange_icons_items[];

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
	char *url;			/* URL this icon points to */
	int selected : 1;		/* Is the icon selected? */
	int tmp_selected : 1;		/* Temp storage for original selection while rubberbanding */
} DesktopIconInfo;

void desktop_icon_info_open      (DesktopIconInfo *dii);
void desktop_icon_info_delete    (DesktopIconInfo *dii);
void desktop_icon_set_busy       (DesktopIconInfo *dii, int busy);

DesktopIconInfo *desktop_icon_info_get_by_filename (char *filename);

file_entry *file_entry_from_file (char *filename);
void        file_entry_free      (file_entry *fe);

gboolean    is_mountable (char *filename, file_entry *fe, int *is_mounted, char **mount_point);
gboolean    is_ejectable (char *filename);
gboolean    do_mount_umount (char *filename, gboolean is_mount);
gboolean    do_eject (char *filename);

void desktop_rescan_devices (void);
void desktop_recreate_default_icons (void);
void desktop_reload_icons (int user_pos, int xpos, int ypos);
void desktop_create_url (const char *filename, const char *title, const char *url, const char *icon);

extern int desktop_wm_is_gnome_compliant;

#endif
