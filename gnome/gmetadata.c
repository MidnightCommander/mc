/* Convenience functions for metadata handling in the MIdnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include <sys/stat.h>
#include <libgnome/libgnome.h>
#include "gmetadata.h"


#define ICON_FILENAME "icon-filename"
#define ICON_POSITION "icon-position"


/**
 * meta_get_icon_for_file
 * @filename:	The name of the file to get the icon for.
 *
 * Computes the name of the file that holds the icon for the specified file.  The
 * resulting string is guaranteed to be non-NULL.  You have to free this string
 * on your own.
 *
 * Returns the icon's file name.
 */
char *
meta_get_icon_for_file (char *filename)
{
	int size;
	char *buf;
	struct stat s;
	int retval;

	g_return_if_fail (filename != NULL);

	if (gnome_metadata_get (filename, ICON_FILENAME, &size, &buf) != 0) {
		/* Return a default icon */

		retval = mc_stat (filename, &s);

		if (!retval && S_ISDIR (s.st_mode))
			return gnome_unconditional_pixmap_file ("gnome-folder.png");
		else
			return gnome_unconditional_pixmap_file ("gnome-unknown.png");
	}

	return buf;
}

/**
 * meta_get_desktop_icon_pos
 * @filename:	The file under ~/desktop for which to get the icon position
 * @x:		The x position will be stored here.  Must be non-NULL.
 * @y:		The y position will be stored here.  Must be non-NULL.
 *
 * Checks if the specified file in the user's desktop directory has an icon position
 * associated to it.  If so, returns TRUE and fills in the x and y values.  Otherwise
 * it returns FALSE and x and y are not modified.
 */
int
meta_get_desktop_icon_pos (char *filename, int *x, int *y)
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
