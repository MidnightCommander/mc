/* Convenience functions for metadata handling in the MIdnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef GMETADATA_H
#define GMETADATA_H


/* Returns the icon filename for the specified file.  You must free the name. */
char *meta_get_icon_for_file (char *filename);

/* Returns the coordinates of the icon corresponding to the specified file.  If no position
 * has been set, returns FALSE.  Else it returns TRUE and sets the *x and *y values.
 */
int meta_get_icon_pos (char *filename, int *x, int *y);

/* Saves the icon position for the specified file */
void meta_set_icon_pos (char *filename, int x, int y);


#endif
