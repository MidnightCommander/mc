/* Convenience functions for metadata handling in the MIdnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include "fs.h"
#include <libgnome/libgnome.h>
#include "gmetadata.h"
#include <sys/stat.h>
#include "../vfs/vfs.h"


#define ICON_POSITION "icon-position"


/**
 * gmeta_get_icon_pos
 * @filename:	The file under ~/desktop for which to get the icon position
 * @x:		The x position will be stored here.  Must be non-NULL.
 * @y:		The y position will be stored here.  Must be non-NULL.
 *
 * Checks if the specified file has an icon position associated to it.  If so, returns TRUE and
 * fills in the x and y values.  Otherwise it returns FALSE and x and y are not modified.
 *
 * Icon position information is expected to be saved using the gmeta_set_icon_pos() function.
 */
int
gmeta_get_icon_pos (char *filename, int *x, int *y)
{
	int size;
	char *buf;
	int tx, ty;

	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (x != NULL, FALSE);
	g_return_val_if_fail (y != NULL, FALSE);

	if (gnome_metadata_get (filename, ICON_POSITION, &size, &buf) != 0)
		return FALSE;

	if (!buf || (sscanf (buf, "%d%d", &tx, &ty) != 2)) {
		g_warning ("Invalid metadata for \"%s\"'s icon position, using auto-placement", filename);
		return FALSE;
	}

	*x = tx;
	*y = ty;
	return TRUE;
}

/**
 * gmeta_set_icon_pos
 * @filename:	The file for which to save icon position information
 * @x:		X position of the icon
 * @y:		Y position of the icon
 *
 * Saves the icon position information for the specified file.  This is expected to be read back
 * using the gmeta_get_icon_pos() function.
 */
void
gmeta_set_icon_pos (char *filename, int x, int y)
{
	char buf[100];

	g_return_if_fail (filename != NULL);

	sprintf (buf, "%d %d", x, y);

	if (gnome_metadata_set (filename, ICON_POSITION, strlen (buf) + 1, buf) != 0)
		g_warning ("Error setting the icon position metadata for \"%s\"", filename);
}
